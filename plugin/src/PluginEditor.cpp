#include "PluginEditor.h"

namespace td1::ui {

using namespace td1::param;

// ============================================================================ PresetBox
void PresetBox::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(col::screen);
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(col::line);
    g.drawRoundedRectangle(r, 3.0f, 1.0f);

    const auto mouse = getMouseXYRelative();
    const bool over = isMouseOver();
    auto arrow = [&](juce::Rectangle<int> a, bool left) {
        const auto c = a.toFloat().getCentre();
        juce::Path p;
        if (left) p.addTriangle(c.x + 3, c.y - 4.5f, c.x + 3, c.y + 4.5f, c.x - 3, c.y);
        else p.addTriangle(c.x - 3, c.y - 4.5f, c.x - 3, c.y + 4.5f, c.x + 3, c.y);
        g.setColour(over && a.contains(mouse) ? col::amber : col::dim);
        g.fillPath(p);
    };
    arrow(leftArrow(), true);
    arrow(rightArrow(), false);
    g.setColour(col::line);
    g.drawVerticalLine(leftArrow().getRight(), r.getY() + 4, r.getBottom() - 4);
    g.drawVerticalLine(rightArrow().getX(), r.getY() + 4, r.getBottom() - 4);

    auto mid = getLocalBounds().reduced(30, 0);
    const auto& e = presets.entries();
    const int idx = presets.currentIndex();
    const juce::String cat = juce::isPositiveAndBelow(idx, e.size()) ? e[idx].category.toUpperCase() : juce::String();
    g.setFont(tracked(Fonts::mono(9.5f), 0.14f));
    g.setColour(col::faint);
    g.drawText(cat, mid.removeFromLeft(78), juce::Justification::centredLeft);
    g.setFont(Fonts::monoMedium(13.0f));
    const bool dirty = presets.isDirty();
    const bool hot = over && !leftArrow().contains(mouse) && !rightArrow().contains(mouse);
    g.setColour(hot ? col::amber : col::text);
    g.drawText(presets.currentName() + (dirty ? " *" : ""), mid, juce::Justification::centredLeft);
}

void PresetBox::mouseUp(const juce::MouseEvent& e) {
    if (leftArrow().contains(e.getPosition())) presets.step(-1);
    else if (rightArrow().contains(e.getPosition())) presets.step(1);
    else showMenu();
    repaint();
}

void PresetBox::showMenu() {
    presets.refresh();
    juce::PopupMenu m;
    const auto& e = presets.entries();
    juce::String lastCat;
    juce::PopupMenu user;
    for (int i = 0; i < e.size(); ++i) {
        if (!e[i].factory) { user.addItem(i + 1, e[i].name, true, i == presets.currentIndex()); continue; }
        if (e[i].category != lastCat) { m.addSectionHeader(e[i].category.toUpperCase()); lastCat = e[i].category; }
        m.addItem(i + 1, e[i].name, true, i == presets.currentIndex());
    }
    m.addSeparator();
    if (user.getNumItems() > 0) m.addSubMenu("User", user);
    m.addItem(10001, "Save As...");
    const bool canDelete = juce::isPositiveAndBelow(presets.currentIndex(), e.size()) && !e[presets.currentIndex()].factory;
    m.addItem(10002, "Delete \"" + presets.currentName() + "\"", canDelete);
    m.addItem(10003, "Show Preset Folder");

    juce::Component::SafePointer<PresetBox> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()),
        [safe](int r) {
            if (safe == nullptr || r == 0) return;
            auto& pm = safe->presets;
            if (r == 10001) safe->saveAs();
            else if (r == 10002) pm.deleteUser(pm.currentIndex());
            else if (r == 10003) { auto d = td1::PresetManager::userDirectory(); d.createDirectory(); d.revealToUser(); }
            else pm.load(r - 1);
            safe->repaint();
        });
}

void PresetBox::saveAs() {
    auto* w = new juce::AlertWindow("Save Preset", "Name for the new user preset:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel(&getLookAndFeel());
    w->addTextEditor("name", presets.entries()[presets.currentIndex()].factory ? juce::String() : presets.currentName());
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PresetBox> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w](int r) {
        if (safe != nullptr && r == 1) safe->presets.saveUser(w->getTextEditorContents("name"));
        if (safe != nullptr) safe->repaint();
    }), true);
}

