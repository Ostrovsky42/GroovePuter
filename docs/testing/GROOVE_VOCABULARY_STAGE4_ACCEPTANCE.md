# Groove Vocabulary Stage 4 — Materializer + Shadow Backend

## Purpose

Stage 4 converts a legal one-bar `RhythmPhrasePlan` into the existing `DrumPatternSet` / `SynthPattern` representation and provides a non-persisted shadow backend that can compare Vocabulary output with an already-produced legacy snapshot.

Normal user generation behavior remains legacy-owned in this stage. The first production call-site and migration of strong rhythmic paths belong to Stage 5.

## Ownership contract

Stage 4 may own:

- transactional rhythm-plan materialization;
- explicit physical bindings supplied by the caller;
- a migration-only backend enum (`LegacyAtlas`, `LegacyProcedural`, `Vocabulary`);
- pure side-by-side topology metrics between legacy and Vocabulary outputs;
- `runVocabularyShadow()`, which realizes and materializes a Vocabulary candidate entirely in scratch state against const legacy patterns.

Stage 4 must not own:

- a `MiniAcid::regeneratePatternsWithGenre()` production call-site;
- Genre -> Vocabulary production mapping;
- a user-facing backend switch;
- Scene persistence of backend, archetype, seed, or shadow state;
- page, bank, pattern slot, Song row, or PhraseCore destination selection;
- automatic `Synth A == Bass` / `Synth B == Lead` semantics;
- pitch generation;
- GateClass -> TB303 slide/engine-articulation projection;
- BarEvolution or multi-bar destination choice;
- default replacement of Atlas or procedural generation.

This boundary is deliberate: wiring shadow execution into the live generation orchestration would force Stage 4 to choose a Genre/archetype/seed mapping and therefore start Stage 5 early.

## Materializer contract

The Stage 4 materializer accepts the Stage 2/3 one-bar Statement surface only.

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

## Shadow backend contract

`runVocabularyShadow()` receives:

- a caller-selected catalog and archetype;
- realization level and generation context;
- const legacy `DrumPatternSet`, Synth A and Synth B snapshots.

It then:

1. realizes P1 to establish deterministic `PhraseRhythmIdentity`;
2. reuses that identity when P2/P3 is requested;
3. materializes the Vocabulary candidate into a local scratch object;
4. compares candidate topology against the const legacy snapshot;
5. returns only statuses, materialization diagnostics and comparison metrics.

The candidate pattern does not escape the function and no Scene/pattern-bank destination is available to the shadow backend.

Bass/Chord/Melodic roles are explicitly ignored by the default shadow request. This keeps legacy pitch generation authoritative until the later VoiceRole/Bass/Phrase stages rather than silently inventing ownership now.

## Shadow metrics contract

`compareShadowPatterns()` is a pure observer over const references. It compares rhythmic topology only:

- legacy onset count;
- Vocabulary onset count;
- shared onsets;
- differing physical slots;
- legacy/Vocabulary/shared accents.

Velocity, timing and FX are deliberately outside the first shadow metric because Stage 4 is validating placement topology before Feel/articulation migration.

The caller can compare drums only or opt into Synth A/Synth B. When synth rhythm roles are explicitly ignored, synth comparison correctly measures legacy synth onsets against empty Vocabulary synth output rather than pretending Stage 4 owns pitch.

## Host tests

```bash
bash tests/run_rhythm_stage1_tests.sh
bash tests/run_rhythm_stage2_tests.sh
bash tests/run_rhythm_stage3_tests.sh
bash tests/run_rhythm_stage4_tests.sh
```

Stage 4 runs under GCC, Clang and ASan+UBSan.

The shadow corpus covers all 20 Stage 3 reference archetypes, seeds 1..16 and P1/P2/P3, repeats each request for determinism, and verifies that the supplied legacy drum/Synth A/Synth B snapshots remain unchanged.

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
- [ ] `runVocabularyShadow()` keeps the candidate scratch-local and returns diagnostics only.
- [ ] all 20 Stage 3 archetypes shadow successfully across the Stage 4 test corpus.
- [ ] backend route is not persisted in Scene.
- [ ] no `MiniAcid`/Genre/Scene/Song/UI production call-site is added in Stage 4.
- [ ] Cardputer ADV normal build passes DRAM gate.
- [ ] SEQTRAK MIDI-only build passes.
- [ ] SDL build passes.
- [ ] default user-visible generation output remains identical because live generation orchestration is untouched.
- [ ] three consecutive control reviews pass on one unchanged final SHA; any finding resets the counter to 0/3.

## Exit contract

Stage 4 is complete when the materializer and `runVocabularyShadow()` contracts pass the host/embedded compatibility gates on one unchanged SHA. Stage 5 may then introduce the first caller in the live generation path, select Vocabulary only for approved strong rhythmic paths, and retain a per-path rollback to legacy generation.
