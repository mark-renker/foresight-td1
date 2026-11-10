// FORESIGHT TD-1 — td1-core DSP kernel. See Kernel.h for the contract.
// Order of operations inside the per-sample loop deliberately mirrors
// TShaperKernel.process() in prototype/foresight_td1_v3.html.
#include "td1/Kernel.h"

#include <algorithm>
#include <cmath>

namespace td1 {

namespace {
inline double clamp01(double x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
inline double dbToLin(double db) { return std::pow(10.0, db * 0.05); }
inline double linToDb(double g) { return 20.0 * std::log10(std::max(1e-6, g)); }
inline int nextPow2(int v) { int p = 1; while (p < v) p <<= 1; return p; }

constexpr double kLensPreMs  = 16.0;
constexpr double kLensPostMs = 28.0;
constexpr double kHitMinIntervalMs = 20.0;
constexpr double kPreLeadMs = 0.5;   // pre-window releases this long before the onset
} // namespace

int Kernel::lookSamplesFor(float ms) const {
    const double v = std::floor((double)ms * 0.001 * sr_ + 0.5);   // JS Math.round
    return (int)std::min<double>(std::max(0.0, v), (double)(maxD_ - 2));
}

double Kernel::c(double ms) const { return 1.0 - std::exp(-1.0 / (sr_ * ms * 0.001)); }

void Kernel::prepare(double sampleRate) {
    sr_ = sampleRate > 0 ? sampleRate : 48000.0;
    maxD_ = (int)std::ceil(sr_ * 0.012) + 4;   // reference: ceil(sr*0.012)+4
    dL_.assign((size_t)maxD_, 0.0f);
    dR_.assign((size_t)maxD_, 0.0f);
    for (auto& ch : ch_) ch.wHist.assign((size_t)maxD_, 0.0f);

    rampLen_  = std::max(1, (int)std::lround(sr_ * 0.020));
    lookXLen_ = std::max(1, (int)std::lround(sr_ * 0.010));
    dipLen_   = std::max(1, (int)std::lround(sr_ * 0.004));
    bypLen_   = std::max(1, (int)std::lround(sr_ * 0.010));
    subLen_   = std::max(16, (int)std::lround(sr_ * 0.001));

    const int ringLen = nextPow2((int)std::ceil(sr_ * (kLensPreMs + kLensPostMs + 10.0) * 0.001));
    rMask_ = ringLen - 1;
    rDry_.assign((size_t)ringLen, 0.0f);
    rOut_.assign((size_t)ringLen, 0.0f);
    rGain_.assign((size_t)ringLen, 1.0f);
    rPre_.assign((size_t)ringLen, 0.0f);
    reset();
}

void Kernel::reset() {
    std::fill(dL_.begin(), dL_.end(), 0.0f);
    std::fill(dR_.begin(), dR_.end(), 0.0f);
    wIdx_ = 0;
    for (auto& ch : ch_) {
        ch.hpLp = ch.eAF = ch.eAS = ch.eSF = ch.eSS = 0;
        ch.g = 1; ch.wAs = 0; ch.tl = 0; ch.adX1 = 0; ch.preWs = 0; ch.wA = 0;
        std::fill(ch.wHist.begin(), ch.wHist.end(), 0.0f);
    }
    msD_ = msW_ = 0; corr_ = 1;

    look_ = lookOld_ = lookSamplesFor(target_.lookaheadMs);
    lookX_.snap(1.0);
    mix_.snap(target_.mix);
    out_.snap(dbToLin(target_.outputDb));
    tilt_.snap(target_.tiltDb);
    pre_.snap(target_.preDb);
    cachedTilt_ = 1e9;

    pending_.target = target_.target; pending_.clip = target_.clip;
    pending_.delta = target_.delta;   pending_.audition = target_.audition;
    active_ = pending_;
    dip_.snap(1.0); dipState_ = 0;
    byp_.snap(target_.bypass ? 1.0 : 0.0);

    subCnt_ = 0; frame_ = MeterFrame{}; fGainMax_ = 0; fGainMin_ = 1e9;
    pos_ = 0; lastHitPos_ = -1000000; pendingHitOnset_ = -1;
    lensCaptureAt_ = -1;
    std::fill(rDry_.begin(), rDry_.end(), 0.0f);
    std::fill(rOut_.begin(), rOut_.end(), 0.0f);
    std::fill(rGain_.begin(), rGain_.end(), 1.0f);
    std::fill(rPre_.begin(), rPre_.end(), 0.0f);
}

void Kernel::setParams(const Params& p) {
    target_ = p;
    if (maxD_ == 0) return;   // not prepared yet; reset() at prepare snaps

    mix_.set(p.mix, rampLen_);
    out_.set(dbToLin(p.outputDb), rampLen_);
    tilt_.set(p.tiltDb, rampLen_);
    pre_.set(p.preDb, rampLen_);
    byp_.set(p.bypass ? 1.0 : 0.0, bypLen_);

    pending_.target = p.target; pending_.clip = p.clip;
    pending_.delta = p.delta;   pending_.audition = p.audition;

    const int nl = lookSamplesFor(p.lookaheadMs);
    if (nl != look_) {
        // Crossfade from wherever we currently are to the new tap.
        lookOld_ = lookX_.moving() ? lookOld_ : look_;
        look_ = nl;
        lookX_.snap(0.0);
        lookX_.set(1.0, lookXLen_);
    }
}

void Kernel::computeCoeffs(Coeffs& k) const {
    const auto& p = target_;
    const double T = 1.0 / std::max(0.25, (double)p.speed);
    k.cAFa = c(0.08);      k.cASa = c(12 * T);  k.cArel = c(30 * T);
    k.cSatk = c(0.5);      k.cSFr = c(45 * T);  k.cSSr = c(450 * T);
    k.cGup = c(0.12);      k.cGdn = c(6);       k.cW = c(0.8);
    k.cTl = 1 - std::exp(-2 * M_PI * std::max(100.0, (double)p.pivotHz) / sr_);
    k.cHp = 1 - std::exp(-2 * M_PI * std::max(10.0, (double)p.hpfHz) / sr_);
    k.cR = c(400); k.cC = c(200); k.cPre = c(0.3);
    k.aDb = p.attackDb; k.sDb = p.sustainDb;
}

double Kernel::updEnv(Ch& st, double a, const Coeffs& k, bool gateOn, double gateDb) {
    st.eAF += (a > st.eAF ? k.cAFa : k.cArel) * (a - st.eAF);
    st.eAS += (a > st.eAS ? k.cASa : k.cArel) * (a - st.eAS);
    st.eSF += (a > st.eSF ? k.cSatk : k.cSFr) * (a - st.eSF);
    st.eSS += (a > st.eSS ? k.cSatk : k.cSSr) * (a - st.eSS);
    double wA = (st.eAF - st.eAS) / (st.eAF + 1e-4);
    if (wA < 0) wA = 0; if (wA > 1) wA = 1;
    double wS = (st.eSS - st.eSF) / (st.eSS + 1e-4);
    if (wS < 0) wS = 0; if (wS > 1) wS = 1;
    if (gateOn) {   // threshold enabled: soft-knee (6 dB) gate on the fast envelope
        const double lvl = 20.0 * std::log10(st.eAF + 1e-12);
        wA *= clamp01((lvl - gateDb) / 6.0 + 0.5);
    }
    st.wAs += k.cW * (wA - st.wAs);
    const double gdb = k.aDb * wA + k.sDb * wS;
    const double gt = std::pow(10.0, gdb * 0.05);
    st.g += (gt > st.g ? k.cGup : k.cGdn) * (gt - st.g);
    st.wA = wA;
    return wA;
}

double Kernel::logcosh(double v) const {
    const double vc = std::max(-20.0, std::min(20.0, v));
    return std::log(std::cosh(vc)) + std::max(0.0, std::abs(v) - 20.0);
}

double Kernel::adaa(Ch& st, double x) {
    const double x1 = st.adX1, dx = x - x1;
    const double y = std::abs(dx) > 1e-6 ? (logcosh(x) - logcosh(x1)) / dx
                                         : std::tanh(0.5 * (x + x1));
    st.adX1 = x;
    return y;
}

void Kernel::process(const float* inL, const float* inR, float* outL, float* outR, int n) {
    if (maxD_ == 0 || n <= 0) return;
    Coeffs k; computeCoeffs(k);
    const bool gateOn = target_.thresholdDb > kThresholdOffDb;
    const double gateDb = target_.thresholdDb;
    const bool link = target_.link;
    const int M = maxD_;
    const int preLead = (int)std::lround(sr_ * kPreLeadMs * 0.001);
    const int hitMin = (int)std::lround(sr_ * kHitMinIntervalMs * 0.001);
    const int hitLook = (int)std::lround(sr_ * 0.002);
    Ch& s0 = ch_[0];
    Ch& s1 = ch_[1];

    for (int i = 0; i < n; ++i) {
        // --- discrete switch state machine (dip -> switch at zero -> rise)
        if (dipState_ == 0 && pending_ != active_) { dipState_ = 1; dip_.set(0.0, dipLen_); }
        const double dip = dip_.next();
        if (dipState_ == 1 && !dip_.moving()) {
            if (active_.clip != pending_.clip) { s0.adX1 = 0; s1.adX1 = 0; }
            active_ = pending_;
            dipState_ = 2; dip_.set(1.0, dipLen_);
        } else if (dipState_ == 2 && !dip_.moving()) {
            dipState_ = 0;
        }
        const int ms = (int)active_.target;

        const double mix = mix_.next();
        const double outG = out_.next();
        const double tiltDb = tilt_.next();
        if (tiltDb != cachedTilt_) {
            cachedTilt_ = tiltDb;
            gHi_ = std::pow(10.0, tiltDb / 40.0);
            gLo_ = std::pow(10.0, -tiltDb / 40.0);
        }
        const double preDb = pre_.next();
        const double byp = byp_.next();

        // --- input + delay (raw L/R stored; encode at read)
        const float x0f = inL ? inL[i] : 0.0f;
        const float x1f = inR ? inR[i] : x0f;
        const double x0 = x0f, x1 = x1f;
        const double w0 = ms ? (x0 + x1) * 0.5 : x0;
        const double w1 = ms ? (x0 - x1) * 0.5 : x1;
        dL_[(size_t)wIdx_] = x0f;
        dR_[(size_t)wIdx_] = x1f;

        double rl, rr;
        {
            int r = wIdx_ - look_; if (r < 0) r += M;
            rl = dL_[(size_t)r]; rr = dR_[(size_t)r];
            if (lookX_.moving()) {
                const double xf = lookX_.next();
                int ro = wIdx_ - lookOld_; if (ro < 0) ro += M;
                rl = dL_[(size_t)ro] * (1 - xf) + rl * xf;
                rr = dR_[(size_t)ro] * (1 - xf) + rr * xf;
            }
        }
        const double dw0 = ms ? (rl + rr) * 0.5 : rl;
        const double dw1 = ms ? (rl - rr) * 0.5 : rr;
        const int wCur = wIdx_;
        if (++wIdx_ >= M) wIdx_ = 0;

        // --- detection
        s0.hpLp += k.cHp * (w0 - s0.hpLp); const double h0 = w0 - s0.hpLp, a0 = std::abs(h0);
        s1.hpLp += k.cHp * (w1 - s1.hpLp); const double h1 = w1 - s1.hpLp, a1 = std::abs(h1);
        const bool lnk = (ms == 0) && link;
        const double lm = a0 > a1 ? a0 : a1;
        const double wA0 = updEnv(s0, lnk ? lm : a0, k, gateOn, gateDb);
        const double wA1 = updEnv(s1, lnk ? lm : a1, k, gateOn, gateDb);

        // --- pre-transient window weight: onset is in the detector but not yet in the audio
        const int preDel = std::max(0, look_ - preLead);
        double gPre0 = 1.0, gPre1 = 1.0;
        {
            s0.wHist[(size_t)wCur] = (float)wA0;
            s1.wHist[(size_t)wCur] = (float)wA1;
            int rp = wCur - preDel; if (rp < 0) rp += M;
            const double pw0 = look_ > 0 ? clamp01(wA0 - s0.wHist[(size_t)rp]) : 0.0;
            const double pw1 = look_ > 0 ? clamp01(wA1 - s1.wHist[(size_t)rp]) : 0.0;
            s0.preWs += k.cPre * (pw0 - s0.preWs);
            s1.preWs += k.cPre * (pw1 - s1.preWs);
            if (preDb != 0.0) {
                gPre0 = std::pow(10.0, preDb * s0.preWs * 0.05);
                gPre1 = std::pow(10.0, preDb * s1.preWs * 0.05);
            }
        }

        // --- shaping (reference shapeMix, plus pre gain)
        auto shapeMix = [&](Ch& st, double xd, double gPre) {
            st.tl += k.cTl * (xd - st.tl);
            const double hpP = xd - st.tl;
            const double tilted = st.tl * gLo_ + hpP * gHi_;
            const double xt = xd + (tilted - xd) * st.wAs;
            const double wet = xt * (st.g * gPre);
            return xd + (wet - xd) * mix;
        };
        const double p0 = (ms == 2) ? dw0 : shapeMix(s0, dw0, gPre0);
        const double p1 = (ms == 1) ? dw1 : shapeMix(s1, dw1, gPre1);
        const double pL = ms ? p0 + p1 : p0, pR = ms ? p0 - p1 : p1;
        const double dL = ms ? dw0 + dw1 : dw0, dR = ms ? dw0 - dw1 : dw1;

        // --- loudness-matched auto gain
        msD_ += k.cR * ((dL * dL + dR * dR) - msD_);
        msW_ += k.cR * ((pL * pL + pR * pR) - msW_);
        const double corrT = std::min(4.0, std::max(0.25, std::sqrt((msD_ + 1e-9) / (msW_ + 1e-9))));
        corr_ += k.cC * (corrT - corr_);
        const double cEff = (target_.autoGain && !active_.delta) ? corr_ : 1.0;

        // --- output stage
        double y0, y1;
        if (active_.audition) { y0 = h0; y1 = h1; }
        else {
            y0 = active_.delta ? (pL - dL) : pL * cEff;
            y1 = active_.delta ? (pR - dR) : pR * cEff;
            if (active_.clip) { y0 = adaa(s0, y0); y1 = adaa(s1, y1); }
            y0 *= outG; y1 *= outG;
        }
        if (dip != 1.0) { y0 *= dip; y1 *= dip; }
        if (byp != 0.0) {
            if (byp >= 1.0) { y0 = dL; y1 = dR; }
            else { y0 = y0 * (1 - byp) + dL * byp; y1 = y1 * (1 - byp) + dR * byp; }
        }
        outL[i] = (float)y0;
        if (outR) outR[i] = (float)y1;

        // --- telemetry -------------------------------------------------------
        // display channel: the shaped component (mid/side) or the stereo channel
        // with the larger excursion
        double gDisp, preDisp;
        {
            const double ga = s0.g * gPre0, gb = s1.g * gPre1;
            if (ms == 1) { gDisp = ga; preDisp = s0.preWs; }
            else if (ms == 2) { gDisp = gb; preDisp = s1.preWs; }
            else {
                const double ra = ga > 1 ? ga : 1 / ga, rb = gb > 1 ? gb : 1 / gb;
                gDisp = ra >= rb ? ga : gb; preDisp = std::max(s0.preWs, s1.preWs);
            }
            if (preDb == 0.0) preDisp = 0.0;   // only show the pre window when it is acting
        }
        // hit detection on the detector timeline (a fast rise in the attack
        // weight; the ratio detector holds wA through decays, so levels alone
        // can't re-arm); displayed when the onset reaches the output
        {
            int rh = wCur - hitLook; if (rh < 0) rh += M;
            const double past = ms == 1 ? s0.wHist[(size_t)rh]
                              : ms == 2 ? s1.wHist[(size_t)rh]
                              : std::max(s0.wHist[(size_t)rh], s1.wHist[(size_t)rh]);
            const double wHit = ms == 1 ? wA0 : (ms == 2 ? wA1 : std::max(wA0, wA1));
            if (wHit - past > 0.35 && pos_ - lastHitPos_ >= hitMin) {
                lastHitPos_ = pos_;
                pendingHitOnset_ = pos_ + look_;
                if (lensCaptureAt_ < 0) {
                    lensOnset_ = pos_ + look_;
                    lensCaptureAt_ = lensOnset_ + (int64_t)std::lround(sr_ * kLensPostMs * 0.001);
                    lensLook_ = look_; hitPeakDb_ = 0;
                }
            }
        }

        const double dPk = std::max(std::abs(dL), std::abs(dR));
        const double oPk = std::max(std::abs(y0), std::abs(y1));
        frame_.dryPeak = std::max(frame_.dryPeak, (float)dPk);
        frame_.outPeak = std::max(frame_.outPeak, (float)oPk);
        frame_.preWeight = std::max(frame_.preWeight, (float)preDisp);
        fGainMax_ = std::max(fGainMax_, gDisp);
        fGainMin_ = std::min(fGainMin_, gDisp);
        if (pendingHitOnset_ >= 0 && pos_ >= pendingHitOnset_) { frame_.hit = true; pendingHitOnset_ = -1; }

        const int ri = (int)(pos_ & rMask_);
        rDry_[(size_t)ri] = (float)((dL + dR) * 0.5);
        rOut_[(size_t)ri] = (float)((y0 + y1) * 0.5);
        rGain_[(size_t)ri] = (float)gDisp;
        rPre_[(size_t)ri] = (float)preDisp;
        if (lensCaptureAt_ >= 0) {
            const double gdb = std::abs(linToDb(gDisp));
            if (gdb > std::abs(hitPeakDb_)) hitPeakDb_ = linToDb(gDisp);
            if (pos_ >= lensCaptureAt_) { captureLens(); lensCaptureAt_ = -1; }
        }

        if (++subCnt_ >= subLen_) {
            frame_.gainMaxDb = (float)linToDb(fGainMax_);
            frame_.gainMinDb = (float)linToDb(fGainMin_);
            frame_.autoGainDb = (float)linToDb(corr_);
            if (sink_) sink_->pushFrame(frame_);
            frame_ = MeterFrame{}; fGainMax_ = 0; fGainMin_ = 1e9; subCnt_ = 0;
        }
        ++pos_;
    }
}

void Kernel::captureLens() {
    const int64_t preS = (int64_t)std::lround(sr_ * kLensPreMs * 0.001);
    const int64_t start = lensOnset_ - preS;
    const int64_t span = pos_ - start + 1;
    if (span <= 0 || span > rMask_) return;
    const int B = LensSnapshot::kBins;
    for (int b = 0; b < B; ++b) {
        const int64_t a = start + (span * b) / B;
        const int64_t e = std::max(a + 1, start + (span * (b + 1)) / B);
        float dmn = 1e9f, dmx = -1e9f, omn = 1e9f, omx = -1e9f, pw = 0; double gs = 0;
        for (int64_t s = a; s < e; ++s) {
            const int ri = (int)(s & rMask_);
            dmn = std::min(dmn, rDry_[(size_t)ri]); dmx = std::max(dmx, rDry_[(size_t)ri]);
            omn = std::min(omn, rOut_[(size_t)ri]); omx = std::max(omx, rOut_[(size_t)ri]);
            gs += rGain_[(size_t)ri]; pw = std::max(pw, rPre_[(size_t)ri]);
        }
        lens_.dryMin[b] = dmn; lens_.dryMax[b] = dmx;
        lens_.outMin[b] = omn; lens_.outMax[b] = omx;
        lens_.gainDb[b] = (float)linToDb(gs / (double)(e - a));
        lens_.preW[b] = pw;
    }
    lens_.windowMs = (float)(span * 1000.0 / sr_);
    lens_.onsetMs = (float)(preS * 1000.0 / sr_);
    lens_.lookaheadMs = (float)(lensLook_ * 1000.0 / sr_);
    lens_.peakGainDb = (float)hitPeakDb_;
    lens_.serial = ++lensSerial_;
    if (sink_) sink_->pushLens(lens_);
}

} // namespace td1
