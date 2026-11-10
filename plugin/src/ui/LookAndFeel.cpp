#include "LookAndFeel.h"
#include "BinaryData.h"

namespace td1::ui {

namespace {
juce::Typeface::Ptr load(const void* data, size_t size) {
    return juce::Typeface::createSystemTypefaceFor(data, size);
}
struct Faces {
    juce::Typeface::Ptr mono = load(BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize);
    juce::Typeface::Ptr monoMed = load(BinaryData::IBMPlexMonoMedium_ttf, BinaryData::IBMPlexMonoMedium_ttfSize);
    juce::Typeface::Ptr display = load(BinaryData::BigShouldersDisplayBold_ttf, BinaryData::BigShouldersDisplayBold_ttfSize);
};
Faces& faces() { static Faces f; return f; }
} // namespace

juce::Font Fonts::mono(float h) { return juce::Font(juce::FontOptions(faces().mono).withHeight(h)); }
juce::Font Fonts::monoMedium(float h) { return juce::Font(juce::FontOptions(faces().monoMed).withHeight(h)); }
juce::Font Fonts::display(float h) { return juce::Font(juce::FontOptions(faces().display).withHeight(h)); }

TD1LookAndFeel::TD1LookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, col::bg);
    setColour(juce::PopupMenu::backgroundColourId, col::panel2);
    setColour(juce::PopupMenu::textColourId, col::text);
    setColour(juce::PopupMenu::headerTextColourId, col::dim);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, col::amber.withAlpha(0.18f));
    setColour(juce::PopupMenu::highlightedTextColourId, col::amber);
    setColour(juce::Label::textColourId, col::text);
    setColour(juce::Label::textWhenEditingColourId, col::amber);
    setColour(juce::Label::backgroundWhenEditingColourId, col::screen);
    setColour(juce::Label::outlineWhenEditingColourId, col::amberDim);
    setColour(juce::TextEditor::backgroundColourId, col::screen);
    setColour(juce::TextEditor::textColourId, col::text);
    setColour(juce::TextEditor::highlightColourId, col::amber.withAlpha(0.3f));
    setColour(juce::TextEditor::outlineColourId, col::line);
    setColour(juce::TextEditor::focusedOutlineColourId, col::amberDim);
    setColour(juce::CaretComponent::caretColourId, col::amber);
    setColour(juce::AlertWindow::backgroundColourId, col::panel);
    setColour(juce::AlertWindow::textColourId, col::text);
    setColour(juce::AlertWindow::outlineColourId, col::line);
    setColour(juce::TextButton::buttonColourId, col::panel2);
    setColour(juce::TextButton::textColourOffId, col::text);
    setColour(juce::TooltipWindow::backgroundColourId, col::panel2);
    setColour(juce::TooltipWindow::textColourId, col::text);
    setColour(juce::TooltipWindow::outlineColourId, col::lineHi);
    setColour(juce::BubbleComponent::backgroundColourId, col::panel2);
    setColour(juce::BubbleComponent::outlineColourId, col::amberDim);
    setDefaultSansSerifTypeface(faces().mono);
}

