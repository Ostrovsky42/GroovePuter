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
| `Alt+V` | Jump to Groove Lab |
| `Alt+\\` | Cycle visual style (`CARBON`/`CYBER`/`AMBER`) |
| `Ctrl+H` | Global help overlay (Highest Priority) |
| `1..9`, `0` | Track mutes (Global - Handles Shift/CapsLock) |
| `Esc` | Back / Dismiss |
| `Ctrl+Alt+Bksp` | Project reset (wipe) |

## Priority & Interception Logic
1.  **Global Help Overlay**: Перехватывает всё, когда открыто.
2.  **Hard-Global Shortcuts**: `Alt/Fn + 0..9`, `Ctrl+H`, `Alt+W` всегда работают первыми.
3.  **Local Page Focus**: 
    - В **Song Page**: если курсор в правой колонке (на кнопке режима), горячие клавиши `Q..I` отключаются, чтобы не мешать управлению меню.
4.  **Local Page Handlers**: Если страница не обработала клавишу, она уходит в глобальные мьюты (`0..9`).

---

## Song Page
| Key | Action |
| --- | --- |
| `Arrows` | Navigate grid |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Q..I` | Assign pattern `1..8` (to cell or selection) |
| **`Ctrl+1..8`** | **Switch edit page `1..8`** |
| `B` | Flip bank `A/B` in cursor/selection |
| `Alt+B` | Toggle edit slot `A/B` |
| `Ctrl+B` | Toggle play slot `A/B` |
| `Alt+X` | LiveMix ON/OFF |
| `X` | Toggle split compare |
| `V` | Lane focus cycle `ALL -> AB -> DR+VO` |
| `Ctrl+W/S` | Jump by `8` rows |
| `Ctrl+Alt+W/S` | Jump by `32` rows |
| `Alt+Q/E/R/T` | Save row marker 1..4 |
| `Ctrl+Alt+Q/E/R/T` | Jump to marker 1..4 |
| `Ctrl+R` | Reverse song direction (Safe from Pattern 3 change) |
| `Ctrl+M` | Merge slots |
| `Ctrl+N` | Alternate slots |
| `Ctrl+L` | Loop mode |
| `Bksp` | Clear cell / selected area |
| `G` | Generate cell |
| `G` double-tap | Generate row |
| `Alt+G` | Generate selected area |
| `Alt+Bksp` | Clear full arrangement |

## Pattern Edit (303)
| Key | Action |
| --- | --- |
| `Arrows` | Navigate steps |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Q..I` | **Select pattern 1..8** (R is now Pattern 3) |
| **`Ctrl+1..2`** | **Switch bank A/B** |
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

## Drum Sequencer
| Key | Action |
| --- | --- |
| `Arrows` | Navigate grid |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Q..I` | **Select pattern 1..8** |
| **`Ctrl+1..2`** | **Switch bank A/B** |
| `Enter` | Toggle hit |
| `A` | Toggle accent |
| `Bksp` / `Del` | **Clear hit** / Clear selection |
| `G` | Randomize pattern |
| `Ctrl+G` | Randomize focused voice |
| `Alt+Bksp` | Clear whole pattern |

## TB303 Params
| Key | Action |
| --- | --- |
| `Q..I` | Quick pattern select `1..8` |
| **`Ctrl+1..2`** | **Switch bank A/B** |
| `Left/Right` | Focus control |
| `Up/Down` | Adjust value |
| `A/Z` | Cutoff +/- |
| `S/X` | Resonance +/- |
| `N` / `M` | Distortion / Delay toggle |

## Feel & Texture
| Key | Action |
| --- | --- |
| `Q..I` | Select Pattern 1..8 (Synth A) |
| `Ctrl+1..2` | Switch Bank A/B |
| `Arrows` | Select parameter / Adjust value |
| `Tab` | Cycle focus (Feel / Drum FX / Presets) |
| `1..4` | Apply feel preset (when Presets focused) |

## Genre Page
| Key | Action |
| --- | --- |
| `Q..I` | Select Pattern 1..8 (Synth A) |
| `Ctrl+1..2` | Switch Bank A/B |
| `Arrows` | Select Genre / Texture / Preset |
| `Enter` | Apply Genre + Texture |
| `M` | Cycle Apply mode (Snd / Pat / BPM) |
| `G` | Toggle Groovebox mode (Acid / Minimal) |
| `C` | Toggle Curated mode (Recommendations) |

> [!TIP]
> **CapsLock Safety**: QWERTY pattern selection (`Q..I`) and Track Mutes (`1..0`) now work even if CapsLock is ON (Shift is ignored for these keys).
