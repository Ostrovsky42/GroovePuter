# GF2-C2 Gate B Materialized Capacity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Measure the full current production Genre/Recipe catalog on a frozen 55-seed × P1/P2/P3 corpus through the real GF2 production path, emit timbre-free structural signatures and complete unordered pairwise classifications, and freeze reproducible review artifacts without changing `src/`.

**Architecture:** A C++ research dump invokes production catalog, frozen-selection, migration and phrase-execution APIs and wraps the existing GF2-C2-V0R `GenerationObservation` with neutralized physical material evidence. `tools/gf2_gate_b.py` consumes only that actual observation dump, computes deterministic dimension-level signatures/distributions/pairwise relations, and writes committed TSV/Markdown review surfaces. A focused shell runner rebuilds the dump, proves replay/compiler determinism, checks source/catalog/seed/pairwise contracts, regenerates artifacts into a temp directory and byte-compares them with committed snapshots.

**Tech Stack:** C++17 host characterization harness, Python 3 standard library, Bash, GitHub Actions, existing GroovePuter GF2 production APIs and GF2-C2-V0R observation support.

**Spec:** `docs/superpowers/specs/2026-09-03-gf2-c2-gate-b-materialized-capacity-design.md`

## Global Constraints

- Frozen measurement base is `9c01b0c34b80aacb1dd6be66bb07c5cef3ad1c38`.
- `src/ DELTA = NONE` and Performance/MIDI ownership delta = none.
- Production materialization evidence comes from the existing GF2-C2-V0R observation plus actual physical material produced by production migration/phrase execution.
- Python must not recreate generation policy from static declarations.
- Catalog enumeration is production-owned; the first observed count is frozen and later silent drift fails.
- Seed corpus is exactly 55 committed deterministic realization identities and is never selected based on results.
- Every production profile is measured at P1, P2 and P3 on the same seed corpus.
- Duration is `NOT_OBSERVED` unless the authoritative observation path exposes it.
- TIMBRE-DEPENDENT requires positive observed timbre evidence; static labels are insufficient.
- GRID 8/32 and DEPTH→role-hierarchy remain frozen negative-capacity findings, not implementation work.
- Gate B records measurements only; GF2-G1 interpretation/recommendations are out of scope.

---

### Task 1: Freeze Gate B corpus and contract

**Files:**
- Create: `tests/support/gf2_gate_b_seeds.tsv`
- Create: `tests/support/gf2_gate_b_contract.json`
- Create: `tests/test_gf2_gate_b_analysis.py`

**Interfaces:**
- Consumes: authoritative production profile count discovered by the production dump.
- Produces: `SEEDS` loaded from `gf2_gate_b_seeds.tsv`; contract keys `schema_version`, `exact_base_sha`, `profile_count`, `seed_count`, `depths`, `pairwise_count`.

- [ ] **Step 1: Commit the frozen seed corpus**

Write one header line `seed` followed by exactly these 55 lowercase hexadecimal uint32 values, in order:

```text
0x00000000
0xcb48ba58
0x5c795f79
0x855ed2f2
0xbf171332
0xf35eccca
0x7234ac89
0x131a143b
0x6236c255
0xf5fbd20b
0x78e44f16
0xd1d80535
0xfea77cca
0x36f8bef5
0x05e7c2a6
0x4bdcf39f
0x60405048
0x2365c8ec
0xd578bfd0
0x92d90cb1
0xdaa1d7d6
0xeb37989b
0x1cba65a9
0x51e2d86e
0x570cbe39
0xd9b435dc
0xbefcd13b
0xbfafc996
0x953871d8
0xfb64b779
0xdcff963c
0xe9382520
0x5e162e2e
0xe1a7db27
0x42a5fe86
0x53dd47bc
0x9e7dff09
0x12053727
0x28d027b7
0xa8fce524
0xbf579e08
0xde41b023
0xe0f12421
0x4ad9a57b
0xb0a9b240
0x5adfd730
0xa70962ac
0xe0c84742
0xec2e1912
0xce859c14
0xdde37077
0xce54f9a1
0x7d7ed331
0x440cb71d
0x8f00b32a
```

The test must assert count=55, uniqueness, uint32 range, and `(seed & 0xffff) != 0xffff` for all seeds.

- [ ] **Step 2: Write the initial corpus contract**

Create deterministic JSON:

```json
{
  "depths": ["P1", "P2", "P3"],
  "exact_base_sha": "9c01b0c34b80aacb1dd6be66bb07c5cef3ad1c38",
  "pairwise_count": 528,
  "profile_count": 33,
  "schema_version": 1,
  "seed_count": 55
}
```

The profile/pair counts are the first frozen authoritative enumeration inherited from the current 33-row production catalog and must later be checked against live production enumeration rather than used to drive it.

- [ ] **Step 3: Write RED analyzer-contract tests**

