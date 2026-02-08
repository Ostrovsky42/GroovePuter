# MiniAcid Key Map (Cardputer)

Canonical key map for current firmware.

Notes:
- Cardputer workflow assumes no Shift/Caps; prefer lowercase + `Ctrl` / `Alt`.
- `Global` works on all pages.
- Page sections are local.

## Global
| Key | Action |
| --- | --- |
| `[` / `]` | Previous / Next page |
| `Alt+1..0` or `Ctrl+1..0` | Direct page jump |
| `Alt+M` | Toggle Song mode |
| `Alt+W` | Waveform overlay |
| `Alt+V` | Jump to Tape page |
| `Alt+\\` | Cycle visual style (`CARBON`/`CYBER`/`AMBER`) |
| `Ctrl+H` | Global help overlay |
| `1..9`, `0` | Track mutes |
| `Esc` | Back |
| `Ctrl+Alt+Bksp` | Project reset (wipe) |

## Genre Page
| Key | Action |
| --- | --- |
| `Tab` | Cycle focus (`Genre -> Texture -> Presets -> Apply`) |
| `Arrows` | Navigate / adjust |
| `Enter` | Apply current selection (or toggle apply mode if focused) |
| `Space` | Toggle apply mode (when `Apply` row focused) |
| `M` | Cycle apply mode (`SND -> S+P -> S+T`) |
| `C` | Curated/Advanced texture compatibility |
| `G` | Cycle Groove mode (`ACID/MINIMAL/BREAKS/DUB/ELECTRO`) |
| `1..8` | Apply preset |
| `0` | Randomize genre+texture |

## Mode Page (`GROOVE LAB`)
| Key | Action |
| --- | --- |
| `Tab` | Focus row (`MODE/FLAVOR/MACROS/PREVIEW`) |
| `Up/Down` | Move focus |
| `Left/Right` | Adjust focused row |
| `Enter` | Action on focused row |
| `A` | Apply flavor voicing to `303A` |
| `B` | Apply flavor voicing to `303B` |
| `T` | Apply flavor tape macro |
| `M` | Toggle `applySoundMacros` |
| `Space` | Preview regenerate + start playback |

## Generator Page
| Key | Action |
| --- | --- |
| `Tab` | Next group |
| `Up/Down` | Select row |
| `Left/Right` | Adjust value |
| `Ctrl`/`Alt` + `Left/Right` | Fast adjust |
| `1..3` | Apply regen preset |

## Pattern Edit (303)
| Key | Action |
| --- | --- |
| `Q..I` | Select pattern `1..8` |
| `B` | Toggle pattern bank `A/B` |
| `Arrows` | Navigate steps |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Ctrl+C` | Copy selection and lock frame |
| `Arrows` (after `Ctrl+C`) | Move locked frame (paste target) |
| `Ctrl+V` | Paste and clear selection |
| `A/Z` | Note +/- |
| `S/X` | Octave +/- |
| `Fn+Arrows` | `Up/Down`: note +/- , `Left/Right`: octave +/- |
| `Alt/Ctrl+A` | Accent toggle (uniform on selection) |
| `Alt/Ctrl+S` | Slide toggle (uniform on selection) |
| `R` | Rest (clear note) |
| `Bksp` / `Del` | Clear step / clear selection |
| `Alt+Bksp` | Clear whole pattern |
| `Esc` / `` ` `` / `~` | Clear selection |
| `G` | Randomize pattern |
| `Tab` | Toggle `303A/303B` |
| `Alt+Esc` | Chain mode (priority over Back) |

## Drum Sequencer
| Key | Action |
| --- | --- |
| `Q..I` | Select pattern `1..8` |
| `B` | Toggle pattern bank `A/B` |
| `Arrows` | Navigate grid |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Ctrl+C` | Copy selection and lock frame |
| `Arrows` (after `Ctrl+C`) | Move locked frame |
| `Ctrl+V` | Paste and clear selection |
| `Enter` | Toggle hit |
| `A` | Toggle accent |
| `G` | Randomize pattern |
| `Ctrl+G` | Randomize focused voice |
| `Alt+G` | Chaos random |
| `Esc` / `` ` `` / `~` | Clear selection |
| `Bksp` / `Del` | Clear hit / clear selection |
| `Alt+Bksp` | Clear whole pattern |
| `Alt+Esc` | Chain mode (priority over Back) |

