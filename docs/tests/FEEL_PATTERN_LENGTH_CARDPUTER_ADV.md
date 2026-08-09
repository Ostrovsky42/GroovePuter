# FEEL Pattern Length — Cardputer ADV acceptance

## Purpose

Verify that the FEEL page exposes the existing Song-row duration control and that `1B / 2B / 4B / 8B` still drive the same `Scene::feel.patternBars` value consumed by Song playback.

This is a UI restoration only. It does not add a second phrase-length owner and does not change Phrase Core capture length.

## Hardware list

- M5Stack Cardputer ADV;
- USB-C data cable;
- built-in speaker or headphones.

## Wiring

No external wiring is required.

Cardputer ADV hardware assumptions remain unchanged. PORT.A I2C, external displays, MIDI adapters, and SEQTRAK are not required for this test.

## Build / Flash

From the repository root:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Use the repository-pinned Cardputer ADV toolchain and normal release build options.

## Expected behavior

1. Open `GENERATE -> FEEL`.
2. The page header reads `FEEL 2/2`.
3. Move focus to `PATTERN LENGTH`.
4. Left/Right cycles exactly:

```text
1B -> 2B -> 4B -> 8B -> 1B
```

and in the opposite direction when pressing Left.
5. FEEL presets do not overwrite Pattern Length.
6. In Song mode, put a clearly audible pattern on two consecutive rows.
7. With `PATTERN LENGTH = 1B`, playback advances after one complete bar.
8. Repeat with `2B`, `4B`, and `8B`; the current Song row remains active for exactly that many complete bars before advancing.
9. Phrase Core capture length remains independent.

## Troubleshooting

- If FEEL shows `2/3`, the firmware is not built from the restored UI branch/head.
- If `PATTERN LENGTH` is missing, confirm `src/ui/pages/feel_page.cpp` contains the restored field and rebuild from a clean checkout.
- If Song advances every bar regardless of the setting, verify `MiniAcid::advanceSongBar_()` still passes `sceneManager_.currentScene().feel.patternBars` to `nextSongCycleBoundary()`.
- If a legacy scene contains an unsupported Pattern Length value, FEEL displays the normalized `1B` value and the next Left/Right edit enters the supported `1/2/4/8` cycle.

## Acceptance checklist

- [ ] Host regressions pass.
- [ ] Cardputer ADV build passes with warnings enabled.
- [ ] FEEL header is `FEEL 2/2`.
- [ ] `PATTERN LENGTH` is visible and focusable.
- [ ] Right cycles `1B -> 2B -> 4B -> 8B -> 1B`.
- [ ] Left cycles in reverse.
- [ ] FEEL preset changes do not alter Pattern Length.
- [ ] Song row lasts 1 complete bar at `1B`.
- [ ] Song row lasts 2 complete bars at `2B`.
- [ ] Song row lasts 4 complete bars at `4B`.
- [ ] Song row lasts 8 complete bars at `8B`.
- [ ] Phrase Core capture length behavior is unchanged.
- [ ] No new audio underruns or transport stalls appear while changing the field.
