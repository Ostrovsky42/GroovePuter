# MiniAcid Key Map (Cardputer)

Canonical key map for current firmware.

## Стандартизация (Base Rules)
| Класс клавиш | Сочетание | Описание | Приоритет |
| --- | --- | --- | --- |
| **Паттерны** | `Q..I` | Выбор паттерна 1..8 | Локальный (блокируется фокусом на меню) |
| **Банки** | `Ctrl + 1..2` | Переключение банков A/B (для тек. трека/страницы) | Локальный |
| **Страницы** | `Alt / Fn + 0..9` | Переключение страниц (Song, Pattern, Drum и др.) | **Глобальный** |
| **Мьюты** | `1..9`, `0` | Звук дорожек 1..9 (Rim), 0 (Clap). Игнорируют CapsLock. | **Глобальный** |

## Global Shortcuts
| Key | Action |
| --- | --- |
| `Alt/Fn + 1..0` | Direct page jump (Global) |
| `[` / `]` | Previous / Next page (Legacy) |
| `Alt+M` | Toggle Song mode |
| `Alt+W` | Waveform overlay |
| `Alt+V` | Jump to Tape Page (Visualizer) |
| `Alt+\\` | Cycle visual style (`CARBON`/`CYBER`/`AMBER`) |
| `Ctrl+H` | Global help overlay (Highest Priority) |
| `1..9`, `0` | Track mutes (Global - Handles Shift/CapsLock) |
| `Esc` / \` | Back / Dismiss / Previous Page toggle |
| `Ctrl+Alt+Bksp` | Project reset (wipe) |

## Priority & Interception Logic
1.  **Global Help Overlay**: Перехватывает всё, когда открыто.
2.  **Hard-Global Shortcuts**: `Alt/Fn + 0..9`, `Ctrl+H`, `Alt+W` всегда работают первыми.
3.  **Local Page Focus**: 
    - В **Song Page**: если курсор в правой колонке (на кнопке режима), горячие клавиши `Q..I` отключаются, чтобы не мешать управлению меню.
4.  **Local Page Handlers**: Если страница не обработала клавишу, она уходит в глобальные мьюты (`0..9`).

---

## Genre Page
| Key | Action |
| --- | --- |
| `Q..I` | Select Pattern 1..8 (Synth A) |
| `Ctrl+1..2` | Switch Bank A/B |
| `Tab` | Focus cycle: `GENRE -> TEXTURE -> APPLY` |
| `Up/Down` | Change Genre or Texture (depends on focus) |
| `Left/Right` | `Texture Amount +/-` (when `TEXTURE` focused) |
| `Enter` | Apply current Genre/Texture (or cycle Apply mode when `APPLY` focused) |
| `Space` | Cycle Apply mode (when `APPLY` focused) |
| `M` | Cycle Apply mode (`SND -> S+P -> S+T`) |
| `G` | Toggle Groovebox mode override (cycles 5 modes) |
| `C` | Toggle Curated mode (Recommendations) |

`Apply` modes:
- `SND`: sound/timbre only, keeps existing patterns
- `S+P`: sound + pattern regeneration
- `S+T`: sound + pattern regeneration + BPM set to genre default

---

## Pattern Edit (303)
| Key | Action |
| --- | --- |
| `Arrows` | Navigate steps |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Q..I` | Select pattern 1..8 |
| `Ctrl+1..2` | Switch bank A/B |
| `Bksp` / `Del` | **REST (Clear step)** / Clear selection |
| `A/Z` | Note +/- |
| `S/X` | Octave +/- |
| `Alt+Left/Right` | Rotate pattern |
| `Alt/Ctrl+A` | Accent toggle |
| `Alt/Ctrl+S` | Slide toggle |
| `Ctrl+C / Ctrl+V` | Copy / Paste |
| `Alt+Bksp` | Clear whole pattern |
| `G` | Randomize pattern |
| `Tab` | Toggle `303A/303B` |
| `Alt+Esc` | Chain mode (priority over Back) |

