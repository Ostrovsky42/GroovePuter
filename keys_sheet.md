# MiniAcid Keyboard Shortcuts & Controls

This document provides a comprehensive reference for all keyboard shortcuts and control combinations in MiniAcid for M5Stack Cardputer.

---

## 🌍 Global Controls
*Available from any page unless specifically overridden.*

### Navigation & UI
| Key | Action |
| :--- | :--- |
| `[` / `]` | Switch to Previous / Next Page |
| `Alt + 1` ... `Alt + 0` | Direct Jump to Page (see Page Mapping below) |
| `Alt + V` | Jump to **Voice Page** |
| `h` | Show quick help toast |
| `Alt + W` | Toggle Waveform Overlay (mini oscilloscope) |

### Transport & Performance
| Key | Action |
| :--- | :--- |
| `Alt + M` | Toggle **Song Mode** ON/OFF (debounced) |
| `1` ... `9` | Toggle Mute for tracks (Synth A, Synth B, Kick, Snare, ...) |
| `0` | Toggle Mute for Clap |

### Direct Page Mapping (Alt + Number)
| Key | Page |
| :--- | :--- |
| `Alt + 1` | Synth A Pattern Edit |
| `Alt + 2` | Synth B Pattern Edit |
| `Alt + 3` | Synth A Parameters (TB-303) |
| `Alt + 4` | Synth B Parameters (TB-303) |
| `Alt + 5` | Drum Sequencer |
| `Alt + 6` | Machine Mode (Acid/Minimal) |
| `Alt + 7` | Play Page (Home) |
| `Alt + 8` | Sequencer Hub (Overview) |
| `Alt + 9` | Song Page (Arrangement) |
| `Alt + 0` | Project Page |

---

## 📝 Page-Specific Controls

### 🏠 Play Page (Home)
| Key | Action |
| :--- | :--- |
| `Space` | Start / Stop Playback |
| `Arrows` | Select Track |
| `Enter` | Toggle OVERVIEW / DETAIL view |
| `x` | Toggle step (in Detail view) |
| `Esc` / `Bksp` | Return to Overview (from Detail) |

### 🎹 Pattern Edit Page (303)
| Key | Action |
| :--- | :--- |
| `Q` ... `U` | Select Pattern (1-8) |
| `A` / `Z` | Adjust Note (+/-) for current step |
| `S` / `X` | Adjust Octave (+/-) for current step |
| `Shift + Q` | Toggle **Slide** for current step |
| `Shift + W` | Toggle **Accent** for current step |
| `g` | Randomize Pattern |
| **`Alt + .`** | **Reset / Clear Pattern** |
| `Enter` | Confirm Bank/Pattern Selection |

### 🥁 Sequencer Hub
| Key | Action |
| :--- | :--- |
| `Arrows` | Navigate Grid (Detail) / Select Track (Overview) |
| `Enter` | Enter Detail View / Toggle Step |
| `x` | Toggle Step |
| `w` | Toggle Accent |
| `a` | Toggle Slide (Synth tracks only) |
| `Q` ... `U` | Quick Pattern Select |
| `Space` | Start / Stop Playback |

### 🎼 Song Page
| Key | Action |
| :--- | :--- |
| `Arrows` | Move Cursor / Select Area (with Shift) |
| `0` ... `7` | Assign Pattern to current cell |
| `Bksp` / `Tab` | Clear current cell |
| `m` | Toggle Song Mode |
| `Ctrl + L` | Toggle Loop Mode |
| `g` | Generate Pattern for current cell |
| `g` (Double Tap) | Generate Pattern for entire row |
| `Alt + G` | Batch generate for selected area |
| **`Alt + .`** | **Clear Entire Song Arrangement** |

### 🗣️ Voice Page
| Key | Action |
| :--- | :--- |
| `Arrows` | Select Phrase / Adjust Params |
| `Space` | Preview (Speak) Phrase |
| `m` | Toggle Mute |
| `s` | Stop speaking immediately |
| `c` | Cache current phrase to SD card |
| `x` | Clear all Voice Cache |
| `r` | Apply "Robot" preset |
| `h` | Apply "High" preset |
| `d` | Apply "Deep" preset |
| `1` ... `0` | Quick select phrases 1-10 |

---

### 🎭 Genre & Texture Page
| Key | Action |
| :--- | :--- |
| `Arrows` | Navigate selected area (Genre / Texture / Presets) |
| `Tab` | Cycle focus between Genre, Texture, and Presets lanes |
| `Enter` | Apply current selection / Load selected preset |
| `1` ... `8` | Quick apply Presets 1-8 |
| `0` | Randomize Genre and Texture |

### ⚙️ Generator Settings
| Key | Action |
| :--- | :--- |
| `UP / DOWN` | Select parameter |
| `LEFT / RIGHT`| Adjust parameter value |
| `Shift + L/R` | Fast/coarse parameter adjustment |
| `Shift + U/P` | Page scroll UP/DOWN |

### 📼 Tape FX & Looper
| Key | Action |
| :--- | :--- |
| `UP / DOWN` | Select control (Mode / Preset / Macro sliders) |
| `LEFT / RIGHT`| Adjust slider or Cycle Mode/Preset |
| `Shift + L/R` | Fine adjustment for sliders |
| `R` | Cycle Tape Mode (Off -> Rec -> Play -> Dub) |
| `P` | Cycle Tape Preset |
| `F` | Toggle FX logic ON/OFF |
| `1` / `2` / `3` | Set Tape Speed (0.5x / 1.0x / 2.0x) |
| `Enter` | Trigger Stutter (momentary) |
| `Bksp` / `Del`| **Eject Tape** (Stop + Clear loop + FX Off) |
| `Space` | Clear Loop buffer (Reset recording) |

---

## 💡 Pro Tips
- Most **Alt + Key** combinations can also be triggered with **Ctrl + Key** if Alt is physically harder to reach on your layout.
- Use **Alt + M** to check your song arrangement without leaving your current pattern editing page.
- **Alt + .** is your "panic/reset" button for the current context (pattern or song).
- In any list (Settings, Voice), use **Shift** with navigation keys for faster movement.