void TD1LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float start, float end, juce::Slider& s) {
    const bool hero = s.getProperties()["hero"];
    const bool bipolar = s.getProperties()["bipolar"];
    const auto accent = s.getProperties().contains("accent")
        ? juce::Colour((juce::uint32)(int)s.getProperties()["accent"]) : col::amber;
    const bool enabled = s.isEnabled();
    const bool hot = s.isMouseOverOrDragging() && enabled;

    const auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(hero ? 4.0f : 3.0f);
    const float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float arcR = r - (hero ? 3.0f : 2.0f);
    const float thick = hero ? 3.5f : 2.5f;
    const float ang = start + pos * (end - start);

    // tick ring
    const int ticks = hero ? 25 : 11;
    for (int i = 0; i < ticks; ++i) {
        const float a = start + (end - start) * (float)i / (float)(ticks - 1);
        const bool major = (i % (hero ? 6 : 5)) == 0 || (bipolar && i == ticks / 2);
        const float r0 = arcR + (hero ? 5.0f : 4.0f), r1 = r0 + (major ? 4.0f : 2.0f);
        g.setColour(major ? col::faint : col::line);
        g.drawLine(c.x + r0 * std::sin(a), c.y - r0 * std::cos(a), c.x + r1 * std::sin(a), c.y - r1 * std::cos(a), 1.0f);
    }

    // track
    juce::Path track;
    track.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour(col::line);
    g.strokePath(track, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (+ soft glow)
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs(ang - from) > 0.002f) {
        juce::Path val;
        val.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, juce::jmin(from, ang), juce::jmax(from, ang), true);
        const auto a = enabled ? accent : col::faint;
        g.setColour(a.withAlpha(hot ? 0.22f : 0.12f));
        g.strokePath(val, juce::PathStrokeType(thick * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(a);
        g.strokePath(val, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // body: machined cap
    const float br = arcR - (hero ? 9.0f : 6.0f);
    {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillEllipse(c.x - br, c.y - br + (hero ? 3.0f : 2.0f), br * 2, br * 2);   // drop shadow
        juce::ColourGradient body(juce::Colour(0xff2d333c), c.x, c.y - br,
                                  juce::Colour(0xff14171b), c.x, c.y + br, false);
        g.setGradientFill(body);
        g.fillEllipse(c.x - br, c.y - br, br * 2, br * 2);
        g.setColour(juce::Colour(0xff08090b));
        g.drawEllipse(c.x - br, c.y - br, br * 2, br * 2, 1.0f);
        // top bevel highlight
        juce::Path bevel;
        bevel.addCentredArc(c.x, c.y, br - 1.2f, br - 1.2f, 0.0f, -2.2f, 2.2f, true);
        g.setColour(juce::Colours::white.withAlpha(hot ? 0.13f : 0.08f));
        g.strokePath(bevel, juce::PathStrokeType(1.0f));
        if (hero) {   // inner face
            const float fr = br * 0.78f;
            juce::ColourGradient face(juce::Colour(0xff22272e), c.x, c.y + fr,
                                      juce::Colour(0xff1a1e23), c.x, c.y - fr, false);
            g.setGradientFill(face);
            g.fillEllipse(c.x - fr, c.y - fr, fr * 2, fr * 2);
        }
    }
    // pointer
    {
        const float p0 = br * (hero ? 0.42f : 0.30f), p1 = br * 0.86f;
        juce::Line<float> ln(c.x + p0 * std::sin(ang), c.y - p0 * std::cos(ang),
                             c.x + p1 * std::sin(ang), c.y - p1 * std::cos(ang));
        g.setColour(enabled ? (hot ? accent : col::text) : col::faint);
        g.drawLine(ln, hero ? 2.6f : 2.0f);
    }
}

void TD1LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                          bool highlighted, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = b.getToggleState();
    const auto accent = b.getProperties().contains("accent")
        ? juce::Colour((juce::uint32)(int)b.getProperties()["accent"]) : col::amber;
    const int seg = b.getProperties()["seg"];
    const float rad = 3.0f;

    juce::Path p;
    const bool tl = seg == 0 || seg == 1, tr = seg == 0 || seg == 3;
    p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), rad, rad, tl, tr, tl, tr);

    auto fill = on ? accent.withAlpha(0.13f).overlaidWith(col::panel2.withAlpha(0.0f)) : col::panel2;
    if (on) fill = col::panel2.interpolatedWith(accent, 0.12f);
    if (highlighted && !on) fill = fill.brighter(0.06f);
    if (down) fill = fill.darker(0.15f);
    g.setColour(fill);
    g.fillPath(p);
    g.setColour(on ? accent.withAlpha(0.85f) : (highlighted ? col::lineHi : col::line));
    g.strokePath(p, juce::PathStrokeType(1.0f));
    if (on) {   // inner glow line
        g.setColour(accent.withAlpha(0.18f));
        g.drawLine(r.getX() + 3, r.getY() + 1.5f, r.getRight() - 3, r.getY() + 1.5f, 1.0f);
    }
}

