// Property tests for td1-core (no reference needed).
#include "td1/Kernel.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) ++g_pass; else { ++g_fail; \
    std::fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } } while (0)

static const double kRates[] = { 44100, 48000, 88200, 96000, 192000 };

struct Sig { std::vector<float> L, R; };

static Sig noiseHits(double sr, double seconds, unsigned seed = 7) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(-1.f, 1.f);
    const size_t n = (size_t)(sr * seconds);
    Sig s{ std::vector<float>(n, 0.f), std::vector<float>(n, 0.f) };
    for (size_t h = 0; h * (size_t)(sr * 0.125) < n; ++h) {
        const size_t n0 = h * (size_t)(sr * 0.125) + (size_t)(sr * 0.011);
        for (size_t i = 0; i < (size_t)(sr * 0.08) && n0 + i < n; ++i) {
            const float e = std::exp(-(float)i / (float)(sr * 0.012));
            s.L[n0 + i] = 0.5f * u(rng) * e;
            s.R[n0 + i] = 0.4f * u(rng) * e;
        }
    }
    return s;
}

static Sig run(td1::Kernel& k, const Sig& in, int block = 256) {
    Sig out{ std::vector<float>(in.L.size()), std::vector<float>(in.L.size()) };
    for (size_t i = 0; i < in.L.size(); i += (size_t)block) {
        const int m = (int)std::min<size_t>((size_t)block, in.L.size() - i);
        k.process(in.L.data() + i, in.R.data() + i, out.L.data() + i, out.R.data() + i, m);
    }
    return out;
}

static double maxAbsDiffDelayed(const Sig& in, const Sig& out, int lat) {
    double m = 0;
    for (size_t i = (size_t)lat; i < in.L.size(); ++i) {
        m = std::max(m, (double)std::abs(out.L[i] - in.L[i - (size_t)lat]));
        m = std::max(m, (double)std::abs(out.R[i] - in.R[i - (size_t)lat]));
    }
    return m;
}

static double maxStep(const std::vector<float>& x, size_t from, size_t to) {
    double m = 0;
    for (size_t i = from + 1; i < to && i < x.size(); ++i) m = std::max(m, (double)std::abs(x[i] - x[i - 1]));
    return m;
}

