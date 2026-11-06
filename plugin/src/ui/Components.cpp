#include "Components.h"

namespace td1::ui {

namespace {
float dbOf(float lin) { return 20.0f * std::log10(juce::jmax(1e-6f, lin)); }

void drawPanel(juce::Graphics& g, juce::Rectangle<float> r) {
    g.setColour(col::screen);
    g.fillRoundedRectangle(r, 4.0f);
    // inner shadow at the top edge: reads as a recessed display
    juce::ColourGradient sh(juce::Colours::black.withAlpha(0.5f), r.getX(), r.getY(),
                            juce::Colours::transparentBlack, r.getX(), r.getY() + 10.0f, false);
    g.setGradientFill(sh);
    g.fillRoundedRectangle(r.withHeight(12.0f), 4.0f);
    g.setColour(col::line);
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
}
} // namespace

// ============================================================================ Knob
Knob::Knob(juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& cap,
           bool isHero, bool bipolar, juce::Colour accent)
    : caption(cap), hero(isHero) {
    param = s.getParameter(paramId);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    slider.setMouseDragSensitivity(hero ? 300 : 220);
    slider.setVelocityModeParameters(0.6, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    slider.getProperties().set("hero", hero);
    slider.getProperties().set("bipolar", bipolar);
    slider.getProperties().set("accent", (int)accent.getARGB());
    slider.addListener(this);
    addAndMakeVisible(slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(s, paramId, slider);
    if (param != nullptr)
        slider.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));

    value.setJustificationType(juce::Justification::centred);
    value.setEditable(false, true, false);
    value.getProperties().set("font", true);
    value.setFont(hero ? Fonts::monoMedium(15.0f) : Fonts::monoMedium(12.0f));
    value.setColour(juce::Label::textColourId, accent);
    value.onEditorShow = [this] {
        if (auto* ed = value.getCurrentTextEditor()) {
            ed->setJustification(juce::Justification::centred);
            ed->setText(juce::String(slider.getValue(), 2), false);
            ed->selectAll();
        }
    };
    value.onTextChange = [this] {
        const auto t = value.getText().trim();
        if (t.equalsIgnoreCase("off") || t.equalsIgnoreCase("zero")) slider.setValue(slider.getMinimum());
        else if (t.containsAnyOf("0123456789")) {
            double v = t.retainCharacters("-+.0123456789").getDoubleValue();
            if (t.containsIgnoreCase("k")) v *= 1000.0;
            if (onGestureStart) onGestureStart();
            slider.setValue(juce::jlimit(slider.getMinimum(), slider.getMaximum(), v), juce::sendNotificationSync);
        }
        refreshValue();
    };
    addAndMakeVisible(value);
    refreshValue();
}

Knob::~Knob() { slider.removeListener(this); }

void Knob::refreshValue() {
    if (param == nullptr) return;
    value.setText(param->getCurrentValueAsText(), juce::dontSendNotification);
    slider.setTitle(caption);
}

void Knob::setDimmed(bool d, const juce::String& why) {
    if (d == dimmed && why == dimReason) return;
    dimmed = d; dimReason = why;
    slider.setAlpha(d ? 0.45f : 1.0f);
    value.setAlpha(d ? 0.45f : 1.0f);
    repaint();
}

void Knob::resized() {
    auto r = getLocalBounds();
    r.removeFromTop(hero ? 22 : 17);
    auto v = r.removeFromBottom(hero ? 22 : 17);
    value.setBounds(v.withSizeKeepingCentre(juce::jmin(v.getWidth(), hero ? 110 : 90), v.getHeight()));
    slider.setBounds(r);
}

void Knob::paint(juce::Graphics& g) {
    auto r = getLocalBounds().removeFromTop(hero ? 22 : 17);
    g.setFont(tracked(hero ? Fonts::monoMedium(13.0f) : Fonts::mono(11.0f), hero ? 0.22f : 0.12f));
    g.setColour(dimmed ? col::faint : (hero ? col::text : col::dim));
    g.drawText(caption.toUpperCase(), r, juce::Justification::centred);
    if (dimmed && dimReason.isNotEmpty()) {
        g.setFont(Fonts::mono(10.0f));
        g.setColour(col::faint);
        g.drawText(dimReason, slider.getBounds().withTrimmedTop(slider.getHeight() - 14), juce::Justification::centred);
    }
}

// ============================================================================ Scope
ScopeView::ScopeView() : hist(kHist) { setOpaque(false); }

void ScopeView::push(const td1::MeterFrame& f) {
    hist[(size_t)head] = f;
    head = (head + 1) % kHist;
    lastGain = std::abs(f.gainMaxDb) >= std::abs(f.gainMinDb) ? f.gainMaxDb : f.gainMinDb;
}

void ScopeView::paint(juce::Graphics& g) {
    const auto full = getLocalBounds().toFloat();
    drawPanel(g, full);
    auto r = full.reduced(10.0f, 8.0f);
    r.removeFromTop(16.0f);   // legend row
    const float mid = r.getCentreY();
    const float ampH = r.getHeight() * 0.46f;
    const float gScale = r.getHeight() * 0.42f / 24.0f;   // px per dB, +/-24 dB

    // grid: gain lines at 0, +/-12, +/-24 dB
    for (int db : { -24, -12, 12, 24 }) {
        const float y = mid - (float)db * gScale;
        g.setColour(col::line.withAlpha(0.55f));
        for (float x = r.getX(); x < r.getRight(); x += 6.0f) g.drawLine(x, y, x + 2.0f, y, 1.0f);
        g.setColour(col::faint);
        g.setFont(Fonts::mono(9.5f));
        g.drawText((db > 0 ? "+" : "") + juce::String(db), juce::Rectangle<float>(r.getRight() - 28.0f, y - 11.0f, 28.0f, 10.0f),
                   juce::Justification::centredRight);
    }
    g.setColour(col::line);
    g.drawHorizontalLine((int)mid, r.getX(), r.getRight());

    // column aggregation: ~3 s across the width
    const int W = juce::jmax(1, (int)r.getWidth());
    const int span = juce::jmin(kHist - 1, 3000);
    const float fpp = (float)span / (float)W;
    struct Col { float dry = 0, out = 0, gMax = -100, gMin = 100, pre = 0; bool hit = false; };
    std::vector<Col> cols((size_t)W);
    for (int x = 0; x < W; ++x) {
        const int a = (int)((float)x * fpp), b = juce::jmax(a + 1, (int)((float)(x + 1) * fpp));
        Col c;
        for (int k = a; k < b; ++k) {
            const auto& f = hist[(size_t)((head - span + k + kHist * 2) % kHist)];
            c.dry = juce::jmax(c.dry, f.dryPeak); c.out = juce::jmax(c.out, f.outPeak);
            c.gMax = juce::jmax(c.gMax, f.gainMaxDb); c.gMin = juce::jmin(c.gMin, f.gainMinDb);
            c.pre = juce::jmax(c.pre, f.preWeight); c.hit |= f.hit;
        }
        cols[(size_t)x] = c;
    }
    auto ampY = [&](float lin) {   // gentle log-ish compression keeps quiet tails visible
        const float v = juce::jlimit(0.0f, 1.0f, lin);
        return std::sqrt(v) * ampH;
    };

    // pre-window shading
    for (int x = 0; x < W; ++x)
        if (cols[(size_t)x].pre > 0.02f) {
            g.setColour(col::pre.withAlpha(0.22f * cols[(size_t)x].pre));
            g.fillRect(r.getX() + (float)x, r.getY(), 1.0f, r.getHeight());
        }

    // dry ghost then output, mirrored
    juce::Path dry, out;
    for (int pass = 0; pass < 2; ++pass) {
        juce::Path& p = pass == 0 ? dry : out;
        p.startNewSubPath(r.getX(), mid);
        for (int x = 0; x < W; ++x) p.lineTo(r.getX() + (float)x, mid - ampY(pass == 0 ? cols[(size_t)x].dry : cols[(size_t)x].out));
        for (int x = W - 1; x >= 0; --x) p.lineTo(r.getX() + (float)x, mid + ampY(pass == 0 ? cols[(size_t)x].dry : cols[(size_t)x].out));
        p.closeSubPath();
    }
    g.setColour(col::ghost.withAlpha(0.9f));
    g.fillPath(dry);
    const auto outCol = delta ? col::delta : col::signal;
    g.setColour(outCol.withAlpha(0.55f));
    g.fillPath(out);
    g.setColour(outCol.withAlpha(0.9f));
    g.strokePath(out, juce::PathStrokeType(0.8f));

    // gain trace: band (min..max) + centre line, fill toward 0 dB
    juce::Path gl, fillUp;
    fillUp.startNewSubPath(r.getX(), mid);
    for (int x = 0; x < W; ++x) {
        const auto& c = cols[(size_t)x];
        const float v = std::abs(c.gMax) >= std::abs(c.gMin) ? c.gMax : c.gMin;
        const float y = mid - juce::jlimit(-24.0f, 24.0f, v) * gScale;
        if (x == 0) gl.startNewSubPath(r.getX(), y); else gl.lineTo(r.getX() + (float)x, y);
        fillUp.lineTo(r.getX() + (float)x, y);
    }
    fillUp.lineTo(r.getRight(), mid);
    fillUp.closeSubPath();
    g.setColour(col::amber.withAlpha(0.10f));
    g.fillPath(fillUp);
    g.setColour(col::amber.withAlpha(0.25f));
    g.strokePath(gl, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(col::amber);
    g.strokePath(gl, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // onset markers
    g.setColour(col::amber.withAlpha(0.7f));
    for (int x = 0; x < W; ++x)
        if (cols[(size_t)x].hit) {
            juce::Path tri;
            const float px = r.getX() + (float)x;
            tri.addTriangle(px - 3.0f, r.getY(), px + 3.0f, r.getY(), px, r.getY() + 5.0f);
            g.fillPath(tri);
        }

    // legend
    auto lg = full.reduced(10.0f, 6.0f).removeFromTop(14.0f);
    g.setFont(tracked(Fonts::mono(10.0f), 0.1f));
    auto key = [&](juce::Colour c, const juce::String& t) {
        g.setColour(c);
        g.fillRoundedRectangle(lg.removeFromLeft(8.0f).withSizeKeepingCentre(8.0f, 8.0f), 1.5f);
        lg.removeFromLeft(5.0f);
        g.setColour(col::dim);
        const float tw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), t) + 14.0f;
        g.drawText(t, lg.removeFromLeft(tw), juce::Justification::centredLeft);
    };
    key(col::ghost.brighter(0.4f), "DRY");
    key(outCol, delta ? "DELTA" : "OUTPUT");
    key(col::amber, "GAIN");
    key(col::pre, "PRE WINDOW");
    g.setFont(Fonts::monoMedium(11.0f));
    g.setColour(col::amber);
    g.drawText((lastGain >= 0 ? "+" : "") + juce::String(lastGain, 1) + " dB", lg, juce::Justification::centredRight);
}

// ============================================================================ Lens
LensView::LensView() { startTimerHz(30); }

void LensView::setSnapshot(const td1::LensSnapshot& s) {
    if (held) return;
    snap = s; has = true; fresh = 1.0f;
    repaint();
}

void LensView::mouseUp(const juce::MouseEvent&) { held = !held; repaint(); }

void LensView::timerCallback() {
    if (fresh > 0.0f) { fresh = juce::jmax(0.0f, fresh - 0.08f); repaint(); }
}

void LensView::paint(juce::Graphics& g) {
    const auto full = getLocalBounds().toFloat();
    drawPanel(g, full);
    auto hdr = full.reduced(10.0f, 6.0f).removeFromTop(14.0f);
    g.setFont(tracked(Fonts::monoMedium(10.5f), 0.16f));
    g.setColour(col::text);
    g.drawText("FORESIGHT LENS", hdr, juce::Justification::centredLeft);
    g.setFont(tracked(Fonts::mono(10.0f), 0.1f));
    g.setColour(held ? col::amber : col::faint);
    g.drawText(held ? "HELD" : "HIT-LOCKED", hdr, juce::Justification::centredRight);

    auto r = full.reduced(10.0f, 8.0f);
    r.removeFromTop(18.0f);
    auto foot = r.removeFromBottom(16.0f);
    if (!has) {
        g.setFont(Fonts::mono(11.0f));
        g.setColour(col::faint);
        g.drawText("waiting for a transient...", r, juce::Justification::centred);
        return;
    }
    const int B = td1::LensSnapshot::kBins;
    const float mid = r.getCentreY();
    const float ampH = r.getHeight() * 0.44f;
    const float gScale = r.getHeight() * 0.42f / 24.0f;
    auto xOf = [&](float ms) { return r.getX() + r.getWidth() * (ms / snap.windowMs); };
    auto bx = [&](int b) { return r.getX() + r.getWidth() * ((float)b + 0.5f) / (float)B; };

    // normalise amplitude to the window peak so every hit fills the lens
    float pk = 1e-4f;
    for (int b = 0; b < B; ++b)
        pk = juce::jmax(pk, juce::jmax(std::abs(snap.dryMin[b]), std::abs(snap.dryMax[b])), juce::jmax(std::abs(snap.outMin[b]), std::abs(snap.outMax[b])));
    const float norm = ampH / pk;

    // lookahead window (onset - L .. onset) and pre shading
    const float xOn = xOf(snap.onsetMs);
    const float xLa = xOf(snap.onsetMs - snap.lookaheadMs);
    if (snap.lookaheadMs > 0.0f) {
        g.setColour(col::amber.withAlpha(0.06f));
        g.fillRect(juce::Rectangle<float>(xLa, r.getY(), xOn - xLa, r.getHeight()));
    }
    for (int b = 0; b < B; ++b)
        if (snap.preW[b] > 0.02f) {
            g.setColour(col::pre.withAlpha(0.28f * snap.preW[b]));
            g.fillRect(juce::Rectangle<float>(bx(b) - r.getWidth() / (float)B * 0.5f, r.getY(), r.getWidth() / (float)B + 0.5f, r.getHeight()));
        }

    g.setColour(col::line);
    g.drawHorizontalLine((int)mid, r.getX(), r.getRight());

    auto band = [&](const float* mn, const float* mx) {
        juce::Path p;
        p.startNewSubPath(bx(0), mid - mx[0] * norm);
        for (int b = 1; b < B; ++b) p.lineTo(bx(b), mid - mx[b] * norm);
        for (int b = B - 1; b >= 0; --b) p.lineTo(bx(b), mid - mn[b] * norm);
        p.closeSubPath();
        return p;
    };
    g.setColour(col::ghost);
    g.fillPath(band(snap.dryMin, snap.dryMax));
    g.setColour(col::signal.withAlpha(0.6f));
    auto ob = band(snap.outMin, snap.outMax);
    g.fillPath(ob);
    g.setColour(col::signal);
    g.strokePath(ob, juce::PathStrokeType(0.7f));

    // gain curve
    juce::Path gp;
    for (int b = 0; b < B; ++b) {
        const float y = mid - juce::jlimit(-24.0f, 24.0f, snap.gainDb[b]) * gScale;
        if (b == 0) gp.startNewSubPath(bx(b), y); else gp.lineTo(bx(b), y);
    }
    g.setColour(col::amber.withAlpha(0.25f + 0.25f * fresh));
    g.strokePath(gp, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(col::amber);
    g.strokePath(gp, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // onset marker + lookahead bracket
    g.setColour(col::text.withAlpha(0.7f));
    const float dashes[] = { 3.0f, 3.0f };
    g.drawDashedLine(juce::Line<float>(xOn, r.getY(), xOn, r.getBottom()), dashes, 2, 1.0f);
    g.setFont(tracked(Fonts::mono(9.5f), 0.1f));
    g.drawText("ONSET", juce::Rectangle<float>(xOn + 3.0f, r.getY(), 60.0f, 11.0f), juce::Justification::centredLeft);
    if (snap.lookaheadMs > 0.0f) {
        const float yb = r.getBottom() - 4.0f;
        g.setColour(col::amber.withAlpha(0.8f));
        g.drawLine(xLa, yb, xOn, yb, 1.0f);
        g.drawLine(xLa, yb - 4.0f, xLa, yb + 1.0f, 1.0f);
        g.drawLine(xOn, yb - 4.0f, xOn, yb + 1.0f, 1.0f);
        // label centred over the bracket when it fits, otherwise beside it, kept inside the lens
        const float lw = 64.0f;
        auto lab = (xOn - xLa) >= lw ? juce::Rectangle<float>(xLa, yb - 13.0f, xOn - xLa, 11.0f)
                                     : juce::Rectangle<float>(xLa - lw - 3.0f, yb - 13.0f, lw, 11.0f);
        if (lab.getX() < r.getX()) lab = juce::Rectangle<float>(xOn + 4.0f, yb - 13.0f, lw, 11.0f);
        g.drawText("LOOKAHEAD", lab, (xOn - xLa) >= lw ? juce::Justification::centred : juce::Justification::centredLeft);
    }

    // footer: numbers
    g.setFont(Fonts::mono(10.0f));
    g.setColour(col::dim);
    g.drawText(juce::String(juce::roundToInt(snap.windowMs)) + " ms window   lookahead " + juce::String(snap.lookaheadMs, 1) + " ms",
               foot, juce::Justification::centredLeft);
    g.setFont(Fonts::monoMedium(11.0f));
    g.setColour(col::amber);
    g.drawText((snap.peakGainDb >= 0 ? "peak +" : "peak ") + juce::String(snap.peakGainDb, 1) + " dB", foot,
               juce::Justification::centredRight);

    if (fresh > 0.0f) {
        g.setColour(col::amber.withAlpha(0.18f * fresh));
        g.drawRoundedRectangle(full.reduced(1.0f), 4.0f, 1.5f);
    }
}

// ============================================================================ Meter
void OutputMeter::push(float peakLin) { pending = juce::jmax(pending, peakLin); }

void OutputMeter::tick() {
    const float db = dbOf(pending);
    pending = 0.0f;
    level = db > level ? db : juce::jmax(db, level - 1.2f);   // ~36 dB/s at 30 Hz
    if (db >= hold) { hold = db; holdAge = 0; } else if (++holdAge > 45) hold = juce::jmax(level, hold - 0.8f);
    maxDb = juce::jmax(maxDb, db);
    if (db > -0.05f) clip = true;
    repaint();
}

void OutputMeter::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    auto label = r.removeFromLeft(30.0f);
    auto readout = r.removeFromRight(52.0f);
    g.setFont(tracked(Fonts::mono(10.0f), 0.12f));
    g.setColour(col::dim);
    g.drawText("OUT", label, juce::Justification::centredLeft);

    auto bar = r.withSizeKeepingCentre(r.getWidth(), 8.0f);
    g.setColour(col::screen);
    g.fillRoundedRectangle(bar, 2.0f);
    g.setColour(col::line);
    g.drawRoundedRectangle(bar, 2.0f, 1.0f);
    const float lo = -48.0f;
    auto xOf = [&](float db) { return bar.getX() + bar.getWidth() * juce::jlimit(0.0f, 1.0f, (db - lo) / -lo); };
    for (float db : { -36.0f, -24.0f, -12.0f, -6.0f, 0.0f }) {
        g.setColour(col::faint);
        g.drawVerticalLine((int)xOf(db), bar.getBottom() + 1.0f, bar.getBottom() + 4.0f);
    }
    const float x = xOf(level);
    if (x > bar.getX() + 1.0f) {
        juce::ColourGradient grad(col::signal.withAlpha(0.8f), bar.getX(), 0, col::amber, xOf(-3.0f), 0, false);
        grad.addColour(1.0, col::delta);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bar.withRight(x).reduced(1.0f), 1.5f);
    }
    g.setColour(col::text);
    g.fillRect(juce::Rectangle<float>(xOf(hold) - 1.0f, bar.getY() + 1.0f, 1.5f, bar.getHeight() - 2.0f));

    g.setFont(Fonts::monoMedium(11.0f));
    g.setColour(clip ? col::delta : col::text);
    g.drawText(maxDb <= -99.0f ? juce::String("-inf") : juce::String(maxDb, 1), readout, juce::Justification::centredRight);
}

} // namespace td1::ui
