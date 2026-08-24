# 0.9.9-E3R-A — DROP / DISPLACE Execution Contract Audit

## Purpose

Audit whether `DROP` and `DISPLACE` can be materialized from the frozen E2 contracts using existing authoritative production semantics. This checkpoint does **not** enable either mutation, change musical policy, or build a new authoritative graph.

The required distinction is:

- **delta representation**: E2c/E2a can describe or produce an operation shape;
- **materialization contract**: `rhythm_realizer` can execute the exact caller-selected operation without a second executor or a new musical interpretation.

A synthetic E2a fixture proves only the first item unless it calls the authoritative execution owner.

## Hardware

NONE — host/research only.

## Wiring

NONE.

## Exact base

- Exact starting SHA: `872fef9331a34e8d48f5703015b1126f11e581c3`.
- Starting branch: `research/20260825-01-0.9.9-e3r-drop-displace-value-audit`.
- PR base: `research/20260824-02-0.9.9-e2-authoritative-variant-graph`.
- The PR-base branch is also at `872fef9331a34e8d48f5703015b1126f11e581c3` at audit start.
- No rebase and no merge from `dev`.

## Owners

- **E2c**: `RhythmMutationDelta` vocabulary, shape, ordering and DISPLACE grammar.
- **E2a**: one-hop candidate producer representation.
- **E2b**: canonical-relative diff and existing `MutationPolicy` / `MutationBudget` legality.
- **rhythm_realizer**: single mutation execution and structural-validation owner.
- **bar_evolution**: trajectory / `BarFunction` planning only; it delegates mutation to `rhythm_realizer`.
- **V0R**: research measurement consumer only; it must fail closed rather than invent unsupported materialization semantics.

## DROP evidence

### DROP_GRAMMAR — YES

E2c freezes `DROP` as:

```text
source -> no target
```

`rhythmMutationDeltaShapeValid()` requires a valid `sourceStep` and `targetStep == kNoMutationStep`. E2c also states that ghost removal is represented as `DROP`.

### DROP_PRODUCER_FIXTURE — YES

E2a enumerates `DROP` when the current site is occupied, the existing budget/intent permits the drop class, `dropSourceMutable()` accepts the source, and topology checks remain safe. The synthetic E2a contract fixture produces both a canonical-anchor DROP allowed through an existing transform rule and a ghost DROP.

This is **DELTA REPRESENTATION TEST** evidence only.

### DROP_EXECUTOR — NO

The only public authoritative mutation executor remains:

```cpp
bool applyRhythmBarFunctionMutation(
    const RhythmArchetype& archetype,
    RhythmPhrasePlan& plan,
    uint8_t bar,
    BarFunction function,
    uint32_t seed);
```

It does not accept a `RhythmMutationDelta`, a selected role/source step, or a target step.

`Reduction` and `Break` are not an exact `DROP(sourceStep)` primitive:

1. both first call `clearGhosts()`, changing every ghost in the bar rather than only a selected source onset;
2. both then call `dropOneStructuralEvent()` up to `maxDrops` times;
3. `dropOneStructuralEvent()` chooses a candidate internally by secondary-first class plus deterministic rank;
4. the helper has no caller-selected `sourceStep` argument;
5. it clears either `secondary` or `structural` at the internally selected step and recomputes lane-derived gate masks;
6. it does not update `accents`.

Therefore legacy destructive transforms are higher-level behavior and cannot be reinterpreted as the frozen per-delta DROP contract.

### DROP_DIFF — YES

E2b emits `RhythmMutationOp::DROP` for an unmatched canonical onset. Existing E2b tests already cover a manually constructed legal DROP result. E3R-A adds a focused **DIFF CONTRACT TEST ONLY** fixture that hand-constructs `W`, then verifies:

- one DROP is classified;
- canonical and candidate structural plans are valid;
- the existing budget accepts the result;
- stale lane-derived gates make `candidatePlanValid == false`;
- a dangling accent on the removed site is rejected as `InvalidCandidateMaterial`.

The fixture does not define how an executor should obtain `W`.

### DROP_STRUCTURAL_VALIDATION — YES

`canonicalRhythmCandidateValid()` delegates candidate structural legality to `rhythmMutationPlanValid()`. That validator checks protected space, lane structural bounds, hard relationships, ornament bounds and exact lane-derived `shortGate` / `heldGate` / `tieGate` masks.

This proves the result can be validated after it exists. It does not provide an exact materialization path.

### DROP_RESEARCH_MATERIALIZABLE — NO

V0R materializes only ADD/GHOST control operations. Its adapter explicitly fails closed for `DROP`, `DISPLACE` and `ACCENT`. E3R-A does not weaken that boundary.

### Exact DROP contract gap

A `rhythm_realizer`-owned, caller-selected materialization contract is missing. It must define, without relying on `Reduction`/`Break` transform selection:

- exact role + `sourceStep` execution;
- whether the source onset is structural, secondary or ghost and how that kind is handled;
- normative accent handling for a removed onset;
- normative gate handling;
- the guarantee that unrelated onsets are unchanged;
- validation/failure behavior when the exact selected source cannot be removed legally.

