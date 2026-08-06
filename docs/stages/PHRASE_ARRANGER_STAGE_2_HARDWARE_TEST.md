# Phrase Arranger Stage 2 — Cardputer ADV hardware test

## Purpose

Validate the experimental fixed-capacity Phrase Arranger after all GitHub
Actions jobs are green.

The arranger stores a 16-position chain of references to Phrase slots A/B/C/D
and materializes that chain into Song rows. It does not own copied note events
or add a second sequencer.

## Published memory contract

```text
Phrase Core PhraseBank: 244 bytes
PhraseArrangement:       18 bytes
Stage 2 PhraseBank:      262 bytes
Stage 2 fixed delta:     +18 bytes
```

The Cardputer ADV ELF fixed-DRAM gate remains mandatory.

## Hardware list

- M5Stack Cardputer ADV;
- USB-C data cable;
- headphones or powered speaker;
- computer with the existing GroovePuter Arduino toolchain.

No external MIDI or I2C hardware is required.

## Wiring

1. Connect Cardputer ADV to the computer by USB-C.
2. Connect headphones or a powered speaker to the normal audio output.
3. Leave PORT.A disconnected.

## Build / flash

Do not flash until PR #90 has one green Actions run for host, SDL, Cardputer ADV
and fixed DRAM.

```bash
git fetch origin
git switch experiment/phrase-arranger-stage-2
git reset --hard origin/experiment/phrase-arranger-stage-2

bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

Required profile:

```text
PSRAM=disabled
PartitionScheme=huge_app
```

## Expected behavior

### Open and close the arranger

1. Open `PHRASE CORE`.
2. Press `Tab`.

Expected title:

```text
PHRASE ARRANGE
```

Pressing `Tab` again returns to `PHRASE CORE`.

### Prepare Phrase slots

Create:

```text
A = MAIN, 4B
B = VARIATION, 2B
C = BREAK, 1B
D = ENDING, 2B
```

### Build the 23-bar chain

In `PHRASE ARRANGE`, press:

```text
1 1 2 1 3 1 2 4
```

Expected:

```text
A A B A C A B D
TOTAL 23B
```

Arithmetic:

```text
4 + 4 + 2 + 4 + 1 + 4 + 2 + 2 = 23B
```

Arrows navigate the cursor only. Stage 2 has no move/swap/reorder command;
changing order requires reassigning the affected positions.

### Destructive-key separation

In `PHRASE ARRANGE`:

- plain `Backspace`: no change;
- `Ctrl+Backspace`: remove the selected position and close the gap;
- `Ctrl+Shift+Backspace`: clear the complete chain.

In `PHRASE CORE`:

- plain `Backspace`: clear the selected Phrase slot;
- `Ctrl+Backspace`: no change.

This verifies that a missed `Tab` cannot destroy the wrong object.

### Safe Song write

1. Rebuild `A A B A C A B D`.
2. Select 23 completely empty Song rows.
3. Return to `PHRASE ARRANGE`.
4. Press `W`.

Expected:

- exactly 23 rows are written;
- Phrase lengths expand correctly;
- Song length grows to include the written range;
- SA/SB/DR references match the Phrase sources.

Repeat `W` on the occupied destination.

Expected:

```text
ARR WRITE: OCCUPIED
```

No row may be partially changed.

### Explicit overwrite

Press `Alt+W` on the occupied destination.

Expected:

- the complete range is overwritten atomically;
- Wave Overlay does not toggle on the Phrase page;
- outside Phrase, `Alt+W` still toggles Wave Overlay.

### Exact 128-bar boundary

1. Capture A/B/C/D as 8-bar Phrases.
2. Fill all 16 arrangement positions.
3. Confirm:

```text
TOTAL 128B
```

4. Use a fresh empty Song and write from zero-based row 0.

Expected: success through row 127 and Song length 128.

5. Use another fresh empty Song and write from zero-based row 1.

Expected:

```text
ARR WRITE: RANGE
```

The Song must remain byte-for-byte unchanged.

### Phrase deletion safety

1. Build a chain containing several B entries.
2. Return to `PHRASE CORE`.
3. Select B and press plain `Backspace`.
4. Return to `PHRASE ARRANGE`.

Expected: every B entry is removed, the remaining order is preserved, and the
total bar count is recalculated.

### Persistence

1. Leave a non-empty arrangement.
2. Stop transport.
3. Save or allow recovery autosave to complete.
4. Reboot and load the same project.

Expected:

- chain order and length are restored;
- total bars are restored;
- Phrase IDs and lengths are restored;
- a chain cleared before save remains empty;
- legacy pre-arranger Phrase scenes load with an empty arrangement.

## Troubleshooting

### Plain Backspace removes an arrangement item

Wrong build. Record the branch head and do not accept it.

### Ctrl+Backspace clears a Phrase in CORE

Wrong input-routing build. A missed Tab must not clear a Phrase.

### Alt+W toggles Wave Overlay on Phrase

Shortcut-routing regression. Do not accept the build.

### W writes only one row

Check the saved Phrase length. Arrangement expansion uses each Phrase's saved
1/2/4/8B length, not the current next-capture length.

### Arrangement disappears after reboot

Confirm save/autosave completed with transport stopped and inspect serial output
for Scene codec or recovery errors.

## Acceptance checklist

- [ ] GitHub Actions host job passed.
- [ ] GitHub Actions SDL build passed.
- [ ] GitHub Actions Cardputer ADV build passed.
- [ ] Fixed DRAM gate passed.
- [ ] `PhraseArrangement` is 18 bytes.
- [ ] `PhraseBank` is 262 bytes, delta +18 bytes.
- [ ] `Tab` switches CORE / ARRANGE.
- [ ] `A A B A C A B D` reports 23B.
- [ ] Arrows navigate only; no reorder is claimed.
- [ ] Plain Backspace is non-destructive in ARRANGE.
- [ ] Ctrl+Backspace removes one ARRANGE position.
- [ ] Ctrl+Shift+Backspace clears ARRANGE.
- [ ] Ctrl+Backspace is non-destructive in CORE.
- [ ] W writes atomically and rejects occupied destinations.
- [ ] Alt+W overwrites only on Phrase.
- [ ] 16 x 8B reports 128B.
- [ ] Row-0 128B write succeeds exactly.
- [ ] Row-1 128B write returns RANGE with no changes.
- [ ] Clearing a Phrase removes its chain references.
- [ ] Save/reload restores the chain exactly.
- [ ] Serial shows no new Scene, recovery or allocation errors.
