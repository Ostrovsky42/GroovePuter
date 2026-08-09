# Groove Vocabulary Stage 6.1 — BarEvolution hardening acceptance

**Status:** technical hardening gate before Stage 7 vocabulary expansion  
**Base:** Stage 6 corrected candidate `ee99c5cda41a8e55ea6eba7a1105249fffe0621b`  
**Scope:** remove known technical ambiguities/cost regressions from the transient BarEvolution core without making it production-reachable.

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
8. undocumented RNG salt-space invariant.

## Changes

### Catalog validation ownership and safety

`evolveRhythmPhrase()` performs only primitive request checks before calling the Stage 2 realizer. It does **not** dereference `catalog.archetypes`, `catalog.trajectories`, lane arrays or relationship arrays first.

`realizeRhythmPhrase()` remains the single owner of full `validateRhythmCatalog()` scanning and archetype/phrase-length request validation. Only after the base realization succeeds does the Stage 6.1 wrapper look up the already-validated archetype and trajectory.

A malformed-catalog regression supplies non-zero counts with null backing arrays and requires `BaseRealizationFailed` with no partial plan/trajectory. This guards against reintroducing a pre-validation lookup while still avoiding a duplicate full-catalog scan.

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
