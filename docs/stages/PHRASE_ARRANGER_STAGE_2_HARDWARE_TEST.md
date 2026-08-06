# Phrase Arranger Stage 2 — Cardputer ADV hardware test

## Purpose

Validate the experimental Phrase Arranger stacked on `integration/genre-song-ui`.

The feature builds a fixed-capacity chain of Phrase slots and expands that chain into Song rows without copying note events into a second sequencer.

The arrangement stores only references to Phrase slots `A/B/C/D`. Each item expands by the selected Phrase's saved `1/2/4/8B` length.

## Hardware list

- M5Stack Cardputer ADV;
- USB-C data cable;
- headphones or powered speaker for playback validation;
- computer with the existing GroovePuter Arduino/Cardputer ADV toolchain.

No external MIDI or I2C hardware is required.

## Wiring

1. Connect Cardputer ADV to the computer using USB-C.
2. Connect headphones or a powered speaker to the normal audio output.
3. Leave PORT.A disconnected. This test does not use I2C.

## Build / flash

```bash
git fetch origin
git switch experiment/phrase-arranger-stage-2
git reset --hard origin/experiment/phrase-arranger-stage-2

bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

Required Cardputer ADV profile:

```text
PSRAM=disabled
PartitionScheme=huge_app
```

Flash the generated firmware using the same method used for the current `dev` hardware acceptance.

Host contract:

```bash
mkdir -p build/host-tests

g++ -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I. \
  tests/test_phrase_arranger.cpp \
  -o build/host-tests/test_phrase_arranger

build/host-tests/test_phrase_arranger
```

## Expected behavior

### 1. Open the arranger

1. Open the SONG workflow.
2. Open `PHRASE CORE` using `[` or `]`.
3. Press `Tab`.

Expected:

```text
PHRASE ARRANGE
CHAIN 0/16  TOTAL 0B
```

Pressing `Tab` again returns to `PHRASE CORE`.

### 2. Build a chain

Prepare valid Phrase slots first. A useful test set is:

```text
A = MAIN, 4B
B = VARIATION, 2B
C = BREAK, 1B
D = ENDING, 2B
```

On `PHRASE ARRANGE`, enter:

```text
1 1 2 1 3 1 2 4
```

Expected chain:

```text
A A B A C A B D
```

Expected total length for the example above:

```text
23B
```

The cursor advances to the next append position after each assignment.

### 3. Edit the chain

- `Left/Right`: move one position;
- `Up/Down`: move eight positions;
- `1..4`: assign A/B/C/D at the cursor;
- `Backspace`: remove the selected position and close the gap;
- `Shift+Backspace`: clear the complete chain.

Expected:

- removing one position shifts later items left;
- no empty hole remains inside the active chain;
- clearing the chain reports `0/16` and `0B`;
- trying to assign an empty Phrase slot shows an error and does not alter the chain.

### 4. Safe Song write

1. Build `A A B A C A B D` again.
2. Return to Song and choose an empty destination with enough consecutive rows.
3. Return to `PHRASE ARRANGE`.
4. Press `W`.

Expected:

- the entire chain expands into consecutive Song rows;
- each Phrase contributes exactly its saved number of bars;
- Song length grows to include the written rows;
- the operation is atomic;
- Synth A, Synth B and Drums references match the source Phrases.

### 5. Occupied destination protection

Select a destination where at least one target track is occupied and press `W`.

Expected:

```text
ARR WRITE: OCCUPIED
```

No destination row may be partially changed.

### 6. Explicit overwrite

On `PHRASE ARRANGE`, use `Alt+W` on an occupied destination.

Expected:

- Phrase Arranger receives the shortcut;
- Wave Overlay does not toggle;
- the complete target range is replaced atomically;
- other Song rows remain unchanged.

Outside the Phrase page, `Alt+W` must continue to toggle Wave Overlay.

### 7. Phrase deletion safety

1. Build a chain containing several references to Phrase `B`.
2. Return to `PHRASE CORE`.
3. Select `B` and clear it.
4. Return to `PHRASE ARRANGE`.

Expected:

- every `B` occurrence is removed from the chain;
- remaining entries keep their relative order;
- the displayed total bar count is updated;
- no invalid or dangling Phrase reference remains.

### 8. Persistence

1. Leave a non-empty arrangement.
2. Stop transport.
3. Allow recovery autosave to run or save the project explicitly.
4. Reboot and reload the same project.

Expected:

- chain order is restored;
- chain length and total bars are restored;
- Phrase slots retain their IDs and saved lengths;
- an arrangement cleared before reboot remains empty.

## Troubleshooting

### `1..4` changes track mutes

Confirm the screen title is `PHRASE ARRANGE`. The Phrase page must consume number keys before the global mute fallback.

### `W` writes only one row

Check each Phrase's saved length on `PHRASE CORE`. Arrangement items expand by the saved Phrase length, not by the current `NEXT` capture length.

### `ARR WRITE: OCCUPIED`

The normal write path is non-destructive. Move the Song cursor to a completely empty destination or use the explicit overwrite shortcut after verifying the target range.

### `Alt+W` toggles Wave Overlay

This is a shortcut-routing regression. Record the exact branch head and do not accept the build.

### Arrangement disappears after reboot

Confirm recovery autosave completed while transport was stopped. Check serial for Scene parse or persistence errors.

### Chain contains an invalid item

Clear or recapture the referenced Phrase. Sanitization must remove invalid arrangement references rather than inventing replacement content.

## Acceptance checklist

- [ ] Branch is `experiment/phrase-arranger-stage-2`.
- [ ] `Tab` switches `PHRASE CORE ↔ PHRASE ARRANGE`.
- [ ] Arrangement capacity is exactly 16 positions.
- [ ] `1..4` assign valid Phrase A/B/C/D entries.
- [ ] Left/Right and Up/Down move the arrangement cursor correctly.
- [ ] Backspace removes one item and closes the gap.
- [ ] Shift+Backspace clears the complete chain.
- [ ] Total bar count matches all saved Phrase lengths.
- [ ] `W` expands the complete chain into empty Song rows.
- [ ] Occupied normal write fails atomically.
- [ ] `Alt+W` overwrites only on the Phrase page and does not toggle Wave Overlay.
- [ ] Outside Phrase, `Alt+W` still toggles Wave Overlay.
- [ ] Clearing a Phrase removes its references from the arrangement.
- [ ] Save/reload or recovery autosave restores the chain exactly.
- [ ] Host Phrase Arranger test passes.
- [ ] Full Cardputer ADV build and DRAM gate pass.
- [ ] Serial shows no new Scene codec, recovery or allocation errors.
