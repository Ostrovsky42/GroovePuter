# 0.9.9-M2-A1 — Cross-bar note lifetime contract audit

Status: **REASONING COMPLETE / RUNTIME EXECUTION PENDING**

Frozen base: `5ad44bb9400ea38d349b7f815f84f833fb18ce6a`

Branch: `research/20260826-07-0.9.9-m2-a1-crossbar-note-lifetime`

Scope: research / characterization only. No production note-lifetime changes.

## Purpose

M1 established one stable logical melodic phrase across multiple physical Synth bars. M2 asks which representation and runtime owner can keep one already-started note alive across a physical pattern boundary.

## Current lifecycle

```text
MelodicMotifPlan
  onsets + bar-local continuations
        |
        v
TonalMaterializer / tonal_pattern_adapter
        |
        v
SynthPattern / SynthStep
        |
        v
MiniAcid PatternPlayer
  triggerSynthStep_()
  gateCountdownA_/B_
  direct internal synth voice
        |
        +----------------------+
        |                      |
        v                      v
 internal voice          MusicalEventQueue
                                |
                                v
                         MidiDispatchTask
                                |
                                v
                          UsbMidiOutput
```

`SynthStep` currently contains note, slide, accent, ghost, velocity, timing, fx, fxParam and probability. It has no explicit duration/tie/hold/cross-bar lifetime field.

## Representation capability

### Semantic continuation

Current melodic continuation topology is bar-local. Validation starts each semantic plan without an already-active note, therefore a continuation at the beginning of a new physical bar is not a legal way to refer to a note started in the previous bar.

The tonal adapter materializes a valid local continuation by copying the current pitched `SynthStep` and setting `slide=true`. This remains another pitched physical step and therefore another PatternPlayer onset; it is not a lifetime token.

### Legacy `note == -2` sentinel

MiniAcid contains a legacy playback/editor `-2 // TIE` branch and the older generator has a tie mask. This is **not** the M2 representation baseline:

- it has no corresponding explicit current melodic semantic contract;
- the tonal adapter does not emit it;
- playback accepts it only while an existing gate countdown is still positive;
- ordinary production gate length is capped below one full sequencer step;
- it has no independently specified cross-pattern ownership or MIDI/internal parity.

M2-P1 must not adopt or extend `-2` as the authoritative cross-bar representation merely because that compatibility branch exists. Legacy behavior may remain untouched until explicitly migrated.

## Boundary characterization

`tests/test_m2_crossbar_note_lifetime_contract.py` is the deterministic source guard.

`tests/test_m2_crossbar_note_lifetime_runtime.cpp` is the runtime characterization. It links the real SDL MiniAcid implementation, uses the existing `MusicalEventQueue` and `UsbMidiOutput`, and opens private engine state only inside the research translation unit for observation. It does not add a production probe or seam.

The runtime runner is `tests/run_m2_a1_crossbar_note_lifetime.sh` and requires identical canonical A-H output under GCC and Clang plus ASan/UBSan-clean execution.

Until that runner has actually executed successfully, the table below is the audit's **expected current behavior**, not frozen runtime evidence.

| Case | Scenario | Expected current internal path | Expected current MIDI path | Genuine cross-bar hold? |
| --- | --- | --- | --- | --- |
| A | note starts/ends inside one bar | gate countdown releases voice | NoteOn then NoteOff | No |
| B | bar0 step15 onset, bar1 empty | ordinary gate expires before next bar | NoteOn then NoteOff | No |
| C | bar0 step15, bar1 step0 same pitch | old gate expires; new startNote | NoteOff then new NoteOn | No, retrigger |
| D | bar0 step15, bar1 step0 different pitch | old gate expires; new startNote | NoteOff then new NoteOn | No, replacement/retrigger |
| E | slide at boundary | slide is articulation on new startNote | new NoteOn lifecycle | No |
| F | Stop while note active | gate cleared and voice released | PatternPlayer AllNotesOff cleanup | Must not survive |
| G | explicit pattern switch while active | switch does not directly release internal voice | PatternPlayer NoteOff cleanup | Paths diverge |
| H | Song physical-pattern advance while active | selection does not directly release internal voice | target-scoped PatternPlayer AllNotesOff | Paths diverge |