E2b's rejection of dangling accents constrains legal output but does **not** decide whether an executor should clear the accent or reject the operation.

## DISPLACE evidence

### DISPLACE_GRAMMAR — YES

Frozen E2c grammar defines:

- same lane / `RhythmRole`;
- bar-local, non-wrapping source -> target;
- distinct valid source and target;
- radius `<= 2`;
- immutable/protected/forbidden restrictions;
- canonical source permission only through a matching `AnchorTransformRule::displaceableCanonical`;
- target in preferred/optional onset space and never an anchor target.

### DISPLACE_PRODUCER_FIXTURE — YES

E2a emits `DISPLACE` only from a current non-ghost onset (`structural | secondary`) when `AllowOptionalDisplace` is present, the target is empty, E2c displacement grammar accepts source/target, and topology remains safe. Its synthetic contract fixture proves a concrete `4 -> 2` delta shape.

This is **DELTA REPRESENTATION TEST** evidence only.

### DISPLACE_EXECUTOR — NO

`applyRhythmBarFunctionMutation()` contains no DISPLACE branch and accepts no mutation delta. No authoritative `rhythm_realizer` primitive moves an exact selected onset `sourceStep -> targetStep`.

No research tool may mirror field edits and call that an authoritative DISPLACE execution path.

### DISPLACE_DIFF — YES

E2b can pair unmatched same-lane onsets into a DISPLACE when:

- onset kind matches (`structural`, `secondary`, or `ghost` in the diff consumer);
- source/target satisfy E2c displacement grammar;
- accent state at source and target is equal.

The focused E3R-A **DIFF CONTRACT TEST ONLY** fixture hand-constructs an accented structural move `4 -> 6` and proves one DISPLACE is classified. If the target is left unaccented, E2b observes DROP + ADD rather than DISPLACE. This is a diff-matching rule, not an executor policy.

### DISPLACE_STRUCTURAL_VALIDATION — YES

A manually valid result is accepted by `rhythmMutationPlanValid()`. A hand-constructed result with stale source gate masks is rejected by the structural validator.

Again, validation after construction is not materialization.

### DISPLACE_RESEARCH_MATERIALIZABLE — NO

Frozen V0R explicitly fails closed on DISPLACE. No authoritative materialization path exists at this checkpoint.

### Exact DISPLACE contract gap

A `rhythm_realizer`-owned exact source -> target executor contract is missing. It must define:

- how structural vs secondary onset kind moves;
- whether ghost displacement is executable at all (E2a currently does not propose it, while E2b can recognize same-kind ghost displacement);
- exact accent behavior;
- exact `shortGate` / `heldGate` / `tieGate` behavior;
- handling of any other coupled state;
- failure behavior if source/target cease to satisfy grammar/structural legality;
- guarantee that unrelated material is unchanged.

The existing grammar and E2b recognition rules are insufficient to choose these execution semantics.

## Coupled-field audit

The table below reports **exact per-delta materialization semantics**. Because neither DROP nor DISPLACE has an authoritative delta executor, current source does not define the requested field transitions. They remain `UNDEFINED`; E3R-A does not promote legacy transform behavior or diff matching into execution policy.

| Field | DROP exact delta | DISPLACE exact delta |
|---|---|---|
| structural | UNDEFINED | UNDEFINED |
| secondary | UNDEFINED | UNDEFINED |
| ghosts | UNDEFINED | UNDEFINED |
| shortGate | UNDEFINED | UNDEFINED |
| heldGate | UNDEFINED | UNDEFINED |
| tieGate | UNDEFINED | UNDEFINED |
| accents | UNDEFINED | UNDEFINED |

Observed source behavior that must **not** be promoted to the table above:

| Existing path | structural | secondary | ghosts | gates | accents |
|---|---|---|---|---|---|
| Legacy `Reduction` / `Break` | selected internally and cleared if chosen | preferred removal class; selected internally and cleared if chosen | **all cleared before structural drop loop** | recomputed after each selected drop and again for bar | untouched by `dropOneStructuralEvent()` |
| E2b DISPLACE recognition | same onset kind must exist at target | same onset kind must exist at target | diff can pair same-kind ghost material | ignored by diff classification; checked later by structural validator | source/target accent state must match |

These are evidence about existing transform/consumer behavior, not a missing executor specification.

## E2a fixture boundary

The E2a synthetic fixture proves the producer can represent the full frozen vocabulary under a permissive synthetic archetype. In particular it asserts nonzero DROP and DISPLACE proposal counts and concrete deltas.

It does **not** call `applyRhythmBarFunctionMutation()` with those deltas, because that API cannot accept them. Therefore:

```text
E2a synthetic fixture = DELTA REPRESENTATION TEST
E2a synthetic fixture != MATERIALIZATION CONTRACT
```

Production V0R measurements remain authoritative for the selected production references: P1 canonical only, P2 GHOST, P3 ADD + GHOST; raw production DROP/DISPLACE/ACCENT proposals remain zero in the frozen snapshot.

## E2b diff evidence