// ============================================================================ Content
Content::Content(TD1Processor& p)
    : proc(p), state(p.state()), presetBox(p.presets()),
      pre(state, kPre, "Pre", true, true, col::pre),
      attack(state, kAttack, "Attack", true, true),
      sustain(state, kSustain, "Sustain", true, true),
      speed(state, kSpeed, "Speed", false, false),
      hpf(state, kHpf, "Det HPF", false, false),
      threshold(state, kThreshold, "Threshold", false, false),
      lookahead(state, kLookahead, "Lookahead", false, false),
      tilt(state, kTilt, "Tilt", false, true),
      pivot(state, kPivot, "Pivot", false, false),
      mix(state, kMix, "Mix", false, false),
      output(state, kOutput, "Output", false, true) {

    addAndMakeVisible(presetBox);
    presetBox.setTooltip("Presets. Arrows step through; click the name for the full list, Save As and user presets.");
    for (auto* b : { &abA, &abB }) { b->setClickingTogglesState(false); addAndMakeVisible(b); }
    abA.getProperties().set("seg", 1);
    abB.getProperties().set("seg", 2);
    abCopy.getProperties().set("seg", 3);
    abA.onClick = [this] { proc.presets().selectSlot('A'); refreshAB(); };
    abB.onClick = [this] { proc.presets().selectSlot('B'); refreshAB(); };
    abCopy.onClick = [this] { proc.presets().copyActiveToOther(); };
    abA.setTooltip("Compare slot A");
    abB.setTooltip("Compare slot B");
    abCopy.setTooltip("Copy the active slot's settings into the other slot");
    addAndMakeVisible(abCopy);
    undoBtn.onClick = [this] { proc.undo().undo(); };
    redoBtn.onClick = [this] { proc.undo().redo(); };
    undoBtn.setTooltip("Undo (Cmd/Ctrl+Z)");
    redoBtn.setTooltip("Redo (Cmd/Ctrl+Shift+Z)");
    addAndMakeVisible(undoBtn);
    addAndMakeVisible(redoBtn);
    proc.presets().onChange = [this] { presetBox.refresh(); refreshAB(); };

    addAndMakeVisible(scope);
    addAndMakeVisible(lens);
    lens.setTooltip("Foresight Lens: a zoomed capture of the latest transient. The gain moves inside the "
                    "lookahead window, before the onset reaches the output. Click to hold or release.");
    scope.setTooltip("Output timeline: dry signal (grey), processed output, and the applied gain (amber). "
                     "The gain leads each transient by the lookahead time.");
    addAndMakeVisible(meter);
    meter.setTooltip("Output peak. Click to reset the max-peak readout.");

    struct K { Knob* k; const char* tip; };
    const K knobs[] = {
        { &pre, "PRE: gain for the moment just before each hit, inside the lookahead window. "
                "Turn down for a clean gap that makes the hit jump out; turn up for a reverse-style swell into it. "
                "Needs lookahead." },
        { &attack, "ATTACK: boost or cut the transient. Detection is level-independent: quiet and loud hits are shaped alike." },
        { &sustain, "SUSTAIN: boost or cut the body and tail after the transient." },
        { &speed, "SPEED: detector time scale. Higher = shorter transient window and tighter tails." },
        { &hpf, "DET HPF: high-pass on the detector only (the audio isn't filtered), so low end doesn't trigger the shaping." },
        { &threshold, "THRESHOLD (bleed focus): onsets quieter than this aren't shaped, e.g. hi-hat bleed in a snare mic. Off = shape everything." },
        { &lookahead, "LOOKAHEAD: how far ahead the detector sees. This is also the plugin's latency (reported to the host). 0 = zero-latency mode." },
        { &tilt, "TILT: spectral tilt applied only during transients. + brightens the attack, - darkens it." },
        { &pivot, "PIVOT: the frequency the transient tilt pivots around." },
        { &mix, "MIX: parallel blend of processed and dry (latency-aligned)." },
        { &output, "OUTPUT: output trim." },
    };
    for (auto& k : knobs) {
        addAndMakeVisible(k.k);
        k.k->setTooltipText(k.tip);
        k.k->onGestureStart = [this] { proc.undo().beginNewTransaction(); };
    }

    // footer toggles
    auto* link = &toggle("LINK", kLink, "Stereo link: both channels share one detector, so the stereo image holds steady.");
    juce::ignoreUnused(link);
    toggle("CLIP", kClip, "Soft clip: anti-aliased (ADAA) tanh clipper on the output, catches boosted peaks.");
    autoBtn = &toggle("AUTO GAIN", kAutoGain, "Auto gain: matches the processed loudness to the dry signal, so A/B and bypass comparisons are fair.", col::good);
    deltaBtn = &toggle("DELTA", kDelta, "Delta: hear only what the processing adds or removes.", col::delta);
    toggle("LISTEN", kAudition, "Listen: audition the detector sidechain (after the HPF).", col::signal);
    bypassBtn = &toggle("BYPASS", kBypass, "Bypass: crossfades to the dry signal, latency-aligned.", col::delta);

    // target segmented control
    auto* tparam = state.getParameter(kTarget);
    targetAttachment = std::make_unique<juce::ParameterAttachment>(*tparam, [this](float v) {
        const int i = juce::roundToInt(v);
        tgtStereo.setToggleState(i == 0, juce::dontSendNotification);
        tgtMid.setToggleState(i == 1, juce::dontSendNotification);
        tgtSide.setToggleState(i == 2, juce::dontSendNotification);
    }, &proc.undo());
    int segIdx = 1;
    for (auto* b : { &tgtStereo, &tgtMid, &tgtSide }) {
        b->getProperties().set("seg", segIdx);
        const int value = segIdx - 1;
        b->onClick = [this, value] { proc.undo().beginNewTransaction(); targetAttachment->setValueAsCompleteGesture((float)value); };
        b->setTooltip("Target: shape the full stereo signal, only the Mid (centre), or only the Sides.");
        addAndMakeVisible(b);
        ++segIdx;
    }
    targetAttachment->sendInitialUpdate();
    refreshAB();
    setSize(kW, kH);
}

