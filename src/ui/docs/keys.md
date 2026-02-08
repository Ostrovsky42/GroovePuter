# MiniAcid Keyboard Map (Cardputer)

This document reflects actual key handling in current code (`src/ui/miniacid_display.cpp` + page handlers).

Keyboard constraint: assume no practical `Shift` / `Caps Lock`. Use lowercase letters with `Ctrl` / `Alt`.

## Scope
- `Global`: works on any page.
- `Local only`: works only on the page that handles it.

## Global Keys
| Key | Action |
| --- | --- |
| `[` / `]` | Previous / Next page |
| `Alt+1..0` or `Ctrl+1..0` | Direct page jump |
| `Alt+M` | Toggle Song mode |
| `Alt+W` | Toggle waveform overlay |
| `Alt+V` | Open Tape page |
| `Alt+\\` | Switch visual style |
| `Ctrl+H` | Toggle global help overlay |
| `1..9`, `0` | Track mutes |
| `Esc` / `Bksp` / `` ` `` | Back to previous page |

## Local-Only Keys
- `Genre Page`: `Tab`, `Arrows`, `Enter`, `Space`, `1..8`, `0`, `M`, `C`, `G`
- `Pattern Edit (303)`: `Q..I`, `Arrows`, `Shift/Ctrl+Arrows` (selection), `B` (bank A/B), `A/Z`, `S/X`, `Alt/Ctrl+A`, `Alt/Ctrl+S`, `R`, `Bksp/Del`, `Alt+Bksp`, `G`, `Ctrl+C/V`, `Tab` (voice A/B), `Alt+Esc` (chain), `Esc`/`` ` ``/`~` (clear selection)
- `Drum Sequencer`: `Q..I`, `Arrows`, `Shift/Ctrl+Arrows` (selection), `B` (bank A/B), `Enter`, `A`, `G`, `Ctrl+G`, `Alt+G`, `Ctrl+C/V`, `Alt+Esc` (chain), `Esc`/`` ` ``/`~` (clear selection)
- `TB303 Params`: `Left/Right`, `Up/Down`, `Ctrl+Up/Down`, `A/Z`, `S/X`, `D/C`, `F/V`, `T/G`, `Y/H`, `N/M`, `Ctrl+Z/X/C/V`
- `Song Page`: `Alt+B` (edit slot), `Ctrl+B` (play slot), `B` (flip cell/selection bank A<->B), `Alt+X` (LiveMix), `Ctrl+R`, `Ctrl+M`, `Ctrl+N`, `Space`, `Arrows`, `Enter`, `Shift/Ctrl+Arrows` (selection), `Q..I`, `Bksp/Tab`, `Ctrl+L`, `G`, `G` double-tap, `Alt+G`, `Alt+.`, `V` (DR/VO lane), `X` (split compare), `Esc`/`` ` ``/`~` (clear selection)
- `Tape Page`: `X` smart workflow, `A` capture, `S` thicken (safe dub x1), `D` wash toggle, `G` loop mute toggle, `Z/C/V` stop/dub/play, `1..3` speed, `F` FX toggle, `Enter` stutter toggle, `Space` clear loop, `Del/Bksp` eject
- `Project Page`: `Arrows`, `Enter`, `G`
- `Settings Page`: `Tab`, `Up/Down`, `Left/Right`, `Ctrl/Alt + Left/Right`, `1..3`

## Priority Rule
1. Hard-global shortcuts are processed first (e.g. `Ctrl+H`, `Alt+W/V/M`, `Alt/Ctrl+1..0`).
2. Then the current page gets the event for local behavior.
3. Global fallback handles back/navigation when page did not consume the event.
