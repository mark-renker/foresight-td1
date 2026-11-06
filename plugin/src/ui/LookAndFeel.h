#pragma once

#include "Theme.h"

namespace td1::ui {

class TD1LookAndFeel final : public juce::LookAndFeel_V4 {
public:
    // Component properties consulted while drawing:
    //   Slider  "bipolar" (bool)  value arc grows from 12 o'clock
    //   Slider  "hero"    (bool)  large machined knob
    //   Slider  "accent"  (int)   ARGB arc colour override
    //   Button  "accent"  (int)   ARGB "on" colour override
    //   Button  "seg"     (int)   segment position: 1 = left, 2 = middle, 3 = right
    TD1LookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos,
                          float start, float end, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground(juce::Graphics&, int w, int h) override;
    void drawTooltip(juce::Graphics&, const juce::String&, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    void drawBubble(juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
    juce::Font getSliderPopupFont(juce::Slider&) override;
    int getSliderPopupPlacement(juce::Slider&) override;
    void drawCornerResizer(juce::Graphics&, int w, int h, bool, bool) override;
};

} // namespace td1::ui
