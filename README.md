# FORESIGHT TD-1 — Anticipatory Transient Designer

Product concept: transient designer whose gain envelope *leads* the waveform
(lookahead anticipation), with ratio-normalized level-independent detection,
transient-only spectral tilt, Mid/Side targeting, and loudness-compensated A/B.

## Repository layout
- `prototype/foresight_td1_v2.html` — reference implementation, core engine (runs in any browser)
- `prototype/foresight_td1_v3.html` — extended engine, V1 feature set (runs in any browser)
- `faust/td1_core.dsp` — Faust port of the v2 core engine (compile-verified, Faust 2.70.3)
- `faust/td1_v3.dsp` — Faust port of the extended engine (compile-verified, Faust 2.70.3)
- `docs/ROADMAP.md` — product roadmap with task ownership

## Building the Faust plugins (macOS)
```
brew install faust
# quick audition without building: paste the .dsp into https://faustide.grame.fr
faust2juce faust/td1_v3.dsp        # generates a JUCE project folder (td1_v3/)
```
Open the generated `.jucer` in Projucer (JUCE 7/8), enable **AU** and **VST3**
plugin formats, save to Xcode, build Release. Copy build products to:
- `~/Library/Audio/Plug-Ins/Components/` (AU — Ableton Live on macOS, Logic)
- `~/Library/Audio/Plug-Ins/VST3/` (VST3 — Live, Bitwig)

CLAP: add `clap-juce-extensions` (github.com/free-audio/clap-juce-extensions)
to the generated project, or defer to the C++ port (Phase 1, see roadmap).

## Engine topology (both .dsp files and both prototypes)
Detection tap (pre-delay) -> sidechain HPF -> |x| -> 4 asymmetric one-pole
envelopes -> ratio-normalized attack/sustain weights (level-independent) ->
gain law 10^((A*wA + S*wS)/20), asymmetric smoothing. Audio path -> ring-buffer
lookahead delay -> transient-only tilt (crossfaded by smoothed wA) -> gain ->
mix -> [delta] -> [clip] -> trim. v3 adds M/S component targeting, RMS
loudness-match auto-gain, detector audition, tilt pivot, ADAA-1 tanh clipper.

Proprietary — all rights reserved. Do not distribute.
