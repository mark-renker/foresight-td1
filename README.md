# FORESIGHT TD-1 — Anticipatory Transient Designer

A transient designer whose detector runs *ahead* of the audio (lookahead). It
uses ratio-normalised, level-independent detection and adds transient-only
spectral tilt, Mid/Side targeting, loudness-compensated A/B, and two features
that only work because the detector sees the future:

- **Pre**: gain for the few milliseconds *before* each onset. Turn it down for
  a clean gap that makes the hit jump out, or up for a reverse-style swell into it.
- **Foresight Lens**: a hit-locked zoom display that shows the gain envelope
  moving inside the lookahead window, before the transient reaches the output.

Also included: **Threshold** (bleed focus), so quiet onsets such as hat bleed in
a snare mic are left alone.

## Repository layout
| Path | What |
|---|---|
| `core/` | **td1-core**: dependency-free C++17 DSP kernel (no JUCE types) |
| `tests/` | Property tests and golden-file tests against the JS reference |
| `plugin/` | JUCE 8 plugin shell (VST3 / AU / Standalone, CLAP opt-in) with a custom GUI |
| `tools/Snapshot.cpp` | Renders the editor to PNG with live audio (UI review and docs) |
| `prototype/` | Browser prototypes. **`foresight_td1_v3.html` is the DSP reference** |
| `faust/` | Original Faust ports (legacy: these don't report latency to the host, so use the C++ plugin) |
| `docs/ROADMAP.md` | Product roadmap |

## Build (macOS)
```sh
# uses ~/dev/JUCE or ~/JUCE if present, else fetches JUCE 8.0.14
cmake -S . -B ~/dev/td1-build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build ~/dev/td1-build/cmake -j --target ForesightTD1_VST3 ForesightTD1_AU ForesightTD1_Standalone
```
Artefacts: `~/dev/td1-build/cmake/plugin/ForesightTD1_artefacts/Release/{AU,VST3,Standalone}`.
To install, copy them to `~/Library/Audio/Plug-Ins/{Components,VST3}/`.

For a universal binary, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.
For CLAP, add `-DTD1_ENABLE_CLAP=ON`, which fetches `clap-juce-extensions` from GitHub.

## Tests
```sh
cmake -S . -B build -DTD1_BUILD_PLUGIN=OFF && cmake --build build -j
ctest --test-dir build --output-on-failure
```
- **Golden**: `tests/golden/render_reference.mjs` extracts `TShaperKernel` straight
  from the v3 prototype HTML (so the reference can't drift from the prototype),
  renders 61 cases (4 signals, 17 parameter sets, 44.1–192 kHz) and the C++
  kernel must match within 1e-5. It currently matches to within about 6e-8,
  i.e. float rounding. This test needs `node`.
- **Properties**: unity at 0/0 (pure delay), silence in gives silence out,
  latency equals lookahead, block-size invariance, level independence,
  threshold gating, pre-window behaviour, and click-free switching of every
  discrete control.
- **Plugin**: pluginval strictness 10 (VST3 and AU, GUI tests included) and
  `auval` both pass locally. CI runs both.

UI snapshot: `td1_snapshot out.png [scale] ["Preset Name"]`.

## Engine
Detection tap (pre-delay) → sidechain HPF → |x| → 4 asymmetric one-pole
envelopes → ratio-normalised attack/sustain weights (level-independent) →
[threshold gate] → gain law 10^((A·wA + S·wS)/20), asymmetric smoothing.
Audio path → ring-buffer lookahead delay (raw L/R, encoded at read) →
transient-only tilt (crossfaded by smoothed wA) → gain × pre-window gain →
mix → [delta] → auto-gain → [ADAA-1 tanh clip] → trim.

**Latency** always equals the lookahead and is reported to the host. Changing it
re-reports latency and crossfades between delay taps. Lookahead 0 is
zero-latency mode. **Declicking**: bypass crossfades against the latency-aligned
dry path; target, delta, listen and clip changes dip to silence for 4 ms and
switch at zero; mix, output, tilt and pre ramp over 20 ms.

Proprietary — all rights reserved. Do not distribute.
