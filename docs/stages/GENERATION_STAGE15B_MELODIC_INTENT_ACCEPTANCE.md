# Generation Stage 15B — Melodic Intent

## Purpose

Stage 15B adds bounded, deterministic **one-bar melodic intent** for Synth B.
It receives the current Stage 14 melodic candidate and may perform a small
rhythmic-intent transformation inside an explicit transient legal-step mask,
then assigns a scale-degree contour and a local pitch-domain motif operation.

Stage 15B owns only:

- one-bar melodic rhythm intent;
- one-bar pitch contour;
- bounded local motif operations.

It does **not** own physical Synth B arbitration, chord-first blocking,
full-groove diversity/history, Phrase state, bass behavior, tonal MIDI-note
projection, or multi-bar evolution.

Current reachability: **API-ONLY / host-testable**. Production migration wiring is
intentionally deferred until the moving Stage 14 materialization contract and a
transient tonal projection adapter are stable. Do not claim hardware musical
acceptance from this checkpoint.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3).
- USB-C cable for build/flash and serial.
- Built-in speaker is sufficient after production wiring.
- Optional Yamaha SEQTRAK for later MIDI audition.

## Wiring

No new wiring is introduced by Stage 15B.

- Cardputer-Adv is powered/programmed over USB-C.
- No new GPIO, I2C, SPI, or UART ownership.
- PORT.A remains GPIO2 SDA / GPIO1 SCL if external I2C hardware is connected;
  Stage 15B does not use that bus.
- No change to 3.3 V logic or existing board power assumptions.

## Build / Flash

Host gate for this isolated checkpoint:

```bash
bash tests/run_generation_stage15b_tests.sh
```

The gate compiles and executes with GCC, Clang when available, and ASan/UBSan.

There is no Stage 15B-specific firmware flash acceptance yet because this branch
has no production caller wiring. The repository-wide Cardputer-Adv and SEQTRAK
MIDI-only builds are still useful compile guards, but they do not make this
API-only feature hardware-reachable.

After Stage 14 stabilizes, the production integration branch must pass the
normal Cardputer-Adv builds before hardware audition.

## Expected behavior

Host tests must show:

```text
Generation Stage 15B source regressions: OK
Generation Stage 15B host matrix: OK
```

The API must:

- provide `Preserve`, `ControlledRest`, `ShiftInteriorEarlier`,
  `ShiftInteriorLater`, and `TerminalEcho` one-bar rhythm operations;
- keep all resulting onset/continuation cells inside caller-supplied
  `allowedSteps`;
- keep onset count at or below `maxOnsets`;
- remove or move a complete onset+continuation chain rather than leaving orphan
  continuation cells;
- permit an intentionally empty melodic bar only when `allowEmptyBar` is true;
- degrade an impossible rhythm preference to `Preserve` instead of violating
  the legal mask or density budget;
- produce deterministic scale-degree offsets for identical initial state;
- support Static, StepUp, StepDown, Arch, InvertedArch, LeapReturn, Neighbor,
  RepeatThenUp, and RepeatThenDown pitch contours;
- apply only one-bar pitch-domain motif operations;
- keep every degree inside the requested bounds and adjacent movement inside the
  requested maximum leap;
- use fixed-capacity storage with no heap allocation, global RNG, or unbounded
  retry loop.

`allowedSteps` is semantic melodic-intent space only. It is deliberately **not**
a replacement for Stage 14 physical Synth B availability: chord/melody blocking
and one-voice arbitration still happen downstream.

## Troubleshooting

If compilation fails in the Stage 15B test:

1. Confirm the branch is based on the current Stage 14 generation headers.
2. Confirm `src/generation/generation_context.cpp` is included in the host build.
3. Do not repair Stage 14 hybrid chord/melody arbitration inside Stage 15B.
4. Do not translate scale-degree offsets directly to arbitrary MIDI notes inside
   this role layer; production wiring needs the resolved current-bar tonal
   context.
5. If Stage 14 changes `MelodicMotifPlan`, adapt only the transient input adapter;
   do not add Phrase or voice-allocation ownership.

If a requested shift cannot fit inside `allowedSteps`, `Preserve` is the expected
fallback. If `TerminalEcho` cannot add a legal onset below `maxOnsets`,
`Preserve` is also expected.

If the source regression reports a forbidden owner such as `Scene`, `PhraseCore`,
full-groove fingerprint history, physical `SynthPattern`, heap containers, or
`StrongRhythmMigration`, the implementation has crossed the Stage 15B boundary.

## Acceptance checklist

- [ ] `bash tests/run_generation_stage15b_tests.sh` passes with GCC.
- [ ] Clang run passes when Clang is installed.
- [ ] ASan/UBSan run passes.
- [ ] `Preserve` leaves input onset and continuation masks unchanged.
- [ ] Controlled rests remove whole note chains with no orphan continuation.
- [ ] Shifts preserve note count and continuation-chain validity.
- [ ] Terminal echo never exceeds `maxOnsets` or leaves `allowedSteps`.
- [ ] Impossible rhythm operations deterministically fall back to `Preserve`.
- [ ] Empty melodic intent is produced only when the caller allows it.
- [ ] Identical initial state produces identical rhythm/contour/motif intent.
- [ ] AUTO reaches more than one legal rhythmic and pitch/motif result over the
      deterministic host matrix.
- [ ] Degree and maximum-leap bounds are never violated.
- [ ] No heap allocation, global RNG, or unbounded retry is introduced.
- [ ] No Scene, Phrase, 15A-history, bass, or physical voice-allocation ownership
      appears.
- [ ] Production tonal projection remains outside the role engine.
- [ ] Production reachability remains explicitly unclaimed until Stage 14 wiring
      is stabilized and separately reviewed.
- [ ] Hardware musical verdict remains **PENDING**.
