# Generation Stage 14.1 — P-Level Semantics Acceptance

## Purpose

Make `P1 / P2 / P3` cumulative and musically distinct without changing the current Stage 14 production sound path.

Contract:

- **P1 Canonical** — stable phrase identity only; no secondary or ghost additions.
- **P2 Variation** — the existing subtle ornament layer; current ReferenceVocabulary remains two ghost additions maximum.
- **P3 Transformation** — structural secondary additions plus the P2 ghost layer, while preserving the same `PhraseRhythmIdentity` and protected anchors.

Stage 14.1 does **not** expose a P-level selector in production UI. The live bridge still defaults to P2; UI reachability and persistence belong to Stage 14.2.

## Hardware list

For automated validation: no hardware required.

Optional later listening check:

- M5Stack Cardputer ADV
- USB cable
- Existing GroovePuter audio/MIDI monitoring setup

## Wiring

No external wiring is required for Stage 14.1.

If an optional Cardputer listening check is performed, use the same audio/MIDI wiring already accepted for the Stage 14 baseline. This change does not touch PORT.A, I2C, SPI, display, or MIDI pin ownership.

## Build / Flash steps

Host regression matrix:

```bash
./tests/run_generation_stage13_tests.sh
```

Full repository regression gate:

```bash
./tests/run_host_tests.sh
```

Cardputer compile/DRAM and SEQTRAK MIDI-only builds remain CI gates. No hardware flash is required to accept Stage 14.1 because production still selects P2.

## Expected behavior

The host test `tests/test_generation_stage14_p_level_semantics.cpp` verifies across 128 deterministic seeds on the hardware-audited `funk_house_bridge` identity:

- P1 has zero secondary events and zero ghosts;
- P2 has zero secondary events and at most two ghosts;
- P3 can contain both secondary events and ghosts;
- P3 surface variation is larger in aggregate than P2;
- P1, P2 and P3 reuse exactly the same `PhraseRhythmIdentity`;
- legacy six-field `MutationBudget{...}` aggregate initialization keeps the old field layout and semantics.

The current live generation bridge remains P2, so existing Stage 14 hardware sound is intentionally unchanged.

## Troubleshooting

### P3 has secondary events but no ghosts

Check `ghostBudgetFor()` in `src/generation/rhythm/rhythm_realizer.cpp`. P3 must inherit the P2 ghost budget when no explicit P3 ghost budget is supplied.

### Older tests fail after the MutationBudget extension

The new fields must remain appended after `allowedIntents`. Do not insert them between legacy fields; existing six-field aggregate initializers depend on positional compatibility.

### Stage 14 hardware sound changes unexpectedly

This stage must not change `StrongRhythmMigrationContext::level`. The live bridge remains `RealizationLevel::P2Variation` until Stage 14.2.

### P3 violates density or protected-space contracts

Both variation passes must continue to use the existing `addPlanSecondary()` / `addPlanGhost()` legality checks. Do not bypass lane bounds, density bounds, protected spaces, or hard relationships.

## Acceptance checklist

- [ ] `tests/run_generation_stage13_tests.sh` passes with GCC.
- [ ] The same matrix passes with Clang when available.
- [ ] ASan/UBSan matrix passes.
- [ ] Full core host regressions pass or any failure is proven inherited from the unchanged Stage 14 base.
- [ ] SDL build passes.
- [ ] Cardputer ADV normal compile passes.
- [ ] Cardputer ADV fixed-DRAM gate passes.
- [ ] Cardputer ADV SEQTRAK MIDI-only build passes.
- [ ] P1 produces no surface additions.
- [ ] P2 retains the previous ghost-only behavior.
- [ ] P3 demonstrates both secondary and ghost additions over the deterministic corpus.
- [ ] P3 aggregate surface distance is greater than P2 over the deterministic corpus.
- [ ] P1/P2/P3 preserve one identical phrase identity when reused.
- [ ] Current production live bridge remains P2.
- [ ] Three consecutive clean reviews are completed on one unchanged final SHA.
