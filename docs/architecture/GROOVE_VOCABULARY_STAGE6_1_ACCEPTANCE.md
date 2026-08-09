# Groove Vocabulary Stage 6.1 — BarEvolution hardening acceptance

**Status:** technical hardening gate before Stage 7 vocabulary expansion  
**Base:** Stage 6 corrected candidate `ee99c5cda41a8e55ea6eba7a1105249fffe0621b`  
**Scope:** remove known technical ambiguities/cost regressions from the transient BarEvolution core and its catalog contracts without making it production-reachable.

## Purpose

Stage 6.1 does not add musical content. It hardens the existing Stage 6 API so later Atlas-derived multi-bar data does not amplify avoidable implementation risks.

The specific review findings closed here are:

1. duplicate whole-catalog validation;
2. unsafe archetype lookup before Stage 2 catalog validation;
3. probabilistic rather than strict secondary-before-structural drop priority;
4. Reduction/Break tests that allowed zero actual drops;
5. missing byte/event equality checks for Statement/Response versus base realization;
6. ambiguous Response semantics;
7. missing explicit stack/candidate-cost guards;
8. undocumented RNG salt-space invariant;
9. transform flags/intents could validate with a zero execution budget;
10. statically impossible copy functions could validate in phrase bar 0;
11. the Stage 6 transaction regression used that statically invalid bar-0 fixture instead of a runtime-only evolution failure.

## Changes

### Catalog validation ownership and safety

`evolveRhythmPhrase()` performs only primitive request checks before calling the Stage 2 realizer. It does **not** dereference `catalog.archetypes`, `catalog.trajectories`, lane arrays or relationship arrays first.

`realizeRhythmPhrase()` remains the single owner of full `validateRhythmCatalog()` scanning and archetype/phrase-length request validation. Only after the base realization succeeds does the Stage 6.1 wrapper look up the already-validated archetype and trajectory.

A malformed-catalog regression supplies non-zero counts with null backing arrays and requires `BaseRealizationFailed` with no partial plan/trajectory. This guards against reintroducing a pre-validation lookup while still avoiding a duplicate full-catalog scan.

### Mutation budget executability

Catalog validation now rejects transform authorization that cannot execute its named operation:

```text
AllowReduction  => maxDrops > 0
AllowBreak      => maxDrops > 0
AllowTurnaround => maxAdds  > 0
```

A mismatch returns `CatalogValidationError::InvalidMutationPolicy` before trajectory-reference validation. This prevents a future vocabulary entry from advertising Reduce/Break/Turnaround while silently producing a no-op solely because its execution budget is zero.

This rule is intentionally narrow. `AllowOptionalAdds` or `AllowGhostConversion` may still coexist with `maxAdds == 0`; that is a legal zero-add budget and remains useful for proving that `RepeatWithGhosts` obeys the budget instead of inventing an implicit fallback add.

### First-bar trajectory semantics

The following functions require an already materialized earlier bar and are therefore statically invalid at phrase bar 0:

```text
Repeat
RepeatWithGhosts
Return
```

`validateTrajectories()` rejects them with `InvalidTrajectoryBarFunction` instead of allowing an inevitably failing runtime trajectory.

The transaction regression no longer relies on bar-0 `Repeat`. It uses a valid two-bar catalog with a hard phrase-scope `Coincide` cardinality of exactly one match and a `Statement -> Repeat` trajectory. Base realization can satisfy the catalog; copying bar 0 into bar 1 necessarily changes the phrase-wide match count to zero or two, so evolution fails only after the valid base realization. The result must remain transactional: no selected trajectory, plan or identity may leak.

### Strict drop precedence

Reduction/Break now rank candidates in two classes:

```text
1. secondary events
2. non-anchor structural events
```

All secondary candidates are exhausted before any structural candidate may be attempted. Deterministic RNG ranks candidates only *inside* one class.

### Statement / Response

Stage 6.1 freezes:

```text
Statement = independently realized base bar + Statement metadata
Response  = independently realized base bar + Response metadata
```

`Response` is intentionally metadata-only in v1. A future topology-changing Response requires Atlas evidence, a named transform/budget, and its own tests.

### RNG salt separation

Trajectory selection uses a salt of:

```text
(phraseBars << 8) | level
```

which is always `>= 0x100` because `phraseBars >= 1`.

Evolution uses the selected `TrajectoryId`, currently `uint8_t`, therefore `<= 0xFF`.

That disjoint salt-space assumption is now documented in source. If `TrajectoryId` widens, this contract must be revisited.

## Runtime cost review

BarEvolution remains command-time, fixed-capacity and no-heap.

For one `dropOneStructuralEvent()` call, the candidate-attempt upper bound is:

```text
2 × active lane count × 16 steps
```

and therefore no more than:

```text
2 × kRhythmRoleCount × 16 <= 256 candidate attempts
```

Each failed candidate performs a transactional plan copy plus full evolved-topology validation. This is bounded but deliberately not suitable for an audio callback.

Stage 6.1 adds host guards for:

```text
sizeof(RhythmPhrasePlan) <= 512 B
sizeof(BarEvolutionResult) <= 704 B
candidate upper bound <= 256
```

and GCC `-fstack-usage` ceilings:

```text
evolveRhythmPhrase      <= 4096 B compiler-bounded host frame
dropOneStructuralEvent  <= 2048 B compiler-bounded host frame
```

