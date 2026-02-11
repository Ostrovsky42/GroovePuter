# Song Slots + Selection v2 Test Guide

This guide is aligned with current implementation.

## Regression Guards

1. Song writes use absolute row space `0..Song::kMaxPositions-1`.
2. `Ctrl+R` affects song position direction (not step direction inside patterns).
3. `Ctrl+C` in grid scope locks selection frame for paste-target positioning.
4. `Ctrl+V` in grid scope pastes and clears selection.
5. Slot controls:
- `Alt+B` = edit slot
- `Ctrl+B` = play slot
6. Long-song shortcuts do not break bank switch:
- `Ctrl+1..8` = edit page select
- `B` = bank flip A/B
- `Alt+Q/E/R/T` / `Ctrl+Alt+Q/E/R/T` = row markers

## Test Matrix

### A. Slot Separation

1. Fill slot A with obvious data
2. `Alt+B` switch to slot B, fill differently
3. Switch back/forth

Expected:
- A and B remain independent
- edit slot indicator changes immediately

### B. Play Slot vs Edit Slot

1. Keep edit slot on A
2. `Ctrl+B` set play slot B
3. Start playback

Expected:
- Playback follows selected play slot
- Edit operations still go to edit slot

### C. Selection v2 in Song Grid

1. Create selection with `Shift/Ctrl + Arrows`
2. `Ctrl+C`
3. Move locked frame with arrows
4. `Ctrl+V`

Expected:
- Target area receives copied data
- Selection clears after paste
- Source area remains unchanged after copy

### D. Whole-song Scope Copy/Paste

1. Move cursor to `PLYHD` or `MODE`
2. `Ctrl+C`
3. switch slot
4. `Ctrl+V`

Expected:
- whole song slot copied/pasted

### E. Reverse / Merge / Alternate

- `Ctrl+R`: audible change of song direction in playback
- `Ctrl+M`: merge other slot into active slot
- `Ctrl+N`: alternate/interleave active with other slot

### F. Long Navigation + Lane Focus

1. `Ctrl+W/S` and `Ctrl+Alt+W/S`
2. `Ctrl+1..8` for page select
3. Save markers on `Alt+Q/E/R/T`
4. Jump markers on `Ctrl+Alt+Q/E/R/T`
5. Press `V` repeatedly

Expected:
- jumps are `8` and `32` rows
- marker save/jump does not trigger page select
- lane focus cycles `ALL -> AB -> DR+VO -> ALL`
- `B` flips bank A/B on cursor/selection

## Quick Diagnostics

If `Ctrl+C` seems dead:
- check debug log for `SongPage key=... ctrl=1`
- verify local fallback path in `SongPage::handleEvent`

If selection does not clear:
- verify `Ctrl+V` path reaches `clearSelection()`
- verify `ESC` / `` ` `` / `~` clears selection locally
