# GF2-C2 Gate B — Materialized Musical Capacity Design

**Checkpoint:** `GF2-C2 — GATE B / ACTUAL MATERIALIZED MUSICAL CAPACITY`

**Exact measurement base:** `dev_0.9.10 @ 9c01b0c34b80aacb1dd6be66bb07c5cef3ad1c38`

**Research branch:** `research/20260903-05-0.9.10-gf2-c2-gate-b-materialized-capacity`

## Scope

Gate B is a deterministic research/measurement checkpoint. It measures the music the current production generation path actually materializes after GF2-I1 through GF2-I5. It does not change production semantics, add a musical axis, expand Genre/Recipe, integrate Performance, or begin PATTERN/PHRASE P2.

`src/ DELTA = NONE` and `PERFORMANCE DELTA = NONE` are hard gates.

## Authority and data flow

The only production-materialization authority is the existing GF2-C2-V0R structured observation seam plus the real production composition/migration/phrase executors:

```text
authoritative production catalog
        ↓
production selection + migration / phrase execution
        ↓
GF2-C2-V0R request/execution/result/provenance observation
        ↓
research-side neutral material projection
        ↓
timbre-free signatures
        ↓
profile distributions + pairwise classification
```

The Python orchestrator must never reimplement generation policy or infer materialized results from static declarations. Static census data may only supply stable names/context and the frozen catalog-count contract.

## Corpus identity

The C++ observation dump enumerates production profiles using `kGenerativeModeCount`, `availableRecipeCount()` and `availableRecipeAt()`. No profile count is used to drive enumeration.

Gate B freezes the first authoritative enumeration as a contract and fails on later silent drift.

The seed corpus contains 55 committed deterministic realization identities. The values are passed through the existing production `generationAttemptOrdinal`; no new RNG seed owner is introduced. For each seed the harness derives two other already-existing production coordinates deterministically:

- `patternAddress = seed % kMaxGlobalPatterns`;
- `phraseGenerationIdentity = low16(seed)` with the reserved `0xFFFF` value forbidden by the committed seed contract.

The same seed is used for every profile and every DEPTH so pairwise and P1/P2/P3 comparisons are matched interventions.

Main corpus cardinality is:

```text
production profiles × 55 seeds × P1/P2/P3
```

If the current catalog is 33 profiles this is 5,445 deterministic realizations. The count is an observed/frozen contract, not a loop bound.

## Production execution settings

Each main realization uses:

- profile/recipe from production enumeration;
- production Auto rhythm selection;
- current profile FEEL through `FeelProfileId::Auto`;
- current musician-facing FEEL default amount `20`;
- tonal materialization enabled;
- root pitch class `0` and Dorian scale as a neutral shared tonal context;
- deterministic neutral pitch-source carriers only where the existing production adapter requires a physical source.

The harness resolves a production `StrongRhythmFrozenSelection` and materializes it through `migrateStrongRhythmFrozenMaterial`. It then creates the existing `GF2Measurement::GenerationObservation` for request/execution/result/provenance and projects the actual physical `DrumPatternSet` / `SynthPattern` output into neutral structural evidence.

## Timbre-free event projection

The research observation keeps only properties actually visible in materialized output:

- step/onset;
- bar ordinal;
- musician-level role bucket;
- timing displacement in ticks;
- accent/structural-importance bit where physically observable;
- synth ghost bit where physically observable;
- pitch class relative to the common root and melodic/bass interval contour;
- production semantic role participation already returned by the migration result.

It excludes synth engine, oscillator, sample/kit, FX identity/parameter, velocity magnitude, UI identity and raw enum ordinals as musical evidence.

Duration is emitted as `NOT_OBSERVED`; the current `SynthStep`/`DrumStep` observation does not expose a duration field and Gate B must not convert missing observation into zero.

## Signatures

`tools/gf2_gate_b.py` computes separate canonical SHA-256-derived IDs for:

- `RhythmSignature` — role onset masks, accent topology, timing topology, density, beat/bar activity and silence placement;
- `BassSignature` — onset relation to drums, relative pitch/interval contour, repetition/movement, timing and silence topology;
- `HarmonySignature` — harmonic-event onset mask/count, chord-role onset topology, relative movement/static topology;
- `PhraseSignature` — admitted materialized bars, bar-to-bar structural differences, return relation and actual bar count;
- `RoleSignature` — musician-level participating roles and chord/melodic/hybrid participation;
- `TransformationSignature` — observed P1/P2/P3 physical topology/activity change, without assuming monotonicity;
- `NegativeSignature` — proved absences/rejections, including fixed GRID 16 and DEPTH→role-hierarchy negative capacity.

No single aggregate distance can decide pairwise distinctness.

## Phrase execution