`tests/test_gf2_gate_b_analysis.py` imports `tools/gf2_gate_b.py` by file path and tests exact functions that the implementation must provide:

```python
load_seeds(path) -> tuple[int, ...]
canonical_id(prefix, payload) -> str
relation_for_sequences(left, right) -> str
pair_classification(relations, timbre_evidence=False) -> str
expected_pair_count(profile_count) -> int
```

Required assertions:

```text
identical matched sequences -> SAME
disjoint sequences -> DISJOINT
partial overlap -> OVERLAP
no observed values -> NOT_OBSERVED
all observed dimensions SAME -> STRUCTURALLY REDUNDANT
one differing structural dimension + remaining SAME -> PARTIALLY DISTINCT
>=2 robust DISJOINT structural dimensions -> STRUCTURALLY DISTINCT
no timbre evidence can never produce TIMBRE-DEPENDENT
expected_pair_count(33) == 528
canonical IDs are stable and prefix-scoped
```

- [ ] **Step 4: Verify RED**

Run:

```bash
python3 tests/test_gf2_gate_b_analysis.py
```

Expected: FAIL because `tools/gf2_gate_b.py` does not exist yet.

- [ ] **Step 5: Commit Task 1**

```bash
git add tests/support/gf2_gate_b_seeds.tsv tests/support/gf2_gate_b_contract.json tests/test_gf2_gate_b_analysis.py
git commit -m "test(gf2-c2): freeze Gate B corpus contract"
```

### Task 2: Add the production-backed neutral observation dump

**Files:**
- Create: `tests/support/gf2_gate_b_observation.h`
- Create: `tools/gf2/gf2_gate_b_dump.cpp`
- Create: `tests/run_gf2_gate_b_dump_tests.sh`

**Interfaces:**
- Consumes: `GF2Measurement::GenerationObservation`, `StrongRhythmFrozenSelection`, `StrongRhythmMigrationResult`, actual `DrumPatternSet`/`SynthPattern`, production phrase executor.
- Produces: deterministic raw TSV with one `REALIZATION` row per profile×seed×depth and one `PHRASE` row per same identity, including neutral physical evidence and separate admission/provenance fields.

- [ ] **Step 1: Write RED dump contract runner**

The runner compiles `tools/gf2/gf2_gate_b_dump.cpp` against the same `COMMON_SOURCES` extraction used by existing GF2 host runners, executes it with `tests/support/gf2_gate_b_seeds.tsv`, and asserts via Python/awk:

```text
META profile_count=33
META seed_count=55
REALIZATION rows = 33*55*3 = 5445
unique (profile_id,seed,depth) = 5445
every depth P1/P2/P3 present for every profile/seed
no output column named engine/oscillator/sample/kit/fx/fx_param/velocity
physical_duration column is always NOT_OBSERVED
all four phrase-law names appear in phrase provenance
```

- [ ] **Step 2: Verify RED**

Run:

```bash
bash tests/run_gf2_gate_b_dump_tests.sh
```

Expected: FAIL because the dump/support files do not exist.

- [ ] **Step 3: Implement neutral material projection**

`tests/support/gf2_gate_b_observation.h` defines research-only POD/string helpers that accept actual materialized output and return canonical text, without selecting or generating anything. It must expose:

```cpp
NeutralMaterialObservation observeNeutralMaterial(
    const DrumPatternSet& drums,
    const SynthPattern& synthA,
    const SynthPattern& synthB,
    const StrongRhythmMigrationResult& migration,
    uint8_t barOrdinal);
```

The observation contains:

```text
kick/backbeat/hat/support onset masks
per-role accent masks
per-role active timing tuples step:delta
synth A/B onset masks
synth A/B accent/ghost masks
synth A/B relative pitch-class sequences and signed interval contours
harmonicEventOnsets / harmonicEventCount
chordOnsets / melodicFillOnsets
chordApplied / melodicApplied / materialized Synth-B role
physical event count
physical_duration = NOT_OBSERVED
```

Do not include velocity, FX, patch/engine/sample identity or raw absolute-note sequences.

- [ ] **Step 4: Implement production corpus dump**

`tools/gf2/gf2_gate_b_dump.cpp`:

1. parses exactly the committed seed file;
2. enumerates `kGenerativeModeCount` + `availableRecipeCount/At`;
3. creates profile IDs `<genre>:BASE` or `<genre>:<recipe_id>`;
4. for every seed and DEPTH constructs `StrongRhythmMigrationContext` with `generationAttemptOrdinal=seed`, deterministic `patternAddress=seed % kMaxGlobalPatterns`, `FeelProfileId::Auto`, amount 20, tonal materialization root 0/Dorian;
5. resolves `StrongRhythmFrozenSelection` through production;
6. initializes deterministic neutral pitch carriers and calls `migrateStrongRhythmFrozenMaterial`;
7. constructs the existing V0R `GenerationObservation` with `observeGeneration` for request/execution/result/provenance;
8. emits the neutral actual physical observation;
9. calls `preparePhraseExecution` for the production-declared phrase length, records declared/requested/admission/effective/fallback fields separately, and materializes every admitted bar via `materializePreparedPhraseBar`;
10. emits trajectory ID only as execution provenance, never as a structural signature.

