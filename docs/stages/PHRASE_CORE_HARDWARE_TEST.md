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

### 4. Write B to Song

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

### 5. Occupied destination safety

Attempt `W` on an occupied destination.

Expected toast:

```text
WRITE: OCCUPIED
```

No destination cell may change. `Alt+W` is the only explicit overwrite path.

### 6. Mutable reference semantics

1. Capture A from a known pattern.
2. Edit one referenced Synth A pattern in its normal pattern editor.
3. Return to `PHRASE CORE` and preview A.

Expected:

- Phrase remains marked `REF` / `REF MUTABLE`;
- the preview reflects the edited backing pattern when it is resolvable on the loaded page;
- the UI never says `COPIED`, `OWNED`, `RECORDED`, or `EXTRACTED`.

### 7. Save/load persistence

1. Save the Scene/project through the normal existing save flow.
2. Load another Scene or reboot.
3. Load the saved Scene/project.
4. Return to `PHRASE CORE`.

Expected:

- valid A/B/C/D slot metadata and references are restored;
- missing Phrase data in an old Scene loads as four empty slots;
- corrupted/unknown Phrase codec version resets only Phrase slots safely;
- normal Song, pattern, sound, MIDI, and transport state still loads according to the existing codec.

### 8. Last-page session restore

1. Leave the device on `PHRASE CORE`.
2. Switch to another workflow and return to `SONG`.

Expected: the SONG workflow returns to `PHRASE CORE`, not always to the Song grid.

### 9. Help and themes

1. Open `Alt+H` on the Phrase page.
2. Compare the listed keys with the footer.
3. Test both current themes.

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
- [ ] Non-overwrite Song write rejects occupied destinations atomically.
- [ ] Explicit `Alt+W` overwrite works.
- [ ] Reference previews never claim independent ownership.
- [ ] Save/load restores Phrase slots.
- [ ] Old Scenes load four empty Phrase slots safely.
- [ ] SONG workflow remembers the Phrase page in the current session.
- [ ] `Alt+H` and footer controls agree.
- [ ] Both themes are readable at 240×135.
- [ ] No new stuck notes, transport changes, MIDI routing changes, audio underrun regression, or heap growth is observed.