## TB303 Params
| Key | Action |
| --- | --- |
| `Left/Right` | Focus control |
| `Up/Down` | Adjust value |
| `Ctrl+Up/Down` | Fine adjust |
| `A/Z` | Cutoff +/- |
| `S/X` | Resonance +/- |
| `D/C` | Env amount +/- |
| `F/V` | Env decay +/- |
| `T/G` | Oscillator +/- |
| `Y/H` | Filter type +/- |
| `N` / `M` | Distortion / Delay toggle |
| `Ctrl+Z/X/C/V` | Reset parameter |
| `Q..I` | Quick pattern select `1..8` |

## Song Page
| Key | Action |
| --- | --- |
| `Alt+B` | Toggle edit slot `A/B` |
| `Ctrl+B` | Toggle play slot `A/B` |
| `B` | Flip pattern bank `A<->B` in cursor/selection |
| `Alt+X` | LiveMix ON/OFF |
| `X` | Toggle split compare |
| `V` | Toggle `DR/VO` lane |
| `Ctrl+R` | Reverse song direction |
| `Ctrl+M` | Merge slots |
| `Ctrl+N` | Alternate slots |
| `Ctrl+L` | Loop mode |
| `Arrows` | Navigate grid |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Ctrl+C` | Copy selection and lock frame |
| `Arrows` (after `Ctrl+C`) | Move locked frame |
| `Ctrl+V` | Paste and clear selection |
| `Q..I` | Assign pattern `1..8` |
| `Bksp` / `Tab` | Clear cell / selected area |
| `Esc` / `` ` `` / `~` | Clear selection |
| `G` | Generate cell |
| `G` double-tap | Generate row |
| `Alt+G` | Generate selected area |
| `Alt+Bksp` | Clear full arrangement |

## Project Page
| Key | Action |
| --- | --- |
| `Tab` | Cycle section (`SCENES -> GROOVE -> LED`) |
| `Up/Down` | Move row in current section |
| `Left/Right` | Adjust focused value |
| `Enter` | Execute/open focused action |
| `G` | Jump to Genre page |

## Tape Page
| Key | Action |
| --- | --- |
| `X` | Smart workflow (`REC/PLAY/DUB`) |
| `A` | CAPTURE (clear + REC + FX ON) |
| `S` | THICKEN (safe DUB for 1 cycle) |
| `D` | WASH ON/OFF |
| `G` | Loop mute ON/OFF |
| `Z` / `C` / `V` | `STOP` / `DUB` / `PLAY` |
| `1` / `2` / `3` | Speed `0.5x / 1.0x / 2.0x` |
| `F` | Tape FX ON/OFF |
| `Enter` | Stutter ON/OFF |
| `Space` | Clear loop |
| `Bksp` / `Del` | Eject/reset loop |
| `Alt+Esc` | Chain mode (priority over Back) |

## Sequencer Hub
| Key | Action |
| --- | --- |
| `Arrows` | Navigate grid and tracks |
| `Enter` | Open track detail sequencer |
| `1..0` | Toggle track mutes (visually mapped) |
| `Q..I` | Swap pattern `1..8` for selected track |
| `X` | Toggle hit (drum track) |
| `A` | Toggle accent (drum track) |
| `Bksp` | Clear current step |
| `Alt+Bksp` | Clear full track pattern |
| `Esc` / `` ` `` | Back |

## Groove Lab (Mode Page)
| Key | Action |
| --- | --- |
| `Tab` | Focus next row |
| `Up/Down` | Select row |
| `Left/Right` | Change Mode/Flavor/Macro |
| `Enter` / `Space` | Preview (Regeneration) |
| `A` / `B` | Apply Mode settings to 303 `A/B` |
| `T` | Apply Mode settings to Tape FX |
| `M` | Toggle sound-macro application |
