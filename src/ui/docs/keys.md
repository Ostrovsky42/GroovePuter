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
| `Alt+V` | Open Voice page |
| `Alt+C` | Open Color Test page |
| `Alt+\\` | Switch visual style |
| `Ctrl+H` | Toggle global help overlay |
| `1..9`, `0` | Track mutes |
| `Esc` / `Bksp` / `b` / `` ` `` | Back to previous page |

## Local-Only Keys
- `Genre Page`: `Tab`, `Arrows`, `Enter`, `Space`, `1..8`, `0`, `M`, `C`, `G`
- `Pattern Edit (303)`: `Q..I`, `Arrows`, `A/Z`, `S/X`, `Alt+A`, `Alt+S`, `R`, `Bksp/Del`, `Alt+Bksp`, `G`, `Ctrl+C/V`, `Alt+Esc`
- `Drum Sequencer`: `Q..I`, `Arrows`, `Enter`, `A`, `G`, `Ctrl+G`, `Alt+G`, `Ctrl+C/V`, `Alt+Esc`
- `TB303 Params`: `Left/Right`, `Up/Down`, `Ctrl+Up/Down`, `A/Z`, `S/X`, `D/C`, `F/V`, `T/G`, `Y/H`, `N/M`, `Ctrl+Z/X/C/V`
- `Song Page`: `Alt+J/K`, `Ctrl+R`, `Ctrl+M`, `Ctrl+N`, `Space`, `Arrows`, `Enter`, `Ctrl+Arrows` (selection without Shift), `Q..I`, `Bksp/Tab`, `Ctrl+L`, `G`, `G` double-tap, `Alt+G`, `Alt+.`
- `Project Page`: `Arrows`, `Enter`, `G`
- `Settings Page`: `Tab`, `Up/Down`, `Left/Right`, `Ctrl/Alt + Left/Right`, `1..3`

## Priority Rule
1. Hard-global shortcuts are processed first (e.g. `Ctrl+H`, `Alt+W/V/C/M`, `Alt/Ctrl+1..0`).
2. Then the current page gets the event for local behavior.
3. Global fallback handles back/navigation when page did not consume the event.