E2b already has independent canonical-relative consumer semantics for both operations:

- unmatched source -> DROP;
- legal same-kind source/target pairing -> DISPLACE;
- budget counters use `maxDrops` / `maxDisplacements` and existing permission flags;
- final structural legality is delegated to `rhythmMutationPlanValid()`.

E3R-A's focused fixture constructs W manually and is labeled **DIFF CONTRACT TEST ONLY**. No helper in the test is exposed or reused as a materializer.

## Result matrix

| Operation | Grammar | Producer shape | Executor | Diff | Structural validator | Safe research graph |
|---|---|---|---|---|---|---|
| ADD | YES | YES | PARTIAL | YES | YES | YES |
| GHOST | YES | YES | PARTIAL | YES | YES | YES |
| DROP | YES | YES | **NO** | YES | YES | **NO** |
| DISPLACE | YES | YES | **NO** | YES | YES | **NO** |
| ACCENT | YES | YES | NO | YES | YES | NO |

Control interpretation:

- ADD/GHOST are the already accepted V0R controls. Their selected production paths have existing owned behavior sufficient for the frozen research adapter; E3R-A does not generalize that adapter into a new production delta executor.
- ACCENT is comparison-only. No scope expansion is made.

## Decision gate

**D — DROP CONTRACT GAP / DISPLACE CONTRACT GAP.**

Do **not** build a hypothetical DROP/DISPLACE graph from hand-authored field edits.

The next production work, if chosen later, must freeze authoritative execution semantics first. E3R-A itself stops here and does not proceed to E3R-B.

## Build / run

From repository root:

```bash
bash tests/run_0_9_9_e3r_a_tests.sh
bash tests/run_host_tests.sh
```

`run_0_9_9_e3r_a_tests.sh` performs:

1. E3R-A zero-`src/` source guard;
2. focused DROP/DISPLACE E2b diff-contract fixture on GCC;
3. the same fixture on Clang when available;
4. ASan/UBSan run;
5. E2b cumulative suite, which reruns E2c and the E1a/Stage 6.1 owner/executor matrix;
6. E2a producer suite, which also reruns frozen E2c/E1a;
7. frozen V0R snapshot authority gate without updating snapshots.

The E3R-A pull-request workflow also runs the complete host suite.

## Expected behavior

Expected successful output includes:

```text
E3R-A source guard: ZERO src delta, DROP/DISPLACE execution gaps preserved
0.9.9-E3R-A DROP/DISPLACE diff-contract audit: OK
0.9.9-E2b canonical rhythm diff / budget: OK
0.9.9-E2a canonical rhythm mutation producer: OK
0.9.9-E3R-A DROP / DISPLACE execution contract audit: OK
```

The V0R authority gate must regenerate the frozen graph and compare committed snapshots byte-for-byte. No snapshot update is permitted in E3R-A.

## Troubleshooting

- **Source guard reports `src/` changes**: reset those production changes; E3R-A is audit/tests/docs only.
- **Source guard says BarFunction executor changed**: stop and re-audit. The main finding may no longer be true.
- **Focused DROP fixture becomes invalid**: inspect E2b diff rules, lane gate validation and existing budget semantics. Do not fix by adding a test-only materializer.
- **Focused DISPLACE becomes DROP + ADD**: check same onset kind, radius/grammar and source/target accent equality. Do not infer an execution policy from the matcher.
- **V0R snapshot drift**: fail. Do not refresh snapshots in this checkpoint.
- **A production reference starts emitting DROP/DISPLACE/ACCENT**: V0R should fail closed. That is a contract-gap signal, not permission to add research-only field edits.

## Acceptance checklist

- [ ] Starting SHA is exactly `872fef9331a34e8d48f5703015b1126f11e581c3`.
- [ ] PR base is `research/20260824-02-0.9.9-e2-authoritative-variant-graph`.
- [ ] `git diff 872fef9331a34e8d48f5703015b1126f11e581c3..HEAD -- src` is empty.
- [ ] DROP grammar = YES.
- [ ] DROP producer shape = YES.
- [ ] DROP executor = NO.
- [ ] DROP diff = YES.
- [ ] DROP structural validator = YES.
- [ ] DROP safe research materialization = NO.
- [ ] DISPLACE grammar = YES.
- [ ] DISPLACE producer shape = YES.
- [ ] DISPLACE executor = NO.
- [ ] DISPLACE diff = YES.
- [ ] DISPLACE structural validator = YES.
- [ ] DISPLACE safe research materialization = NO.
- [ ] Coupled-field execution table contains only source-supported values; exact DROP/DISPLACE fields remain UNDEFINED.
- [ ] Synthetic E2a fixture is explicitly labeled representation-only.
- [ ] E3R-A focused diff tests pass on GCC/Clang/sanitizers.
- [ ] E2b cumulative suite passes.
- [ ] E2a cumulative suite passes.
- [ ] Frozen V0R snapshot authority gate passes without snapshot changes.
- [ ] Full host suite passes.
- [ ] Draft PR remains unmerged.
- [ ] No E3R-B work is included.
