// Renders the TD-1 editor to PNG after running a synthetic drum loop through
// the processor, so the scope and Foresight Lens are populated.
// usage: td1_snapshot <out.png> [scale] [preset name]
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <random>

static void synthDrums(juce::AudioBuffer<float>& buf, double sr, int64_t offset) {
    static std::mt19937 rng(3);
    std::uniform_real_distribution<float> u(-1.f, 1.f);
    const double spb = 60.0 / 112.0;
    for (int i = 0; i < buf.getNumSamples(); ++i) {
        const double t = (double)(offset + i) / sr;
        const double beat = std::fmod(t / spb, 4.0);
        const double tb = std::fmod(t, spb * 0.5);   // eighth-note position (s)
        float s = 0;
        const double tk = std::fmod(t, spb * 2.0);   // kick on 1 and 3
        s += 0.9f * (float)(std::sin(2 * M_PI * (50 + 120 * std::exp(-tk * 30)) * tk) * std::exp(-tk * 8));
        if (beat >= 1.0 && beat < 2.0 || beat >= 3.0) {   // snare on 2 and 4
            const double ts = std::fmod(t, spb);
            s += (float)(0.6 * u(rng) * std::exp(-ts * 18) + 0.3 * std::sin(2 * M_PI * 190 * ts) * std::exp(-ts * 25));
        }
        s += 0.18f * u(rng) * (float)std::exp(-tb * 60);   // hats
        s += 0.04f * (float)std::sin(2 * M_PI * 82.4 * t); // bass bed
        buf.setSample(0, i, s * 0.8f);
        buf.setSample(1, i, s * 0.75f);
    }
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out(juce::File::getCurrentWorkingDirectory().getChildFile(argc > 1 ? argv[1] : "td1.png"));
    const float scale = argc > 2 ? (float)std::atof(argv[2]) : 1.0f;

    auto proc = std::make_unique<TD1Processor>();
    const double sr = 48000.0; const int bs = 512;
    proc->setPlayConfigDetails(2, 2, sr, bs);
    proc->prepareToPlay(sr, bs);
    if (argc > 3) {
        auto& pm = proc->presets();
        for (int i = 0; i < pm.entries().size(); ++i)
            if (pm.entries()[i].name == juce::String(argv[3])) pm.load(i);
    }

    proc->uiScale = scale;
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc->createEditor());
    auto* editor = dynamic_cast<TD1Editor*>(ed.get());
    juce::AudioBuffer<float> buf(2, bs);
    juce::MidiBuffer midi;
    int64_t pos = 0;
    const int blocks = (int)(3.2 * sr / bs);
    for (int b = 0; b < blocks; ++b) {
        synthDrums(buf, sr, pos);
        proc->processBlock(buf, midi);
        pos += bs;
        if (b % 4 == 0) editor->pumpForSnapshot();
    }
    for (int i = 0; i < 40; ++i) editor->pumpForSnapshot();   // settle meters / fades

    const auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    out.deleteFile();
    juce::FileOutputStream os(out);
    juce::PNGImageFormat png;
    if (!os.openedOk() || !png.writeImageToStream(img, os)) { std::fprintf(stderr, "write failed\n"); return 1; }
    std::printf("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
