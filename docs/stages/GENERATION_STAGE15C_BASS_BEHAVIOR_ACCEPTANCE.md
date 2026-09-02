# Generation Stage 15C — Bass Pitch Behavior + Articulation

## Purpose

Stage 15C adds bounded, deterministic one-bar bass tonal behavior and
engine-neutral articulation intent for Synth A. It consumes an already-resolved
bass rhythm topology plus a transient `BassBehaviorPolicy` and never changes
where bass onsets or continuations occur.

Genre/Variant/composition owns policy resolution. Stage 15C contains no hidden
Genre or `RhythmFamily` routing. It does not claim Stage 9 rhythm ownership,
physical synth-engine ownership, Phrase state, Scene state, persistence, or
multi-bar bass evolution.

Current reachability: **API-ONLY / host-testable**. This current-base rebuild sits
on the verified Tonal Projector checkpoint `f170584a`. The shared projector is
therefore available to the later Stage 15 integration layer, but this role engine
does not call it directly. Production wiring is intentionally deferred to the
single Stage 15 integration path, which will provide harmonic root / `ScaleType`
/ register context and project the role's tagged tonal intent through the shared
Tonal Projector. Hardware musical acceptance is not claimed here.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3).
- USB-C cable for build/flash and serial.
- Built-in speaker is sufficient after production wiring.
- Optional Yamaha SEQTRAK for later MIDI audition.

## Wiring

No new wiring is introduced by Stage 15C.

- Cardputer-Adv is powered/programmed over USB-C.
- No new GPIO, I2C, SPI, or UART ownership.
- PORT.A remains GPIO2 SDA / GPIO1 SCL if external I2C hardware is connected;
  Stage 15C does not use that bus.
- No change to 3.3 V logic or existing board power assumptions.

## Build / Flash

Host gate for this isolated checkpoint:

```bash
bash tests/run_generation_stage15c_tests.sh
```

The gate compiles and executes with GCC, Clang when available, and ASan/UBSan.

There is no Stage 15C-specific firmware flash acceptance yet because this branch
has no production caller wiring. Repository-wide Cardputer-Adv, fixed-DRAM,
SEQTRAK MIDI-only, and SDL jobs are compile/regression guards only; they do not
make this API-only feature hardware-reachable.

The later Stage 15 integration branch must pass the normal host, Tonal Projector,
SDL, Cardputer-Adv fixed-DRAM, and SEQTRAK MIDI-only gates before hardware
audition.

## Expected behavior

Host tests must show:

```text
Generation Stage 15C source regressions: OK
Generation Stage 15C host matrix: OK
```

The API must:

- default to the conservative vocabulary `RootAnchor / Plain` for every seed;
- require explicit composition-layer opt-in before wider contour/articulation
  vocabulary is reachable;
- reject preferred masks that escape their corresponding allowed masks;
- reject explicitly requested vocabulary that policy forbids;
- preserve the input bass onset and continuation masks exactly;
- emit a fixed-capacity tagged tonal representation:

```cpp
int8_t tonalOffsets[kStepsPerBar]{};
uint16_t semitoneOffsetOrdinals = 0;
```

- interpret bit N of `semitoneOffsetOrdinals` as the unit tag for
  `tonalOffsets[N]`: set = semitones, clear = scale degrees;
- encode `RootFifth` as tagged `+7` semitones;
- encode `RootOctave` as tagged `+12` semitones;
- keep `NeighborReturn` and `StepApproach` as untagged scale-degree intent;
- allow mixed tagged vocabulary such as `RootFifthNeighbor` without ambiguous
  units;
- apply `minDegreeOffset`, `maxDegreeOffset`, and `maxLeapDegrees` only to
  untagged scale-degree entries;
- never compare a scale-degree entry directly with a semitone-tagged entry for
  `maxLeapDegrees`; the common musical leap is checked only after the shared
  Tonal Projector resolves both to absolute MIDI notes in the integration layer;
- emit accent and slide masks as strict subsets of real bass onsets;
- emit slide intent only when existing timing already connects the previous
  onset to the destination directly or through uninterrupted continuations;
