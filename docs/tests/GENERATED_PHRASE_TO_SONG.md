# Generated Phrase to Song test

## Purpose

Verify that Groove Lab can generate a connected `1B`, `2B`, `4B`, or `8B` phrase into consecutive one-bar Song rows without changing the 16-step pattern storage format.

The phrase uses one generated base bar and derives related roles:

- `2B`: `BASE -> MICRO VARIATION`;
- `4B`: `BASE -> MICRO VARIATION -> RETURN -> FILL`;
- `8B`: `BASE -> MICRO VARIATION -> RETURN -> FILL -> DEVELOPMENT -> BREAKDOWN -> BUILD -> ENDING FILL`.

For compiled Atlas recipes the same Song workflow maps `P1/P2/P1/P3` across the first four bars.

## Hardware list

- M5Stack Cardputer-Adv;
- USB-C data cable;
- built-in speaker or headphones;
- microSD card only when testing pattern-page persistence.

## Wiring

No external wiring is required. The test uses the built-in keyboard, display, ES8311 audio path, and internal storage model.

## Build and flash

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Test procedure

1. Open **Groove Lab**.
2. Select a Mode and Flavor, or select an Atlas recipe on Genre first.
3. Select `PHRASE 4 BARS -> SONG`.
4. Move the Song cursor to four empty rows.
5. Ensure four consecutive pattern slots on the current pattern page are empty.
6. Focus `GENERATE` and press `Enter`, or press `G`.
7. Open Song and play the generated rows.
8. Repeat with `2B` and `8B`.

## Expected behavior

- Generation writes to the active Song slot only.
- Synth A, Synth B, and Drums use the same consecutive pattern IDs for each bar.
- The Song rows are one bar long after generation.
- Procedural `4B` returns to the base idea on bar 3 and produces a clear fill on bar 4.
- Atlas `4B` uses `P1 -> P2 -> P1 -> P3`.
- `8B` contains an audible breakdown/build section without becoming eight unrelated loops.
- Existing occupied Song rows are never overwritten.
- Existing occupied pattern slots are never overwritten.
- When generation fails, committed temporary bars are rolled back.

## Troubleshooting

- `Song rows are not empty`: move the Song cursor to an empty range or clear those rows.
- `Need consecutive empty slots`: clear or move patterns on the current page. A phrase requires `2`, `4`, or `8` consecutive local slots shared by Synth A, Synth B, and Drums.
- `Pattern page unavailable`: wait for the requested pattern page to finish loading.
- Only the first bar repeats: confirm Song mode is active and the generated Song rows contain consecutive pattern IDs.
- Every Song row repeats for several bars: regenerate the phrase; generation normalizes `FEEL Length` to `1B` so each generated row advances after one bar.
- Phrase sounds unrelated: compare bars with FX disabled. Bar 3 of a procedural `4B` phrase must preserve the base note layout, and bar 4 must mainly change end-of-bar articulation and drums.

## Acceptance checklist

- [ ] Host tests pass.
- [ ] Cardputer-Adv release build passes with warnings enabled.
- [ ] `1B`, `2B`, `4B`, and `8B` can be selected.
- [ ] `4B` creates four consecutive pattern IDs and four Song rows.
- [ ] Bar 2 is related to bar 1 rather than independently regenerated.
- [ ] Bar 3 of procedural `4B` returns to the base note layout.
- [ ] Bar 4 contains a stronger last-quarter fill.
- [ ] Atlas `4B` follows `P1/P2/P1/P3`.
- [ ] `8B` contains development, breakdown, build, and ending-fill roles.
- [ ] Song row length becomes `1B` after phrase generation.
- [ ] Occupied Song rows reject the operation without partial writes.
- [ ] Insufficient consecutive pattern slots reject the operation without partial writes.
- [ ] Generator failure rolls back committed bars.
- [ ] Existing scenes and binary pattern-page format remain compatible.
- [ ] No new audio underruns appear while playing the generated phrase.