int main() {
    for (double sr : kRates) {
        // ---- unity at 0/0: neutral settings are a pure delay of exactly `lookahead`
        {
            td1::Params p; p.attackDb = 0; p.sustainDb = 0; p.clip = false;
            for (float look : { 0.0f, 3.0f, 10.0f }) {
                p.lookaheadMs = look;
                td1::Kernel k; k.setParams(p); k.prepare(sr);
                const Sig in = noiseHits(sr, 0.5);
                const Sig out = run(k, in);
                const int lat = k.latencySamples();
                CHECK(lat == (int)std::floor(look * 0.001 * sr + 0.5), "latency %d @%g look %g", lat, sr, look);
                const double d = maxAbsDiffDelayed(in, out, lat);
                CHECK(d < 1e-6, "unity @%g look %g: max diff %g", sr, look, d);
            }
        }
        // ---- silence in -> silence out, for aggressive settings
        {
            td1::Params p; p.attackDb = 24; p.sustainDb = 24; p.preDb = 24; p.tiltDb = 12;
            p.autoGain = true; p.thresholdDb = -30;
            td1::Kernel k; k.setParams(p); k.prepare(sr);
            Sig in{ std::vector<float>((size_t)sr / 4, 0.f), std::vector<float>((size_t)sr / 4, 0.f) };
            const Sig out = run(k, in);
            double m = 0; for (size_t i = 0; i < out.L.size(); ++i) m = std::max(m, (double)std::abs(out.L[i]) + std::abs(out.R[i]));
            CHECK(m == 0.0, "silence @%g: got %g", sr, m);
        }
        // ---- latency: impulse response peak lands exactly at latencySamples()
        {
            td1::Params p; p.attackDb = 0; p.clip = false;
            for (float look : { 1.0f, 5.0f, 10.0f }) {
                p.lookaheadMs = look;
                td1::Kernel k; k.setParams(p); k.prepare(sr);
                Sig in{ std::vector<float>(4096, 0.f), std::vector<float>(4096, 0.f) };
                in.L[100] = 1.f; in.R[100] = 1.f;
                const Sig out = run(k, in);
                size_t pk = 0; for (size_t i = 0; i < out.L.size(); ++i) if (std::abs(out.L[i]) > std::abs(out.L[pk])) pk = i;
                CHECK((int)pk - 100 == k.latencySamples(), "impulse peak offset %d != %d @%g", (int)pk - 100, k.latencySamples(), sr);
            }
        }
    }

    // ---- block-size invariance
    {
        const double sr = 48000;
        td1::Params p; p.attackDb = 15; p.sustainDb = -6; p.preDb = -9; p.tiltDb = 6; p.thresholdDb = -40;
        const Sig in = noiseHits(sr, 0.5);
        td1::Kernel a; a.setParams(p); a.prepare(sr);
        td1::Kernel b; b.setParams(p); b.prepare(sr);
        const Sig oa = run(a, in, 1), ob = run(b, in, 977);
        CHECK(maxAbsDiffDelayed(oa, ob, 0) == 0.0, "block-size dependent output");
    }

    // ---- level independence: same shape at -20 dB and 0 dB (no threshold)
    {
        const double sr = 48000;
        td1::Params p; p.attackDb = 12; p.sustainDb = -6; p.clip = false;
        const Sig in = noiseHits(sr, 0.5);
        Sig quiet = in; for (auto* v : { &quiet.L, &quiet.R }) for (auto& x : *v) x *= 0.1f;
        td1::Kernel a; a.setParams(p); a.prepare(sr);
        td1::Kernel b; b.setParams(p); b.prepare(sr);
        const Sig oa = run(a, in), ob = run(b, quiet);
        double m = 0; for (size_t i = 0; i < oa.L.size(); ++i) m = std::max(m, (double)std::abs(oa.L[i] * 0.1f - ob.L[i]));
        // Not bit-exact (the +1e-4 regulariser in the ratio), but close.
        CHECK(m < 2e-3, "level dependence: %g", m);
    }

    // ---- threshold gates quiet onsets but keeps loud ones
    {
        const double sr = 48000;
        auto shapedEnergy = [&](float amp, float thr) {
            td1::Params p; p.attackDb = 18; p.clip = false; p.thresholdDb = thr;
            td1::Kernel k; k.setParams(p); k.prepare(sr);
            Sig in = noiseHits(sr, 0.5);
            for (auto* v : { &in.L, &in.R }) for (auto& x : *v) x *= amp;
            const Sig out = run(k, in);
            const int lat = k.latencySamples();
            double e = 0;
            for (size_t i = (size_t)lat; i < in.L.size(); ++i) { const double d = out.L[i] - in.L[i - (size_t)lat]; e += d * d; }
            double ref = 0; for (float x : in.L) ref += (double)x * x;
            return e / ref;
        };
        const double loudOn = shapedEnergy(1.0f, -30), quietOn = shapedEnergy(0.01f, -30);
        const double quietOff = shapedEnergy(0.01f, td1::kThresholdOffDb);
        CHECK(loudOn > 0.1, "threshold kills loud hits: %g", loudOn);
        CHECK(quietOn < 1e-3 * quietOff + 1e-9, "threshold fails to gate quiet hits: %g vs %g", quietOn, quietOff);
    }

    // ---- pre-transient shaping: ducks the lookahead window, leaves the onset
    {
        const double sr = 48000;
        td1::Params p; p.attackDb = 0; p.sustainDb = 0; p.clip = false; p.lookaheadMs = 5; p.preDb = -24;
        td1::Kernel k; k.setParams(p); k.prepare(sr);
        const size_t n = (size_t)(sr * 0.3);
        Sig in{ std::vector<float>(n, 0.f), std::vector<float>(n, 0.f) };
        // steady low tone, then a sharp hit at 150 ms
        for (size_t i = 0; i < n; ++i) in.L[i] = in.R[i] = 0.05f * (float)std::sin(2 * M_PI * 220 * i / sr);
        const size_t onset = (size_t)(sr * 0.15);
        for (size_t i = 0; i < (size_t)(sr * 0.02); ++i) in.L[onset + i] = in.R[onset + i] += 0.8f * std::exp(-(float)i / (float)(sr * 0.004));
        const Sig out = run(k, in);
        const int lat = k.latencySamples();
        auto rms = [&](const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double)x[i] * x[i]; return std::sqrt(s / (double)(b - a)); };
        // window = ~1.5..4 ms before the onset (in output time: onset + lat)
        const size_t oOn = onset + (size_t)lat;
        const double inPre = rms(in.L, onset - (size_t)(sr * 0.004), onset - (size_t)(sr * 0.0015));
        const double outPre = rms(out.L, oOn - (size_t)(sr * 0.004), oOn - (size_t)(sr * 0.0015));
        CHECK(outPre < inPre * 0.25, "pre window not ducked: in %g out %g", inPre, outPre);
        const double inHit = rms(in.L, onset + (size_t)(sr * 0.0003), onset + (size_t)(sr * 0.003));
        const double outHit = rms(out.L, oOn + (size_t)(sr * 0.0003), oOn + (size_t)(sr * 0.003));
        CHECK(outHit > inHit * 0.85, "onset damaged by pre duck: in %g out %g", inHit, outHit);
        // pre has no effect at zero lookahead
        p.lookaheadMs = 0; td1::Kernel z; z.setParams(p); z.prepare(sr);
        const Sig oz = run(z, in);
        CHECK(maxAbsDiffDelayed(in, oz, 0) < 1e-6, "pre active with zero lookahead");
    }

    // ---- declick: switching discrete params / lookahead / bypass mid-signal never steps hard
    {
        const double sr = 48000;
        const size_t n = (size_t)sr;
        Sig in{ std::vector<float>(n), std::vector<float>(n) };
        for (size_t i = 0; i < n; ++i) { in.L[i] = 0.7f * (float)std::sin(2 * M_PI * 100 * i / sr); in.R[i] = 0.7f * (float)std::cos(2 * M_PI * 100 * i / sr); }
        // a 100 Hz sine at 0.7 has max per-sample step ~ 0.0092; allow 3x for transitions
        const double kMaxStep = 0.03;
        std::vector<std::function<void(td1::Params&)>> changes = {
            [](td1::Params& q) { q.target = td1::Target::Side; },
            [](td1::Params& q) { q.target = td1::Target::Mid; },
            [](td1::Params& q) { q.delta = true; },
            [](td1::Params& q) { q.audition = true; },
            [](td1::Params& q) { q.bypass = true; },
            [](td1::Params& q) { q.lookaheadMs = 9.0f; },
            [](td1::Params& q) { q.mix = 0.0f; },
            [](td1::Params& q) { q.outputDb = -12.0f; },
            [](td1::Params& q) { q.clip = false; },
        };
        int idx = 0;
        for (auto& ch : changes) {
            td1::Params p; p.attackDb = 0; p.sustainDb = 0;
            td1::Kernel k; k.setParams(p); k.prepare(sr);
            Sig out{ std::vector<float>(n), std::vector<float>(n) };
            const size_t sw = n / 2 + 123;
            k.process(in.L.data(), in.R.data(), out.L.data(), out.R.data(), (int)sw);
            ch(p); k.setParams(p);
            k.process(in.L.data() + sw, in.R.data() + sw, out.L.data() + sw, out.R.data() + sw, (int)(n - sw));
            const double st = std::max(maxStep(out.L, sw - 10, sw + 4000), maxStep(out.R, sw - 10, sw + 4000));
            CHECK(st < kMaxStep, "click on change #%d: step %g", idx, st);
            ++idx;
        }
    }

    // ---- meter sink receives frames and lens snapshots
    {
        struct Sink : td1::MeterSink {
            int frames = 0, lenses = 0, hits = 0; float peakGain = 0;
            void pushFrame(const td1::MeterFrame& f) override { ++frames; hits += f.hit; peakGain = std::max(peakGain, f.gainMaxDb); }
            void pushLens(const td1::LensSnapshot& s) override { ++lenses; (void)s; }
        } sink;
        const double sr = 48000;
        td1::Params p; p.attackDb = 12;
        td1::Kernel k; k.setParams(p); k.prepare(sr); k.setMeterSink(&sink);
        const Sig in = noiseHits(sr, 1.0);
        run(k, in);
        CHECK(sink.frames > 900 && sink.frames < 1100, "frames %d", sink.frames);
        CHECK(sink.hits >= 6, "hits %d", sink.hits);
        CHECK(sink.lenses >= 6, "lenses %d", sink.lenses);
        CHECK(sink.peakGain > 6, "peak gain %g", sink.peakGain);
    }

    std::printf("properties: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
