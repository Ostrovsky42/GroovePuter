# 0.9.9-PHRASE-W1R — W1 replay on finalized H1-F1

## Purpose

Replay the already frozen PHRASE-W1 harmonic-WHEN ownership semantics on the finalized H1-F1 ancestry. This checkpoint does not research or change harmonic policy. Its only successful outcome is semantic replay without drift while preserving H1-F1 byte-for-byte in its authoritative `chord_progression.*` owner.

## Frozen predecessors

- H1 frozen: `74456bcfec0fc74138ec0d8c652dde642c7e16b6`.
- H1-F1 FINAL: `eae498dc5b6377ddc4a45c2e62a7c33afab92e6c`.
- H1-F1 Decision A: **GLOBAL PROGRESSION SOURCE REPRESENTATION READY**.
- old W1 final: `34912cd050c04727c13533575b2cf999816e0549`.
- frozen old-W1 decision: **B — PHRASE-WIDE HARMONIC CLOCK PROJECTION POLICY GAP**.

W1R reproduces that W1 decision; it does not reconsider it.

## Ownership

```text
ChordRhythm
= physical chord articulation

HarmonicRhythm
= WHEN harmony advances inside one physical/semantic bar

ChordProgressionSource
= WHAT harmony advances to
```

Accepted W1 finite-consumer flow remains:

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

Those files replay the old W1 final state byte-for-byte. No new production owner is introduced.

## H1-F1 firewall

`src/generation/roles/chord_progression.h` and `src/generation/roles/chord_progression.cpp` must remain byte-identical to H1-F1 FINAL.

W1R does not call the H1-F1 arbitrary-ordinal accessor `chordProgressionEventAt()`. It also does not wire `ChordProgressionSource` into the W1 production path. Arbitrary phrase-global WHAT projection remains downstream work after H2R.

The H1-F1 ordinal-8 proof is already frozen H1-F1 evidence and is not re-executed as W1R wiring. W1R compatibility instead checks that TwoFiveOne remains intrinsic `period=3`, that intrinsic event 2 is stable, and that W1 finite cardinality remains owned by HarmonicRhythm.

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
- exact W1 production owner set;
- static/moving F08 fingerprints;
- ChordRhythm articulation independence;
- HarmonicRhythm event-count ownership;
- accepted F08 corpus replay;
- frozen H1 -> W1 corpus characterization;
- old W1 final -> W1R tonal dump exact parity;
- H1-F1 TwoFiveOne intrinsic period 3 and intrinsic-event-2 stability;
- arbitrary source accessor consumption is absent;
- deterministic repeat;
- GCC, Clang, ASan, and UBSan.

Normal exact-head validation must additionally pass Core host, SDL, Cardputer ADV, fixed DRAM, SEQTRAK MIDI-only, Stage15 baseline/materializer/integration/final acceptance, Tonal Projector, global scale, plus the focused H1-F1 gate when triggered. Queued or running jobs are not PASS.

## Hardware list

No physical hardware is required for the focused replay gate. Cardputer ADV CI is a build/static-memory validation only; SEQTRAK CI is MIDI-only compile/regression validation.

## Wiring

None. No I2C, SPI, display, audio, or MIDI hardware wiring is used by the focused test.

## Build / flash

No firmware flashing is required for the focused gate. Run the command above on the host. Exact-head CI performs the normal Cardputer ADV build separately.

## Expected behavior

The focused runner reports exact old-W1 tonal replay parity, prints `DECISION_B reproduced`, reports `period=3`, reports intrinsic-event-2 stability, reports `arbitrary source accessor consumed=NO`, and exits zero.

## Troubleshooting

- A `chord_progression.*` diff means H1-F1 was not preserved: stop; do not resolve by changing that owner.
- A production file differing from old W1 means replay drift: stop.
- A tonal dump difference between old W1 and W1R means semantic drift: stop and report the exact corpus difference.
- Any F08.1 vocabulary, phrase-global accessor consumption, H2R owner, or P1R wiring is a scope violation, not a test-update opportunity.

## Memory

W1R adds no heap use and no persistent production allocation. H1-F1 bounded source representation remains unchanged. Cardputer fixed-DRAM evidence is static/linker evidence only and must not be described as runtime largest-free-block telemetry.

## Acceptance checklist

- [ ] branch parent / merge-base is exact H1-F1 FINAL `eae498dc5b6377ddc4a45c2e62a7c33afab92e6c`;
- [ ] exactly one W1R replay commit above H1-F1 FINAL;
- [ ] H1-F1 `chord_progression.*` byte-identical;
- [ ] W1 production delta exactly three owner files;
- [ ] old W1 production files replay exactly;
- [ ] static fingerprint `0001/1`;
- [ ] moving fingerprint `0101/2`;
- [ ] F08.1 absent;
- [ ] ChordRhythm remains articulation-only;
- [ ] HarmonicRhythm owns finite event cardinality;
- [ ] TwoFiveOne source period remains 3;
- [ ] intrinsic event 2 remains stable;
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
