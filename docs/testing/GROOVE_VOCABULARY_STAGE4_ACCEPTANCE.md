# Groove Vocabulary Stage 4 — Materializer + Shadow Backend

## Purpose

Stage 4 converts a legal one-bar `RhythmPhrasePlan` into the existing `DrumPatternSet` / `SynthPattern` representation and introduces a non-persisted backend boundary for shadow comparison.

Normal user generation behavior remains legacy-owned in this stage. Strong genre migration starts only in Stage 5.

## Ownership contract

Stage 4 may own:

- transactional rhythm-plan materialization;
- explicit physical bindings supplied by the caller;
- a migration-only backend enum (`LegacyAtlas`, `LegacyProcedural`, `Vocabulary`);
- pure side-by-side topology metrics between legacy and Vocabulary outputs;
- later in this stage, a shadow orchestration hook that computes Vocabulary output without applying it.

Stage 4 must not own:

- Genre -> Vocabulary production mapping;
- a user-facing backend switch;
- Scene persistence of backend, archetype, seed, or shadow state;
- page, bank, pattern slot, Song row, or PhraseCore destination selection;
- automatic `Synth A == Bass` / `Synth B == Lead` semantics;
- pitch generation;
- GateClass -> TB303 slide/engine-articulation projection;
- BarEvolution or multi-bar destination choice;
- default replacement of Atlas or procedural generation.

## Materializer contract

The current Stage 4 materializer accepts the Stage 2/3 one-bar Statement surface only.

Every realized role with onsets must be one of:

1. bound to exactly one drum voice;
2. bound to exactly one synth destination with a caller-supplied temporary pitch;
3. explicitly listed in `ignoredRoles`.

A role is never silently dropped.

Bindings may not collide: two rhythm roles cannot claim the same physical drum voice or the same synth destination in one materialization pass.

Materialization is transactional. Validation and scratch construction complete before `destination` is assigned. On `InvalidPlan`, `InvalidBinding`, or `UnboundRole`, destination patterns and diagnostics remain unchanged.

## Representation rules

- `Structural` -> velocity 110.
- `Secondary` -> velocity 86.
- `Ghost` -> velocity 52; synth ghost flag is set.
- Rhythm accents map to the existing accent flag.
- timing remains zero; FEEL remains external.
- drum FX remain `DRUM_FX_NONE`; synth FX remain zero.
- synth slide remains false in Stage 4.
- Short/Held/Tie intent is counted in `deferredGateEvents` rather than silently converted to engine-specific articulation.
- output `PatternGroove` remains at its normal `-1` inheritance defaults; the materializer does not force swing/humanize.

## Shadow metrics contract

`compareShadowPatterns()` is a pure observer over const references. It compares rhythmic topology only:

- legacy onset count;
- Vocabulary onset count;
- shared onsets;
- differing physical slots;
- legacy/Vocabulary/shared accents.

Velocity, timing and FX are deliberately outside the first shadow metric because Stage 4 is validating placement topology before Feel/articulation migration.

The caller can compare drums only or opt into Synth A/Synth B. This is important because early Stage 4 shadowing may intentionally leave Bass/Chord/Melodic roles unbound while legacy synth pitch generation remains authoritative.

## Host tests

```bash
bash tests/run_rhythm_stage1_tests.sh
bash tests/run_rhythm_stage2_tests.sh
bash tests/run_rhythm_stage3_tests.sh
bash tests/run_rhythm_stage4_tests.sh
```

Stage 4 runs under GCC, Clang and ASan+UBSan.

## Acceptance checklist

- [ ] Stage 1/2/3 focused suites remain green.
- [ ] Stage 4 source ownership regressions pass.
- [ ] Stage 4 GCC matrix passes.
- [ ] Stage 4 Clang matrix passes.
- [ ] Stage 4 ASan+UBSan matrix passes.
- [ ] invalid plan leaves destination and diagnostics unchanged.
- [ ] unbound realized role fails instead of being dropped.
- [ ] explicit ignored role is measurable in diagnostics.
- [ ] duplicate physical bindings fail.
- [ ] synth destination is never inferred from RhythmRole.
- [ ] gate intent is not translated to slide in Stage 4.
- [ ] materializer does not overwrite FEEL timing ownership.
- [ ] shadow comparison does not mutate legacy or Vocabulary output.
- [ ] backend route is not persisted in Scene.
- [ ] Cardputer ADV normal build passes DRAM gate.
- [ ] SEQTRAK MIDI-only build passes.
- [ ] SDL build passes.
- [ ] shadow orchestration can execute Vocabulary candidate generation without changing applied legacy patterns.
- [ ] default user-visible generation output remains identical to the legacy path.
- [ ] three consecutive control reviews pass on one unchanged final SHA; any finding resets the counter to 0/3.

## Exit contract

Stage 4 is complete only after the shadow orchestration boundary is wired and proven non-mutating. Stage 5 may then map selected strong rhythmic paths to `Vocabulary` while retaining a per-path rollback to legacy generation.
