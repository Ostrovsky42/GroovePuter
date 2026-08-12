# Generated Phrase -> Song — Cardputer ADV acceptance

## Purpose

Verify the recovered user-facing `1B` / `2B` / `4B` / `8B` generated Phrase workflow on the current **PHRASE CORE** page after the explicit `TO:` destination UX.

PHRASE owns one visible Song destination:

- `Ctrl+Left` / `Ctrl+Right` move `TO:` by one row;
- `Ctrl+Up` / `Ctrl+Down` move `TO:` by eight rows;
- `G` generates a fresh connected phrase into free **Synth A / Synth B / Drums** lanes beginning at `TO:`;
- `W` inserts the selected saved Phrase before `TO:` and shifts later Song rows;
- `Alt+W` replaces saved-Phrase lanes at `TO:` without shifting rows.

`G` deliberately does not inherit `W` insertion semantics. Fresh generation allocates consecutive pattern IDs and therefore preflights the destination Phrase lanes plus reference-safe empty pattern slots before the first mutation. Non-Phrase Song lanes are not generation destinations and are preserved. This keeps generated material transactional and prevents an accidental arranger shift or Phrase-lane overwrite. After a successful `G`, `TO:` advances by the generated length so repeated generation can build consecutive regions.

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
python3 tests/test_phrase_ui_source_regressions.py
python3 tests/test_song_generation_source_regressions.py
./tests/run_host_tests.sh
./scripts/check_cardputer_dram.sh
./scripts/build.sh
./scripts/build_seqtrak_midi_only.sh
```

Flash with the normal Cardputer-Adv procedure and open Serial Monitor at the project's normal baud rate.

## Test procedure

1. Stop transport.
2. Select the desired Genre/recipe and tonal settings.
3. Open **PHRASE CORE**.
4. Use `Up/Down` until the visible `NEXT` length is `4B`.
5. Use `Ctrl+Arrow` until `TO:` points at four consecutive rows whose Synth A / Synth B / Drums lanes are free.
6. Press plain `G` once.
7. Confirm `TO:` advances by four rows.
8. Open Song and verify the four generated rows start at the original visible `TO:` destination.
9. Play the generated four bars.
10. Repeat for `1B`, `2B`, and `8B`.
11. Put a Synth A, Synth B, or Drums reference in one target row and press `G`; verify rejection without Song shifting or overwrite.
12. If a target row has data only in a non-Phrase lane, press `G` and verify that data survives while Synth A/B/Drums are generated.
13. Capture or select a saved Phrase and verify `W` still INSERTS at `TO:` while `Alt+W` still REPLACES at `TO:`.
14. While transport is playing, return to PHRASE and press plain `G` once.

## Expected behavior

- `TO:` is the only visible destination coordinate used by PHRASE placement commands.
- Plain `G` uses the visible PHRASE length `1/2/4/8B` and starts exactly at `TO:`.
- Success toast: for example `4B GEN -> SONG 9-12`.
- Serial success contains `Generated 4B phrase -> Song rows ...`.
- After successful `G`, `TO:` advances by exactly the generated bar count and clamps to Song row 128.
- `G` writes only to the active Song slot.
- `G` requires the destination Synth A / Synth B / Drums lanes to be free; it never shifts existing Song rows and never overwrites those occupied Phrase lanes.
- Non-Phrase Song lanes in the destination rows are preserved.
- `W` remains the explicit INSERT command for a saved Phrase and shifts complete Song rows.
- `Alt+W` remains the explicit REPLACE command for a saved Phrase and does not shift rows.
- Synth A, Synth B, and Drums use the same consecutive pattern IDs for each generated bar.
- Generated Song rows are normalized to one bar each.
- Procedural `4B` returns to the base idea on bar 3 and produces a stronger fill on bar 4.
- Atlas `4B` keeps `P1 -> P2 -> P1 -> P3` identity through the current migration path.
- `8B` adds development, breakdown, build, and ending-fill roles instead of eight unrelated regenerations.
- A pattern slot is not reusable merely because its pattern data is empty: if its global pattern ID is still referenced by either Song slot, the allocator skips it.
- Existing occupied pattern data is never overwritten.
- If generation unexpectedly fails after partial commit, temporary bars are rolled back.
- `Enter`, `D`, `W`, `Alt+W`, slot selection, preview navigation, destination navigation, and Phrase capture length controls keep their existing behavior.
- During PLAY, `G` shows `STOP PLAYBACK FOR PHRASE`; transport keeps running and no generation is committed. There is no hidden stop/generate/restart cycle.

## Troubleshooting

- `STOP PLAYBACK FOR PHRASE`: expected protection. Stop transport and press `G` again.
- `Song rows are not empty`: at least one target Synth A / Synth B / Drums lane is already occupied. Move `TO:` to free Phrase lanes, clear those Phrase references, or use saved-Phrase `W`/`Alt+W` for their documented insert/replace semantics.
- Wrong generated destination: read `TO:` before pressing `G`; generation must not use a hidden Song playhead destination.
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
- [ ] `Ctrl+Left/Right` moves `TO:` by exactly one row; `Ctrl+Up/Down` moves it by exactly eight.
- [ ] Plain `G` generates exactly the selected length starting at visible `TO:` rather than the hidden/current Song playhead.
- [ ] After successful `G`, `TO:` advances by exactly the selected length.
- [ ] `4B` creates four consecutive pattern IDs and four Song rows.
- [ ] Bar 2 is related to bar 1 rather than independently regenerated.
- [ ] Bar 3 of procedural `4B` returns to the base note layout.
- [ ] Bar 4 contains a stronger last-quarter fill.
- [ ] Atlas `4B` follows `P1/P2/P1/P3`.
- [ ] `8B` contains development, breakdown, build, and ending-fill roles.
- [ ] Current Genre + root/scale affect generated material through the current migration path.
- [ ] Song rows advance as `1B` after successful generation.
- [ ] Occupied Synth A / Synth B / Drums destination lanes reject `G` without shifting, overwrite, or partial writes.
- [ ] A row occupied only in a non-Phrase lane accepts `G` and preserves that lane.
- [ ] Saved-Phrase `W` still inserts and shifts complete Song rows at `TO:`.
- [ ] Saved-Phrase `Alt+W` still replaces without shifting at `TO:`.
- [ ] Referenced empty-looking pattern IDs are skipped.
- [ ] Insufficient safe consecutive slots reject without partial writes.
- [ ] Generator failure rolls back committed temporary bars.
- [ ] During PLAY, `G` does not stop/restart transport and shows `STOP PLAYBACK FOR PHRASE`.
- [ ] `Enter=Capture`, `D=Derive`, `W=Insert`, `Alt+W=Replace`, `1-4=Slot`, and plain arrows keep their current behavior.
- [ ] Screen shows the success/rejection toast and Serial emits the matching generated/rejected line.
- [ ] No reset, watchdog, stuck transport, or new sustained underrun appears during the smoke test.
