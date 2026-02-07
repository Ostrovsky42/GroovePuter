# Song Page Quick Start

This document reflects current behavior in `src/ui/pages/song_page.cpp`.

## Core Controls

- `Arrows`: move cursor
- `Shift/Ctrl + Arrows`: create/extend rectangular selection
- `Q..I`: assign pattern `1..8`
- `Bksp` / `Tab`: clear current cell (or selected area)
- `ESC` / `` ` `` / `~`: clear selection

## Selection v2 (Lock + Paste Target)

1. Select area with `Shift/Ctrl + Arrows`
2. Press `Ctrl+C` to copy and lock selection frame
3. Move frame with `Arrows` (this changes paste target only)
4. Press `Ctrl+V` to paste at frame target
5. Selection is cleared automatically after paste

Notes:
- If cursor is on `PLYHD` or `MODE`, copy/cut/paste works on whole song slot.
- In grid scope, copy/cut/paste works on cell or selected area.

## Slots / Playback / Mix

- `Alt+B`: toggle **edit slot** `A/B`
- `Ctrl+B`: toggle **play slot** `A/B`
- `Alt+X`: toggle `LiveMix` ON/OFF
- `Ctrl+R`: toggle song reverse direction (queued on safe boundary while playing)
- `Ctrl+M`: merge songs
- `Ctrl+N`: alternate songs (interleave)

## View / Lanes

- `V`: toggle 3rd lane `DR` <-> `VO`
- `X`: toggle split compare view

## Generation

- `G`: generate current cell
- double-tap `G`: generate current row
- `Alt+G`: generate selected area
- `Ctrl+G`: cycle generator mode

## Quick Test Checklist

- [ ] Selection expands with `Shift/Ctrl + Arrows`
- [ ] `Ctrl+C` locks frame, arrows move frame
- [ ] `Ctrl+V` pastes and clears selection
- [ ] `Alt+B` changes edit slot, `Ctrl+B` changes play slot
- [ ] `Ctrl+R` audibly changes song direction while playing
- [ ] `V` toggles `DR/VO`, `X` toggles split compare

