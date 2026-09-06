# GroovePuter

[![Status](https://img.shields.io/badge/status-active%20development-yellow)](#development-status)
[![Platform](https://img.shields.io/badge/platform-M5Stack%20Cardputer%20ADV-blue)](#hardware)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

> **Portable standalone groovebox and hardware musical brain for M5Stack Cardputer ADV.**
>
> GroovePuter generates, varies, performs, and commits editable musical material for its
> own synths and drums, external instruments such as Yamaha SEQTRAK, and DAWs such as REAPER.

GroovePuter is designed to remain fully useful with nothing connected while also serving
as a portable MIDI companion/controller and a source of structured musical material for
hardware synths and VST instruments.

Based on the original **MiniAcid** by [urtubia/miniacid](https://github.com/urtubia/miniacid).

## Development status

GroovePuter is under active development.

The repository uses versioned development, integration, research and hardware-acceptance
branches. **`main` is the public project landing branch and must not be treated as the
source of truth for the newest experimental firmware behavior.**

Current release work is developed and validated on versioned branches such as:

- [`dev_0.9.8`](https://github.com/Ostrovsky42/GroovePuter/tree/dev_0.9.8) — safe persistent editing / Undo and hardware closure;
- [`dev_0.9.9`](https://github.com/Ostrovsky42/GroovePuter/tree/dev_0.9.9) — musical-material lifecycle and bounded activation;
- later research/stacked branches — MIDI input/controllers and future workflow work.

Before flashing a build, use the README, release document and acceptance notes from the
**exact branch/commit you intend to run**. A green software build is not automatically a
claim of Cardputer ADV hardware acceptance.

The longer-term product direction is documented in
[`docs/PRODUCT_POSITIONING.md`](docs/PRODUCT_POSITIONING.md).

## Product model

GroovePuter is **standalone-first, but not standalone-only**. It has three first-class
roles:

```text
                         GroovePuter
                              |
          +-------------------+-------------------+
          |                   |                   |
      STANDALONE          COMPANION           DAW BRAIN
          |                   |                   |
 internal synths         external synths       REAPER / VST
 drums + Song/Phrase     SEQTRAK first-class   editable MIDI material
 live performance        generic MIDI devices  arrangement / mix in DAW
```

### Standalone groovebox

GroovePuter must remain a self-contained instrument. Internal synths/drums, generation,
Pattern, Song, Phrase, performance and project workflows are first-class even when no
computer or external MIDI device is connected.

### Hardware companion

GroovePuter can own musical structure and performance intent while an external device
owns sound generation. Yamaha SEQTRAK is the first-class reference integration, while
the musical core remains generic enough for other MIDI instruments.

### DAW musical brain

With REAPER, GroovePuter is intended to generate and perform editable musical material,
not duplicate a desktop DAW. Long-form arrangement, detailed automation, audio editing,
mixing and mastering stay on the DAW side.

The target handoff is:

```text
Generate
   -> Audition / Variation
   -> Commit
   -> internal engines / SEQTRAK / generic MIDI / REAPER
```

## Architectural principles

The project deliberately separates musical identity, timing, persistence and routing.
The core rules are:

```text
GENRE != FEEL != GENERATION REQUEST != SOUND
PREPARE != COMMIT != ACTIVATE
MUSICAL ROLE != MIDI CHANNEL
STANDALONE != HOST DEPENDENCY
```

### Genre / Feel / Generation / Sound

- **GENRE** chooses the musical corridor and vocabulary.
- **FEEL** owns timing and velocity character.
- **GENERATION REQUEST** controls realization/variation intent.
- **SOUND** belongs to synth/drum/FX owners rather than being hidden inside generation.

### Prepare / Commit / Activate

Persistent musical work is moving toward an explicit lifecycle:

```text
PREPARE
  candidate creation
  no persistent mutation
        |
        v
COMMIT
  one persistent mutation
  one revision / Undo owner
        |
        v
ACTIVATE
  publish at the correct musical boundary
  no second persistent mutation
```

This keeps realtime activation separate from project-state ownership.

### Musical role / MIDI channel

Roles such as drums, bass, chords and melody express musical intent. Device Profiles
project those roles onto physical MIDI channels and destinations for SEQTRAK, Generic
MIDI, REAPER-oriented routing or future devices.

## Current 0.9.x development line

The active 0.9.x firmware line includes or is actively consolidating these capabilities:

- two swappable synth voices and an internal drum engine;
- genre-aware rhythm/tonal generation with bounded P1/P2/P3 variation semantics;
- Pattern, Song and Phrase workflows;
- live performance tools including arpeggiation, chords, strum, ratchet and Euclidean transformations;
- USB-MIDI output and transport integration;
- realtime SMF/MIDI-file playback, inspection, mute and routing workflows;
- SEQTRAK-specific routing/capability support without hardcoding SEQTRAK into the musical core;
- Generic MIDI / General MIDI / SEQTRAK Device Profile work;
- INTERNAL / MIDI / LAYER output ownership for local/external playback choices;
- bounded persistent-mutation / Undo ownership work;
- musical-boundary generation activation work;
- Scene/project persistence and recovery work;
- Cardputer ADV memory, realtime and hardware acceptance gates.

Exact availability varies by release branch. Treat branch-specific release documents and
acceptance evidence as authoritative over this summary.

## Product north star

A high-value end-to-end workflow is:

> Create synchronized **drums, bass, chords and melody**, audition alternatives through
> GroovePuter or SEQTRAK, commit the chosen material, and record it into separate REAPER
> tracks with stable timing and clean note lifecycle.

The project should be judged less by raw feature count and more by how reliably it turns
musical intent into reusable, editable material.

Preferred reusable musical units are generally:

```text
1 bar
2 bars
4 bars
8 bars
```

These lengths map naturally between GroovePuter Phrase/Song workflows, hardware targets
and DAW clips/items.

## What GroovePuter should not become

GroovePuter is not intended to replace REAPER or another full DAW.

Lower-priority or out-of-scope directions include:

- desktop-style unrestricted arrangement timelines;
- large general-purpose automation editors;
- full DAW-scale mixing/mastering workflows;
- audio comping and detailed waveform editing;
- plugin hosting/management as a central product feature.

A useful boundary is:

> If a control expresses a **musical decision**, it may belong in GroovePuter.  
> If it expresses detailed **production automation**, it probably belongs in the DAW.

## Screenshots

| Firmware screen | Preview |
|:---|:---|
| **GENRE** | ![GENRE screen](docs/screenshots/genre.png) |
| **OVERVIEW / SEQUENCER HUB** | ![OVERVIEW screen](docs/screenshots/sequencer_hub.png) |
| **DRUMS** | ![DRUMS screen](docs/screenshots/drum_page_cyber.png) |
| **SYNTH** | ![SYNTH screen](docs/screenshots/synth_params.png) |
| **PATTERN / NOTES** | ![PATTERN screen](docs/screenshots/pattern_edit.png) |
| **SONG** | ![SONG screen](docs/screenshots/song_page.png) |

## Hardware

Primary target:

- **M5Stack Cardputer ADV**;
- **ESP32-S3**;
- internal Cardputer display/keyboard;
- current normal release profiles do not rely on PSRAM;
- USB is used for MIDI/serial workflows depending on the selected build profile.

Yamaha SEQTRAK is the main external hardware reference target, but it is optional.
GroovePuter must continue to boot, generate, edit, perform, save and play without it.

## Building and testing

The active development branches contain the current build scripts, CI contracts and
release-specific instructions. Typical 0.9.x validation uses:

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

`bash scripts/upload.sh /dev/ttyACM0` builds the current sources with the selected
FQBN, checks the static DRAM budget, then uploads. A failed build or budget check
stops the upload even if an older binary exists. `--prebuilt` explicitly selects
`release_bins/miniacid.ino.bin` and fails if it is missing. Boot logs print the
firmware ELF SHA-256 and reset reason before hardware initialization; keep this
identity with hardware acceptance logs. A successful build is not hardware
acceptance.

For SD/boot investigation, `bash scripts/build_cardputer_recovery_diagnostics.sh`
builds a separate CDC image with error-level driver logs. This keeps the normal
startup and playback paths and runs the same DRAM gate. USB MIDI testing on a
computer also needs a MIDI consumer: `bash scripts/midi_sink.sh -q` drains MIDI
without producing audio. A serial monitor alone does not consume MIDI packets.

SEQTRAK MIDI-only builds on branches that provide that profile use:

```bash
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Do not use this landing-page summary as a substitute for the acceptance checklist tied
to the exact firmware SHA being tested.

## Documentation

Start with:

- [`docs/PRODUCT_POSITIONING.md`](docs/PRODUCT_POSITIONING.md) — product identity, boundaries and prioritization;
- the README on the versioned branch you intend to build;
- that branch's `docs/releases/` documents for release scope and acceptance;
- hardware acceptance notes for the exact Cardputer ADV candidate being flashed.

The repository intentionally keeps research, architecture, release and hardware evidence
separate so unfinished ideas are not silently presented as shipped behavior.

## Contributing

Keep changes narrow, testable and ownership-aware.

- Preserve standalone operation.
- Keep one owner per realtime responsibility.
- Do not add a second transport, scheduler, MIDI dispatcher or active-note owner.
- Keep SEQTRAK-specific behavior in routing/profile/capability layers.
- Keep generated material editable rather than turning generation into opaque output.
- Prefer bounded realtime structures and explicit failure behavior.
- Separate research/design claims from release-accepted behavior.

## Credits

- Original inspiration: [urtubia/miniacid](https://github.com/urtubia/miniacid)
- Hardware: M5Stack Cardputer ADV
- Reference hardware integration: Yamaha SEQTRAK
- DAW workflow target: REAPER

## License

MIT — see [`LICENSE`](LICENSE).
