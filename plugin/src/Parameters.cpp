#include "Parameters.h"

namespace td1::param {

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using NR  = juce::NormalisableRange<float>;

namespace {
juce::String fmtDb(float v, int) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; }
juce::String fmtHz(float v, int) { return v >= 1000 ? juce::String(v / 1000.0f, 2) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz"; }
juce::String fmtMs(float v, int) { return v <= 0.0f ? juce::String("Zero") : juce::String(v, 1) + " ms"; }
juce::String fmtPct(float v, int) { return juce::String(juce::roundToInt(v)) + " %"; }
juce::String fmtX(float v, int) { return juce::String(v, 2) + juce::String(juce::CharPointer_UTF8("\xc3\x97")); }
juce::String fmtThr(float v, int) { return v <= td1::kThresholdOffDb ? juce::String("Off") : juce::String(v, 1) + " dB"; }

NR skewed(float lo, float hi, float centre, float step = 0.0f) {
    NR r(lo, hi, step); r.setSkewForCentre(centre); return r;
}

std::unique_ptr<APF> fl(const char* id, const char* name, NR range, float def,
                        juce::String (*fmt)(float, int), const char* unit = "") {
    return std::make_unique<APF>(juce::ParameterID{ id, kVersionHint }, name, range, def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(fmt).withLabel(unit));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(fl(kAttack,    "Attack",     NR(-24.f, 24.f, 0.1f), 6.f, fmtDb, "dB"));
    l.add(fl(kSustain,   "Sustain",    NR(-24.f, 24.f, 0.1f), 0.f, fmtDb, "dB"));
    l.add(fl(kPre,       "Pre",        NR(-24.f, 24.f, 0.1f), 0.f, fmtDb, "dB"));
    l.add(fl(kSpeed,     "Speed",      skewed(0.5f, 2.f, 1.f, 0.01f), 1.f, fmtX));
    l.add(fl(kHpf,       "Detector HPF", skewed(20.f, 500.f, 100.f, 1.f), 60.f, fmtHz, "Hz"));
    l.add(fl(kThreshold, "Threshold",  NR(td1::kThresholdOffDb, 0.f, 0.1f), td1::kThresholdOffDb, fmtThr, "dB"));
    l.add(fl(kTilt,      "Tilt",       NR(-12.f, 12.f, 0.1f), 0.f, fmtDb, "dB"));
    l.add(fl(kPivot,     "Tilt Pivot", skewed(200.f, 2000.f, 650.f, 1.f), 750.f, fmtHz, "Hz"));
    l.add(fl(kLookahead, "Lookahead",  NR(0.f, (float)td1::kMaxLookaheadMs, 0.1f), 3.f, fmtMs, "ms"));
    l.add(fl(kMix,       "Mix",        NR(0.f, 100.f, 1.f), 100.f, fmtPct, "%"));
    l.add(fl(kOutput,    "Output",     NR(-12.f, 12.f, 0.1f), 0.f, fmtDb, "dB"));
    l.add(std::make_unique<APC>(juce::ParameterID{ kTarget, kVersionHint }, "Target",
                                juce::StringArray{ "Stereo", "Mid", "Side" }, 0));
    l.add(std::make_unique<APB>(juce::ParameterID{ kClip, kVersionHint }, "Soft Clip", true));
    l.add(std::make_unique<APB>(juce::ParameterID{ kLink, kVersionHint }, "Stereo Link", true));
    l.add(std::make_unique<APB>(juce::ParameterID{ kDelta, kVersionHint }, "Delta", false));
    l.add(std::make_unique<APB>(juce::ParameterID{ kAutoGain, kVersionHint }, "Auto Gain", false));
    l.add(std::make_unique<APB>(juce::ParameterID{ kAudition, kVersionHint }, "Detector Listen", false));
    l.add(std::make_unique<APB>(juce::ParameterID{ kBypass, kVersionHint }, "Bypass", false));
    return l;
}

void Refs::bind(juce::AudioProcessorValueTreeState& s) {
    auto g = [&](const char* id) { auto* p = s.getRawParameterValue(id); jassert(p != nullptr); return p; };
    attack = g(kAttack); sustain = g(kSustain); pre = g(kPre); speed = g(kSpeed); hpf = g(kHpf);
    threshold = g(kThreshold); tilt = g(kTilt); pivot = g(kPivot); lookahead = g(kLookahead);
    mix = g(kMix); output = g(kOutput); target = g(kTarget); clip = g(kClip); link = g(kLink);
    delta = g(kDelta); autoGain = g(kAutoGain); audition = g(kAudition); bypass = g(kBypass);
}

td1::Params Refs::read() const {
    td1::Params p;
    p.attackDb = attack->load();    p.sustainDb = sustain->load();  p.preDb = pre->load();
    p.speed = speed->load();        p.hpfHz = hpf->load();          p.thresholdDb = threshold->load();
    p.tiltDb = tilt->load();        p.pivotHz = pivot->load();      p.lookaheadMs = lookahead->load();
    p.mix = mix->load() * 0.01f;    p.outputDb = output->load();
    p.target = (td1::Target)juce::jlimit(0, 2, (int)std::lround(target->load()));
    p.clip = clip->load() > 0.5f;   p.link = link->load() > 0.5f;   p.delta = delta->load() > 0.5f;
    p.autoGain = autoGain->load() > 0.5f; p.audition = audition->load() > 0.5f;
    p.bypass = bypass->load() > 0.5f;
    return p;
}

const juce::StringArray& allIds() {
    static const juce::StringArray ids{ kAttack, kSustain, kPre, kSpeed, kHpf, kThreshold, kTilt, kPivot,
        kLookahead, kMix, kOutput, kTarget, kClip, kLink, kDelta, kAutoGain, kAudition, kBypass };
    return ids;
}

} // namespace td1::param
