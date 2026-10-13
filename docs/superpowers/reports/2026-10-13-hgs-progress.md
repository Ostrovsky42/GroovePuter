# Unified Harmonic System v2 execution progress

## HGS2-0 — legacy replay witness and focused runner

- Worktree: `/home/gg/.codex/worktrees/performance-smart-play`.
- Branch: `feature/smart-play-return`.
- Frozen starting code SHA: `5fbccbe6e06c161343a9f172ef2a770d43a122f0` (SP-6/SP-7
  implementation is committed; no HGS2 production changes existed at this point).
- Pinned HGS v2 source: upstream commit
  `70ec2011098487c6daf551bd8cb6ff38a9294578`, blob
  `63d4ff762b404c3e952a1c79f6068dddf19063f9`.
- HGS v1 is historical prior data only; v2 architecture controls implementation.
- Added `tests/run_hgs_tests.sh` with exact named target `legacy`; no arguments run every
  registered target, unknown targets fail, `CXX` and `HGS_CXXFLAGS` are respected, and test
  binaries are written below `build/hgs-tests/`.
- Added `tests/test_hgs_legacy_replay.cpp` and frozen semantic-field fixture
  `tests/fixtures/hgs_legacy_replay.tsv`. The fixture contains 3,840 successful source/trace
  rows: seeds 0..63, AUTO across all eight valid RhythmFamily values, each legacy explicit
  ProgressionId 1..7, and phrase lengths 1/2/4/8. It also records six invalid request/source
  outcomes. The serialization names every semantic event member and never compares struct
  padding.
- Compile-time guards freeze the existing enum ordinals and baseline sizes:
  every ProgressionId, ChordQuality, RhythmFamily and GenerationDomain ordinal,
  ChordProgressionStatus 0..3; `HarmonicEvent=3`, `ChordProgressionSource=14`,
  `ChordProgressionSourceRequest=16`, `ChordProgressionSourceResult=15`,
  `ChordProgressionPlan=26`, `ChordProgressionRequest=16`,
  `HarmonyEventKey=16`, `HarmonySnapshot=56` bytes on host GCC 13.3.
- `bash tests/run_hgs_tests.sh legacy` — PASS before any HGS2 production change.
- `HGS_CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' CXX=g++ bash tests/run_hgs_tests.sh legacy` — PASS.
- Unknown-target rejection — PASS (exit 2); `git diff --check` — PASS.
- Pinned v2 design SHA-256: `d3a288a2c57dbdda1e04c8cd295429a699ffb5ec55c0dae4fd8b47e4398ef7c2`.
- Frozen replay fixture SHA-256: `f872619624e74d88766a6d8f423a86c153b5d3ef0d70a5215e42c716a8833da8`.
- No production harmonic routing changed. Phase A remains mandatory; conditional HGS2-9/10
  remain pending listening evidence. Production mapping is still pending; the base contract
  explicitly excludes treating every device genre as a full HGS profile.
- Hardware and listening acceptance remain `NOT_RUN`.
- NEXT_TASK=HGS2-1a: pin corpus sources and import vocabulary without MIDI generation.
