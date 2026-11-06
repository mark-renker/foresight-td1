#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace td1 {

// Factory presets (compiled in) + user presets (XML files on disk), plus A/B
// compare slots. All methods are message-thread only.
class PresetManager {
public:
    struct Entry { juce::String name, category; bool factory = true; juce::File file; };

    PresetManager(juce::AudioProcessorValueTreeState& state, juce::UndoManager& undo);

    void refresh();
    const juce::Array<Entry>& entries() const { return entries_; }
    int currentIndex() const { return current_; }
    juce::String currentName() const;
    bool isDirty() const;

    void load(int index);
    void step(int delta);
    bool saveUser(const juce::String& name);
    bool deleteUser(int index);
    static juce::File userDirectory();

    // A/B: two full parameter snapshots; switching stores the live state into
    // the active slot and recalls the other.
    char activeSlot() const { return slot_ == 0 ? 'A' : 'B'; }
    void selectSlot(char s);
    void copyActiveToOther();

    // Persisted alongside plugin state.
    juce::ValueTree toTree() const;
    void fromTree(const juce::ValueTree& t);

    std::function<void()> onChange;

private:
    juce::ValueTree snapshot() const;
    void apply(const juce::ValueTree& snap);
    void applyMap(const std::map<juce::String, float>& values);

    juce::AudioProcessorValueTreeState& state_;
    juce::UndoManager& undo_;
    juce::Array<Entry> entries_;
    int current_ = 0;
    juce::ValueTree loadedSnap_;
    juce::ValueTree slots_[2];
    int slot_ = 0;
};

} // namespace td1
