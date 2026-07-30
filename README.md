# GroovePuter v0.1.0

[![Status](https://img.shields.io/badge/status-beta-yellow)](#status)
[![Platform](https://img.shields.io/badge/platform-M5Stack%20Cardputer%20\(ESP32--S3\)-blue)](#requirements)
[![Build](https://img.shields.io/badge/build-release.sh%20%7C%20arduino--cli-brightgreen)](#build--flash)

> **Portable real-time groove computer for M5Stack Cardputer.**
> A **time-feel engine** that separates **what** is generated from **how it feels in time** and **how it sounds right now**.

Based on the original **MiniAcid** by [urtubia/miniacid](https://github.com/urtubia/miniacid) — this fork focuses on **timing FEEL**, **scene persistence**.



## Status

**Beta.** Core flow is playable and stable on device. APIs/UI may change as the instrument evolves.

## Features

* **Two Swappable Synth Voices:** swap engines on the fly (click-free) between TB-303, OPL2 (FM), AY/YM2149 (PSG), and SID.
* **TR-808–inspired drum section**
* **Pattern + song arrangement**
* **PERFORM / PATTERN / ARRANGE workflow:** scale-aware two-row keyboard, explicit NOTE mode, monophonic last-note priority, and transport-safe Synth A ownership.
* **Dual song slots (`A/B`)** with split compare and live mix controls
* **RETRO split Song view** aligned with cyber theme styling
* **FEEL system (live):**
  * Grid: `1/8 · 1/16 · 1/32`
  * Timebase: `HALF · NORMAL · DOUBLE`
  * Length: `1B … 8B` (cycle length)
* **TEXTURE layer (live):** Lo-Fi / Drive / Drum FX (Comp, Transient, Reverb)
* **MIDI Routing:** Interactive grid for mapping MIDI channels to instruments
* **Genre-driven generator:** rhythmic masks, motif length, scale preference, density traits
* **Groove Lab page:** mode/flavor/macros + corridor/budget preview
* **Drum Automation page:** 4 automation lanes + per-pattern groove override
* **Scene persistence:** safe load for older scenes (optional fields)

## Controls

### Workflow quick reference

| Key | Action |
|---|---|
| `Fn + Tab` | Cycle `PERFORM → PATTERN → ARRANGE` |
| `Fn + Shift + Tab` | Cycle backward |
| `1` / `2` / `3` on PERFORM | Open PERFORM / PATTERN / ARRANGE |
| `Space` | Transport play / stop |
| `N` on PERFORM | Toggle NOTE mode |
| `[` / `]` on PERFORM | Previous / next scale |
| `-` / `=` on PERFORM | Octave down / up |
| `X` on PERFORM | Release the live-owned Synth A note |

### PERFORM — NOTE MODE: ON

![GroovePuter PERFORM keymap with NOTE mode enabled](docs/keymaps/cardputer_adv_perform_note_mode_on.svg)

`NOTE MODE: ON` is the default after reboot.

* `QWERTYUIOP` is the upper scale-aware manual.
* `ASDFGHJKL` is the lower scale-aware manual.
* The upper row is exactly one octave above the matching lower-row scale degree.
* Synth A is monophonic and uses last-note priority.
* While transport runs, note keys remain **reserved and consumed**, but emit no `NoteOn`. They cannot fall through to legacy randomize or BPM commands.
* `X` is a target-scoped live panic. It does not release PatternPlayer-owned Synth A/B voices.

### PERFORM — NOTE MODE: OFF

![GroovePuter PERFORM keymap with NOTE mode disabled](docs/keymaps/cardputer_adv_perform_note_mode_off.svg)

`NOTE MODE: OFF` returns the musical letters to the legacy command layer.

| Key | Legacy action |
|---|---|
| `I` | Randomize Synth A pattern |
| `O` | Randomize Synth B pattern |
| `P` | Randomize drums |
| `K` / `L` | BPM down / up |
| `N` | Return to NOTE mode |

> [!IMPORTANT]
> Page commands always get first refusal. NOTE mode only receives an unmodified key after the active page declines it. The step editor therefore keeps its own editing controls.

### Editor conventions

| Key | Common action |
|---|---|
| `Arrows` | Move cursor or navigate lists |
| `Enter` | Confirm, apply, or toggle the focused item |
| `Tab` | Change focus or section on supported pages |
| `Esc` | Back or dismiss |
| `Alt/Fn + 1..0` | Direct jump to detailed pages |
| `Q..I` | Pattern slots `1..8` in Pattern, Drum, and Song contexts |
| `1..9`, `0` | Track mutes when the active page does not consume the digit |

The canonical page-by-page reference is in [`src/ui/docs/keys.md`](src/ui/docs/keys.md). The Cardputer-ADV performance acceptance procedure is in [`docs/tests/PERFORMANCE_WORKFLOW_CARDPUTER_ADV.md`](docs/tests/PERFORMANCE_WORKFLOW_CARDPUTER_ADV.md).

## Screenshots

| Page | Preview |
| :--- | :--- |
| **Genre** | ![Genre](docs/screenshots/genre.png) |
| **Sequencer Hub** | ![Sequencer Hub](docs/screenshots/sequencer_hub.png) |
| **Drum Section** | ![Drum Page](docs/screenshots/drum_page_cyber.png) |
| **Synth Params** | ![Synth Params](docs/screenshots/synth_params.png) |
| **Pattern Edit** | ![Pattern Edit](docs/screenshots/pattern_edit.png) |
| **Song Page** | ![Song Page](docs/screenshots/song_page.png) |
| **Groove Lab** | ![Groove Lab](docs/screenshots/groove_lab.png) |

## MIDI & Drums

### MIDI Matrix Routing
The `MidiAdvance` dialog now features a unified **Track Map** (4x4 grid). This allows you to import complex MIDI files and route multiple source channels to any internal track (`Synth A`, `Synth B`, `Drums`). 
- **Auto-Routing:** Smart detection scans track names for keywords like "bass" or "percussion" to pre-configure imports.
- **Smart Destination:** Automatically finds empty patterns for a seamless workflow.

### Advanced Drum FX
The drum section now includes a dedicated FX chain to ensure your percussion cuts through the mix:
- **One-Knob Compressor:** Parallel compression for punch and weight.
- **Transient Shaper:** Independent control over attack snap and sustain tail.
- **Drum Reverb:** Algorithmic reverb specifically tuned for percussion.

## Requirements

* **Hardware:** M5Stack Cardputer ADV (ESP32-S3)
* **Tooling:** `arduino-cli` (recommended) or Arduino IDE 2.x
* **ESP32 core:** `esp32:esp32`

## Build & Flash
### Recommended

```bash
# Build release
./release.sh

# Upload to device
./upload.sh /dev/ttyACM0
```

### Manual (arduino-cli)

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc

arduino-cli upload \
  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  -p /dev/ttyACM0
```


## Troubleshooting

### Upload fails / can’t connect

* Use a **data** USB cable (not charge-only)
* Confirm the port exists:

  ```bash
  ls /dev/tty* | head
  ```
* Confirm board detection:

  ```bash
  arduino-cli board list
  ```

### Audio crackle under heavy load

* This build uses **adaptive FX safety** (FX dries out briefly instead of crackling).
* If crackling persists: reduce FX intensity (Tape mix/feedback, delay mix), then re-test.


## Contributing

This is an **experimental instrument**. If you want to contribute:

* Keep PRs small and testable.
* Prefer changes that preserve the core rule: **GENRE ≠ FEEL ≠ GENERATOR ≠ TEXTURE**.
* If you’re unsure where a change belongs, open an issue first.


## Credits

* Original inspiration: [urtubia/miniacid](https://github.com/urtubia/miniacid)
* Hardware: M5Stack Cardputer
* References: TB-303 / TR-808 lineage 

## License

MIT (`LICENSE`)
