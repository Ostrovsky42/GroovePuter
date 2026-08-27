# 0.9.9-PHRASE-W1R — W1 replay on H1-F1

## Purpose

Replay the already frozen PHRASE-W1 harmonic-WHEN ownership semantics on the corrected H1-F1 ancestry. This checkpoint does not research or change harmonic policy. Its only successful outcome is semantic replay without drift while preserving H1-F1 byte-for-byte in its authoritative `chord_progression.*` owner.

## Frozen predecessors

- H1 frozen: `74456bcfec0fc74138ec0d8c652dde642c7e16b6`.
- H1-F1 FINAL: `ead3d7a0666f2dab6c62443cebe504e5e13e8be0`.
- H1-F1 Decision A: **GLOBAL PROGRESSION SOURCE REPRESENTATION READY**.

H1-F1 remains the authoritative WHAT-source representation:

```text
ChordProgressionSource
= selected intrinsic grammar
= period <= 4
= arbitrary phrase-global ordinal truth

ChordProgressionPlan
= finite expanded consumer window
= kMaxHarmonicEvents remains 8
```

## Old W1 reference

- old W1 base: H1 `74456bcfec0fc74138ec0d8c652dde642c7e16b6`.
- old W1 final: `34912cd050c04727c13533575b2cf999816e0549`.
- frozen old-W1 decision: **B — PHRASE-WIDE HARMONIC CLOCK PROJECTION POLICY GAP**.

W1R reproduces that decision; it does not reconsider it.

## Ownership

```text
ChordRhythm
= physical chord articulation

HarmonicRhythm
= WHEN harmony advances inside one physical/semantic bar

ChordProgression / ChordProgressionSource
= WHAT harmony advances to
```

Accepted W1 flow remains:

```text
ProgressionId
    -> HarmonicRhythm
       eventCount -> ChordProgression finite plan consumer
       onsets     -> TonalMaterializer

ChordRhythm
    -> physical articulation -> TonalMaterializer
```

The forbidden old coupling from `onsetCount(chord.plan.onsets)` is not restored. Finite progression cardinality remains sourced from `harmonic.plan.eventCount`.

## Replay delta

Production replay from H1-F1 is limited exactly to:

```text
src/generation/roles/harmonic_rhythm.h
src/generation/migration/strong_rhythm_migration.h
src/generation/migration/strong_rhythm_migration.cpp
```

Those files replay the old W1 final state. No new production owner is introduced.

## H1-F1 firewall

`src/generation/roles/chord_progression.h` and `.cpp` must remain byte-identical to H1-F1 FINAL.

W1R does not call `realizeChordProgressionSource()` or `chordProgressionSourceEventAt()`. Arbitrary phrase-global WHAT projection remains future P1R wiring after H2 replay.

Compatibility evidence additionally requires TwoFiveOne to remain intrinsic `period=3` and ordinal `8` to resolve as intrinsic event `2`.

## F08 compatibility

Accepted F08 bootstrap remains exact:

```text
StaticModal  {0}   count=1
PedalDrone   {0}   count=1
moving       {0,8} count=2

static fingerprint = 0001/1
moving fingerprint = 0101/2
```

No F08.1 clock vocabulary is imported. In particular there is no `{0,4,8,12}`, `{0,6,10}`, or `{0,12}` policy, and no progression/genre/BPM/feel-specific clock table.

`phraseBarOrdinal` and `phraseHarmonicPosition` remain carried boundary coordinates only. W1R does not choose a phrase-wide static anchor law or moving concatenation law.

## Fixture roles

The old frozen W1 three-role separation is replayed unchanged:

1. **HISTORICAL PRE-F13** — provenance fixture only.
2. **FROZEN F13** — previous frozen baseline role.
3. **CURRENT ACCEPTED F08 GOLDEN** — accepted causal authority.

The accepted golden is neither regenerated nor reapproved by W1R.

## Validation

Focused host validation:

```bash
bash tests/run_0_9_9_phrase_w1r_tests.sh
```

It proves:

- exact H1-F1 `chord_progression.*` identity;
- exact old-W1 production-file replay;
- static/moving F08 fingerprints;
- ChordRhythm articulation independence;
- HarmonicRhythm event-count ownership;
- accepted F08 corpus replay;
- frozen H1 -> W1 corpus characterization;
- old W1 final -> W1R tonal dump exact parity;
- H1-F1 TwoFiveOne period 3 and ordinal-8 truth;
- deterministic repeat;
- GCC, Clang, ASan, and UBSan.

Normal exact-head validation must additionally pass Core host, SDL, Cardputer ADV, fixed DRAM, SEQTRAK MIDI-only, Stage15 baseline/materializer/integration/final acceptance, Tonal Projector, and global scale. Queued or running jobs are not PASS.

### Hardware list

No physical hardware is required for the focused replay gate. Cardputer ADV CI is a build/static-memory validation only; SEQTRAK CI is MIDI-only compile/regression validation.

### Wiring

None. No I2C, SPI, display, audio, or MIDI hardware wiring is used by the focused test.

### Build / flash

No firmware flashing is required for the focused gate. Run the command above on the host. Exact-head CI performs the normal Cardputer ADV build separately.

### Expected behavior

The focused runner prints `DECISION_B reproduced`, reports exact old-W1 tonal replay parity, reports `period=3` and `ordinal8=intrinsic-event-2`, and exits zero.

### Troubleshooting

- A `chord_progression.*` diff means H1-F1 was not preserved: stop; do not resolve by changing that owner.
- A tonal dump difference between old W1 and W1R means semantic drift: stop and report the exact corpus difference.
- Any F08.1 vocabulary or phrase-global accessor use in W1 production is a scope violation, not a test update opportunity.

## Memory

W1R adds no heap use and no persistent production allocation. H1-F1 bounded source representation remains unchanged. Cardputer fixed-DRAM evidence is static/linker evidence only and must not be described as runtime largest-free-block telemetry.

## Provenance

The old red P1R-T0 blocker remains immutable evidence that a finite `ChordProgressionPlan` cannot act as a phrase-global WHAT source for period-3 II-V-I. H1-F1 resolved that representation defect. W1R only replays W1 one-bar WHEN ownership above that corrected ancestry; it does not consume the new arbitrary-ordinal source API.

## Acceptance checklist

- [ ] branch base / merge-base is exact H1-F1 FINAL `ead3d7a...`;
- [ ] H1-F1 `chord_progression.*` byte-identical;
- [ ] W1 production delta exactly three owner files;
- [ ] old W1 production files replay exactly;
- [ ] static fingerprint `0001/1`;
- [ ] moving fingerprint `0101/2`;
- [ ] F08.1 absent;
- [ ] ChordRhythm remains articulation-only;
- [ ] HarmonicRhythm owns finite event cardinality;
- [ ] TwoFiveOne source period remains 3;
- [ ] source ordinal 8 resolves intrinsic event 2;
- [ ] W1 arbitrary source accessor consumption absent;
- [ ] old W1 tonal dump equals W1R tonal dump;
- [ ] focused GCC/repeat/Clang/ASan/UBSan green;
- [ ] full exact-head matrix terminal green;
- [ ] hardware listening not required;
- [ ] H2R not started;
- [ ] P1R not resumed.

## Decision

Successful replay must reproduce exactly:

```text
PHRASE-W1R
REPLAY COMPLETE

W1 SEMANTIC DECISION REPRODUCED:
B — PHRASE-WIDE HARMONIC CLOCK PROJECTION POLICY GAP

H1-F1:
UNCHANGED
```

Until the exact final head passes the full required matrix, this document is replay-candidate evidence only. On terminal green, freeze that exact W1R SHA and HARD STOP. The next legitimate checkpoint is PHRASE-H2R; it is not created here.