Content::~Content() { proc.presets().onChange = nullptr; }

juce::TextButton& Content::toggle(const juce::String& text, const char* paramId, const juce::String& tip, juce::Colour accent) {
    auto* b = toggles.add(new juce::TextButton(text));
    b->setClickingTogglesState(true);
    b->getProperties().set("led", true);
    b->getProperties().set("accent", (int)accent.getARGB());
    b->setTooltip(tip);
    b->onClick = [this] { proc.undo().beginNewTransaction(); };
    addAndMakeVisible(b);
    toggleAttachments.add(new BA(state, paramId, *b));
    return *b;
}

void Content::refreshAB() {
    const bool a = proc.presets().activeSlot() == 'A';
    abA.setToggleState(a, juce::dontSendNotification);
    abB.setToggleState(!a, juce::dontSendNotification);
    abCopy.setButtonText(a ? "A>B" : "B>A");
}

void Content::resized() {
    // header
    presetBox.setBounds(318, 14, 282, 28);
    abA.setBounds(612, 14, 32, 28);
    abB.setBounds(644, 14, 32, 28);
    abCopy.setBounds(676, 14, 46, 28);
    undoBtn.setBounds(732, 14, 50, 28);
    redoBtn.setBounds(782, 14, 50, 28);
    undoBtn.getProperties().set("seg", 1);
    redoBtn.getProperties().set("seg", 3);
    bypassBtn->setBounds(844, 14, 100, 28);

    // displays
    scope.setBounds(16, 62, 600, 236);
    lens.setBounds(628, 62, 316, 236);

    // hero knobs
    const int heroY = 330, heroW = 142, heroH = 176;
    pre.setBounds(20, heroY, heroW, heroH);
    attack.setBounds(20 + heroW + 8, heroY, heroW, heroH);
    sustain.setBounds(20 + (heroW + 8) * 2, heroY, heroW, heroH);

    // secondary knobs
    const int sx = 498, sw = 111, sh = 92;
    Knob* row1[] = { &speed, &hpf, &threshold, &lookahead };
    Knob* row2[] = { &tilt, &pivot, &mix, &output };
    for (int i = 0; i < 4; ++i) {
        row1[i]->setBounds(sx + i * sw, 326, sw, sh);
        row2[i]->setBounds(sx + i * sw, 438, sw, sh);
    }

    // footer
    int x = 84;
    for (auto* b : { &tgtStereo, &tgtMid, &tgtSide }) { b->setBounds(x, 551, 68, 28); x += 68; }
    x += 16;
    for (auto* b : toggles) {
        if (b == bypassBtn) continue;
        const int w = b->getButtonText().length() * 8 + 40;
        b->setBounds(x, 551, w, 28);
        x += w + 6;
    }
    meter.setBounds(724, 551, 220, 28);
}