## Internal synth vs MIDI ownership

PatternPlayer internal audio is rendered directly by MiniAcid. `InternalSynthOutput` deliberately ignores `MusicalEventSource::PatternPlayer` fan-out to avoid a second internal trigger owner.

PatternPlayer MIDI uses the event queue and `UsbMidiOutput`. Song selection publishes target lifecycle barriers before changing physical Synth selection; explicit pattern switch publishes PatternPlayer NoteOff.

Therefore M2 must not "fix" parity by simply routing PatternPlayer events back through `InternalSynthOutput`. The single lifetime decision should remain at sequencer/playback ownership while internal audio and MIDI remain separate execution mechanisms.

## Frozen M2/M3 ownership boundary

```text
M2 owns:
  whether an already-started note remains alive

M3 owns:
  which harmonic source is active at phrase time

M3 does NOT:
  retrigger or retune a held note merely because harmony changed

M2 does NOT:
  advance or select harmonic source
```

Pitch/harmonic projection is decided at note onset. If harmony changes while M2 keeps a note alive, that already-started note remains alive without an M3-forced NoteOn or retune. The next new melodic onset uses the then-current M3 harmonic source.

Future joint M2+M3 integration acceptance must include:

```text
bar0 onset under harmony A
harmony changes to B while note is held
held note continues unchanged
next new melodic onset uses harmony B
```

## Stop invariant

Stop is the negative lifetime invariant for M2:

```text
After Stop:
  internal active lifetime = none
  gate countdown = 0
  MIDI PatternPlayer ownership = none
  pending cross-bar carry = none
```

M2-P1 must preserve this as a regression guard.

## Owner decision

The future cross-bar lifetime **decision owner should be the sequencer / PatternPlayer playback lifecycle**, because that layer already owns gate countdown, physical pattern transitions, direct internal voice execution and PatternPlayer event publication.

Semantic/tonal layers must provide enough bounded representation to request a hold, but they must not independently own output lifetime. MIDI dispatcher remains an executor/cleanup owner, not the musical lifetime policy owner.

## Decision

**DECISION B — REPRESENTATION GAP**, pending runtime execution closure.

Static evidence already establishes that the current semantic + `SynthStep` path cannot express an authoritative genuine cross-bar hold. The runtime characterization is required before freezing the exact G/H internal-vs-MIDI boundary behavior as executed evidence.

This is not Decision C: playback divergence is important, but even a perfect playback owner would still lack a current semantic/physical representation for an explicit cross-bar lifetime request.

## Smallest safe next production direction

Do not implement it in this audit.

M2-T1/M2-P1 should introduce the smallest bounded explicit lifetime representation that can distinguish onset, continuation/hold and release across physical bars, while preserving:

- sequencer as the single lifetime decision owner;
- separate internal and MIDI execution mechanisms;
- dual-engine parity at pattern/Song transition;
- Stop as a hard barrier;
- legacy `-2` compatibility without making it authoritative;
- no harmonic-source ownership in M2.

A transition with an active held note must produce the same musical decision on internal and MIDI paths: **CONTINUE / RELEASE / RETRIGGER**. Any disagreement is a failure even if neither side leaves an orphan note.

## Execution closure

Required command:

```bash
bash tests/run_m2_a1_crossbar_note_lifetime.sh
```

The runner performs:

1. source characterization twice and exact diff;
2. runtime A-H with GCC;
3. runtime A-H with Clang;
4. exact canonical GCC/Clang comparison;
5. GCC ASan+UBSan runtime;
6. sanitizer diagnostic rejection.

Only after this command reports `M2-A1 execution closure: PASS` may this document change to **CLOSED** and Decision B become frozen executed evidence.
