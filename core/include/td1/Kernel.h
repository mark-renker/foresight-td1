// FORESIGHT TD-1 — td1-core DSP kernel (dependency-free C++17, no JUCE types).
//
// Faithful port of TShaperKernel in prototype/foresight_td1_v3.html (the
// reference). With the post-prototype features at their neutral settings
// (pre = 0 dB, threshold = off) and all parameters settled, output matches the
// reference within float rounding — see tests/golden.
//
// Additions over the reference:
//   * Pre-transient shaping ("Pre"): gain applied to the lookahead window that
//     precedes each detected onset. Only possible because the detector runs
//     ahead of the audio. Duck (-dB) for a clean gap, swell (+dB) for a lead-in.
//   * Threshold ("bleed focus"): soft-knee gate on transient detection so quiet
//     onsets (hat bleed in a snare mic) are not shaped. Off at its minimum.
//   * Declicking: lookahead changes crossfade between delay taps; bypass
//     crossfades against the latency-aligned dry path; target / delta /
//     audition / clip switches dip to silence for a few ms and switch at zero;
//     mix, output, tilt and pre ramp linearly. At rest everything is exact.
//   * Delay line stores raw L/R (encoding at read) so target switches never
//     play stale M/S-encoded samples.
//   * Telemetry for the GUI: per-millisecond meter frames and hit-locked
//     "Foresight Lens" snapshots, delivered through a MeterSink interface.
#pragma once

#include <cstdint>
#include <vector>

namespace td1 {

constexpr const char* kVersion = "0.4.0";
constexpr double kMaxLookaheadMs = 10.0;
constexpr float kThresholdOffDb = -60.0f;   // threshold <= this disables the gate

enum class Target : int { Stereo = 0, Mid = 1, Side = 2 };

struct Params {
    float attackDb    = 6.0f;    // -24 .. 24
    float sustainDb   = 0.0f;    // -24 .. 24
    float preDb       = 0.0f;    // -24 .. 24   (pre-transient window gain)
    float speed       = 1.0f;    // 0.5 .. 2    (detector time scale)
    float hpfHz       = 60.0f;   // 20 .. 500   (detector sidechain HPF)
    float thresholdDb = kThresholdOffDb; // -60 (off) .. 0
    float tiltDb      = 0.0f;    // -12 .. 12   (transient-only spectral tilt)
    float pivotHz     = 750.0f;  // 200 .. 2000
    float lookaheadMs = 3.0f;    // 0 .. 10
    float mix         = 1.0f;    // 0 .. 1
    float outputDb    = 0.0f;    // -12 .. 12
    Target target     = Target::Stereo;
    bool clip     = true;
    bool link     = true;
    bool delta    = false;
    bool bypass   = false;
    bool autoGain = false;
    bool audition = false;
};

// One frame per ~1 ms of output. Times are on the *output* timeline, so the
// gain trace visibly leads the (delayed) transient by the lookahead.
struct MeterFrame {
    float dryPeak  = 0;  // latency-aligned dry input peak (linear)
    float outPeak  = 0;  // final output peak (linear)
    float gainMaxDb = 0; // applied transient gain, max in frame
    float gainMinDb = 0; // applied transient gain, min in frame
    float preWeight = 0; // pre-window weight 0..1 (max in frame)
    float autoGainDb = 0;// loudness-match correction currently computed
    bool  hit = false;   // a transient onset reached the output in this frame
};

// Hit-locked high-resolution capture around one onset (output timeline).
struct LensSnapshot {
    static constexpr int kBins = 400;
    float dryMin[kBins], dryMax[kBins];  // mono dry, aligned
    float outMin[kBins], outMax[kBins];  // mono output
    float gainDb[kBins];                 // applied gain (display channel)
    float preW[kBins];                   // pre-window weight
    float windowMs = 0;                  // total span
    float onsetMs = 0;                   // onset position from window start
    float lookaheadMs = 0;               // lookahead in effect
    float peakGainDb = 0;                // max |gain| excursion during hit
    uint32_t serial = 0;
};

class MeterSink {
public:
    virtual ~MeterSink() = default;
    virtual void pushFrame(const MeterFrame&) = 0;        // audio thread, must not block
    virtual void pushLens(const LensSnapshot&) = 0;       // audio thread, must not block
};

class Kernel {
public:
    // Allocates. Call off the audio thread. Resets state and snaps all ramps.
    void prepare(double sampleRate);
    // Clears state; snaps ramps/discrete switches to the current params.
    void reset();

