# Generation Stage 9 — Bass Rhythm Vocabulary v2 Acceptance

## Purpose

Separate bass onset identity from pitch contour and synth articulation on the
normal `GENRE -> MATERIALIZE` path.

## Ownership

- `BassRhythmPlan` owns only bass onsets, tied continuation cells and the
  declared Kick relationship.
- The existing legacy generator remains the pitch, velocity and articulation
  source during migration.
- `SemanticPatternProjector` cannot invent pitch or choose Synth A/B; its caller
  supplies the explicit destination.
- Scene stores no derived BassRhythm selection. AUTO is reproducible from Genre,
  selected rhythm identity and generation context.

## Stable identities

`ROOT PULSE`, `KICK LOCK`, `KICK ANSWER`, `GAP FILL`, `OFFBEAT PUSH`,
`SPARSE ANCHOR`, `ROLLING DRIVE`, `HALF-TIME POCKET`, `SYNCOPATED HOOK`, and
`SUSTAIN/DROP`.

## Acceptance

- the same Kick mask realizes multiple distinct bass identities;
- an explicit bass identity is reusable across rhythm families;
- changing the pitch source preserves the BassRhythm onset identity;
- sparse profiles may produce an intentionally empty bar;
- held/tied cells never create a new pitch decision;
- projection failure leaves drums and both synth patterns unchanged;
- AUTO uses append-only `GenerationDomain::BassRhythmSelection`;
- GCC, Clang and ASan/UBSan host matrices pass;
- physical Cardputer listening remains `HARDWARE_PENDING`.
