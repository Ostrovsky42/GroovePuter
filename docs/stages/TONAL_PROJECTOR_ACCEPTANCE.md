# Tonal Projector — Shared Transient Materialization Adapter

## Purpose

The Tonal Projector is a shared transient adapter between Stage 15 tonal intent
and Stage 14 materialization. It converts tagged one-bar tonal offsets into
absolute MIDI notes using the current harmonic root, the existing `ScaleType`
ABI value, a register corridor, and an optional post-projection adjacent-leap
limit.

Conceptual boundary:

```text
15B melodic intent --\
                     +--> Tonal Projector --> Stage 14 materialization
15C bass intent -----/
```

This is **not Stage 15D** and it is not a generator owner. It does not choose
Genre/Variant policy, rhythm, physical voice allocation, Scene state, Phrase
state, persistence, or timbre.

Current reachability: **API-ONLY / host-testable**. This checkpoint does not wire
15B or 15C into production materialization and therefore does not claim hardware
musical acceptance.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3) for later integration acceptance.
- USB-C cable for build/flash and serial.
- Built-in speaker is sufficient after production wiring.
- Optional Yamaha SEQTRAK for later MIDI audition.

## Wiring

No new wiring is introduced.

- Cardputer-Adv is powered/programmed over USB-C.
- No new GPIO, I2C, SPI, UART, or audio-pin ownership.
- PORT.A remains GPIO2 SDA / GPIO1 SCL if external I2C hardware is connected;
  the Tonal Projector does not use that bus.
- No change to the existing 3.3 V logic assumptions.

## Build / Flash

Run the isolated host gate:

```bash
bash tests/run_tonal_projector_tests.sh
```

The gate runs source ownership checks and compiles/executes with GCC, Clang when
available, and ASan/UBSan under C++17.

There is no projector-specific firmware flash acceptance yet because this branch
has no production caller. Repository Cardputer/SDL builds are compile guards
only until a later integration PR wires the adapter.

## Expected behavior

The input is fixed-capacity and tagged by **onset ordinal**:

```cpp
int8_t tonalOffsets[kStepsPerBar]{};
uint16_t semitoneOffsetOrdinals = 0;
```

For ordinal `N`:

- tag bit set: `tonalOffsets[N]` is an exact chromatic semitone displacement from
  the transient root;
- tag bit clear: `tonalOffsets[N]` is a scale-degree displacement.

### Existing ScaleType without Scene ownership

`ScaleType` is currently declared inside the large `scenes.h`. Importing that
header into this small shared adapter would drag Scene dependencies and existing
C++20 bit-field extensions into a C++17 host contract. The projector therefore
stores only the compact numeric value:

```cpp
using ScaleTypeValue = uint8_t;
ScaleTypeValue scaleTypeValue;
```

A later integration adapter passes:

```cpp
static_cast<uint8_t>(currentScaleType)
```

No second C++ scale enum is defined. The source regression parses the actual
`enum ScaleType` in `scenes.h` and pins its exact current order:

```text
MINOR, MAJOR, DORIAN, PHRYGIAN, LYDIAN, MIXOLYDIAN, LOCRIAN,
PENTATONIC_MJ, PENTATONIC_MN, CHROMATIC
```

It also pins the existing default `ScaleType scale = DORIAN`. Any ABI reorder
must therefore fail the projector gate instead of silently changing music.

The projector must:

- keep `TonalProjectionRequest` fixed-capacity at 24 bytes;
- use the real `ScaleType` cardinality:
  - seven degrees for MINOR/MAJOR/DORIAN/PHRYGIAN/LYDIAN/MIXOLYDIAN/LOCRIAN;
  - five degrees for PENTATONIC_MJ/PENTATONIC_MN;
  - twelve degrees for CHROMATIC;
- resolve negative scale degrees with musical floor division rather than C++
  truncation toward zero;
- preserve tagged semitone relations exactly, including `+7` fifth and `+12`
  octave intent;
- choose the selected root pitch-class occurrence nearest the center of the
  inclusive MIDI register as a deterministic transient root anchor;
- apply each displacement exactly from that anchor;
- return `NoteOutOfRegister` instead of silently octave-folding an exact tagged
  relation into a different interval;
- return `RootOutOfRegister` when the register contains no selected root pitch
  class;
- evaluate the common adjacent-leap limit only after both inputs have become
  absolute MIDI notes;
- return `LeapExceeded` when that projected limit is exceeded;
- expose an atomic result: `noteCount` becomes non-zero only after the entire
  projection succeeds; every failure leaves `noteCount == 0`;
- return an explicit status for invalid/empty requests;
- use fixed-capacity storage, no heap allocation, no global RNG, and no retry
  loop.

The existing `AdvancedPatternGenerator::quantizeToScale()` is deliberately not
used as this contract: its seven-mode mapping cannot represent the pentatonic
and chromatic `ScaleType` values correctly.

## Troubleshooting

If the isolated host gate fails:

1. Confirm the actual `ScaleType` order in `scenes.h` still matches the pinned
   compact ABI values.
2. Confirm the projector header does not include `scenes.h` or define a second
   `ScaleType` enum.
3. Confirm pentatonic cardinality is 5 and chromatic cardinality is 12.
4. Confirm negative degree conversion uses floor division.
5. Confirm semitone-tagged entries bypass scale-degree conversion.
6. Confirm register failures return status instead of octave-folding.
7. Confirm mixed degree/semitone entries are compared only after absolute MIDI
   projection.
8. Confirm failure statuses do not publish a non-zero `noteCount`.
9. Do not add Genre, policy selection, rhythm, Synth A/B routing, Scene, Phrase,
   persistence, or synth-engine ownership to solve integration failures.
10. Do not wire 15B/15C production callers in this checkpoint PR.

## Acceptance checklist

- [ ] `bash tests/run_tonal_projector_tests.sh` passes with GCC.
- [ ] Clang run passes when Clang is installed.
- [ ] ASan/UBSan run passes.
- [ ] Projector C++ header remains independent of `scenes.h`.
- [ ] No second C++ `ScaleType` enum is introduced.
- [ ] Source gate pins the actual `ScaleType` order and DORIAN default.
- [ ] `TonalProjectionRequest` remains exactly 24 bytes.
- [ ] All ten existing `ScaleType` ABI values are accepted; invalid values reject.
- [ ] Seven-note modes report cardinality 7.
- [ ] Major/minor pentatonic report cardinality 5.
- [ ] Chromatic reports cardinality 12.
- [ ] Positive and negative scale degrees resolve correctly.
- [ ] Pentatonic octave occurs after degree 5, not degree 7.
- [ ] Chromatic octave occurs after degree 12.
- [ ] A tagged fifth remains exactly +7 semitones even in Locrian.
- [ ] Mixed tagged/untagged intent projects unambiguously.
- [ ] Common adjacent-leap validation happens after MIDI projection.
- [ ] Exact out-of-register intent returns `NoteOutOfRegister`.
- [ ] Missing root pitch class returns `RootOutOfRegister`.
- [ ] Failure result leaves `noteCount == 0`.
- [ ] No silent octave-folding is introduced.
- [ ] No heap allocation, global RNG, or unbounded retry is introduced.
- [ ] No Genre, policy, rhythm, voice, Scene, Phrase, persistence, or timbre
      ownership appears.
- [ ] No production 15B/15C wiring appears in this PR.
- [ ] Hardware musical verdict remains **PENDING**.
