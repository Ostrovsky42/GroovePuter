# MiniAcid Key Map (Cardputer)

Canonical key map for the currently active firmware version.

## Стандартизация (Base Rules)
| Класс клавиш | Сочетание | Описание | Приоритет |
| --- | --- | --- | --- |
| **Performance notes** | `QWERTYUIOP` + `ASDFGHJKL` | Scale-aware Synth A keyboard while NOTE mode is ON | После локальной команды страницы, до legacy fallback |
| **Паттерны** | `Q..I` | Выбор паттерна 1..8 вне активного performance-note context | Локальный |
| **Банки** | `Ctrl + 1..2` | Переключение банков A/B (для тек. трека/страницы) | Локальный |
| **Страницы** | `Alt / Fn + 0..9` | Переключение страниц (Song, Pattern, Drum и др.) | **Глобальный** |
| **Мьюты** | `1..9`, `0` | Звук дорожек 1..9 (Rim), 0 (Clap). Игнорируют CapsLock. | **Глобальный fallback** |

## Global Shortcuts
| Key | Action |
| --- | --- |
| `Fn+Tab` | Cycle `PERFORM → PATTERN → ARRANGE` |
| `Fn+Shift+Tab` | Cycle workflow backward |
| `Space` | Transport Play / Stop |
| `Alt/Fn + 1..0` | Direct page jump (Global) |
| `[` / `]` | Previous / Next page outside PERFORM; scale select on PERFORM |
| `Alt+M` | Toggle Song mode |
| `Alt+W` | Waveform overlay |
| `Alt+V` | Jump to Groove Lab |
| `Alt+\` | Cycle visual style (`CARBON`/`CYBER`/`AMBER`) |
| `Ctrl+H` | Global help overlay (Highest Priority) |
| `1..9`, `0` | Track mutes when the active page does not consume the digit |
| `Esc` | Back / Dismiss |
| `Ctrl+Alt+Bksp` | Project reset (wipe) |

## Priority & Interception Logic
1. **Global Help Overlay**: intercepts input while visible.
2. **Hard-Global Shortcuts**: `Fn+Tab`, transport, `Alt/Fn + 0..9`, `Ctrl+H`, `Alt+W`.
3. **Local Page Handler**: the active page gets first refusal.
4. **Performance NOTE Layer**: on supported pages, an unmodified performance key is routed to `PerformanceKeyboard`.
   - NOTE mode ON + transport stopped: emit live note events.
   - NOTE mode ON + transport running: consume the key without emitting `NoteOn`.
   - NOTE mode OFF: return the key to legacy fallback commands.
5. **Legacy Global Fallback**: page navigation, help, track mutes, randomize/BPM shortcuts.

This order prevents `I/O/P/K/L` from reaching randomize/BPM commands while NOTE mode is active, even when transport temporarily owns Synth A.

---

## PERFORM Page

### Workflow and performance controls
| Key | Action |
| --- | --- |
| `1` | Open PERFORM |
| `2` | Open PATTERN |
| `3` | Open ARRANGE |
| `N` | Toggle NOTE mode ON/OFF |
| `[` / `]` | Previous / next scale |
| `-` / `=` | Octave down / up |
| `X` | Release the live-owned Synth A note |
| `Space` | Transport Play / Stop |

### NOTE MODE: ON
| Keys | Action |
| --- | --- |
| `ASDFGHJKL` | Lower scale-aware manual, starting at C2 by default |
| `QWERTYUIOP` | Upper manual, exactly one octave above the lower row |

Synth A is monophonic and uses last-note priority. Starting transport clears held performance notes and gives PatternPlayer exclusive ownership. Performance keys remain consumed while transport runs, so legacy pattern/BPM commands cannot fire accidentally.

### NOTE MODE: OFF
| Key | Legacy action |
| --- | --- |
| `I` | Randomize Synth A pattern |
| `O` | Randomize Synth B pattern |
| `P` | Randomize drums |
| `K` / `L` | BPM down / up |
| `N` | Return to NOTE mode |

NOTE mode, scale, root, and octave are runtime-only in this stage and are not written to scene JSON.

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
| `Ctrl+R` | Reverse song direction |
| `Ctrl+M` | Merge slots |
| `Ctrl+N` | Alternate slots |
| `Ctrl+L` | Loop mode |
| `Bksp` | Clear cell / selected area |
| `G` | Generate cell |
| `G` double-tap | Generate row |
| `Alt+G` | Generate selected area |
| `Alt+Bksp` | Clear full arrangement |

## Project Page & MIDI Import
| Key | Action |
| --- | --- |
| `Arrows` | Navigate list / Adjust params |
| `Enter` | **Open folder** / Select scene or file |
| `Backspace` | **Go up one level** (in MIDI folders) / Close dialog |
| `Tab` | Open Advanced Import Settings (in MIDI dialog) |
| `X` | Delete selected scene or MIDI file |
| `G` | Randomize scene name (in Save dialog) |

## Pattern Edit (303)
| Key | Action |
| --- | --- |
| `Arrows` | Navigate steps |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Q..I` | **Select pattern 1..8** |
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
| `Q..I` | Quick pattern select `1..8` when NOTE mode is OFF; live notes when NOTE mode is ON |
| **`Ctrl+1..2`** | **Switch bank A/B** |
| `Left/Right` | Focus control |
| `Up/Down` | Adjust value |
| `A/Z` | Cutoff +/- when consumed by the page |
| `S/X` | Resonance +/- when consumed by the page |
| `N` / `M` | Distortion / Delay toggle when consumed by the page |

## Feel & Texture
| Key | Action |
| --- | --- |
| `Q..I` | Select Pattern 1..8 when consumed by the page; otherwise live notes in NOTE mode |
| `Ctrl+1..2` | Switch Bank A/B |
| `Arrows` | Select parameter / Adjust value |
| `Tab` | Cycle focus (Feel / Drum FX / Presets) |
| `1..4` | Apply feel preset (when Presets focused) |

## Genre Page
| Key | Action |
| --- | --- |
| `Q..I` | Select Pattern 1..8 (Synth A) |
| `Ctrl+1..2` | Switch Bank A/B |
| `Arrows` | Select Genre / Texture / Preset / Pattern |
| `Enter` | Apply Genre + Texture |
| `M` | Cycle Apply mode (Snd / Pat / BPM) |
| `G` | Toggle Groovebox mode (Acid / Minimal) |
| `C` | Toggle Curated mode (Recommendations) |

> [!TIP]
> **CapsLock Safety**: QWERTY pattern selection (`Q..I`) and Track Mutes (`1..0`) work even if CapsLock is ON (Shift is ignored for these keys).