    // Real-time safe. Continuous params ramp; discrete switches declick.
    void setParams(const Params& p);
    const Params& params() const { return target_; }

    // Real-time safe. inR may be null (mono in); outR may be null (mono out).
    // In-place processing (out == in) is supported.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n);

    int latencySamples() const { return lookSamplesFor(target_.lookaheadMs); }
    int maxLatencySamples() const { return lookSamplesFor((float)kMaxLookaheadMs); }
    double sampleRate() const { return sr_; }

    void setMeterSink(MeterSink* s) { sink_ = s; }

private:
    struct Ch {
        double hpLp = 0, eAF = 0, eAS = 0, eSF = 0, eSS = 0;
        double g = 1, wAs = 0, tl = 0, adX1 = 0;
        double preWs = 0, wA = 0;
        std::vector<float> wHist;   // gated wA history for the pre-window
    };
    struct Coeffs {
        double cAFa, cASa, cArel, cSatk, cSFr, cSSr, cGup, cGdn, cW, cTl, cHp, cR, cC, cPre;
        double aDb, sDb;
    };
    struct Ramp {
        double cur = 0, tgt = 0, step = 0; int left = 0;
        void snap(double v) { cur = tgt = v; left = 0; step = 0; }
        void set(double v, int len) {
            if (!(v < tgt) && !(v > tgt)) return;
            tgt = v;
            if (len <= 0) { cur = v; left = 0; return; }
            left = len; step = (tgt - cur) / len;
        }
        double next() {
            if (left > 0) { cur += step; if (--left == 0) cur = tgt; }
            return cur;
        }
        bool moving() const { return left > 0; }
    };
    struct Discrete {
        Target target = Target::Stereo; bool clip = true, delta = false, audition = false;
        bool operator==(const Discrete& o) const {
            return target == o.target && clip == o.clip && delta == o.delta && audition == o.audition;
        }
        bool operator!=(const Discrete& o) const { return !(*this == o); }
    };

    int lookSamplesFor(float ms) const;
    double c(double ms) const;
    double updEnv(Ch& st, double a, const Coeffs& k, bool gateOn, double gateDb);
    double logcosh(double v) const;
    double adaa(Ch& st, double x);
    void computeCoeffs(Coeffs& k) const;
    void captureLens();

    double sr_ = 48000.0;
    Params target_;
    MeterSink* sink_ = nullptr;

    // delay line (raw L/R, float like the reference's Float32Array)
    int maxD_ = 0, wIdx_ = 0;
    std::vector<float> dL_, dR_;
    int look_ = 0, lookOld_ = 0;
    Ramp lookX_;            // 0 -> 1 crossfade old tap -> new tap
    int lookXLen_ = 0;

    Ch ch_[2];
    double msD_ = 0, msW_ = 0, corr_ = 1;

    Ramp mix_, out_, tilt_, pre_;
    int rampLen_ = 0;
    double cachedTilt_ = 1e9, gHi_ = 1, gLo_ = 1;

    Discrete active_, pending_;
    Ramp dip_;              // 1 = open, 0 = silent (switch point)
    int dipLen_ = 0, dipState_ = 0;   // 0 idle, 1 fading out, 2 fading in
    Ramp byp_;              // 0 = processed, 1 = bypassed
    int bypLen_ = 0;

    // telemetry
    int subLen_ = 48, subCnt_ = 0;
    MeterFrame frame_;
    double fGainMax_ = 0, fGainMin_ = 1e9;
    int64_t pos_ = 0, lastHitPos_ = -1000000;
    int64_t pendingHitOnset_ = -1;          // output-timeline onset position awaiting display
    int64_t lensCaptureAt_ = -1, lensOnset_ = 0;
    double hitPeakDb_ = 0; int lensLook_ = 0;
    std::vector<float> rDry_, rOut_, rGain_, rPre_;
    int rMask_ = 0;
    uint32_t lensSerial_ = 0;
    LensSnapshot lens_;
};

} // namespace td1
