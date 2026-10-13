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
- HGS2-0 commit: `a32a83b3` (`test: freeze legacy harmonic replay`).

## HGS2-1a — pinned offline chord/scale vocabulary — complete

- Added `source_lock.json` for Fannon commit
  `352f28004a3dc0f1a130682b925c4673f8a3d13b` and ldrolez commit
  `79d8dfcd7072bd9027231575b1d6c5f1ed809335`. Locks include exact source/package-lock/license
  hashes. Full Fannon, ldrolez and Tonal.js MIT notices are in `tools/harmony/licenses/`.
- Fannon source hashes: `src/generateChords.ts` 61abf9e8…f36aa51,
  `src/generateScales.ts` fa9edf91…f30c70bce, `package-lock.json`
  7fbbe80b…de4bbf11. Resolved @tonaljs/tonal 4.10.0, chord-type 4.8.0,
  scale-type 4.8.1; all MIT. `npm ci --ignore-scripts --no-audit --no-fund` ran in
  ignored `build/hgs-cache/`, not the project root. Python uses system 3.12.3 stdlib; no venv
  or third-party Python dependency is needed.
- Added a safe TypeScript exporter that parses only the pinned source alias arrays and calls
  Tonal.js at C. It does not import/execute either upstream generator and writes no MIDI.
  Source commit, files, package versions/licenses and 24/17 input cardinalities are checked.
- Added root-independent normalizers, explicit registry IDs, deterministic merge/provenance,
  digest validation and quarantine output. Existing IDs 0..9 are unchanged; IDs 10..22 add 13
  chord formulas and IDs 10..16 add 7 scale formulas. Source aliases/root/export variants merge
  by exact pitch-class formula. The fixture compiles to 23 unique quality formulas and 17
  scales, with 0 quarantined rows.
- `runtime_bootstrap_count=10` keeps the generated firmware vocabulary on the existing
  bootstrap prefix. `python3 tests/test_harmonic_registry_generation.py` confirms byte-for-byte
  parity with the checked-in generated header; the broad offline IDs do not expand runtime
  selection in this checkpoint.
- `bash tests/run_hgs_tests.sh` — PASS (`legacy`, `vocab`); six Python vocabulary tests pass.
- Exported twice and imported twice from the pinned checkout; `cmp` confirms both export JSON
  and normalized vocabulary/manifest bytes match. Normalized content SHA-256:
  `955909d61666537c8ae756a2f895267e793f149c0da8e1474c657ad869b2f16e`.
- `git diff --check` — PASS. No HGS runtime graph or genre behavior changed.
- NEXT_TASK=HGS2-1b: source-specific Roman progression parser from ldrolez `chords.py`.
- HGS2-1a commit: pending local commit.
