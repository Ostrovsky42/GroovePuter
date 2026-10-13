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

## HGS2-1b — source-specific progression parser — complete

- Start SHA: `0c3ded40c4b366a56e7a2e1940650e1dc11fb388`.
- Locked ldrolez source commit `79d8dfcd7072bd9027231575b1d6c5f1ed809335`, including
  `chords.py` and the `c2m.py` slash-bass caller. Locked the actual Mingus 0.6.1 tag
  commit `f131620eb7353bcfbf1303b24b951a95cad2ac20`; pinned hashes cover
  `progressions.py`, `chords.py`, and the full GPL-3.0 license notice. No upstream
  Python module is imported or executed; parser input extraction uses Python AST literals.
- `normalize_roman.py` preserves source token spelling, degree, accidental, registry
  quality ID, source dialect, key-derived context, slash-bass root and interval.
  Major/modal source lists resolve against major keys; minor lists resolve against a
  natural-minor key. Thus source case is preserved as metadata, while Mingus key context
  determines the implicit chord quality.
- Fixture tests pin `I7`=major seventh, `Idom7`=dominant seventh, and `ii5`=power fifth.
  Transposed C/D absolute-chord fixtures normalize to the same route as Roman `I V vi IV`.
  Repeated route tokens remain repeated; mood labels remain source annotations.
- Import covers 195 source rows: 189 accepted, 6 quarantined. The quarantines are exactly
  the routes containing `M-5`, `add9`, or `madd9`, which are absent from Mingus 0.6.1's
  resolver dictionary. Full rows, raw strings, labels and source line locators are retained;
  there is no triad fallback. Corpus fixture and importer agree on counts.
- **Ruling:** quarantine the resolver-unknown suffixes instead of assigning guessed formulas.
  This follows the pinned resolver and no-silent-fallback contract; cost if wrong: six
  routes are omitted from transition priors until a deliberate alias/resolver decision.
- Importer outputs twice compared byte-for-byte. Latest normalized content SHA-256:
  `3d15aea7f38da67c7daeb369f0190314e27ebee01bb6d8f452a4c8977d67880d`.
- `python3 tests/test_harmony_roman_import.py` — PASS (4 tests; all 195 rows accounted).
  `bash tests/run_hgs_tests.sh` — PASS (`legacy`, `vocab`, `roman`); registry generated
  header byte parity — PASS; unknown HGS runner target returns exit 2; `git diff --check` — PASS.
- HGS-only offline importer. Hardware/listening acceptance remain `NOT_RUN`.
- NEXT_TASK=HGS2-1c: normalized corpus provenance/schema and reproducible manifest.

## HGS2-1c — normalized source manifest — implementation complete

- Start SHA: `5c17c3c07855da56fcf2eba1a01a28c8ef40e366`.
- Added `validate_harmony_model.py` to combine the pinned vocabulary and progression
  importer outputs into schema v1 `harmony_manifest.json`. It validates source commits,
  exact source file hashes, normalized input checksums, schema versions, stable registry IDs,
  formulas/masks/cardinality, route references, source locators, and distributed license
  notice hashes before writing output.
- The final manifest carries quality/scale registries, aliases and canonical route tokens,
  sorted mood tags, source row locators and source/license provenance. Fannon is explicitly
  `vocabulary_only`; it cannot add rows to progression corpus totals. Every unique normalized
  route receives weight 1; exact duplicate routes merge locators/tags; ordered repeated
  tokens remain in place. Input order does not affect compiled output.
- Full pinned source result: 195 rows; 189 accepted, 6 quarantined, 188 unique routes,
  1 duplicate. `content_sha256=993d1045ca03180c8d30f69b2e397aff50fb8ba802c28b95564b8b6b3f3a638b`.
  Full result is pinned as small cardinality/checksum fixture, not a generated firmware table.
- Actual validator CLI run twice against pinned source outputs; `cmp` confirmed identical
  `harmony_manifest.json` bytes (file SHA-256
  `139c3349d855fc3cd8de714581159735881d55f28a7125726b47e1dc4fbf549b`).
- `python3 tests/test_harmony_source_manifest.py` — PASS (5 tests, including full pinned
  fixtures, deterministic permutation, one-weight dedup, ordered repeats, vocabulary/corpus
  separation, malformed ID/schema/reference/checksum/provenance rejection).
  `bash tests/run_hgs_tests.sh` — PASS (`legacy`, `vocab`, `roman`, `manifest`); registry
  generated header parity — PASS; Python `py_compile` — PASS; `git diff --check` — PASS.
- HGS2-1c does not change runtime or firmware. Hardware/listening acceptance remain `NOT_RUN`.
- NEXT_TASK=HGS2-2: pitch-set algebra and exact bounded pitch-class voice-leading.
