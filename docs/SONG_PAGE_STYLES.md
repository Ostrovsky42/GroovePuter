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
- `V`: lane focus cycle `ALL -> AB -> DR+VO`
- `X`: split compare toggle

## Long Song Keys
- `Ctrl+W` / `Ctrl+S`: move `-8 / +8` rows
- `Ctrl+Alt+W` / `Ctrl+Alt+S`: move `-32 / +32` rows
- `Ctrl+1..8`: switch edit page `1..8`
- `Alt+Q/E/R/T`: save row marker
- `Ctrl+Alt+Q/E/R/T`: jump to marker

## Future Notes
- Add optional one-frame flash on marker jump target row for better spatial confirmation.
- Consider tiny marker badges (`1..4`) in BAR column when marked rows are visible.
- Keep `Ctrl+R` debounce window configurable if keyboard repeat differs between SDL and hardware.