juce::Font TD1LookAndFeel::getTextButtonFont(juce::TextButton&, int h) {
    return tracked(Fonts::monoMedium(juce::jmin(11.5f, (float)h * 0.46f)), 0.1f);
}

void TD1LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool highlighted, bool) {
    const bool on = b.getToggleState();
    const auto accent = b.getProperties().contains("accent")
        ? juce::Colour((juce::uint32)(int)b.getProperties()["accent"]) : col::amber;
    auto r = b.getLocalBounds();
    const bool led = b.getProperties()["led"];
    if (led) {   // status LED on the left
        auto ledR = r.removeFromLeft(r.getHeight()).toFloat().withSizeKeepingCentre(6.0f, 6.0f).translated(4.0f, 0.0f);
        if (on) { g.setColour(accent.withAlpha(0.35f)); g.fillEllipse(ledR.expanded(3.0f)); }
        g.setColour(on ? accent : col::faint);
        g.fillEllipse(ledR);
        r.removeFromRight(6);
    }
    g.setFont(getTextButtonFont(b, b.getHeight()));
    g.setColour(!b.isEnabled() ? col::faint : on ? accent : (highlighted ? col::text : col::text.withAlpha(0.82f)));
    g.drawFittedText(b.getButtonText(), r, juce::Justification::centred, 1);
}

juce::Font TD1LookAndFeel::getLabelFont(juce::Label& l) {
    return l.getProperties().contains("font") ? l.getFont() : Fonts::mono(12.0f);
}
juce::Font TD1LookAndFeel::getPopupMenuFont() { return Fonts::mono(13.0f); }

void TD1LookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int w, int h) {
    g.fillAll(col::panel2);
    g.setColour(col::lineHi);
    g.drawRect(0, 0, w, h, 1);
}

void TD1LookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int w, int h) {
    auto r = juce::Rectangle<int>(w, h).toFloat();
    g.setColour(col::panel2);
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(col::lineHi);
    g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
    juce::AttributedString s;
    s.setWordWrap(juce::AttributedString::byWord);
    s.append(text, Fonts::mono(12.0f), col::text);
    juce::TextLayout tl;
    tl.createLayout(s, (float)w - 16.0f);
    tl.draw(g, r.reduced(8.0f, 6.0f));
}

juce::Rectangle<int> TD1LookAndFeel::getTooltipBounds(const juce::String& text, juce::Point<int> pos,
                                                       juce::Rectangle<int> parent) {
    juce::AttributedString s;
    s.setWordWrap(juce::AttributedString::byWord);
    s.append(text, Fonts::mono(12.0f), col::text);
    juce::TextLayout tl;
    tl.createLayout(s, 300.0f);
    const int w = (int)std::ceil(tl.getWidth()) + 18, h = (int)std::ceil(tl.getHeight()) + 14;
    return juce::Rectangle<int>(pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 16, w, h)
        .constrainedWithin(parent);
}

juce::Label* TD1LookAndFeel::createSliderTextBox(juce::Slider& s) {
    auto* l = LookAndFeel_V4::createSliderTextBox(s);
    l->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

void TD1LookAndFeel::drawBubble(juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&,
                                const juce::Rectangle<float>& body) {
    g.setColour(col::panel2);
    g.fillRoundedRectangle(body, 3.0f);
    g.setColour(col::amberDim);
    g.drawRoundedRectangle(body.reduced(0.5f), 3.0f, 1.0f);
}

juce::Font TD1LookAndFeel::getSliderPopupFont(juce::Slider&) { return Fonts::monoMedium(13.0f); }
int TD1LookAndFeel::getSliderPopupPlacement(juce::Slider&) { return juce::BubbleComponent::above; }

void TD1LookAndFeel::drawCornerResizer(juce::Graphics& g, int w, int h, bool over, bool dragging) {
    g.setColour((over || dragging) ? col::amber : col::faint);
    for (int i = 1; i <= 3; ++i) {
        const float o = (float)i * 4.0f;
        g.drawLine((float)w - o, (float)h - 2.0f, (float)w - 2.0f, (float)h - o, 1.0f);
    }
}

} // namespace td1::ui
