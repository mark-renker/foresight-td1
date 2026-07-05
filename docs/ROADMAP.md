# TD-1 Product Roadmap

Owner key: [C] = Claude can execute (in-chat or via Claude Code on the repo).
[M] = Mark only (credentials, hardware, money, or legal identity required).
[C+M] = Claude produces, Mark runs/verifies locally.

## Phase 0 — Infrastructure (now)
- [M] Create private GitHub repo `foresight-td1`; push this repo bundle
- [M] Local Faust build of td1_v3 -> AU/VST3 into plugin folders; test in Live/Bitwig
- [C] Repo hygiene: CI skeleton (GitHub Actions), issue templates

## Phase 1 — Engine port and verification (2–4 wks)
- [C] `core/` dependency-free C++17 kernel (td1-core), no JUCE types
- [C] Golden-file test harness: render impulse/burst/loop through JS reference,
      assert C++ output within tolerance at 44.1/48/88.2/96/192 kHz
- [C] Property tests: unity at 0/0, silence->silence, latency == lookahead
- [M] Run test suite locally; APx555 spot-checks of clipper THD+N and latency

## Phase 2 — Plugin shell (3–6 wks)
- [C] JUCE 8 CMake project wrapping td1-core; VST3 + AU
- [C] CLAP via clap-juce-extensions
- [M] Execute Steinberg VST3 proprietary license agreement (free, identity required)
- [C] Fixed-latency PDC design + zero-latency mode; state save/restore
- [C] Preset system, A/B, undo

## Phase 3 — GUI (4–8 wks)
- [C] Port v3 visual identity to JUCE; GPU-friendly scope; resizable HiDPI
- [M] Design review passes; hardware-UI taste decisions

## Phase 4 — QA (continuous)
- [C] pluginval strictness-10 in CI; auval
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

## V2 candidates (post-launch)
- Onset classification with selective shaping (bleed control) — flagship
- Per-hit envelope conform (drawable target envelope)
- Multiband with per-band transient timing alignment
- Transient synthesis/augmentation
