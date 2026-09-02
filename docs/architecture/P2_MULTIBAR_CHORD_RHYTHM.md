# P2 — Bounded Multi-Bar ChordRhythm

**Base:** `fc42763e7798866e61895bf1b8d62339ec59e0a7`  
**Scope:** capability prototype only; no H6 vocabulary or live migration

## Question

Can the existing ChordRhythm owner preserve its complete gate topology across up to four 16-step bars without confusing rhythmic articulations with harmonic progression events?

## Current Stage15 fact

The production one-bar `ChordRhythmPlan` already has three independent masks:

```text
onsets
continuations
releasePoints
```

This matters. `HeldPad` has an explicit release point, while stab profiles have onsets with no continuation mask. Therefore P2 must not derive holds from onset topology alone.

An early P2 prototype did exactly that and was rejected before freeze because it could turn a stab into a held pad.

## Corrected contract

```text
ChordRhythmTimelineRequest
  barCount 1..4
  onsets          uint64 timeline mask
  continuations   uint64 timeline mask
  releasePoints   uint64 timeline mask

        -> validation/preservation ->

ChordRhythmTimelinePlan
  barCount
  stepCount <= 64
  onsetCount <= 64
  onsets
  continuations
  releasePoints
```

Logical timeline step 0 maps to bit 63 and step 63 maps to bit 0, preserving the existing MSB-first StepMask convention.

P2 does not synthesize continuation or release policy. It validates and preserves the already-owned ChordRhythm gate semantics.

## Topology validation

The three masks must be disjoint and inside the active phrase. A continuation requires an immediately active gate. A release point requires an immediately active gate and closes it. A logical step with no continuation ends a previous gate, so a later continuation after a gap is invalid.

This permits an explicitly declared continuation to cross a bar boundary, but a bar boundary by itself never creates a hold or release.

An empty timeline is `ValidButEmpty`.

## Ownership

A rhythm onset is not automatically a new progression event. P2 contains no progression-event array, degree, chord quality, absolute MIDI, synth destination, genre routing or Scene state.

A four-bar timeline may therefore contain more than the current harmonic event capacity. The acceptance matrix deliberately uses 16 stab onsets across four bars while leaving progression capacity untouched.

P2 does not define same-chord retrigger semantics. P3 remains separate.

## Bounds

```text
bars          1..4
steps         16..64
mask storage  3 x uint64_t
heap          none
runtime alloc none
```

Requests with barCount 0/>4, active bits outside the phrase, overlapping semantic masks, orphan continuations or orphan releases are rejected.

## Explicit exclusions

- no H6 rhythm masks/candidates;
- no H6 harmonic progressions;
- no retrigger action;
- no change to harmonic event capacity;
- no ChordProgression edits;
- no Genre/Scene/P-level routing;
- no synth/MIDI output;
- no live Stage15 migration wiring.

A later integration PR may compose P2 with P3 only after both are independently frozen.

## Acceptance

Host gates cover current HeldPad-style release behavior, stab/no-hold behavior, explicitly declared cross-bar continuation, four-bar maximum, >8 rhythm onsets, invalid bounds/overlap, orphan gate events, empty phrases and StepMask conversion.

ESP32-S3 normal and midi-only builds measure a linker-retained P2 probe while proving ordinary product builds dead-strip the isolated capability. Static measurements do not claim future live runtime CPU/heap behavior.
