# Generation Stage 9 — Bass Rhythm Vocabulary v2 Acceptance

Status: `PARTIAL` — BassRhythm is implemented; BassPitchContour and
BassArticulation are deferred.

## Purpose

Implement the BassRhythm axis on the normal `GENRE -> MATERIALIZE` path while
keeping it independent from pitch contour and synth articulation.

The Stage 9 roadmap target contains three independent axes:

```text
BassRhythm + BassPitchContour + BassArticulation
```

This implementation delivers only `BassRhythm`. It projects pitch, velocity,
accent and slide from the legacy Synth A source. That is a deliberate migration
boundary, not an implementation of `BassPitchContour` or
`BassArticulation`.

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

## Deferred completion work

- define a pitch-contour vocabulary without coupling contour to Genre or
  BassRhythm;
- define an articulation policy for slide/accent/gate behavior;
- bind both axes transactionally without changing Synth A destination or
  Scene ownership;
- run a dedicated Acid-character hardware audition after all three axes are
  reachable.

Until then, Stage 9 must not be reported as complete and the roadmap's Acid
character acceptance target is not claimable.
