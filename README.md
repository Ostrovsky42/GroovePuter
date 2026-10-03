# GroovePuter

[![Status](https://img.shields.io/badge/status-public%20beta%20candidate-orange)](#current-status)
[![Platform](https://img.shields.io/badge/platform-M5Stack%20Cardputer%20ADV-blue)](#hardware)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

## Generate a groove. Keep what works. Develop it into a song.

**GroovePuter is a pocket composition groovebox for M5Stack Cardputer ADV.**

It generates editable drums, bass, harmony and melody, lets you keep the material you like, and develops related sections instead of solving every musical decision with another unrelated random pattern.

```text
GENERATE
   ↓
KEEP + EDIT
   ↓
DEVELOP
   ↓
SONG / MIDI / external gear
```

GroovePuter works standalone with its internal synths and drums, and can also play or control external MIDI instruments such as Yamaha SEQTRAK.

Originally based on [MiniAcid](https://github.com/urtubia/miniacid); the current project has grown into a substantially larger composition, sequencing, MIDI and musical-material system.

## Why it is different

### Generate
Genre-aware rhythm, bass, harmonic and melodic material is generated as editable project data, not as an opaque audio result.

### Keep + edit
Accepted material remains yours: it can be edited, arranged, saved, undone and reused rather than disappearing when you ask for another idea.

### Develop
The development workflow is built around a different question from ordinary randomization:

> Given the material I accepted, what may change, what must stay recognizable, and what related section should come next?

That machinery is still being productized, but it is the central direction of GroovePuter.

## Current status

The active line is a **0.9.14 / 0.9.15 public-beta candidate**. Software CI is consolidated into seven product-facing pipelines; Cardputer ADV hardware acceptance for the current USB Host / external-keyboard line is still in progress.

This means:

- the project is actively developed and tested;
- the current candidate is ahead of the last packaged GitHub release;
- a green build is not automatically a hardware-release claim;
- public installer/Launcher packaging for the new candidate is not finished yet.

Until hardware closure is complete, use the candidate branch and its acceptance notes rather than assuming `main` or the old release assets describe the newest firmware.

Current integration work: [`#477`](https://github.com/Ostrovsky42/GroovePuter/pull/477).

## First musical win on the current candidate

The intended product gesture is simple:

```text
CREATE TAKE
    ↓
DEVELOP
```

The **current beta candidate is not fully simplified yet**. Its bounded flagship development path currently requires a 4-bar REWORK/P3 take:

1. `Space` — start playback.
2. `Alt+V` — open GENRE; choose Genre / Variant / Rhythm.
3. `G` — generate and listen.
4. `Fn+M` — open the workspace launcher and go to MATERIAL.
5. Set `LENGTH = 4B` and `STYLE = REWORK` (P3).
6. `G` — create a fresh TAKE.
7. `D` — develop the eligible TAKE into related DEVELOPMENT / BREAK material.
8. `Ctrl+Z` — undo the retained cycle if you do not want it.

This extra P3 setup is a known first-run UX blocker, not the desired final onboarding flow.

For the complete current key map, see [`src/ui/docs/keys.md`](src/ui/docs/keys.md). On-device page-aware help is available with `Alt+H`.

## Screenshots

| Firmware screen | Preview |
|:---|:---|
| **GENRE** | ![GENRE screen](docs/screenshots/genre.png) |
| **OVERVIEW / SEQUENCER HUB** | ![OVERVIEW screen](docs/screenshots/sequencer_hub.png) |
| **DRUMS** | ![DRUMS screen](docs/screenshots/drum_page_cyber.png) |
| **SYNTH** | ![SYNTH screen](docs/screenshots/synth_params.png) |
| **PATTERN / NOTES** | ![PATTERN screen](docs/screenshots/pattern_edit.png) |
| **SONG** | ![SONG screen](docs/screenshots/song_page.png) |

## Standalone + MIDI

GroovePuter is **standalone-first, but not standalone-only**.

```text
                         GroovePuter
                              |
          +-------------------+-------------------+
          |                   |                   |
      STANDALONE          COMPANION           DAW / MIDI
          |                   |                   |
 internal synths         external synths       editable material
 drums + Song/Phrase     SEQTRAK / MIDI        routing / recording
 live performance        external keyboard      downstream arrange
```

The musical material should remain independent of where sound is produced. A bass or melody idea can belong to GroovePuter even when an external device owns the actual synth voice.

## Hardware

Primary target:

- **M5Stack Cardputer ADV** / ESP32-S3;
- internal display and keyboard;
- DRAM-only product configuration (no PSRAM dependency);
- internal synth and drum engines;
- SD-backed project/material storage;
- USB/MIDI integration depending on the selected boot role/profile.

Yamaha SEQTRAK is the main external reference device but is optional.

## Building the current candidate

For development builds:

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
```

The accepted Cardputer path uses the repository's FS1B dynamic-FatFs build and the exact release/hardware procedure tied to the candidate SHA. Do not flash an arbitrary stock-FatFs build as a substitute for release evidence.

The active GitHub Actions surface is intentionally small:

```text
core.yml
cardputer-adv.yml
midi-targets.yml
memory.yml
release.yml
nightly.yml
research-manual.yml
```

Many more test scripts and research contracts remain behind those entry points. CI surface was reduced; verification depth was not.

## Documentation

Start at [`docs/README.md`](docs/README.md).

Useful direct links:

- [`src/ui/docs/keys.md`](src/ui/docs/keys.md) — canonical current key map;
- [`docs/PRODUCT_POSITIONING.md`](docs/PRODUCT_POSITIONING.md) — product boundaries and long-term direction;
- [`docs/releases/`](docs/releases/) — release and hardware-acceptance evidence;
- [`docs/0.9.14/`](docs/0.9.14/) — current musical-development research and contracts;
- [`docs/ci/CI_ORCHESTRATION.md`](docs/ci/CI_ORCHESTRATION.md) — CI ownership after productization.

The repository contains a large historical research corpus. Historical stage/checkpoint documents are evidence and design history; they are not automatically descriptions of the current UI.

## Engineering notes

GroovePuter deliberately separates musical identity, realtime timing, persistence and routing. Some recurring architectural rules are:

```text
GENRE != FEEL != SOUND
PREPARE != COMMIT != ACTIVATE
MUSICAL ROLE != MIDI CHANNEL
VERIFICATION DEPTH != CI SURFACE
```

The deeper architecture, semantic research, memory work and hardware acceptance remain public because they are useful engineering evidence — they are simply no longer the first thing a new user should have to understand.

## Contributing

Keep changes narrow, testable and ownership-aware.

- Preserve standalone operation.
- Keep one owner per realtime responsibility.
- Do not add duplicate transport, scheduling or active-note ownership.
- Keep generated material editable.
- Prefer bounded realtime structures and explicit failure behavior.
- Separate research claims from release-accepted behavior.

## Credits

- Original project lineage: [urtubia/miniacid](https://github.com/urtubia/miniacid)
- Hardware: M5Stack Cardputer ADV
- Reference external integration: Yamaha SEQTRAK

## License

MIT — see [`LICENSE`](LICENSE).
