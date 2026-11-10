#pragma once

#include "PluginProcessor.h"
#include "ui/Components.h"
#include "ui/LookAndFeel.h"

namespace td1::ui {

// Preset browser: < name > with a categorised popup menu.
class PresetBox final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit PresetBox(td1::PresetManager& pm) : presets(pm) {}
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    void refresh() { repaint(); }

private:
    juce::Rectangle<int> leftArrow() const { return getLocalBounds().removeFromLeft(26); }
    juce::Rectangle<int> rightArrow() const { return getLocalBounds().removeFromRight(26); }
    void showMenu();
    void saveAs();
    td1::PresetManager& presets;
};

// Fixed-size (960x600) design surface; the editor scales it uniformly.
class Content final : public juce::Component {
public:
    explicit Content(TD1Processor& p);
    ~Content() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void tick();   // ~60 Hz from the editor

    static constexpr int kW = 960, kH = 600;

private:
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    juce::TextButton& toggle(const juce::String& text, const char* paramId, const juce::String& tip,
                             juce::Colour accent = col::amber);
    void refreshAB();

    TD1Processor& proc;
    juce::AudioProcessorValueTreeState& state;

    PresetBox presetBox;
    juce::TextButton abA{ "A" }, abB{ "B" }, abCopy{ "A>B" }, undoBtn{ "UNDO" }, redoBtn{ "REDO" };

    ScopeView scope;
    LensView lens;
    OutputMeter meter;

    Knob pre, attack, sustain;
    Knob speed, hpf, threshold, lookahead, tilt, pivot, mix, output;

    juce::OwnedArray<juce::TextButton> toggles;
    juce::OwnedArray<BA> toggleAttachments;
    juce::TextButton* bypassBtn = nullptr;
    juce::TextButton* deltaBtn = nullptr;
    juce::TextButton* autoBtn = nullptr;

    juce::TextButton tgtStereo{ "STEREO" }, tgtMid{ "MID" }, tgtSide{ "SIDE" };
    std::unique_ptr<juce::ParameterAttachment> targetAttachment;

    int frameCount = 0;
    float autoGainDb = 0.0f;
    juce::String latencyText;
};

} // namespace td1::ui

class TD1Editor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TD1Editor(TD1Processor&);
    ~TD1Editor() override;
    void resized() override;
    void paint(juce::Graphics& g) override { g.fillAll(td1::ui::col::bg); }
    bool keyPressed(const juce::KeyPress&) override;
    void pumpForSnapshot() { content.tick(); }   // tools/Snapshot.cpp

private:
    void timerCallback() override;
    TD1Processor& proc;
    td1::ui::TD1LookAndFeel lnf;
    td1::ui::Content content;
    juce::TooltipWindow tooltips{ this, 900 };
};