For each main realization the harness first uses the production frozen selection to obtain the declared phrase law and declared phrase length. It then calls `preparePhraseExecution()` using that declared length. It records independently:

- declared phrase law;
- requested bars;
- `PhraseExecutionStatus`;
- `PhraseLengthRequestStatus` / reject reason;
- resolved production trajectory as provenance only;
- admitted YES/NO;
- actual effective/planned bars;
- fallback reason.

Only `PhraseExecutionStatus::Ready` material is used for phrase signatures. Each admitted physical bar is produced with `materializePreparedPhraseBar()` and neutralized like the main material. A trajectory ID is never itself treated as musical distinctness; only its materialized bar consequences are.

CI requires corpus evidence for Loop, RepeatReply, DevelopReturn and SparseDrift, and records a law that is declared but never admitted as negative/insufficient evidence rather than fabricating a multi-bar case.

## Axis checks

### Density

Record the production-resolved structural-density target from `StrongRhythmFrozenSelection`, total physical role-event count, structural onset topology, spread and saturation/collision behavior over all profiles/seeds.

### FEEL

Compare canonical step time with materialized `timing` ticks for each observed event. Distinguish no timing intent, Auto→zero, concrete displacement and sub-threshold/inert observations. Zero effect is a result, not automatically a defect.

### secondaryRole

Measure actual chord/melodic/hybrid participation from migration result plus physical Synth-B activity across the whole corpus.

### DEPTH

Compare matched profile+seed P1/P2/P3 realizations. Measure topology/activity/timing transformation magnitude. Separately verify whether the materialized Synth-B role identity/participation changes when only DEPTH changes. Any systematic role change is a Gate B finding against the frozen I5 characterization; production is not modified.

### GRID

Preserve the frozen I4 result: Core-v1 structural GRID is 16; 8/32 remain intentional negative capacity. Gate B does not implement them.

## Pairwise model

For `N` production profiles emit exactly `N*(N-1)/2` unordered rows. Each dimension is compared independently using matched-seed/depth distribution overlap and exact signature-set evidence.

Classification is exactly one of:

- `STRUCTURALLY DISTINCT`;
- `PARTIALLY DISTINCT`;
- `TIMBRE-DEPENDENT`;
- `STRUCTURALLY REDUNDANT`;
- `INSUFFICIENT EVIDENCE`.

`TIMBRE-DEPENDENT` requires positive observed production timbre evidence. Because the Gate B material observation intentionally removes timbre and the current V0R seam does not expose sound-engine/sample identity, Gate B must not infer this classification from labels or static timbre declarations alone. Pairs with coincident structure but no observed timbre evidence are `STRUCTURALLY REDUNDANT` or `INSUFFICIENT EVIDENCE`, depending on observation coverage.

Same-Genre/different-Recipe collisions and cross-Genre collisions are reported separately. Profiles whose nearest structural distinction is carried by exactly one measured axis are listed as one-dimensional identities.

## Determinism

Required:

- same profile/seed/depth/phrase request reproduces identical raw normalized observation;
- GCC process replay byte-identical;
- Clang dump byte-identical to GCC where the runner has Clang;
- Python artifact ordering deterministic;
- generated committed artifacts compare byte-for-byte;
- no timestamps or UUIDs in committed evidence.

## Committed review artifacts

Required:

- `docs/research/GF2_GATE_B_MATERIALIZED_CORPUS.tsv`
- `docs/research/GF2_GATE_B_PROFILE_SIGNATURES.tsv`
- `docs/research/GF2_GATE_B_PAIRWISE_DISTINCTNESS.tsv`
- `docs/research/GF2_GATE_B_FINDINGS.md`

Machine-readable JSON may accompany these files, but the TSV surfaces above remain authoritative for review.

## CI

`.github/workflows/gf2-c2-gate-b.yml` runs `tests/run_gf2_gate_b_tests.sh` and proves:

- exact-base/source provenance;
- no `src/` delta from `9c01b0c34b80aacb1dd6be66bb07c5cef3ad1c38`;
- catalog enumeration/count freeze;
- 55-seed freeze;
- P1/P2/P3 completeness;
- phrase-law coverage/admission accounting;
- secondary-role coverage;
- deterministic GCC replay;
- GCC/Clang normalized dump equality where Clang exists;
- pairwise uniqueness/completeness;
- committed artifact equality;
- `git diff --check`.

The runner generates artifacts into a temporary build directory and compares them to committed outputs. It never refreshes committed snapshots automatically.

## Gate B conclusion boundary

The findings document is answer-first and measurement-only. It may say which axes are causal, overlapping, inert, redundant, one-dimensional, unobserved or negative capacity. It must not recommend deleting profiles, merging Genres, exposing controls, renaming DEPTH, redesigning UI, or building the next feature. Those decisions belong to `GF2-G1 — INTERPRET ACTUAL CAPACITY`.