The runner also compiles with `-Wvla -Werror`. GCC may classify a frame as `static` or `dynamic,bounded`; unbounded `dynamic` is rejected. The numeric ceiling remains mandatory in either accepted classification.

These are **host regression ceilings, not ESP32-S3 measurements**.

## ESP32-S3 stack high-water gate

Normal Stage 5 `GENRE MATERIALIZE` still does not call BarEvolution, so the current firmware cannot provide a meaningful production-task high-water measurement for this code path.

Before the first PR that wires multi-bar BarEvolution into a real Cardputer generation command, that PR MUST record:

```text
stack high-water before generation
stack high-water after worst-case 4-bar generation
minimum remaining stack words/bytes
largest internal heap block before/after
execution duration for worst-case Reduction/Break trajectory
```

The measurement belongs to the actual task/call site that will own generation. A synthetic host value must not be presented as ESP32 stack high-water.

Stage 7 vocabulary-data work may proceed without this hardware probe **only while production BarEvolution wiring remains absent**. The first production wiring PR is blocked until the probe passes.

## Hardware list

No new hardware is required for Stage 6.1 host acceptance.

Future production-wiring probe target:

```text
M5Stack Cardputer-Adv
ESP32-S3
normal project build profile
```

## Wiring

No external wiring. USB-C is used only for flash/Serial when the future hardware stack probe is run.

PORT.A / I2C pins and devices are unrelated and must not be changed.

## Build / test

Focused host matrix:

```bash
bash tests/run_rhythm_stage6_tests.sh
bash tests/run_rhythm_stage6_1_tests.sh
```

Normal project gates:

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
```

CI additionally builds SDL and the SEQTRAK MIDI-only profile.

## Expected behavior

No user-visible behavior change is expected.

The shipped reference vocabulary still exposes only one-bar Statement reachability. Stage 6.1 changes internal validation/order/tests only.

Host output must include:

```text
Groove Vocabulary Stage 6.1 source regressions: OK
Groove Vocabulary Stage 6.1 hardening: OK
Groove Vocabulary Stage 6.1 stack usage: evolve=...B(...) drop=...B(...)
Groove Vocabulary Stage 6.1 hardening host matrix: OK
```

## Troubleshooting

If Reduction/Break hardening fails because the fixture contains no secondary event, do not weaken the assertion back to `<=`. Fix the fixture or realizer contract so the test demonstrates one real bounded drop.

If a transform-budget regression returns `TrajectoryLevelConflict` instead of `InvalidMutationPolicy`, the mutation policy is still being accepted too early; validate transform executability before trajectory refs.

If a bar-0 copy-function fixture reaches BarEvolution runtime, `validateTrajectories()` has regressed; `Repeat`, `RepeatWithGhosts` and `Return` must be rejected before realization.

If the runtime-only transaction fixture starts failing catalog validation, do not return to a statically invalid bar-0 fixture. Preserve a catalog-valid case whose failure is created only by applying the selected multi-bar function.

If the malformed-catalog regression crashes, inspect for any `archetypeFor()` / trajectory / lane access performed before successful `realizeRhythmPhrase()` validation.

If stack-usage output becomes unbounded `dynamic`, a VLA is reported by `-Wvla`, or the numeric ceiling is exceeded, inspect new local arrays/value copies before changing the gate.

If aggregate Core regressions fail only on the inherited Cardputer ADV `PA_EN` source assertion after Stage 1–6.1 pass, treat it separately from Stage 6.1.

## Acceptance checklist

```text
[ ] Stage 6 legacy matrix passes unchanged.
[ ] Stage 6.1 GCC passes.
[ ] Stage 6.1 Clang passes.
[ ] Stage 6.1 ASan+UBSan passes.
[ ] Full catalog validation is not duplicated in BarEvolution wrapper.
[ ] Catalog arrays are not dereferenced before Stage 2 validation.
[ ] Malformed non-zero-count/null-array catalog fails without crash or partial result.
[ ] AllowReduction with maxDrops == 0 is InvalidMutationPolicy.
[ ] AllowBreak with maxDrops == 0 is InvalidMutationPolicy.
[ ] AllowTurnaround with maxAdds == 0 is InvalidMutationPolicy.
[ ] Repeat in bar 0 is InvalidTrajectoryBarFunction.
[ ] RepeatWithGhosts in bar 0 is InvalidTrajectoryBarFunction.
[ ] Return in bar 0 is InvalidTrajectoryBarFunction.
[ ] Transaction regression uses a catalog-valid runtime-only failure.
[ ] Failed evolution exposes no partial trajectory/plan/identity.
[ ] Secondary candidates are strictly exhausted before structural drops.
[ ] Reduction test performs at least one actual drop and stays within maxDrops.
[ ] Break test performs at least one actual drop and stays within maxDrops.
[ ] Statement event topology equals the same base realization.
[ ] Response event topology equals the same base realization.
[ ] Response is documented as metadata-only v1.
[ ] RNG salt-space invariant is documented.
[ ] Fixed-capacity footprint guards pass.
[ ] `-Wvla -Werror` passes and host stack usage remains compiler-bounded and below ceilings.
[ ] SDL build passes.
[ ] Cardputer-Adv compile + fixed DRAM gate pass.
[ ] SEQTRAK MIDI-only compile/check passes.
[ ] No Scene/UI/Genre/Stage5 production wiring was added.
```

A real ESP32-S3 task high-water measurement remains mandatory at the first production BarEvolution call site, not in this unreachable hardening PR.
