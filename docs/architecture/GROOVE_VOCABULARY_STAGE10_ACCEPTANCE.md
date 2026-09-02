# Generation Stage 10 — Chord and Harmonic Rhythm Acceptance

## Purpose

Separate chord placement/duration from progression, voicing and timbre while
keeping the normal GENRE materialization transaction atomic.

## Ownership

- `ChordRhythmPlan` owns onset, tied continuation and explicit release sites.
- Legacy Synth B supplies the progression/voicing event sequence.
- Feel may alter bounded event timing only.
- Derived ChordRhythm AUTO identity is transient; Scene persists the concrete
  resulting SynthPattern, including held/tied cells.

## Stable identities

`HELD PAD`, `WHOLE BAR HOLD`, `HALF BAR CHANGE`, `OFFBEAT STAB`,
`BACKBEAT STAB`, `ANTICIPATED CHANGE`, `SPARSE CHORD REPLY`,
`DUB CHORD SPACE`, and `SYNCOPATED COMP`.

## Acceptance

- changing the progression preserves ChordRhythm topology;
- held and half-bar material uses explicit continuation cells;
- protected Bass/Chord space removes illegal onset collisions;
- intentional empty Dub/sparse bars are valid;
- Stage 8 role-relative Feel reaches semantic chord onsets;
- Scene round-trip preserves a whole-bar tied chord;
- drums, Synth A and Synth B commit atomically;
- AUTO uses append-only `GenerationDomain::ChordRhythmSelection`;
- GCC, Clang and ASan/UBSan pass;
- hardware listening remains `HARDWARE_PENDING`.
