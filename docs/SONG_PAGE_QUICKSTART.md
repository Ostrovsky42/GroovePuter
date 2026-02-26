# Song Page Quick Start

Current behavior in `src/ui/pages/song_page.cpp`.

## Core Editing
- `Arrows`: move cursor
- `Enter`: **Quick Jump** to pattern editor for track at cursor
- `Shift/Ctrl + Arrows`: rectangular selection
- `Q..I`: assign pattern `1..8`
- `B`: flip pattern bank `A<->B` in current cell or entire selected area
- `Bksp` / `Tab`: clear current cell (or selected area)
- `Esc` / `` ` `` / `~`: clear selection

## Selection v2 (Lock + Paste Target)
1. Create selection with `Shift/Ctrl + Arrows`
2. `Ctrl+C` copies and locks the frame
3. Move locked frame with `Arrows` (target only)
4. `Ctrl+V` pastes at target
5. Selection auto-clears after paste

Notes:
- On `PLYHD` / `MODE` pseudo-columns, copy/cut/paste applies to whole active song slot.
- In grid cells, copy/cut/paste applies to cell or selected area.

## Slots / Playback / Mix
- `Alt+B`: toggle **edit slot** `A/B`
- `Ctrl+B`: toggle **play slot** `A/B`
- `Alt+X`: toggle `LiveMix` ON/OFF
- `Ctrl+R`: toggle song reverse direction (queued on safe boundary while playing)
- `Ctrl+M`: merge songs
- `Ctrl+N`: alternate songs (interleave)
- `V`: lane focus cycle `ALL -> AB -> DR+VO`
- `X`: split compare ON/OFF

## Long Song Navigation
- `Ctrl+W` / `Ctrl+S`: jump `-8 / +8` rows
- `Ctrl+Alt+W` / `Ctrl+Alt+S`: jump `-32 / +32` rows
- `Ctrl+1..8`: switch edit page `1..8`
- `Alt+Q/E/R/T`: save row marker `1..4`
- `Ctrl+Alt+Q/E/R/T`: jump to marker `1..4`
- `<` / `,`: jump to row `001`
- `>` / `.`: jump to song end

## Generation
- `G`: generate current cell
- double-tap `G`: generate current row
- `Alt+G`: generate selected area
- `Ctrl+G`: cycle generator mode

## Quick Checklist
- [x] Selection expands with `Shift/Ctrl + Arrows`
- [x] `Ctrl+C` locks frame and arrows move target
- [x] `Ctrl+V` pastes and clears selection
- [x] `Alt+B` and `Ctrl+B` are independent (edit vs play)
- [x] `Ctrl+R` changes song direction (not pattern step reverse)
- [x] `X` split compare and `V` lane focus both work
- [x] `Ctrl+1..8` switches edit page, `B` flips bank `A/B`
- [x] Scrolling grid handles 128-row songs
