# Generation Stage 12 — Reference Phrase Catalog Acceptance

Status: `CANDIDATE-REACHABLE / PRODUCTION-BLOCKED`

Base: `agent/20260810-18-generation-p-level-semantics`

## Purpose

Make the already implemented Stage 12 `BarEvolution` / `PhraseEvolution` path
musically testable against repository production archetypes without changing the
accepted one-bar `GENRE -> G` behavior.

This stage adds a fixed-capacity `ReferenceVocabulary::phraseEvolutionCatalog()`
that overlays bounded 2/4-bar trajectories only on rhythm identities that can
satisfy their current relationship contracts. The normal
`ReferenceVocabulary::catalog()` remains one-bar and remains the only production
catalog until the Stage 6.1 ESP32-S3 physical gate passes.

The promoted trajectory vocabulary reuses the existing Stage 12 fixture model:

```text
1 bar : Statement
2 bars: Statement -> Repeat
2 bars: Statement -> RepeatWithGhosts                 P2/P3
4 bars: Statement -> Response -> Repeat -> Return
4 bars: Statement -> Repeat -> Reduction -> Return    P2/P3
4 bars: Statement -> Build -> RepeatWithGhosts -> Turnaround  P3
4 bars: Statement -> RepeatWithGhosts -> Break -> Return       P3
```

The explicit Break trajectory is evidence-gated by Atlas Pass 2 observations of
`DROP_ONLY` and `MIXED` transitions. P2 may remove at most one event; P3 may
remove at most three. Anchors, protected space, lane minima and hard
relationships remain enforced by `BarEvolution` validation.

## Admitted rhythm identities

```text
404 broken_techno
413 two_step_roll
414 ghosted_roll
415 sparse_fast_break
416 halftime_switch
417 classic_2step
418 skippy_2step
420 machine_syncopation
712 electro_backskip
714 electro_gap_push
```

These cover the first Broken / DnB / UK Garage / Electro phrase-evolution
candidate set.

`419 shuffled_4x4` is intentionally not admitted. Its existing hard Coincide
`maxMatches` contract is phrase-wide, so repeating the one-bar contract across
2/4 bars is invalid. Fixing that semantic contract is separate work; the gate is
not weakened here.

The strong one-bar loop identities remain one-bar in this candidate catalog,
including `straight_drive`, `offbeat_open_hat`, `hypnotic_sparse`,
`straight_acid`, `rolling_acid` and hardware-audited `stacked_quarters`.

## Hardware list

For this candidate PR:

- development host with GCC; Clang is used when installed;
- no Cardputer is required for host acceptance.

For the later production-wiring gate:

- M5Stack Cardputer-Adv, ESP32-S3;
- USB-C cable for flash and Serial.

No external I2C/SPI hardware is required.

## Wiring

No external wiring.

The future Cardputer probe uses USB-C only. PORT.A and its I2C pins are not
involved and must not be changed.

## Build / Flash steps

Run the focused Stage 12 matrix:

```bash
bash tests/run_phrase_stage12_tests.sh
```

Then run repository regressions and compile gates:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
```

No firmware flash is required for this PR because the production live bridge is
still hard-guarded to one bar. Do not claim a musical hardware verdict from this
candidate catalog.

Before a later PR may route normal `G` through multi-bar evolution, flash that
later candidate to Cardputer ADV and record the Stage 6.1 physical measurements:

```text
stack high-water before generation
stack high-water after worst-case 4-bar generation
minimum remaining stack words/bytes
largest internal heap block before/after
worst-case 4-bar Reduction/Break duration
```

## Expected behavior

Production firmware behavior is unchanged:

```text
ReferenceVocabulary::catalog() -> one-bar Statement vocabulary
strong migration request.phraseBars -> 1
normal DRUMS/G generation -> existing Stage 14 behavior
```

Host Stage 12 acceptance additionally proves:

```text
phraseEvolutionCatalog() validates as a complete RhythmCatalogView
admitted identities reach 2 and 4 bars directly
admitted identities reach 8 bars through two deterministic 4-bar segments
P2 Reduction performs a real bounded subtraction on elastic representatives
P3 Break performs a real bounded subtraction on elastic representatives
one-bar production catalog remains unchanged
```

Expected focused output includes:

```text
Generation Stage 12 reference catalog source regressions: OK
Generation Stage 12 host matrix: OK
```

There is no new screen or Serial UI output in this PR.

## Troubleshooting

If catalog validation fails with `ImpossibleHardRelationship`, do not widen
cardinality or remove a hard relationship merely to admit an archetype. Leave
that identity one-bar and fix its phrase-level semantic contract separately.

If `Reduction` or `Break` succeeds but removes nothing in the representative
subtractive test, inspect preferred density, lane minima and P-level additions.
Do not weaken the assertion to allow a no-op transform.

If a production source starts referencing `phraseEvolutionCatalog()` or
`evolveMultiBarPhrase()`, revert that wiring. The physical Stage 6.1 gate must be
recorded first.

If the original Stage 12 test that asserts `ReferenceVocabulary::catalog()` is
one-bar starts failing, this branch has accidentally changed production
reachability instead of the candidate overlay.

## Acceptance checklist

```text
[ ] Existing Stage 12 fixture matrix passes under GCC.
[ ] Existing Stage 12 fixture matrix passes under Clang when available.
[ ] Existing Stage 12 fixture matrix passes ASan+UBSan.
[ ] Reference phrase catalog validates with no catalog errors.
[ ] Only the declared candidate identities gain 1/2/4-bar capability.
[ ] shuffled_4x4 remains one-bar pending its Coincide cardinality fix.
[ ] Strong FourFloor/Acid/stacked_quarters loop identities remain one-bar.
[ ] Every admitted identity reaches 2 bars with Statement -> Repeat.
[ ] Every admitted identity reaches 4 bars with bounded Reduction.
[ ] Every admitted identity reaches 8 bars via two 4-bar Break segments.
[ ] P2 Reduction demonstrably removes material without breaking invariants.
[ ] P3 Break demonstrably removes material without breaking invariants.
[ ] Normal ReferenceVocabulary::catalog() remains one-bar.
[ ] strong_rhythm_migration.cpp still sets request.phraseBars = 1.
[ ] No Scene/Song/PhraseCore ownership is added.
[ ] No heap allocation or random global RNG is added.
[ ] Full host regressions pass.
[ ] SDL build passes.
[ ] Cardputer ADV normal + fixed-DRAM builds pass.
[ ] SEQTRAK MIDI-only build passes.
[ ] Production wiring remains blocked pending physical Stage 6.1 measurements.
```
