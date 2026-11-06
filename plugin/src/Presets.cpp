#include "Presets.h"
#include "Parameters.h"

namespace td1 {

namespace {
struct Factory { const char* category; const char* name; std::map<juce::String, float> values; };

// Values not listed fall back to parameter defaults.
const std::vector<Factory>& factory() {
    using namespace td1::param;
    static const std::vector<Factory> f = {
        { "Init",    "Init",                {} },
        { "Kick",    "Kick - Punch",        { { kAttack, 9.f }, { kSustain, -3.f }, { kHpf, 30.f }, { kLookahead, 3.f } } },
        { "Kick",    "Kick - Tighten",      { { kAttack, 3.f }, { kSustain, -12.f }, { kHpf, 30.f }, { kSpeed, 1.3f } } },
        { "Kick",    "Kick - Gap Punch",    { { kAttack, 7.f }, { kPre, -14.f }, { kLookahead, 5.f }, { kHpf, 30.f } } },
        { "Snare",   "Snare - Crack",       { { kAttack, 10.f }, { kTilt, 6.f }, { kPivot, 1500.f } } },
        { "Snare",   "Snare - Fat Body",    { { kAttack, 2.f }, { kSustain, 8.f }, { kSpeed, 0.8f } } },
        { "Snare",   "Snare - Bleed Focus", { { kAttack, 9.f }, { kThreshold, -24.f }, { kHpf, 180.f } } },
        { "Toms",    "Toms - Ring Control", { { kAttack, 5.f }, { kSustain, -10.f }, { kThreshold, -30.f } } },
        { "Bus",     "Drum Bus - Glue Punch", { { kAttack, 5.f }, { kSustain, -2.f }, { kMix, 60.f }, { kAutoGain, 1.f } } },
        { "Bus",     "Drum Bus - Mid Snap", { { kTarget, 1.f }, { kAttack, 8.f }, { kTilt, 3.f } } },
        { "Room",    "Room - Tame",         { { kAttack, -6.f }, { kSustain, -9.f }, { kAutoGain, 1.f } } },
        { "Room",    "Room - Bloom",        { { kAttack, -3.f }, { kSustain, 10.f }, { kSpeed, 0.7f } } },
        { "Room",    "Wide Tails",          { { kTarget, 2.f }, { kAttack, -4.f }, { kSustain, 6.f } } },
        { "Pre",     "Pre - Reverse Swell", { { kAttack, 0.f }, { kPre, 12.f }, { kLookahead, 10.f } } },
        { "Pre",     "Pre - Clean Gap",     { { kAttack, 3.f }, { kPre, -24.f }, { kLookahead, 4.f } } },
        { "Instrument", "Guitar - Pick Softener", { { kAttack, -9.f }, { kTilt, -6.f }, { kPivot, 1800.f } } },
        { "Instrument", "Bass - Finger Pluck",    { { kAttack, 6.f }, { kHpf, 40.f }, { kSpeed, 0.7f } } },
        { "Instrument", "Percussion - Snap Back", { { kAttack, 12.f }, { kSpeed, 1.5f }, { kSustain, -4.f } } },
        { "Instrument", "Piano - Hammer Soften",  { { kAttack, -6.f }, { kTilt, -4.f }, { kLookahead, 2.f } } },
    };
    return f;
}

constexpr const char* kPresetTag = "TD1Preset";
constexpr const char* kSnapTag = "Snapshot";
} // namespace

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& s, juce::UndoManager& u)
    : state_(s), undo_(u) {
    refresh();
    loadedSnap_ = snapshot();
}

juce::File PresetManager::userDirectory() {
#if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library/Audio/Presets/Black Tie Ventures/Foresight TD-1");
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Black Tie Ventures/Foresight TD-1/Presets");
#endif
}

void PresetManager::refresh() {
    const juce::String keep = entries_.isEmpty() ? juce::String() : currentName();
    entries_.clear();
    for (const auto& f : factory()) entries_.add({ f.name, f.category, true, {} });
    auto dir = userDirectory();
    if (dir.isDirectory()) {
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.td1preset");
        files.sort();
        for (auto& file : files) entries_.add({ file.getFileNameWithoutExtension(), "User", false, file });
    }
    if (keep.isNotEmpty())
        for (int i = 0; i < entries_.size(); ++i)
            if (entries_[i].name == keep) { current_ = i; break; }
}

juce::String PresetManager::currentName() const {
    return juce::isPositiveAndBelow(current_, entries_.size()) ? entries_[current_].name : juce::String("Init");
}

juce::ValueTree PresetManager::snapshot() const {
    juce::ValueTree t(kSnapTag);
    for (auto& id : param::allIds()) {
        if (id == param::kBypass) continue;   // bypass is never part of a preset
        if (auto* p = state_.getParameter(id))
            t.setProperty(id, p->convertFrom0to1(p->getValue()), nullptr);
    }
    return t;
}

