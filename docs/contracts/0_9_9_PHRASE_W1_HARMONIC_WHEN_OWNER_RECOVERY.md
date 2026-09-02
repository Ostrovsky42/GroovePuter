# 0.9.9-PHRASE-W1 — Harmonic WHEN Owner Recovery

## Purpose

Recover the already hardware-accepted 0.9.9-F08 one-bar HarmonicRhythm owner into the frozen PHRASE-H1 lineage without importing F08.1 vocabulary or inventing a phrase-wide harmonic clock law.

W1 starts exactly from H1 `74456bcfec0fc74138ec0d8c652dde642c7e16b6`. PHRASE-P1 #388 is blocker evidence only and is not ancestry.

## Authoritative inputs

- PHRASE-H1 #387: `74456bcfec0fc74138ec0d8c652dde642c7e16b6`
- accepted F08 #343: `cfb1f9a8e214cfcb823a5e75445f26356b55bed6`
- F08 exact pre-base: `78bc8394ede5e6d81464cff5878c29bbf754c555`
- excluded F08.1 #362: `ee0fa06e6db0c78f84e85e6d2736db21268d590d`
- blocked P1 #388: `f05c0c90e0350589429d7554143967ddc8029aca` (evidence only)

The F08 and H1 heads diverge at `78bc8394ede5e6d81464cff5878c29bbf754c555`; W1 recovers only the F08 semantic owner, not branch history.

## Hardware list

- Cardputer ADV: the original F08 owner was hardware accepted on F08.
- No new physical run is performed by W1.
- W1 does **not** claim phrase-wide behavior is hardware accepted.
- Hardware-acceptance inheritance for the recovered one-bar path is asserted only after exact semantic/output equivalence is demonstrated by the final W1 evidence. Until then it remains pending evidence, not an assumption.

## Wiring

None. W1 is generation semantics only.

## Ownership

Frozen ownership remains:

- `ChordRhythm` = physical attack / continuation / release topology.
- `HarmonicRhythm` = WHEN harmonic state advances and harmonic event cardinality.
- `ChordProgression` = WHAT harmonic values are traversed.
- `PhraseHarmonicTimeline` = phrase-wide representation of WHEN coordinates; it does not select a clock.
- H1 frozen progression source = one WHAT source per logical phrase.

Recovered one-bar data flow:

```text
ProgressionId
    -> HarmonicRhythm
         -> eventCount -> ChordProgression
         -> onsets     -> TonalMaterializer harmonicEventOnsets

ChordRhythm
    -> physical role onsets/continuations
    -> TonalMaterializer role articulation
```

The forbidden coupling is removed:

```cpp
progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);
```

## F08 recovery delta

### Exact accepted F08 production semantic delta

F08 added `src/generation/roles/harmonic_rhythm.h` and changed the StrongRhythm integration so that:

1. HarmonicRhythm is realized from progression identity independently of ChordRhythm.
2. `ChordProgressionRequest::harmonicEventCount` comes from `harmonic.plan.eventCount`.
3. TonalMaterializer harmonic timing comes from `harmonic.plan.onsets`.
4. physical chord articulation remains `chord.plan.onsets` / continuations.
5. StrongRhythm result exposes harmonic status, onsets, and count.

### H1 equivalents / conflicts

C1/H1 introduced newer phrase identity, frozen selection, phrase coordinates, and H1 progression-source contracts. Those are preserved. W1 does not transplant the historical F08 StrongRhythm file; it applies only the owner separation to the newer H1 integration points.

The recovered production files are bounded to:

- `src/generation/roles/harmonic_rhythm.h`
- `src/generation/migration/strong_rhythm_migration.h`
- `src/generation/migration/strong_rhythm_migration.cpp`

No unrelated StrongRhythm cleanup is included.

## Accepted F08 bootstrap

Default one-bar production behavior remains exactly:

```text
StaticModal -> {0}, eventCount=1
PedalDrone  -> {0}, eventCount=1
moving progression identities -> {0,8}, eventCount=2
```

The canonical W1 semantic fingerprint is:

```text
static:0001/1;moving:0101/2
```

where masks are 16-step hexadecimal StepMask values.

Accepted F08 causal corpus authority remains historical evidence:

```text
rows=256
changed=93
topology changes=0
articulation changes=0
pitch changes=93
static changed=0/102
accepted generated SHA-256=bbc1544bf289c7ef7f062997bde3f0b8dae3a317ace54b0998cef6649872ac3f
```

The current H1 Stage15 golden is not byte-identical to the F08 golden (different repository-era fixture/corpus state). W1 therefore does not blindly transplant the 256-row F08 file and does not create a new musical golden. Current-H1 compatibility is evaluated by normal regressions plus focused owner invariants; exact hardware inheritance is only stated if final evidence establishes equivalence on a replayable/overlapping domain.

## F08 vs F08.1 boundary

F08.1 is not imported. W1 does not add progression-specific default clocks such as quarter-cycle, `{0,6,10}`, `{0,12}`, or other named clock tables. There is no genre, feel, BPM, ChordRhythm, pattern address, Song, storage, or transport owner in `HarmonicRhythmRequest`.

The existing F08 explicit bounded `harmonicEventCount` API remains because it is part of the accepted F08 owner boundary; W1 does not use it to create new production clock vocabulary.

## Phrase projection audit

### Evidence from accepted F08

F08 explicitly says `phraseBarOrdinal` and `phraseHarmonicPosition` are carried coordinates only. F08 does **not** create a phrase scheduler, cross-bar harmonic state, lifecycle, or phrase-wide clock policy.

### Static multi-bar case

Accepted one-bar F08 gives `{0}` for every independent static request, but existing accepted contracts do not answer whether a logical multi-bar phrase means:

- a harmonic event at the start of every semantic bar, or
- one phrase-start harmonic event with unchanged state across later bars.

The carried `phraseBarOrdinal` does not choose between those interpretations.

Result: **AMBIGUOUS**.

### Moving multi-bar case

Accepted one-bar F08 gives `{0,8}` for every independent moving request, but existing accepted contracts do not state that the one-bar clock restarts at every semantic phrase bar. Repeating `{0,8}` per bar would therefore be a new cross-bar clock/evolution law unless separately frozen.

Result: **AMBIGUOUS**.

### Consequence

W1 does not populate C1 `PhraseHarmonicTimeline` from repeated one-bar plans. No projection implementation is added. C1 remains representation-only.

Phrase lengths 1/2/4/8 remain valid C1 phrase lengths, but W1 does not invent their harmonic clock evolution. The H1 32-position fixture remains synthetic capacity evidence only; accepted F08 moving bootstrap remains two positions per one-bar plan.

REST-heavy melodic emptiness is not an owner of harmonic time, but because phrase projection itself is unresolved W1 does not manufacture a REST-heavy phrase timeline.

H1 still requires one progression WHAT source per logical phrase. W1 does not select a progression source per bar.

## Build / validation

Focused:

```bash
bash tests/run_0_9_9_phrase_w1_tests.sh
```

The runner executes:

- syntax-aware source firewall
- GCC
- deterministic GCC repeat
- Clang parity when available
- ASan
- UBSan

Normal matrix remains required on final head:

- Core host
- SDL
- Cardputer ADV
- fixed DRAM
- SEQTRAK MIDI-only
- inherited C1/H1 contracts

## Expected behavior

Focused output includes concrete values:

```text
F08 owner recovered YES
static mask=0001 count=1
moving mask=0101 count=2
ChordRhythm independence=YES
static multibar interpretation=AMBIGUOUS
moving multibar interpretation=AMBIGUOUS
F08.1 imported=NO
PHRASE-W1 DECISION_B
```

No phrase projection is expected.

## Troubleshooting

- If `progressionRequest.harmonicEventCount` is derived from chord onsets, ownership recovery failed.
- If tonal harmonic timing receives `chord.plan.onsets`, ownership recovery failed.
- If a progression-specific clock table appears, F08.1 leaked into W1.
- If normal Stage15 exact-golden regression fails, do not regenerate a W1 musical golden. First classify whether the current H1 corpus is directly replayable against exact F08 authority.
- If static or moving multi-bar behavior becomes asserted without a previously accepted owner, remove the guessed projection and keep Decision B.

## Provenance / supersession

| Frozen claim | W1 status | New / recovered owner | Evidence |
| --- | --- | --- | --- |
| P1 #388: production harmonic WHEN owner absent in H1 ancestry | SUPERSEDED for one-bar production only | recovered F08 HarmonicRhythm | independent request + StrongRhythm wiring |
| M3-A1: ChordRhythm currently acts as timing input | SUPERSEDED for one-bar production | recovered F08 separation | harmonic event count/onsets no longer derived from chord articulation |
| C1: PhraseHarmonicTimeline is representation-only | STILL TRUE | C1 | W1 adds no clock selection to timeline |
| H1: ChordProgression owns WHAT | STILL TRUE | H1 frozen progression source | HarmonicRhythm only supplies count/WHEN |
| F08: independent HarmonicRhythm ownership | RECOVERED | HarmonicRhythm | static/moving bootstrap and integration |
| F08.1: expanded clock vocabulary | NOT IMPORTED | none | source firewall |

The P1 #388 blocker is only **partially resolved**: the missing one-bar production WHEN owner is recovered, while phrase-wide projection remains unresolved. #388 itself is immutable and unmodified.

## Memory

`HarmonicRhythmRequest` and `HarmonicRhythmPlan` are trivially copyable fixed-capacity values and remain guarded at `<= 8 B` each.

W1 adds no heap, dynamic container, resident phrase scheduler, or phrase timeline cache. The only StrongRhythm result additions are bounded status/onset/count fields recovered from F08. Final Cardputer fixed-DRAM figures are recorded from exact-head CI; static linker remaining DRAM is not reported as runtime largest-free-block memory.

## Acceptance checklist

- [x] exact H1 base used
- [x] F08 semantic owner recovered without F08 merge
- [x] ChordRhythm remains physical-only
- [x] HarmonicRhythm independently owns one-bar WHEN/cardinality
- [x] ChordProgression remains WHAT
- [x] accepted `{0}` / `{0,8}` bootstrap preserved
- [x] F08.1 not imported
- [x] no genre/BPM/feel/storage/transport harmonic owner
- [x] no Song/UI/storage/lifetime work
- [x] static multi-bar ambiguity explicitly identified
- [x] moving multi-bar ambiguity explicitly identified
- [x] no guessed PhraseHarmonicTimeline projection added
- [x] 32-position fixture remains synthetic-only
- [ ] exact-head focused CI terminal green
- [ ] normal matrix terminal green
- [ ] final memory figures recorded
- [ ] hardware-acceptance inheritance asserted only if equivalence evidence permits it

## Decision

**DECISION B — PHRASE-WIDE HARMONIC CLOCK PROJECTION POLICY GAP**

The accepted F08 independent one-bar HarmonicRhythm owner is cleanly recovered into H1-compatible production. Existing accepted F08 + C1/H1 contracts do not choose static or moving clock behavior across semantic phrase boundaries. W1 therefore stops without a phrase projection law.

Do not resume P1, start P1R, C2, I2, Song publication, UI, MIDI/internal synth lifetime, hardware phrase listening, or F08.1 from this checkpoint.
