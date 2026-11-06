#include "PluginProcessor.h"
#include "PluginEditor.h"

TD1Processor::TD1Processor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state_(*this, &undo_, "TD1", td1::param::createLayout()) {
    refs_.bind(state_);
    presets_ = std::make_unique<td1::PresetManager>(state_, undo_);
    state_.addParameterListener(td1::param::kLookahead, this);
    kernel_.setMeterSink(&telemetry_);
    kernel_.setParams(refs_.read());
    kernel_.prepare(48000.0);
}

TD1Processor::~TD1Processor() {
    cancelPendingUpdate();
    state_.removeParameterListener(td1::param::kLookahead, this);
}

bool TD1Processor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void TD1Processor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    kernel_.setParams(refs_.read());
    kernel_.prepare(sampleRate);
    scratch_.setSize(2, juce::jmax(1, samplesPerBlock));
    telemetry_.clearFrames();
    updateLatency();
}

void TD1Processor::reset() {
    kernel_.reset();
}

// Latency always equals the lookahead currently in effect. Changing lookahead
// re-reports latency so hosts re-align (PDC); the kernel crossfades taps so the
// change itself is click-free. Lookahead 0 = zero-latency mode.
void TD1Processor::updateLatency() {
    const double sr = getSampleRate() > 0 ? getSampleRate() : kernel_.sampleRate();
    const double ms = juce::jlimit(0.0, td1::kMaxLookaheadMs, (double)refs_.lookahead->load());
    const int maxD = (int)std::ceil(sr * 0.012) + 4;
    const int samples = juce::jlimit(0, maxD - 2, (int)std::floor(ms * 0.001 * sr + 0.5));
    if (samples != getLatencySamples()) setLatencySamples(samples);
}

void TD1Processor::parameterChanged(const juce::String& id, float) {
    if (id == td1::param::kLookahead) triggerAsyncUpdate();
}

void TD1Processor::handleAsyncUpdate() { updateLatency(); }

void TD1Processor::processInternal(juce::AudioBuffer<float>& buffer, bool forceBypass) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin(buffer.getNumChannels(), getTotalNumInputChannels());
    if (n == 0 || nch == 0) return;

    auto p = refs_.read();
    if (forceBypass) p.bypass = true;
    kernel_.setParams(p);

    float* l = buffer.getWritePointer(0);
    float* r = nch > 1 ? buffer.getWritePointer(1) : nullptr;
    kernel_.process(l, r, l, r, n);
    for (int c = nch; c < buffer.getNumChannels(); ++c) buffer.clear(c, 0, n);
}

void TD1Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    processInternal(buffer, false);
}

// Hosts that bypass without our bypass parameter still get the latency-aligned
// dry signal (and the crossfade).
void TD1Processor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    processInternal(buffer, true);
}

juce::AudioProcessorParameter* TD1Processor::getBypassParameter() const {
    return state_.getParameter(td1::param::kBypass);
}

void TD1Processor::getStateInformation(juce::MemoryBlock& destData) {
    auto st = state_.copyState();
    st.setProperty("pluginVersion", td1::kVersion, nullptr);
    st.setProperty("uiScale", uiScale, nullptr);
    st.removeChild(st.getChildWithName("Presets"), nullptr);
    st.appendChild(presets_->toTree(), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, destData);
}

void TD1Processor::setStateInformation(const void* data, int sizeInBytes) {
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(state_.state.getType())) return;
    auto st = juce::ValueTree::fromXml(*xml);
    uiScale = juce::jlimit(0.6f, 2.0f, (float)st.getProperty("uiScale", 1.0f));
    const auto presetTree = st.getChildWithName("Presets").createCopy();
    st.removeChild(st.getChildWithName("Presets"), nullptr);
    state_.replaceState(st);
    presets_->fromTree(presetTree);
    undo_.clearUndoHistory();
    triggerAsyncUpdate();
}

juce::AudioProcessorEditor* TD1Processor::createEditor() { return new TD1Editor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TD1Processor(); }