---

## Drum Sequencer
| Key | Action |
| --- | --- |
| `Arrows` | Navigate grid |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Tab` | Cycle Drum subpages: `Main -> Settings -> Automation` |
| `Q..I` | Select pattern 1..8 |
| `Ctrl+1..2` | Switch bank A/B |
| `Enter` | Toggle hit |
| `A` | Toggle accent |
| `Bksp` / `Del` | Clear hit / Clear selection |
| `G` | Randomize pattern |
| `Ctrl+G` | Randomize focused voice |
| `Alt+G` | Chaos random |
| `Alt+Bksp` | Clear whole pattern |
| `Alt+Esc` | Chain mode |

### Drum Settings (Drum subpage)
| Key | Action |
| --- | --- |
| `Up/Down` | Select FX row |
| `Left/Right` | Adjust value (Compressor, Attack, Sustain, Reverb) |
| `Ctrl/Alt+L/R` | Fast adjust |

### Drum Automation (Drum subpage)
| Key | Action |
| --- | --- |
| `Arrows Up/Down` | Select row (parameter) |
| `Arrows Left/Right` | Adjust selected value |
| `N` | Add node to current lane |
| `X` | Remove selected node |
| `Enter` on `SWING/HUMAN` | Reset override to `AUTO` (`-1`) |

---

## Song Page
| Key | Action |
| --- | --- |
| `Arrows` | Navigate grid |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Q..I` | Assign pattern 1..8 (to cell or selection) |
| `B` | Flip bank A/B in cursor/selection |
| `Alt+B` | Toggle edit slot A/B |
| `Ctrl+B` | Toggle play slot A/B |
| `Alt+X` | LiveMix ON/OFF |
| `X` | Toggle split compare |
| `V` | Lane focus cycle `ALL -> AB -> DR+VO` |
| `Ctrl+R` | Reverse song direction |
| `Ctrl+M` | Merge slots |
| `Ctrl+N` | Alternate slots |
| `Ctrl+L` | Loop mode on/off |
| `Bksp` | Clear cell / selected area |
| `G` | Generate cell |
| `G` double-tap | Generate row |
| `Alt+G` | Generate selected area |
| `Alt+Bksp` | Clear full arrangement |

---

## Sequencer Hub
| Key | Action |
| --- | --- |
| `Arrows` | Navigate tracks / steps |
| `Enter` | Open track detail / edit |
| `Q..I` | Swap pattern for tracked track |
| `Ctrl+1..2` | Switch bank for tracked track |
| `-` / `=` | Decrease/Increase track volume |
| `X` | Toggle hit (on Drum track) |
| `A` | Toggle accent (on Drum track) |

---

## Groove Lab (Mode Page)
| Key | Action |
| --- | --- |
| `Tab` / `Up/Dn` | Focus row |
| `Left/Right` | Change value |
| `Enter` / `Space` | Preview (Regenerate) |
| `A` / `B` | Apply to 303 A/B |
| `D` | Apply to Drums |
| `M` | Toggle Sound Macros |

---

## Tape Page
| Key | Action |
| --- | --- |
| `X` | Smart REC/PLAY/DUB workflow |
| `A` | CAPTURE (clear + REC + FX ON) |
| `S` | THICKEN (safe DUB x1 cycle) |
| `D` | WASH ON/OFF |
| `G` | Loop mute ON/OFF |
| `Z` / `C` / `V` | STOP / DUB / PLAY |
| `1` / `2` / `3` | Speed 0.5x / 1x / 2x |
| `F` | Toggle tape FX |
| `Enter` | Stutter ON/OFF |
| `Space` | Clear loop |
| `Bksp` | Eject / Reset loop |

> [!TIP]
> **CapsLock Safety**: QWERTY pattern selection (`Q..I`) and Track Mutes (`1..0`) now work even if CapsLock is ON (Shift is ignored for these keys).
