# GF2 G4-CC1A-P1-C1A — Connected Fixture + Lifetime Harness Discovery

## Scope

Discovery only. This checkpoint does not claim connected lifetime safety, does not run a new CI workflow, and does not change production semantics.

## Authoritative inspected state

```text
branch=feature/20260911-01-g4-cc1a-p1-acid-articulation-authority
inspected_sha=0cdfaa6107d56b7f9b78fcf627689e55ff360aaa
```

Fresh remote inspection matched the previously known SHA. The existing exact-head P1 workflow run at this SHA was used only as retained evidence; the 1152-row census was not rerun for C1A.

## Connected witness

```text
owner=Acid / BASE
recipe=0
identity=2
P-level=P1
archetype=405
bass_identity=10 (SUSTAIN/DROP)
onset_mask=0x8008
continuation_mask=0x7F07
predecessor_step=0
continuation_step=1
predecessor_note=MIDI 36
continuation_carrier=SynthStep(note=36, slide=true, accent=false, ghost=false)
fresh_onset_on_continuation=false
causal_predecessor_present=true
```

Repository StepMask order is MSB-first (`stepBit(0) == bit 15`). Therefore `0x8008` means onsets at logical steps 0 and 12; `0x7F07` means continuations at steps 1..7 and 13..15.

The exact-head P1 evidence names `BASE / P1 / identity=2` as the first connected witness and records `archetype=405`, `bass_identity=10`, `onset_mask=32776`, `continuation_mask=32519`.

Materialization provenance for the named pair is:

```text
Acid / BASE
-> resolveStrongRhythmFrozenSelection(... identity=2 ...)
-> selected bass identity 10 / SustainAndDrop
-> BassRhythmPlan.onsets=0x8008
-> BassRhythmPlan.continuations=0x7F07
-> BassPitchBehaviorPlan copies the same onset/continuation topology
-> tonal materialization produces the active predecessor note
-> adaptTonalPlanToSynthPattern copies the active SynthStep into continuation cells
   and marks the continuation carrier slide=true without creating a fresh onset
```

For the test context (`rootPitchClass=0`, Dorian, Acid bass corridor 24..47), the deterministic first onset projects to MIDI 36. Step 1 copies that active step and forces `slide=true`; it is a continuation carrier, not an independent onset.

## Historical lifetime harness

```text
file=tests/test_pattern_phrase_p2_lifecycle_barriers.cpp
primary_helper=expectTargetBarrier(...)
fixture_entry=startPattern(...)
source_switch_case=caseSequencedSourceTransfer(...)
reverse_case=casePhraseToPatternSequencedSourceTransfer(...)
make_phrase_case=caseMakePhraseTransfer(...)
runtime_seam=MiniAcid::setSequencedSource -> hardBarrierPatternPlayback_(voice) -> RuntimeSynthPlaybackState::hardBarrier -> consumePatternPlaybackActions_
preferred_transition=PATTERN -> PHRASE
```

This is the correct historical ownership boundary because it directly exercises the old sequenced source lifetime seam, checks target-scoped release rather than panic cleanup, observes the physical Synth voice, and checks the Pattern ownership bit after the transition.

The current helper activates a synthetic single RuntimeSynthEvent. It does not inject a materialized connected Acid pattern.

## Existing coverage map

| Invariant | Existing test / function | Coverage |
| --- | --- | --- |
| source switch | `caseSequencedSourceTransfer`, `casePhraseToPatternSequencedSourceTransfer` | YES |
| targeted NoteOff | `expectTargetBarrier`; reverse source-transfer case | YES |
| old ownership clear | `expectTargetBarrier` / `patternOwnsInternalSynth` | YES |
| stale continuation invalidation | no source-switch test advances a materialized connected continuation after the switch | NO |
| stuck-note protection | `expectTargetBarrier` / `noteHeld` | YES |
| physical held note preserved | no unrelated physical held-note preservation case in the source-transfer fixture | NO |
| monophonic ownership | `testReplacingOnsetReleasesBeforeStart` in `test_pattern_phrase_p2_runtime_playback.cpp` proves one RuntimeSynthPlaybackState releases before replacement | YES (generic) |
| connected continuation case | lifecycle source-transfer fixture uses a synthetic fresh onset, not a connected materialized Acid note | NO |

A separate P0 runtime fixture has generation-aware dispatch and Song-boundary cleanup, but it is not the minimal historical `setSequencedSource` ownership seam for this checkpoint.

## C1B insertion point

```text
file=tests/test_pattern_phrase_p2_lifecycle_barriers.cpp
helper/area=startPattern + caseSequencedSourceTransfer
runtime seam=MiniAcid::setSequencedSource(synth, SequencedSource::Phrase)
transition=PATTERN -> PHRASE
```

Minimal C1B shape: add a fixture helper that materializes the named `Acid / BASE / P1 / identity=2` Synth-A pattern (or projects that already-proven SynthPattern into the existing RuntimeSynthPlaybackState), starts its step-0 lifetime, performs the existing PATTERN->PHRASE transition before step 1, then advances far enough to prove the old step-1 continuation cannot reassert sound/ownership.

Do not replace `expectTargetBarrier`; reuse its targeted NoteOff, voice, and ownership assertions. Add only the connected fixture setup and the post-switch stale-continuation observation/negative control required by C1B.

## Outcome

```text
HARNESS_REUSABLE_CONNECTED_FIXTURE_MISSING
```

The fixture and historical boundary are both known. Lifetime safety is deliberately NOT assessed in C1A.

## Production changes

```text
NONE in C1A
```