void Content::paint(juce::Graphics& g) {
    // background: graphite with a soft top light
    juce::ColourGradient bg(juce::Colour(0xff15181d), 0, 0, col::bg, 0, (float)kH, false);
    g.setGradientFill(bg);
    g.fillAll();

    // header strip
    g.setColour(juce::Colour(0xff12151a));
    g.fillRect(0, 0, kW, 54);
    g.setColour(col::line);
    g.drawHorizontalLine(54, 0, (float)kW);
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    g.drawHorizontalLine(55, 0, (float)kW);

    // wordmark
    g.setFont(tracked(Fonts::display(34.0f), 0.06f));
    g.setColour(col::text);
    g.drawText("FORESIGHT", 18, 7, 150, 34, juce::Justification::centredLeft);
    g.setColour(col::amber);
    g.drawText("TD-1", 158, 7, 70, 34, juce::Justification::centredLeft);
    g.setFont(tracked(Fonts::mono(8.5f), 0.2f));
    g.setColour(col::dim);
    g.drawText("ANTICIPATORY TRANSIENT DESIGNER", 19, 38, 240, 11, juce::Justification::centredLeft);

    // latency readout (left of presets)
    g.setFont(tracked(Fonts::mono(9.0f), 0.14f));
    g.setColour(col::faint);
    g.drawText("LATENCY", 236, 14, 72, 12, juce::Justification::centredRight);
    g.setFont(Fonts::monoMedium(11.5f));
    g.setColour(latencyText == "ZERO" ? col::good : col::text);
    g.drawText(latencyText, 236, 27, 72, 14, juce::Justification::centredRight);

    // section captions with hairlines
    auto section = [&](int x, int y, int w, const juce::String& t) {
        g.setFont(tracked(Fonts::mono(9.5f), 0.22f));
        g.setColour(col::dim);
        const int tw = (int)juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), t) + 10;
        g.drawText(t, x, y - 6, tw, 12, juce::Justification::centredLeft);
        g.setColour(col::line);
        g.drawHorizontalLine(y, (float)(x + tw), (float)(x + w));
    };
    section(20, 314, 442, "SHAPE");
    section(498, 314, 446, "DETECTOR");
    section(498, 430, 446, "TONE / OUTPUT");

    // divider between hero and secondary sections
    g.setColour(col::line);
    g.drawVerticalLine(480, 326.0f, 520.0f);

    // footer strip
    g.setColour(juce::Colour(0xff111418));
    g.fillRect(0, 536, kW, kH - 536);
    g.setColour(col::line);
    g.drawHorizontalLine(536, 0, (float)kW);
    g.setFont(tracked(Fonts::mono(9.5f), 0.18f));
    g.setColour(col::dim);
    g.drawText("TARGET", 18, 551, 60, 28, juce::Justification::centredLeft);
    if (autoBtn != nullptr && autoBtn->getToggleState()) {
        g.setFont(Fonts::mono(10.0f));
        g.setColour(col::good);
        g.drawText("matched " + juce::String(autoGainDb >= 0 ? "+" : "") + juce::String(autoGainDb, 1) + " dB",
                   724, 538, 220, 12, juce::Justification::centredRight);
    }
}

void Content::tick() {
    auto& tel = proc.telemetry();
    float lastAuto = autoGainDb;
    tel.drainFrames([&](const td1::MeterFrame& f) {
        scope.push(f);
        meter.push(f.outPeak);
        lastAuto = f.autoGainDb;
    });
    if (const auto* s = tel.takeLens()) lens.setSnapshot(*s);
    scope.setDeltaMode(deltaBtn != nullptr && deltaBtn->getToggleState());
    scope.repaint();

    if (++frameCount % 2 == 0) meter.tick();
    if (frameCount % 6 == 0) {
        const float look = state.getRawParameterValue(kLookahead)->load();
        const juce::String lt = look <= 0.0f ? juce::String("ZERO") : juce::String(look, 1) + " ms";
        const bool autoChanged = std::abs(lastAuto - autoGainDb) > 0.05f;
        autoGainDb = lastAuto;
        if (lt != latencyText || autoChanged) { latencyText = lt; repaint(0, 0, 320, 54); repaint(700, 536, 260, 16); }
        pre.setDimmed(look <= 0.0f, "needs lookahead");
        undoBtn.setEnabled(proc.undo().canUndo());
        redoBtn.setEnabled(proc.undo().canRedo());
    }
    if (frameCount % 15 == 0) { presetBox.refresh(); refreshAB(); }
}

} // namespace td1::ui

// ============================================================================ Editor
TD1Editor::TD1Editor(TD1Processor& p) : AudioProcessorEditor(p), proc(p), content(p) {
    const float s = juce::jlimit(0.75f, 2.0f, p.uiScale);   // read before limits trigger resized()
    setLookAndFeel(&lnf);
    addAndMakeVisible(content);
    tooltips.setLookAndFeel(&lnf);
    using C = td1::ui::Content;
    setResizable(true, true);
    setResizeLimits(C::kW * 3 / 4, C::kH * 3 / 4, C::kW * 2, C::kH * 2);
    getConstrainer()->setFixedAspectRatio((double)C::kW / (double)C::kH);
    setSize(juce::roundToInt(C::kW * s), juce::roundToInt(C::kH * s));
    setWantsKeyboardFocus(true);
    startTimerHz(60);
}

TD1Editor::~TD1Editor() {
    stopTimer();
    tooltips.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void TD1Editor::resized() {
    const float s = (float)getWidth() / (float)td1::ui::Content::kW;
    content.setTransform(juce::AffineTransform::scale(s));
    content.setBounds(0, 0, td1::ui::Content::kW, td1::ui::Content::kH);
    proc.uiScale = s;
}

bool TD1Editor::keyPressed(const juce::KeyPress& k) {
    if (k == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)) { proc.undo().undo(); return true; }
    if (k == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { proc.undo().redo(); return true; }
    return false;
}

void TD1Editor::timerCallback() { content.tick(); }
