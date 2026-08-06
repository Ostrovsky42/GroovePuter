# Phrase Core — Cardputer ADV hardware acceptance

## Purpose

Validate the first usable Phrase Core slice on M5Stack Cardputer ADV: four persisted Phrase slots backed by bounded references to existing Song pattern material.

This stage validates `REFERENCE VIEW` behavior. It does not claim independent event ownership, SMF extraction, retrospective live capture, cadence generation, or external `SEND ONCE`.

## Hardware list

- M5Stack Cardputer ADV, ESP32-S3, PSRAM disabled
- built-in 240×135 display and QWERTY keyboard
- headphones or powered speaker for internal playback
- optional Yamaha SEQTRAK is not required

## Wiring

No external wiring is required.

For audio monitoring, use the normal GroovePuter audio output path. Phrase Core does not change I2S, USB MIDI, PORT.A, SPI, I2C, or GPIO ownership.

## Build and flash

```bash
git fetch origin
git switch agent/phrase-core-foundation
git pull --ff-only
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

Flash with the normal Cardputer ADV procedure used for `dev`.

Required profile:

```text
PSRAM=disabled
PartitionScheme=huge_app
```

## Controls

Inside the `SONG` workflow, use `[` / `]` to switch between `SONG` and `PHRASE CORE`.

```text
1..4        select Phrase A/B/C/D
Up/Down     capture length 1/2/4/8 bars
Left/Right  preview a bar inside the selected Phrase
R           cycle role MAIN/VARIATION/BREAK/ENDING
P           select derive parent
Enter       capture from current Song row
D           derive parent into selected slot
W           write to an empty Song destination
Alt+W       overwrite the Song destination explicitly
Backspace   clear selected Phrase slot
Alt+H       page-aware help
```

## Test preparation

1. Open `SONG`.
2. Generate or assign at least four consecutive rows containing Synth A, Synth B, and Drums material.
3. Leave the Song cursor on the first prepared row.
4. Start playback once and confirm that the original Song still sounds normal.
5. Stop playback before mutation checks.

## Expected behavior

### 1. Capture Phrase A

1. Press `]` until `PHRASE CORE` opens.
2. Press `1`.
3. Use `Up/Down` until `4B` is shown.
4. Select role `MAIN` with `R`.
5. Press `Enter`.

Expected screen:

```text
A REF
MAIN 4B PATTERN
ID: non-zero
P: 0
```

Expected toast:

```text
CAPTURED A #<id>
```

The three compact grids show Synth A, Synth B, and Drums for the current preview bar when those pattern references belong to the loaded page. Cross-page references remain labelled but may show an unresolved outline instead of a fabricated mask.

### 2. Preview all bars

Press `Left/Right` four times.

Expected:

- bar indicator wraps within `1/4 .. 4/4`;
- no full-screen navigation or page change;
- no audio stop/panic;
- the energy bar and masks update only for the selected preview bar.

### 3. Derive Phrase B

1. Press `2` to select B.
2. Press `P` until parent A is shown.
3. Select role `VARIATION` with `R`.
4. Press `D`.

Expected:

```text
B REF
VARIATION 4B DERIVED
P: <Phrase A id>
```

Phrase B receives a new ID. Phrase A remains unchanged.

### 4. REF MUTABLE preview invalidation

This is a required semantic check, not an optional visual check.

1. Return to Phrase A and select a bar with a visible Drums mask.
2. Memorize or photograph the Drums mini-grid.
3. Return to the normal pattern editor and change a hit in the exact referenced Drums pattern.
4. Return to `PHRASE CORE` and select Phrase A and the same bar.

Expected:

- Phrase A is still labelled `REF` / `REF MUTABLE`;
- the Drums mini-grid changes immediately after the Scene revision changes;
- the energy value changes when the edited hit changes bounded density;
- no recapture is required;
- the UI never says `COPIED`, `OWNED`, `RECORDED`, or `EXTRACTED`.

Failure condition:

- the pattern editor audibly changes the backing pattern but Phrase A keeps the old mask or energy. This means the preview cache is stale and the screen is lying about `REF MUTABLE`.

### 5. Write B to an empty Song destination

1. Return to `SONG` with `[`.
2. Move to an empty destination row with enough room for four bars.
3. Return to `PHRASE CORE` with `]`.
4. Select B with `2`.
5. Press `W`.

Expected:

- the operation succeeds only if all selected destination cells are empty;
- Song length expands when needed;
- returning to `SONG` shows the same bounded pattern references in the destination rows;
- playback of the destination matches the referenced source material.

### 6. Occupied destination safety

Attempt `W` on an occupied destination.

Expected toast:

```text
WRITE: OCCUPIED
```

No destination cell may change. Record the existing destination references before the attempt and compare them afterward.

### 7. Explicit destructive overwrite

1. Keep the cursor on the same occupied destination used in the previous step.
2. Record the existing Synth A, Synth B, and Drums references.
3. Return to `PHRASE CORE`, select B, and press `Alt+W`.
4. Return to `SONG`.

Expected:

- the overwrite succeeds;
- every selected destination track now contains B's references;
- unselected tracks remain unchanged;
- no partial row or partial Phrase write occurs;
- the Song remains playable after the overwrite.

`Alt+W` is the only accepted destructive write path.

### 8. Clear slot and persist the clear

1. Select Phrase B.
2. Press `Backspace`.
3. Confirm B shows an empty slot while A remains valid.
4. Save the Scene/project through the normal save flow.
5. Reboot or load another Scene, then reload the saved Scene.

Expected:

- B remains empty after reload;
- A remains restored with the same ID, role, length, source, references, and `REF MUTABLE` semantics;
- clearing B does not reset the complete PhraseBank or unrelated Scene data;
- the next allocated Phrase ID remains valid and non-zero.

### 9. Save/load persistence with valid slots

1. Ensure Phrase A is valid and at least one other slot is valid or deliberately empty.
2. Save the Scene/project through the normal existing save flow.
3. Load another Scene or reboot.
4. Load the saved Scene/project.
5. Return to `PHRASE CORE`.

Expected:

- valid A/B/C/D slot metadata and references are restored;
- cleared slots remain empty;
- missing Phrase data in an old Scene loads as four empty slots;
- corrupted/unknown Phrase codec version resets only Phrase slots safely;
- normal Song, pattern, sound, MIDI, and transport state still loads according to the existing codec.

### 10. Last-page session restore

1. Leave the device on `PHRASE CORE`.
2. Switch to another workflow and return to `SONG`.

Expected: the SONG workflow returns to `PHRASE CORE`, not always to the Song grid.

### 11. Help and themes

1. Open `Alt+H` on the Phrase page.
2. Compare the listed keys with the footer.
3. Test all currently exposed themes.

Expected:

- help and footer describe the same active controls;
- A/B/C/D, role, length, source, parent, `REF`, grids, and energy remain readable at 240×135;
- no footer overlap hides the main state.

## Serial expectations

Phrase operations do not require continuous serial logging. Normal boot/profile diagnostics remain unchanged. There must be no repeated parser errors, audio underrun storm, allocation failure, watchdog reset, or MIDI panic caused by opening or using Phrase Core.

## Troubleshooting

### `CAPTURE: RANGE`

The requested 1/2/4/8-bar region extends beyond current Song length or row 128. Move the Song cursor earlier or choose a shorter length.

### `CAPTURE: EMPTY`

The selected region contains no Synth A, Synth B, or Drums pattern references. Generate or assign Song material first.

### `DERIVED: PARENT`

The selected parent slot is empty or invalid. Capture a valid parent Phrase and select it with `P`.

### `WRITE: OCCUPIED`

The destination already contains selected track material. Choose an empty region or use `Alt+W` only when destructive overwrite is intentional.

### Preview shows outlines instead of steps

The Phrase references another pattern page. The UI intentionally keeps the reference label and refuses to scan storage or invent a step mask during drawing. Load the referenced page to resolve its preview.

### Phrase preview does not change after editing a pattern

First confirm that the edited pattern is the exact pattern reference shown by Phrase A. If the audible pattern changes but the Phrase mask or energy remains stale after returning to the page, fail the acceptance: Scene revision invalidation is not working.

### Phrase changes after editing a pattern

Expected for this foundation. `REF MUTABLE` means the Phrase stores bounded pattern references, not an independent event copy.

## Acceptance checklist

- [ ] Firmware builds with PSRAM disabled and passes fixed DRAM gate.
- [ ] SDL target builds.
- [ ] Normal and SEQTRAK MIDI-only Cardputer ADV profiles build.
- [ ] SONG workflow exposes both SONG and PHRASE CORE pages.
- [ ] A/B/C/D selection works.
- [ ] Capture succeeds for 1, 2, 4, and 8 bars.
- [ ] Invalid/empty/out-of-range capture is atomic.
- [ ] A→B derivation records a new ID and correct parent ID.
- [ ] Editing a referenced pattern immediately updates Phrase masks and energy after Scene revision.
- [ ] Non-overwrite Song write rejects occupied destinations atomically.
- [ ] Explicit `Alt+W` overwrite succeeds and replaces only selected tracks.
- [ ] `Backspace` clears one slot without clearing other slots.
- [ ] A cleared slot remains empty after save/reload.
- [ ] Reference previews never claim independent ownership.
- [ ] Save/load restores valid Phrase slots.
- [ ] Old Scenes load four empty Phrase slots safely.
- [ ] SONG workflow remembers the Phrase page in the current session.
- [ ] `Alt+H` and footer controls agree.
- [ ] All current themes are readable at 240×135.
- [ ] No new stuck notes, transport changes, MIDI routing changes, audio underrun regression, or heap growth is observed.
