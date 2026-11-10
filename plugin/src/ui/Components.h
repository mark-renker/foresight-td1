#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "td1/Kernel.h"

namespace td1::ui {

// Rotary knob with caption above and an editable value readout below.
// Double-click knob = default; double-click value = type a number;
// shift-drag = fine.
class Knob final : public juce::Component, private juce::Slider::Listener {
public:
    Knob(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& caption,
         bool hero, bool bipolar, juce::Colour accent = col::amber);
    ~Knob() override;

    void resized() override;
    void paint(juce::Graphics&) override;
    void setTooltipText(const juce::String& t) { slider.setTooltip(t); value.setTooltip(t); }
    void setDimmed(bool d, const juce::String& why = {});
    std::function<void()> onGestureStart;

    juce::Slider slider;

private:
    void sliderValueChanged(juce::Slider*) override { refreshValue(); }
    void sliderDragStarted(juce::Slider*) override { if (onGestureStart) onGestureStart(); }
    void refreshValue();

    juce::RangedAudioParameter* param = nullptr;
    juce::String caption;
    bool hero;
    bool dimmed = false;
    juce::String dimReason;
    juce::Label value;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Scrolling ~3 s history: dry ghost, processed output, applied gain trace,
// pre-window shading and onset markers, all on the output timeline.
class ScopeView final : public juce::Component, public juce::SettableTooltipClient {
public:
    ScopeView();
    void push(const td1::MeterFrame& f);
    void paint(juce::Graphics&) override;
    void setDeltaMode(bool d) { delta = d; }
    float currentGainDb() const { return lastGain; }

private:
    static constexpr int kHist = 4096;   // frames (~1 ms each)
    std::vector<td1::MeterFrame> hist;
    int head = 0;
    float lastGain = 0;
    bool delta = false;
};

// Hit-locked zoom of the most recent transient: shows the gain envelope moving
// *before* the onset — the anticipation made visible. Click to hold.
class LensView final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    LensView();
    void setSnapshot(const td1::LensSnapshot& s);
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool isHeld() const { return held; }

private:
    void timerCallback() override;
    td1::LensSnapshot snap{};
    bool has = false, held = false;
    float fresh = 0;   // flash on new capture
};

// Horizontal peak meter with hold.
class OutputMeter final : public juce::Component, public juce::SettableTooltipClient {
public:
    void push(float peakLin);
    void tick();
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override { clip = false; maxDb = -100.0f; repaint(); }

private:
    float level = -100.0f, hold = -100.0f, maxDb = -100.0f, pending = 0.0f;
    int holdAge = 0;
    bool clip = false;
};

} // namespace td1::ui