bool PresetManager::isDirty() const {
    const auto now = snapshot();
    for (int i = 0; i < now.getNumProperties(); ++i) {
        const auto id = now.getPropertyName(i);
        if (std::abs((float)now[id] - (float)loadedSnap_.getProperty(id, (float)now[id])) > 1e-4f) return true;
    }
    return false;
}

void PresetManager::apply(const juce::ValueTree& snap) {
    undo_.beginNewTransaction("Load preset");
    for (auto& id : param::allIds()) {
        if (id == param::kBypass) continue;
        if (auto* p = state_.getParameter(id)) {
            const float v = snap.hasProperty(id) ? (float)snap[id] : p->convertFrom0to1(p->getDefaultValue());
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(v));
            p->endChangeGesture();
        }
    }
}

void PresetManager::applyMap(const std::map<juce::String, float>& values) {
    juce::ValueTree t(kSnapTag);
    for (auto& [k, v] : values) t.setProperty(k, v, nullptr);
    apply(t);
}

void PresetManager::load(int index) {
    if (!juce::isPositiveAndBelow(index, entries_.size())) return;
    const auto& e = entries_[index];
    if (e.factory) {
        for (const auto& f : factory()) if (e.name == f.name) { applyMap(f.values); break; }
    } else if (auto xml = juce::XmlDocument::parse(e.file)) {
        const auto t = juce::ValueTree::fromXml(*xml);
        if (t.hasType(kPresetTag)) apply(t.getChildWithName(kSnapTag));
    }
    current_ = index;
    loadedSnap_ = snapshot();
    if (onChange) onChange();
}

void PresetManager::step(int delta) {
    if (entries_.isEmpty()) return;
    load((current_ + delta + entries_.size()) % entries_.size());
}

bool PresetManager::saveUser(const juce::String& rawName) {
    const auto name = juce::File::createLegalFileName(rawName.trim());
    if (name.isEmpty()) return false;
    auto dir = userDirectory();
    if (!dir.createDirectory()) return false;
    juce::ValueTree t(kPresetTag);
    t.setProperty("version", 1, nullptr);
    t.setProperty("product", "Foresight TD-1", nullptr);
    t.appendChild(snapshot(), nullptr);
    const auto file = dir.getChildFile(name + ".td1preset");
    if (auto xml = t.createXml(); xml == nullptr || !xml->writeTo(file)) return false;
    refresh();
    for (int i = 0; i < entries_.size(); ++i)
        if (!entries_[i].factory && entries_[i].file == file) current_ = i;
    loadedSnap_ = snapshot();
    if (onChange) onChange();
    return true;
}

bool PresetManager::deleteUser(int index) {
    if (!juce::isPositiveAndBelow(index, entries_.size()) || entries_[index].factory) return false;
    if (!entries_[index].file.deleteFile()) return false;
    refresh();
    current_ = juce::jlimit(0, entries_.size() - 1, current_);
    if (onChange) onChange();
    return true;
}

void PresetManager::selectSlot(char s) {
    const int want = s == 'B' ? 1 : 0;
    if (want == slot_) return;
    slots_[slot_] = snapshot();
    slot_ = want;
    if (slots_[slot_].isValid()) apply(slots_[slot_]);
    if (onChange) onChange();
}

void PresetManager::copyActiveToOther() {
    slots_[1 - slot_] = snapshot();
    if (onChange) onChange();
}

juce::ValueTree PresetManager::toTree() const {
    juce::ValueTree t("Presets");
    t.setProperty("current", currentName(), nullptr);
    t.setProperty("slot", slot_, nullptr);
    for (int i = 0; i < 2; ++i) {
        juce::ValueTree s("Slot");
        s.setProperty("index", i, nullptr);
        if (slots_[i].isValid()) s.appendChild(slots_[i].createCopy(), nullptr);
        t.appendChild(s, nullptr);
    }
    juce::ValueTree loaded("Loaded");
    loaded.appendChild(loadedSnap_.createCopy(), nullptr);
    t.appendChild(loaded, nullptr);
    return t;
}

void PresetManager::fromTree(const juce::ValueTree& t) {
    if (!t.isValid()) return;
    refresh();
    const auto name = t["current"].toString();
    for (int i = 0; i < entries_.size(); ++i) if (entries_[i].name == name) { current_ = i; break; }
    slot_ = juce::jlimit(0, 1, (int)t.getProperty("slot", 0));
    for (auto s : t) {
        if (s.hasType("Slot")) {
            const int i = juce::jlimit(0, 1, (int)s["index"]);
            slots_[i] = s.getChildWithName(kSnapTag).createCopy();
        } else if (s.hasType("Loaded")) {
            const auto snap = s.getChildWithName(kSnapTag);
            if (snap.isValid()) loadedSnap_ = snap.createCopy();
        }
    }
    // No onChange here: hosts may restore state off the message thread. The
    // editor polls preset/A-B state instead.
}

} // namespace td1
