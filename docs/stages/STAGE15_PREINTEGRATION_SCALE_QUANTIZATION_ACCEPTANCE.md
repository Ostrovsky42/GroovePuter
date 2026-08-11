# Stage 15 pre-integration — Scale Quantization Correctness

## Purpose

Fix the legacy `AdvancedPatternGenerator` scale quantizer before Stage 15 gains a production absolute-pitch owner.

The old code indexed a seven-row interval table with `scale % 7`. `ScaleType` has ten values, so the last three values were aliased incorrectly:

- `PENTATONIC_MJ` -> MINOR;
- `PENTATONIC_MN` -> MAJOR;
- `CHROMATIC` -> DORIAN.

This PR is intentionally separate from Tonal Materialization. It changes only legacy synth scale quantization and its tests. It does not wire ChordProgression, Melody intent, Bass intent, or Tonal Projector into production.

This is a **musically audible compatibility fix** for scenes/generation paths that select the three affected scales. The seven existing modal scales keep their current quantization behavior.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3) for optional smoke verification.
- USB-C cable for build/flash and serial.
- No external hardware is required for the automated gate.

## Wiring

No wiring changes.

- Cardputer-Adv is powered/programmed over USB-C.
- No GPIO, I2C, SPI, UART, MIDI, or audio routing ownership changes.
- PORT.A remains GPIO2 SDA / GPIO1 SCL if external I2C hardware is connected; this fix does not use that bus.

## Build / Flash

Run the focused host gate:

```bash
bash tests/run_scale_quantization_tests.sh
```

The gate runs:

- source-regression checks;
- GCC C++17 build/run;
- Clang C++17 build/run when available;
- ASan/UBSan build/run.

Repository-wide Core regressions, SDL, Cardputer ADV fixed-DRAM, and SEQTRAK MIDI-only builds must also remain green on the final frozen SHA.

No special firmware UI is introduced. If flashed, screen and Serial behavior remain unchanged; only generated synth notes under the affected scale selections may differ.

## Expected behavior

For root C, quantization across one chromatic octave must produce exactly these pitch-class sets:

| ScaleType | Expected pitch classes |
| --- | --- |
| MINOR | 0,2,3,5,7,8,10 |
| MAJOR | 0,2,4,5,7,9,11 |
| DORIAN | 0,2,3,5,7,9,10 |
| PHRYGIAN | 0,1,3,5,7,8,10 |
| LYDIAN | 0,2,4,6,7,9,11 |
| MIXOLYDIAN | 0,2,4,5,7,9,10 |
| LOCRIAN | 0,1,3,5,6,8,10 |
| PENTATONIC_MJ | 0,2,4,7,9 |
| PENTATONIC_MN | 0,3,5,7,10 |
| CHROMATIC | 0..11 |

`CHROMATIC` must be identity quantization: every input MIDI note remains unchanged for any tested root.

The implementation must use cardinalities `7,7,7,7,7,7,7,5,5,12`. No `% 7` aliasing is allowed.

Invalid/out-of-range `ScaleType` values fall back to DORIAN, matching the existing default scale rather than indexing outside the table.

## Scope boundary

This correctness gate does **not** consolidate interval-table ownership yet. The current Tonal Projector remains unchanged. Interval consolidation belongs to the subsequent Stage 15 final integration work so the audible `% 7` repair stays independently reviewable.

No changes are allowed to:

- ChordProgression;
- MelodicPitchIntent;
- BassPitchBehavior;
- Tonal Projector semantics;
- StrongRhythmMigration production routing;
- Scene persistence;
- Song/Phrase/FEEL;
- synth engine allocation.

## Assertion mutation ledger

| Assertion | Mutation that must fail it |
| --- | --- |
| all ten ScaleType values have distinct intended tables | restore `scale % 7` |
| pentatonic major has five tones | set its count to 7 or map it to MINOR |
| pentatonic minor has five tones | set its count to 7 or map it to MAJOR |
| chromatic has twelve tones | set its count below 12 or map it to DORIAN |
| chromatic is identity | remove one chromatic interval or use seven-tone iteration |
| enum/table order is explicit | reorder `ScaleType` without updating the static assertion |
| cardinality controls iteration | replace `i < intervalCount` with `i < 7` |

## Troubleshooting

If the focused host gate fails to compile:

1. Confirm the branch still contains the exact ten-value `ScaleType` order from `scenes.h`.
2. Confirm `advanced_pattern_generator.cpp` has a 12-column table and the explicit cardinality array.
3. Confirm the test links `src/dsp/advanced_pattern_generator.cpp` and does not call its private quantizer directly.
4. If Clang/ASan reports an unrelated repository problem, reproduce with the same compiler flags before changing production code.

If `PENTATONIC_MJ`, `PENTATONIC_MN`, or `CHROMATIC` still produce a seven-note modal set, search for a remaining `% 7` or fixed `i < 7` in the quantization path.

## Acceptance checklist

- [ ] `bash tests/run_scale_quantization_tests.sh` passes with GCC.
- [ ] Clang run passes when Clang is available.
- [ ] ASan/UBSan run passes.
- [ ] Source regression confirms `scale % 7` is absent.
- [ ] All ten `ScaleType` values produce the exact expected pitch-class set.
- [ ] `PENTATONIC_MJ` cardinality is 5.
- [ ] `PENTATONIC_MN` cardinality is 5.
- [ ] `CHROMATIC` cardinality is 12.
- [ ] `CHROMATIC` preserves every tested input note unchanged for roots C, F, and B.
- [ ] Seven existing modal scales retain their existing interval definitions.
- [ ] `ScaleType` enum/table order is mutation-pinned.
- [ ] No Tonal Projector, ChordProgression, Melody, Bass, Scene, Song, Phrase, or synth-allocation behavior is changed.
- [ ] Full repository host regressions pass on the frozen SHA.
- [ ] SDL build passes.
- [ ] Cardputer ADV normal + fixed DRAM build passes.
- [ ] SEQTRAK MIDI-only build passes.
- [ ] Three review passes are CLEAN on one unchanged final SHA.
