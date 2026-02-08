# Song Page Styles

Active mapping in `SongPage`:
- `MINIMAL` -> `drawMinimalStyle`
- `RETRO_CLASSIC` -> `drawRetroClassicStyle`
- `AMBER` -> `drawAmberStyle`
- `MINIMAL_DARK` -> `drawTEGridStyle`

## Split Rule
When split compare is enabled (`X`), SongPage forces `drawTEGridStyle` for readability and slot contrast.

## Runtime Style Switching
- Global style key: `Alt+\\`
- SongPage follows global style unless split compare is ON.

## Selection UX (all styles)
- `Shift/Ctrl + Arrows`: extend selection
- `Ctrl+C`: copy + lock frame
- `Arrows`: move locked frame
- `Ctrl+V`: paste + clear selection
- `Esc` / `` ` `` / `~`: clear selection

## Slot/Mix Controls (style-independent)
- `Alt+B`: edit slot A/B
- `Ctrl+B`: play slot A/B
- `B`: bank flip A/B in cursor/selection
- `Alt+X`: LiveMix ON/OFF
- `Ctrl+R`: reverse song direction
- `Ctrl+M`: merge slots
- `Ctrl+N`: alternate slots
- `V`: DR/VO lane toggle
- `X`: split compare toggle