Failed selection/materialization/admission is emitted as a row with status/reason and empty/`NOT_OBSERVED` physical fields; it is never discarded.

- [ ] **Step 5: Verify GREEN and deterministic replay**

Run:

```bash
bash tests/run_gf2_gate_b_dump_tests.sh
```

Expected: PASS and two consecutive raw dump files compare byte-for-byte.

- [ ] **Step 6: Commit Task 2**

```bash
git add tests/support/gf2_gate_b_observation.h tools/gf2/gf2_gate_b_dump.cpp tests/run_gf2_gate_b_dump_tests.sh
git commit -m "research(gf2-c2): observe materialized Gate B corpus"
```

### Task 3: Implement deterministic signature and pairwise analysis

**Files:**
- Create: `tools/gf2_gate_b.py`
- Modify: `tests/test_gf2_gate_b_analysis.py`

**Interfaces:**
- Consumes: raw production-backed TSV only plus frozen corpus/contract files.
- Produces: `GF2_GATE_B_MATERIALIZED_CORPUS.tsv`, `GF2_GATE_B_PROFILE_SIGNATURES.tsv`, `GF2_GATE_B_PAIRWISE_DISTINCTNESS.tsv`, `GF2_GATE_B_FINDINGS.md` into a caller-specified directory.

- [ ] **Step 1: Extend RED tests with a minimal synthetic raw corpus**

Add tests that feed two/three synthetic profiles through the public parser/aggregator and assert:

```text
signature IDs exclude raw provenance enum IDs
relative pitch transposition-equivalent payloads produce the same pitch relation signature
profile rows aggregate all seeds/depths
unordered pairs have no A-B/B-A duplicates
pairwise evidence names per-dimension relations
same-genre collisions are discoverable
cross-genre structural collisions are discoverable
one-dimensional identity detection reports the sole differing axis
TIMBRE-DEPENDENT remains unreachable without positive timbre_evidence
```

- [ ] **Step 2: Verify RED**

Run:

```bash
python3 tests/test_gf2_gate_b_analysis.py
```

Expected: FAIL on missing parser/aggregation behavior.

- [ ] **Step 3: Implement `tools/gf2_gate_b.py`**

Required CLI:

```text
python3 tools/gf2_gate_b.py \
  --raw <raw.tsv> \
  --seeds tests/support/gf2_gate_b_seeds.tsv \
  --contract tests/support/gf2_gate_b_contract.json \
  --output-dir <dir>
```

The implementation uses only Python stdlib and:

- validates metadata/cardinality/seed/depth/profile uniqueness;
- turns neutral observed fields into seven canonical dimension payloads and IDs;
- uses `NOT_OBSERVED` instead of synthetic zero values;
- aggregates per-profile signature sets/frequencies/ranges and P1/P2/P3 matched transformation evidence;
- compares unordered profile pairs by matched corpus sequences with relation values exactly `SAME`, `DISJOINT`, `OVERLAP`, `NOT_OBSERVED`;
- classifies pairs explainably with dimension evidence rather than a scalar distance;
- never emits TIMBRE-DEPENDENT unless an explicit positive observed timbre-evidence field exists;
- detects same-Genre recipe collisions, cross-Genre collisions and one-dimensional profiles;
- writes all files with `\n`, sorted deterministic ordering, no timestamps/UUIDs.

- [ ] **Step 4: Verify GREEN**

Run:

```bash
python3 tests/test_gf2_gate_b_analysis.py
```

Expected: PASS.

- [ ] **Step 5: Run analyzer over actual dump**

Run the dump, then:

```bash
python3 tools/gf2_gate_b.py \
  --raw build/host-tests/gf2-gate-b/raw.tsv \
  --seeds tests/support/gf2_gate_b_seeds.tsv \
  --contract tests/support/gf2_gate_b_contract.json \
  --output-dir build/host-tests/gf2-gate-b/generated
```

Expected: exactly four required review artifacts plus deterministic summary output.

- [ ] **Step 6: Commit Task 3**

```bash
git add tools/gf2_gate_b.py tests/test_gf2_gate_b_analysis.py
git commit -m "research(gf2-c2): analyze Gate B structural capacity"
```

### Task 4: Freeze actual Gate B artifacts and findings

