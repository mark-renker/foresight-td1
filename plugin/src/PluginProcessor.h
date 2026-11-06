#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "Presets.h"
#include "Telemetry.h"
#include "td1/Kernel.h"

class TD1Processor final : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::AsyncUpdater {
public:
    TD1Processor();
    ~TD1Processor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState& state() { return state_; }
    juce::UndoManager& undo() { return undo_; }
    td1::PresetManager& presets() { return *presets_; }
    td1::Telemetry& telemetry() { return telemetry_; }
    double kernelSampleRate() const { return kernel_.sampleRate(); }

    // UI scale persisted with the session.
    float uiScale = 1.0f;

private:
    void parameterChanged(const juce::String& id, float newValue) override;
    void handleAsyncUpdate() override;
    void updateLatency();
    void processInternal(juce::AudioBuffer<float>&, bool forceBypass);

    juce::UndoManager undo_{ 30000, 60 };
    juce::AudioProcessorValueTreeState state_;
    td1::param::Refs refs_;
    std::unique_ptr<td1::PresetManager> presets_;
    td1::Telemetry telemetry_;
    td1::Kernel kernel_;
    juce::AudioBuffer<float> scratch_;
    std::atomic<bool> needsReset_{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TD1Processor)
};
