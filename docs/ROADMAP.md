# TD-1 Product Roadmap

Owner key: [C] = Claude can execute (in-chat or via Claude Code on the repo).
[M] = Mark only (credentials, hardware, money, or legal identity required).
[C+M] = Claude produces, Mark runs/verifies locally.

## Phase 0 — Infrastructure (now)
- [M] Create private GitHub repo `foresight-td1`; push this repo bundle
- [x] Local Faust build of td1_v3 -> AU/VST3 (superseded by the C++ plugin)
- [x] Repo hygiene: CI (GitHub Actions: core tests on 3 OSes, macOS/Windows plugin + pluginval + auval), issue templates

## Phase 1 — Engine port and verification (2–4 wks)
- [x] `core/` dependency-free C++17 kernel (td1-core), no JUCE types
- [x] Golden-file test harness: render impulse/burst/loop through JS reference,
      assert C++ output within tolerance at 44.1/48/88.2/96/192 kHz (61 cases, worst 6e-8)
- [x] Property tests: unity at 0/0, silence->silence, latency == lookahead (+ block-size, level independence, declick)
- [M] Run test suite locally; APx555 spot-checks of clipper THD+N and latency

## Phase 2 — Plugin shell (3–6 wks)
- [x] JUCE 8 CMake project wrapping td1-core; VST3 + AU + Standalone
- [~] CLAP via clap-juce-extensions (wired, opt-in `-DTD1_ENABLE_CLAP=ON`; not yet built/verified)
- [M] Execute Steinberg VST3 proprietary license agreement (free, identity required)
- [x] Latency reported = lookahead (re-reported on change, tap crossfade); zero-latency mode; state save/restore
- [x] Preset system (19 factory + user .td1preset files), A/B, undo/redo

## Phase 3 — GUI (4–8 wks)
- [x] Port v3 visual identity to JUCE; scope + Foresight Lens; resizable (75–200%) HiDPI
- [M] Design review passes; hardware-UI taste decisions

## Phase 4 — QA (continuous)
- [x] pluginval strictness-10 in CI; auval (both pass locally, VST3 + AU)
- [M] Manual host matrix: Live, Logic, Reaper, Cubase, Studio One, Bitwig, FL
- [C+M] Bounce-vs-realtime null tests; automation torture scripts

## Phase 5 — Signing & distribution
- [M] Apple Developer Program ($99/yr); Developer ID + notarization
- [M] Azure Artifact Signing account (US individual/entity, ~$9.99/mo)
- [C] Installer scripts (pkgbuild macOS, Inno Setup Windows); signing in CI
- [M] Merchant of record account (Paddle / Lemon Squeezy / FastSpring)
- [C] Ed25519 offline license-file scheme + in-plugin verification

## Phase 6 — iOS AUv3 (v1.5)
- [C] AUv3 extension + container app scaffolding
- [M] App Store Connect setup, review submission, TestFlight beta
- Deferred: Ableton Link (standalone app only, once a tempo-synced feature exists)

## Phase 7 — Launch
- [M] Beta cohort recruitment (10–15 working engineers)
- [C] Demo video scripts, product copy, measurement-suite documentation
- [M] APx measurement captures for published spec sheet; pricing decision ($79–99 anchor, $49 intro)

## Added post-prototype (v0.4)
- [x] Pre-transient shaping ("Pre"): gain on the lookahead window before each onset
- [x] Threshold / bleed focus: soft-knee gate on onset detection (lightweight precursor to onset classification)
- [x] Foresight Lens: hit-locked zoom display of the anticipation
- [x] Declicking for every discrete switch, lookahead changes and bypass
- [M] Listen tests in Live/Logic: PDC alignment against a parallel dry track, preset voicing

## V2 candidates (post-launch)
- Onset classification with selective shaping (bleed control) — flagship
- Per-hit envelope conform (drawable target envelope)
- Multiband with per-band transient timing alignment
- Transient synthesis/augmentation