- never create or extend a continuation merely to realize a slide;
- emit semantic tagged tonal intent only; it must not choose `ScaleType`, a MIDI
  register, an absolute MIDI note, or a physical synth;
- use fixed-capacity storage with no heap allocation, global RNG, or unbounded
  retry loop.

`tonalOffsets` are not final MIDI notes. The later Stage 15 integration adapter
reads the current transient root, `ScaleType`, register bounds, values, and unit
tags, constructs `TonalProjectionRequest`, and asks the already-existing shared
Tonal Projector for absolute MIDI notes with an explicit status. The projector
uses the real scale cardinality (5, 7, or 12).

Stage 15C does not own that projector and does not gain Genre, rhythm, voice,
Scene, Phrase, persistence, scale mapping, register, or synth ownership through
it.

## Troubleshooting

If compilation fails in the Stage 15C test:

1. Confirm the branch is based on the current Tonal Projector checkpoint
   `f170584a` (or its reviewed descendant), not the old `c8cd061` Stage 14 line.
2. Confirm `request.family` is absent from `bass_pitch_behavior.*` and tests.
3. Confirm `selectContour()` and `selectArticulation()` use policy masks only.
4. Confirm the default policy allows only `RootAnchor` and `Plain`.
5. Confirm `RootFifth` is tagged `+7` semitones and `RootOctave` is tagged `+12`.
6. Confirm neighbor/approach values remain scale-degree intent with their tag
   bits clear.
7. Do not apply `maxLeapDegrees` across mixed-unit adjacent entries.
8. Do not move bass onset generation from `bass_rhythm.*` into Stage 15C.
9. Do not map semantic articulation directly to TB303/SID/AY internals here.
10. Do not create/extend ties or gates to make a requested slide possible.
11. Do not choose `ScaleType`, register bounds, absolute MIDI notes, projector
    calls, or physical synth types inside the Bass role. Those belong to the
    Stage 15 integration adapter and downstream allocation/materialization.

If the source regression reports `Scene`, `PhraseCore`, `StrongRhythmMigration`,
`SynthPattern`, melodic ownership, `request.family`, `ScaleType`, Tonal Projector
request/result calls, MIDI register/note fields, physical synth types, heap
containers, or global randomness, the implementation has crossed the Stage 15C
boundary.

## Acceptance checklist

- [ ] `bash tests/run_generation_stage15c_tests.sh` passes with GCC.
- [ ] Clang run passes when Clang is installed.
- [ ] ASan/UBSan run passes.
- [ ] Default AUTO remains `RootAnchor / Plain` across the seed sweep.
- [ ] `BassBehaviorPolicy` is fixed-capacity and transient.
- [ ] No `request.family` or hidden Genre/Variant routing exists in Stage 15C.
- [ ] Input bass onset mask equals output onset mask.
- [ ] Input continuation mask equals output continuation mask.
- [ ] `RootFifth` yields tagged `+7` semitone intent.
- [ ] `RootOctave` yields tagged `+12` semitone intent.
- [ ] `NeighborReturn` and `StepApproach` remain untagged scale-degree intent.
- [ ] Mixed unit entries never use `maxLeapDegrees` across the unit boundary.
- [ ] Degree-only bounds remain enforced for degree-only adjacency.
- [ ] Accent and slide masks never contain a non-onset cell.
- [ ] Slide intent is absent across an existing empty timing gap.
- [ ] No onset/continuation is created or moved by Stage 15C.
- [ ] No heap allocation, global RNG, or unbounded retry is introduced.
- [ ] No `ScaleType` mapping, register selection, absolute MIDI projection,
      projector call, or physical synth selection appears in the role engine.
- [ ] Shared Tonal Projector remains outside the Bass role and is consumed only
      by the later Stage 15 integration adapter.
- [ ] Production reachability remains explicitly unclaimed until Stage 15
      integration is separately reviewed.
- [ ] Hardware bass musical verdict remains **PENDING**.
