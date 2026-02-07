# Song Page Styles

Current style mapping in `SongPage`:

- `MINIMAL` -> `drawMinimalStyle`
- `RETRO_CLASSIC` -> `drawRetroClassicStyle`
- `AMBER` -> `drawAmberStyle`
- `MINIMAL_DARK` -> `drawTEGridStyle`

Special rule:
- when split compare is enabled (`X`), SongPage forces `drawTEGridStyle` for side-by-side readability.

## Runtime Style Switching

Global UI style is switched by:

- `Alt+\\` (global)

SongPage uses current global style unless split compare is ON.

## Split + Selection UX

Relevant to all styles:

- `Shift/Ctrl + Arrows`: selection
- `Ctrl+C`: copy and lock selection frame
- `Arrows`: move locked frame (paste target)
- `Ctrl+V`: paste and clear selection
- `ESC` / `` ` `` / `~`: clear selection

## Slot / Mix Controls (Style-independent)

- `Alt+B`: edit slot A/B
- `Ctrl+B`: play slot A/B
- `Alt+X`: LiveMix ON/OFF
- `Ctrl+R`: reverse song direction
- `Ctrl+M`: merge slots
- `Ctrl+N`: alternate slots
- `V`: DR/VO lane toggle
- `X`: split compare toggle

