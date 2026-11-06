// Visual identity carried over from prototype v3: graphite panels, amber
// signal, Big Shoulders Display wordmark, IBM Plex Mono for everything else.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace td1::ui {

namespace col {
inline const juce::Colour bg      { 0xff0e1014 };
inline const juce::Colour panel   { 0xff161a1f };
inline const juce::Colour panel2  { 0xff1c2026 };
inline const juce::Colour raised  { 0xff232831 };
inline const juce::Colour line    { 0xff262b33 };
inline const juce::Colour lineHi  { 0xff323946 };
inline const juce::Colour screen  { 0xff0a0c0f };
inline const juce::Colour text    { 0xffc9cfd6 };
inline const juce::Colour dim     { 0xff7c8590 };
inline const juce::Colour faint   { 0xff4a525c };
inline const juce::Colour amber   { 0xffe6a23c };
inline const juce::Colour amberDim{ 0xff8a6526 };
inline const juce::Colour signal  { 0xff6fa8b8 };
inline const juce::Colour ghost   { 0xff34424c };
inline const juce::Colour pre     { 0xff9a86e8 };
inline const juce::Colour delta   { 0xffc96a4a };
inline const juce::Colour good    { 0xff7fb87a };
} // namespace col

class Fonts {
public:
    static juce::Font mono(float h);
    static juce::Font monoMedium(float h);
    static juce::Font display(float h);
};

// Tracking (letter-spacing) helper for small uppercase labels.
inline juce::Font tracked(juce::Font f, float amount = 0.08f) { return f.withExtraKerningFactor(amount); }

} // namespace td1::ui