**Files:**
- Create: `docs/research/GF2_GATE_B_MATERIALIZED_CORPUS.tsv`
- Create: `docs/research/GF2_GATE_B_PROFILE_SIGNATURES.tsv`
- Create: `docs/research/GF2_GATE_B_PAIRWISE_DISTINCTNESS.tsv`
- Create: `docs/research/GF2_GATE_B_FINDINGS.md`

**Interfaces:**
- Consumes: actual generated artifacts from Task 3.
- Produces: committed byte-for-byte review snapshots.

- [ ] **Step 1: Review actual cardinalities before committing**

Assert from generated data:

```text
profile_count == frozen contract profile_count
seed_count == 55
main realization rows == profile_count * 55 * 3
pairwise rows == profile_count*(profile_count-1)/2
if profile_count == 33, pairwise rows == 528
all unordered pair keys unique
```

- [ ] **Step 2: Review findings boundary**

Confirm `GF2_GATE_B_FINDINGS.md` contains answer-first sections in this exact conceptual order:

```text
Exact base
Corpus
Determinism
Measured axis capacity: Genre/Recipe, Density, Feel, Phrase law, secondaryRole, DEPTH, GRID
Pairwise results
Same-Genre Recipe collisions
Cross-Genre collisions
One-dimensional profiles
Negative capacity
Observation limitations
Gate B conclusion
```

It must contain no delete/merge/promote/control/UI/rename/next-feature recommendation.

- [ ] **Step 3: Commit generated evidence deliberately**

```bash
git add docs/research/GF2_GATE_B_MATERIALIZED_CORPUS.tsv \
        docs/research/GF2_GATE_B_PROFILE_SIGNATURES.tsv \
        docs/research/GF2_GATE_B_PAIRWISE_DISTINCTNESS.tsv \
        docs/research/GF2_GATE_B_FINDINGS.md
git commit -m "docs(gf2-c2): freeze Gate B measured capacity"
```

### Task 5: Add focused reproducibility CI and close the research branch

**Files:**
- Create: `tests/run_gf2_gate_b_tests.sh`
- Create: `.github/workflows/gf2-c2-gate-b.yml`

**Interfaces:**
- Consumes: Tasks 1–4.
- Produces: one focused CI gate proving provenance, source firewall, corpus completeness, compiler/process determinism and committed artifact equality.

- [ ] **Step 1: Write the focused runner**

`tests/run_gf2_gate_b_tests.sh` must:

```text
verify exact base commit exists
run git diff --check exact-base...HEAD
FAIL if git diff exact-base...HEAD -- src/ is non-empty
run Python analyzer unit tests
compile/run raw dump twice with GCC and diff outputs
compile/run with Clang when available and diff normalized raw output with GCC
validate catalog-count/55-seed/P1/P2/P3/phrase-law/secondary-role coverage
run tools/gf2_gate_b.py into a fresh temp output directory
validate unordered pair uniqueness/completeness and explicit 528 rows when profile_count=33
cmp each generated artifact byte-for-byte with committed docs/research artifact
```

- [ ] **Step 2: Write workflow**

`.github/workflows/gf2-c2-gate-b.yml` runs on push to the research branch, pull requests touching Gate B surfaces, and `workflow_dispatch`. Ubuntu checkout must use full history (`fetch-depth: 0`), install/use GCC+Clang available on runner, then execute:

```bash
bash tests/run_gf2_gate_b_tests.sh
```

- [ ] **Step 3: Verify locally where possible and commit**

```bash
bash tests/run_gf2_gate_b_tests.sh
git diff --check 9c01b0c34b80aacb1dd6be66bb07c5cef3ad1c38...HEAD
git diff --quiet 9c01b0c34b80aacb1dd6be66bb07c5cef3ad1c38...HEAD -- src/
```

Expected: PASS.

Commit:

```bash
git add tests/run_gf2_gate_b_tests.sh .github/workflows/gf2-c2-gate-b.yml
git commit -m "ci(gf2-c2): prove Gate B materialized capacity"
```

- [ ] **Step 4: Create Draft PR and verify exact-head CI**

Create Draft PR:

```text
0.9.10-GF2-C2 — Gate B materialized musical capacity
```

Base `dev_0.9.10`. Record the exact branch head and focused workflow run ID/jobs/result. Do not merge Gate B in this checkpoint unless separately instructed.

- [ ] **Step 5: Re-check authoritative dev compatibility without rebasing corpus**

Fetch/inspect current `dev_0.9.10` after measurement. If it remains `9c01...`, record exact compatibility. If Performance integration advanced it, compare frozen Gate B base to new dev and classify whether delta is Performance/MIDI-only or changes GF2 generation semantics. Do not rebase the frozen measurement branch mid-corpus.

- [ ] **Step 6: Final verification**

Verify:

```text
src delta = empty
Performance delta = none
required four artifacts present
focused CI green on exact branch head
Gate B findings contain no GF2-G1 recommendation
```
