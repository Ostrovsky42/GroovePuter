# Song Page Quick Start

Current behavior in `src/ui/pages/song_page.cpp`.

## Core Editing
- `Arrows`: move cursor
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

## View / Lanes
- `X`: split compare ON/OFF
- `V`: `DR` / `VO` lane toggle

## Generation
- `G`: generate current cell
- double-tap `G`: generate current row
- `Alt+G`: generate selected area
- `Ctrl+G`: cycle generator mode

## Quick Checklist
- [ ] Selection expands with `Shift/Ctrl + Arrows`
- [ ] `Ctrl+C` locks frame and arrows move target
- [ ] `Ctrl+V` pastes and clears selection
- [ ] `Alt+B` and `Ctrl+B` are independent (edit vs play)
- [ ] `Ctrl+R` changes song direction (not pattern step reverse)
- [ ] `X` split compare and `V` lane toggle both work
