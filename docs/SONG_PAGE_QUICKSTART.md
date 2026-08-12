# Song Page Quick Start

Current behavior in `src/ui/pages/song_page.cpp`.

## Core Editing
- `Left/Right`: move `Synth A -> Synth B -> Drums`; crossing the outer edge changes edit Song slot `A/B`
- `Up/Down`: move Song row
- `Enter`: **Quick Jump** to pattern editor for track at cursor
- `Shift/Ctrl + Arrows`: rectangular selection inside the visible track grid; never crosses Song slot
- `Q..I`: assign pattern `1..8` from the visible `PAT:A/B` context
- `B`: toggle visible `PAT:A/B` assignment context (no Song mutation)
- `Alt+B`: flip pattern bank `A<->B` in current stored cell or selected area
- `Bksp` / `Tab`: clear current cell (or selected area)
- `Esc` / `` ` `` / `~`: clear selection

## Selection v2 (Lock + Paste Target)
1. Create selection with `Shift/Ctrl + Arrows`
2. `Ctrl+C` copies and locks the frame
3. Move locked frame with `Arrows` (target only)
4. `Ctrl+V` pastes at target
5. Selection auto-clears after paste

Notes:
- Keyboard focus is limited to visible musical columns; there is no hidden `MODE` pseudo-column.
- In grid cells, copy/cut/paste applies to the cell or selected area.
- `B` changes only the visible `PAT:A/B` assignment context. `Alt+B` is the explicit stored-reference bank flip.

## Slots / Playback / Mix
- Edit Song slot changes by crossing the Left/Right outer track boundary.
- `Ctrl+B`: toggle **play slot** `A/B`
- `Alt+X`: toggle `LiveMix` ON/OFF
- `Ctrl+R`: toggle song reverse direction (queued on safe boundary while playing)
- `Ctrl+N`: insert row at cursor
- `Ctrl+M`: delete row at cursor
- `V`: lane focus cycle `ALL -> AB -> DR`
- `X`: split compare ON/OFF

## Long Song Navigation
- `Ctrl+W` / `Ctrl+S`: jump `-8 / +8` rows
- `Ctrl+Alt+W` / `Ctrl+Alt+S`: jump `-32 / +32` rows
- `Ctrl+1..8`: switch edit page `1..8`
- `Alt+Q/E/R/T`: save row marker `1..4`
- `Ctrl+Alt+Q/E/R/T`: jump to marker `1..4`
- `Alt+<` / `Alt+,`: jump to row `001`
- `Alt+>` / `Alt+.`: jump to song end

## Generation
- `G`: generate current cell
- double-tap `G`: generate current row
- `Alt+G`: generate selected area
- `Ctrl+G`: cycle generator mode

## Quick Checklist
- [x] Selection expands with `Shift/Ctrl + Arrows`
- [x] `Ctrl+C` locks frame and arrows move target
- [x] `Ctrl+V` pastes and clears selection
- [x] Outer Left/Right boundary changes edit Song slot; `Ctrl+B` changes play slot
- [x] `Ctrl+R` changes song direction (not pattern step reverse)
- [x] `X` split compare and `V` lane focus both work
- [x] `Ctrl+1..8` switches edit page; `B` changes PAT assignment context and `Alt+B` flips a stored reference bank
- [x] Plain Left/Right crosses edit Song Slot `A <-> B` at the track edge without entering a hidden column
- [x] Scrolling grid handles 128-row songs
