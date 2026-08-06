# GroovePuter v0.1.0

[![Status](https://img.shields.io/badge/status-beta-yellow)](#status)
[![Platform](https://img.shields.io/badge/platform-M5Stack%20Cardputer%20ADV-blue)](#requirements)
[![Build](https://img.shields.io/badge/build-arduino--cli-brightgreen)](#build--flash)

> **Portable real-time groove computer for M5Stack Cardputer ADV.**
> A time-feel instrument that separates musical language, generated material,
> movement in time, and current sound.

Based on the original **MiniAcid** by [urtubia/miniacid](https://github.com/urtubia/miniacid). This fork focuses on generative patterns, timing FEEL, scene persistence, arrangement, portable performance, and bounded MIDI integration.

## Status

**Beta.** The core groovebox, Song, performance, USB-MIDI, and realtime SMF workflows are usable on Cardputer ADV. APIs and UI may still change.

[`PLAN.md`](PLAN.md) is the roadmap. This README describes capabilities present in the current firmware; it does not promote planned work to shipped behavior.

## Product model

GroovePuter is a standalone groovebox. Yamaha SEQTRAK and other MIDI devices are optional output targets, not runtime dependencies.

```text
GENRE != FEEL != GENERATOR != TEXTURE
```

```text
GroovePuter
├── generators, patterns, synths and drums
├── independent GENRE / FEEL / GENERATOR / TEXTURE decisions
├── pattern and Song arrangement
├── live performance keyboard and performance tools
├── sample-timed USB-MIDI output and transport
└── realtime SMF playback, inspection and routing
```

Atlas remains an optional source of curated factory seed patterns.

## Features

### Sound and arrangement

* **Two swappable synth voices.** The selectable runtime catalog is:
  * `TB303`
  * `SID`
  * `AY` / YM2149
  * `SH101`
  * `SN76489`
  * `WAVEMORPH`
* **Legacy OPL2 scene compatibility:** the persisted OPL2 enum/value is decode-only. Loading or requesting it falls back to `TB303`; OPL2 is not a selectable runtime engine.
* **TR-808-inspired drum section** with per-pattern automation, groove overrides, compression, transient shaping, and reverb.
* **Pattern and Song arrangement** with two Song slots (`A/B`), split compare, live mix, reverse/loop controls, markers, and block copy/paste.
* **FEEL:** `1/8`, `1/16`, `1/32`; half/normal/double timebase; `1B` through `8B` lengths.
* **TEXTURE:** Lo-Fi, Drive, Tape/space coloration, and Drum FX.
* **Genre-driven generation:** rhythmic masks, scale/range constraints, density traits, mode/flavor corridors, and deterministic generation domains.

### PERFORMANCE TOOLS

Open the PERFORM tools layer with `Tab`. The current firmware provides:

| Key | Tool |
|---|---|
| `1` | ARPEGGIATOR |
| `2` | DIRECTION |
| `3` | CHORD |
| `4` | MEMORY |
| `5` | STRUM |
| `6` | RATCHET |
| `7` | EUCLIDEAN |
| `8` | ROTATE |

`Shift+1..8` cycles adjustable tools backward. Generated performance events use the existing performance event router and MIDI dispatcher; the tools do not create a second MIDI writer or scheduler.

### Song Generate

Song pattern references and pattern content are distinct. The controls deliberately separate assignment from generation:

| Key | Song action |
|---|---|
| `Q..I` | Assign an already existing pattern slot `1..8`; do not regenerate its content |
| `G` | Generate real material into a safe free pattern slot, then assign the selected Song cell to that slot |
| double-tap `G` | Generate and materialize Synth A, Synth B, and Drums for the current Song row as one logical mutation |

Generation is copy-on-write: it never silently overwrites a pattern referenced by another Song cell. If no safe slot is available, `NO EMPTY PATTERN SLOTS` is shown and Scene data is unchanged.

### MIDI Player and HUB MIDI

The realtime SMF workflow includes:

* physical-track mute mixer (`U`) and direct physical-track mute hotkeys `1..9`;
* channel inspector (`I`);
* structural inspector (`S`);
* performance/throughput panel (`D`);
* waveform overlay (`Alt+W`);
* Player → HUB MIDI navigation with `H` after a file is loaded;
* return from HUB MIDI with `H` or `Esc`, preserving the loaded-file session, Player panel state, scroll, and selection;
* physical SMF track selection in the mute mixer and HUB MIDI;
* per-physical-track route override in HUB MIDI: `AUTO` or `CH1..CH10`;
* route editing with `C`, `Left/Right`, and `Enter` while playback is stopped or paused in SEQTRAK routing mode;
* bounded per-file route profiles restore `AUTO` / `CH1..CH10` assignments across reloads and reboots when the normalized path, size, physical-track count, and semantic fingerprint still match. A changed, corrupt, or unknown file starts at `AUTO`.

SEQTRAK-safe routing maps drums to `CH1..CH7`, Synth 1 to `CH8`, Synth 2 to `CH9`, and DX to `CH10`. RAW routing passes source MIDI channels through and does not apply SEQTRAK track overrides.

HUB MIDI is an inspection/mute/routing surface. It does not own SMF transport, scheduling, TinyUSB writes, or note lifecycle.

### Persistence and UI

* Scene Save/Load persists patterns, Song references, synth/drum state, and the supported scene codec fields.
* UI session persistence stores the active page, last page in each workflow, master volume, visual style, and waveform-overlay state.
* Player ↔ HUB MIDI panel, scroll, and selection state is preserved during the current loaded session. Per-file route overrides are stored separately in bounded CRC-protected profiles and can be restored after reopening the same unchanged file or rebooting; a different or changed file starts at `AUTO`.
* The public global theme cycle is `CARBON ↔ CYBER`. `AMBER` remains a legacy compatibility value for older UI/session data and specialized page code, not a normal selectable theme in the global cycle.

## Controls

### Global navigation

| Key | Action |
|---|---|
| `Alt+H` | Open page-aware help for the active screen; `Up/Down` scrolls and `Left/Right` jumps to the top/end |
| `Fn+M` | Open the workspace launcher |
| `Fn+Tab` / `Fn+Shift+Tab` | Cycle workflow forward / backward |
| `[` / `]` | Previous / next page inside the current workflow |
| `Fn+[` / `Fn+]` | Previous / next workflow |
| `Alt+[` / `Alt+]` | Previous / next detailed page |
| `Alt/Fn+1..0` | Direct page jump |
| `Space` | Active transport play / stop; the active page gets first refusal |
| `Alt+P` | Open MIDI Player |
| `Alt+V` | Open MODE / FLAVOR (Groove Lab) |
| `Alt+W` | Toggle waveform overlay |
| `Alt+X` | Toggle LiveMix |
| `Alt+M` | Toggle Song mode |
| `Alt+\` | Toggle `CARBON ↔ CYBER` |
| `1..0` | Track-mute fallback when the active page does not consume the digit |
| `Esc` / `Backspace` / `` ` `` | Back, dismiss, or return to the previous page |
| `Ctrl+Alt+Backspace` | Reset the current project after releasing active notes |

The active page receives keys before global fallbacks. This prevents page-local commands, NOTE mode, Song assignment, and MIDI mute controls from colliding.

### PERFORM

`NOTE MODE: ON` is the default after reboot.

* `QWERTYUIOP` is the upper scale-aware manual.
* `ASDFGHJKL` is the lower manual, one octave below the matching upper keys.
* `\` cycles the target: internal/USB synth targets, DX, and native seven-lane drums.
* `,` / `.` select scale; `-` / `=` shift octave; `X` sends panic for the live-owned target.
* While Pattern/Song transport owns Synth A, live note keys remain consumed and do not emit competing `NoteOn` events.
* `Tab` opens PERFORMANCE TOOLS.

With NOTE mode off, the legacy `I`, `O`, `P`, `K`, and `L` pattern/BPM commands are available again.

The canonical page-by-page key reference is [`src/ui/docs/keys.md`](src/ui/docs/keys.md).

## Screenshots

The previews below use the current `CYBER` UI. Their footer hints are the same primary controls shown by `Alt+H`; the overlay also lists secondary commands that do not fit in the 240×135 footer.

| Firmware screen | Preview |
|:---|:---|
| **GENRE** | ![GENRE screen](docs/screenshots/genre.png) |
| **OVERVIEW / SEQUENCER HUB** | ![OVERVIEW screen](docs/screenshots/sequencer_hub.png) |
| **DRUMS** | ![DRUMS screen](docs/screenshots/drum_page_cyber.png) |
| **SYNTH A SOUND** | ![SYNTH A SOUND screen](docs/screenshots/synth_params.png) |
| **SYNTH A PATTERN** | ![SYNTH A PATTERN screen](docs/screenshots/pattern_edit.png) |
| **SONG** | ![SONG screen](docs/screenshots/song_page.png) |
| **MODE / FLAVOR** | ![MODE / FLAVOR screen](docs/screenshots/groove_lab.png) |

## MIDI ownership

Accepted live, Pattern, Song, transport, and realtime SMF events converge on the existing event router and sample-timed USB-MIDI dispatcher. The dispatcher remains the sole TinyUSB MIDI writer. New pages and performance tools must not add parallel schedulers or direct `tud_midi_*` calls.

## Requirements

* **Hardware:** M5Stack Cardputer ADV with ESP32-S3FN8.
* **Memory profile:** PSRAM disabled.
* **Tooling:** `arduino-cli` and the dependencies pinned by `scripts/install_arduino_deps.sh`.
* **Normal profile:** USB MIDI plus CDC Serial, built by `scripts/build.sh`.
* **MIDI-only profile:** class-compliant USB MIDI without CDC, built by `scripts/build_seqtrak_midi_only.sh`.
* **Partition:** `huge_app` as set by the build scripts.

Fixed-DRAM checks are CI regression gates against the repository budget. They are not presented as proof of a universal runtime safety threshold; hardware acceptance and runtime telemetry remain required.

## Build & Flash

Normal Cardputer ADV build:

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

MIDI-only build:

```bash
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

No external wiring is required. The firmware uses the built-in screen, keyboard, ES8311 audio codec, speaker/headphone output, and USB-C for flashing, Serial, and USB MIDI.

## Troubleshooting

### Upload fails

Use a data-capable USB cable and confirm the port:

```bash
arduino-cli board list
```

### Song generation shows `NO EMPTY PATTERN SLOTS`

No pattern or Song reference was changed. Clear an unused pattern slot that is not referenced by Song A or Song B, or switch to another pattern page/bank and retry.

### Audio crackle or hangs under load

Reduce Tape/delay intensity and monitor `[PERF]`. `underruns` must not grow continuously during ordinary playback and navigation.

## Contributing

* Read [`PLAN.md`](PLAN.md) before proposing a new feature lane.
* Keep PRs narrow and testable.
* Preserve **GENRE ≠ FEEL ≠ GENERATOR ≠ TEXTURE**.
* Preserve standalone groovebox behavior and the existing transport/MIDI owners.

## Credits

* Original inspiration: [urtubia/miniacid](https://github.com/urtubia/miniacid)
* Hardware: M5Stack Cardputer ADV
* References: TB-303 / TR-808 lineage

## License

MIT (`LICENSE`)
