#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "td1/Kernel.h"

namespace td1::param {

// Stable parameter IDs — never rename once shipped (host automation + presets).
inline constexpr const char* kAttack    = "attack";
inline constexpr const char* kSustain   = "sustain";
inline constexpr const char* kPre       = "pre";
inline constexpr const char* kSpeed     = "speed";
inline constexpr const char* kHpf       = "hpf";
inline constexpr const char* kThreshold = "threshold";
inline constexpr const char* kTilt      = "tilt";
inline constexpr const char* kPivot     = "pivot";
inline constexpr const char* kLookahead = "lookahead";
inline constexpr const char* kMix       = "mix";
inline constexpr const char* kOutput    = "output";
inline constexpr const char* kTarget    = "target";
inline constexpr const char* kClip      = "clip";
inline constexpr const char* kLink      = "link";
inline constexpr const char* kDelta     = "delta";
inline constexpr const char* kAutoGain  = "autogain";
inline constexpr const char* kAudition  = "audition";
inline constexpr const char* kBypass    = "bypass";

inline constexpr int kVersionHint = 1;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Cached raw pointers for lock-free reads on the audio thread.
struct Refs {
    std::atomic<float>* attack{}; std::atomic<float>* sustain{}; std::atomic<float>* pre{};
    std::atomic<float>* speed{};  std::atomic<float>* hpf{};     std::atomic<float>* threshold{};
    std::atomic<float>* tilt{};   std::atomic<float>* pivot{};   std::atomic<float>* lookahead{};
    std::atomic<float>* mix{};    std::atomic<float>* output{};  std::atomic<float>* target{};
    std::atomic<float>* clip{};   std::atomic<float>* link{};    std::atomic<float>* delta{};
    std::atomic<float>* autoGain{}; std::atomic<float>* audition{}; std::atomic<float>* bypass{};

    void bind(juce::AudioProcessorValueTreeState& s);
    td1::Params read() const;
};

// All parameter IDs in a stable order (presets, A/B snapshots).
const juce::StringArray& allIds();

} // namespace td1::param
