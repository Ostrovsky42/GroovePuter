# Generated Phrase -> Song — Cardputer ADV acceptance

## Purpose

Verify the recovered user-facing `1B` / `2B` / `4B` / `8B` generated Phrase workflow on the current **PHRASE CORE** page.

`G` generates a connected phrase into consecutive one-bar Song rows beginning at the current Song position. Existing Phrase Core operations remain separate: `Enter` captures, `D` derives, and `W` writes an existing Phrase slot.

Procedural roles are:

- `2B`: `BASE -> MICRO VARIATION`;
- `4B`: `BASE -> MICRO VARIATION -> RETURN -> FILL`;
- `8B`: `BASE -> MICRO VARIATION -> RETURN -> FILL -> DEVELOPMENT -> BREAKDOWN -> BUILD -> ENDING FILL`.

Atlas-backed recipes keep the historical role mapping `P1/P2/P1/P3` across the first four bars, then reuse the same three bounded variation coordinates for the 8-bar roles. Generated material passes through the current strong-rhythm + Stage 15 tonal migration before Song commit.

## Hardware list

- M5Stack Cardputer-Adv;
- USB-C data cable;
- built-in speaker or headphones;
- optional Yamaha SEQTRAK only for a combined integration smoke; it is not required for this test.

## Wiring

No GPIO or I2C wiring is required. Use the built-in keyboard/display/audio path.

If SEQTRAK is also attached for the combined runtime-recovery smoke, keep the existing GroovePuter USB-MIDI connection; this test adds no new MIDI wiring.

## Build / Flash

From the repository root:

```bash
./tests/run_host_tests.sh
./scripts/build.sh
./scripts/build_seqtrak_midi_only.sh
```

Flash with the normal Cardputer-Adv procedure and open Serial Monitor at the project's normal baud rate.

## Test procedure

1. Stop transport.
2. Select the desired Genre/recipe and tonal settings.
3. Open **PHRASE CORE**.
4. Use `Up/Down` until the visible `NEXT` length is `4B`.
5. Make sure the current Song row and the next three rows are empty for Synth A, Synth B, and Drums.
6. Press `G` once.
7. Open Song and verify four consecutive rows were assigned.
8. Play the generated four bars.
9. Repeat for `1B`, `2B`, and `8B`.
10. While transport is playing, return to PHRASE and press `G` once.

## Expected behavior

- `G` uses the visible PHRASE length `1/2/4/8B`.
- Success toast: for example `4B GEN -> SONG 1-4`.
- Serial success contains `Generated 4B phrase -> Song rows ...`.
- Generation writes only to the active Song slot.
- Synth A, Synth B, and Drums use the same consecutive pattern IDs for each generated bar.
- Generated Song rows are normalized to one bar each.
- Procedural `4B` returns to the base idea on bar 3 and produces a stronger fill on bar 4.
- Atlas `4B` keeps `P1 -> P2 -> P1 -> P3` identity through the current migration path.
- `8B` adds development, breakdown, build, and ending-fill roles instead of eight unrelated regenerations.
- Occupied destination Song rows are rejected without writes.
- A pattern slot is not reusable merely because its pattern data is empty: if its global pattern ID is still referenced by either Song slot, the allocator skips it.
- Existing occupied pattern data is never overwritten.
- If generation unexpectedly fails after partial commit, temporary bars are rolled back.
- `Enter`, `D`, `W`, slot selection, preview navigation, and Phrase capture length controls keep their existing behavior.
- During PLAY, `G` shows `STOP PLAYBACK FOR PHRASE`; transport keeps running and no generation is committed. There is no hidden stop/generate/restart cycle.

## Troubleshooting

- `STOP PLAYBACK FOR PHRASE`: expected protection. Stop transport and press `G` again.
- `Song rows are not empty`: move Song position to an empty range or clear the destination rows.
- `Need consecutive empty slots`: free enough consecutive local pattern slots on the current page. Also check whether empty-looking IDs are still referenced by Song A or Song B.
- `Pattern page unavailable`: wait for the requested pattern page to finish loading.
- Only one bar plays: confirm Song mode is active and the generated rows contain consecutive pattern references.
- A row lasts more than one bar: regression; successful generated Phrase commit must normalize Song row length to `1B`.
- Procedural bars sound unrelated: bar 3 of a `4B` phrase should retain the base note layout; bar 4 should mainly strengthen end-of-bar articulation/drums.
- No Serial success line after `G`: check the on-screen toast first; a rejection/failure should also emit a warning line describing the reason.

## Acceptance checklist

- [ ] Host suite passes.
- [ ] Cardputer-Adv normal build passes.
- [ ] Cardputer-Adv fixed-DRAM gate passes.
- [ ] SEQTRAK MIDI-only build passes.
- [ ] PHRASE still selects `1B`, `2B`, `4B`, and `8B` with `Up/Down`.
- [ ] `G` generates exactly the selected length starting at current Song position.
- [ ] `4B` creates four consecutive pattern IDs and four Song rows.
- [ ] Bar 2 is related to bar 1 rather than independently regenerated.
- [ ] Bar 3 of procedural `4B` returns to the base note layout.
- [ ] Bar 4 contains a stronger last-quarter fill.
- [ ] Atlas `4B` follows `P1/P2/P1/P3`.
- [ ] `8B` contains development, breakdown, build, and ending-fill roles.
- [ ] Current Genre + root/scale affect generated material through the current migration path.
- [ ] Song rows advance as `1B` after successful generation.
- [ ] Occupied Song rows reject without partial writes.
- [ ] Referenced empty-looking pattern IDs are skipped.
- [ ] Insufficient safe consecutive slots reject without partial writes.
- [ ] Generator failure rolls back committed temporary bars.
- [ ] During PLAY, `G` does not stop/restart transport and shows `STOP PLAYBACK FOR PHRASE`.
- [ ] `Enter=Capture`, `D=Derive`, `W=Write`, `1-4=Slot`, and `Left/Right=Preview bar` remain unchanged.
- [ ] Screen shows the success/rejection toast and Serial emits the matching generated/rejected line.
- [ ] No reset, watchdog, stuck transport, or new sustained underrun appears during the smoke test.
