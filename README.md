# GroovePuter v0.9.1

[![Status](https://img.shields.io/badge/status-release%20candidate-yellow)](#status)
[![Platform](https://img.shields.io/badge/platform-M5Stack%20Cardputer%20ADV-blue)](#requirements)
[![Build](https://img.shields.io/badge/build-arduino--cli-brightgreen)](#build--flash)

> **Portable standalone groovebox and hardware musical brain for M5Stack Cardputer ADV.**
> GroovePuter generates, varies, performs, and commits editable musical material for its
> own synths and drums, external instruments such as Yamaha SEQTRAK, and DAWs such as REAPER.

It is designed to remain fully useful with nothing connected while also serving as a
portable MIDI companion/controller and a source of structured musical material for
hardware synths and VST instruments.

Based on the original **MiniAcid** by [urtubia/miniacid](https://github.com/urtubia/miniacid).

## Status

**0.9.1 release candidate.** The release runtime is frozen on Cardputer ADV after the
Stage 15 generation line, runtime-recovery integration, Phrase/Song workflow recovery,
and final HUD cleanup. Release-facing documentation is being aligned without changing
the frozen musical/runtime behavior.

The normative release record is
[`docs/releases/0_9_1_RELEASE.md`](docs/releases/0_9_1_RELEASE.md).
Historical `0.9`/PR-#131 gate documents remain in the repository as implementation
evidence and are not 0.9.1 release gates.

The longer-term product boundary and prioritization rules are documented in
[`docs/PRODUCT_POSITIONING.md`](docs/PRODUCT_POSITIONING.md). That document describes
product direction; it does not imply that every future MIDI/DAW workflow is already
implemented in this release.

## Product model

GroovePuter is **standalone-first, but not standalone-only**. It has three first-class
operating roles:

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

### Standalone

GroovePuter remains a self-contained groovebox. The internal synth/drum engines,
generation, Pattern, Song, Phrase, performance and project workflows must remain useful
without a computer, SEQTRAK or any other MIDI device.

### Companion

GroovePuter can be the musical front end for external synthesizers and grooveboxes:
it owns musical structure, generation and performance intent while the external device
may own sound generation. Yamaha SEQTRAK is the first-class reference integration, but
other MIDI instruments remain valid targets and must not require a different musical
core.

### DAW brain

With REAPER, GroovePuter is intended to provide editable generated material and musical
performance controls rather than duplicate a desktop DAW. REAPER remains the natural
place for long-form arrangement, detailed automation, audio editing, mixing and
mastering. The same GroovePuter material should be usable with SEQTRAK or with VST
instruments by changing routing/profile rather than generation semantics.

SEQTRAK, REAPER and other MIDI devices are therefore optional targets, not runtime
dependencies.

The current workflow map has **12 active pages**:

```text
PERFORM:  MIDI KEYBOARD -> MIDI PLAYER
GENERATE: GENRE -> FEEL
HUB:      OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS
SONG:     SONG -> PHRASE -> PHRASE CORE
SETTINGS: PROJECT / SETUP
```

Persisted legacy `GENERATION` and `TEXTURE` page IDs resolve to FEEL. Persisted
standalone Synth A/B SOUND page IDs resolve to their owning Synth A/B page. Sound
editing now lives in each synth's local `NOTES -> KNOBS -> MORE` tabs.

The musical ownership rule remains:

```text
GENRE != FEEL != GENERATION REQUEST != SOUND
```

`GENRE` chooses the musical corridor and vocabulary. `FEEL` owns timing/velocity.
The session-scoped generation request carries P1/P2/P3 realization strength. Synth
and FX pages own sound design.

Future MIDI/DAW work should additionally preserve two product-level boundaries:

```text
MUSICAL ROLE != MIDI CHANNEL
STANDALONE != HOST DEPENDENCY
```

Roles such as drums, bass, chords and melody express musical intent. Device Profiles
project those roles onto channels/destinations appropriate for Standalone, Generic
MIDI, SEQTRAK or REAPER-oriented workflows.

## Main features

### Sound and arrangement

- Two swappable synth voices: `TB303`, `SID`, `AY`, `SH101`, `SN76489`, `WAVEMORPH`.
- Legacy OPL2 scene value is decode-only and falls back safely; OPL2 is not a current selectable engine.
- Drum sequencer with independent pattern/bank ownership and strong-rhythm generation.
- Pattern address model: `PAGE 1..16 x BANK A/B x SLOT 1..8`.
- Song slots `A/B`, pattern assignment, row editing, selection, markers, loop/reverse and LiveMix.
- Phrase Core slots `A/B/C/D`, lengths `1/2/4/8`, roles, derivation, Scene persistence and Song placement.
- Scene Save/Load preserves synth TYPE/parameters, patterns, Song, Phrase state and supported UI/session state.

### GENERATE: GENRE -> FEEL

#### GENRE 1/2

GENRE owns Genre/Variant/Rhythm and apply policy.

- `G` — explicit full Stage 15 generation.
- `P` — cycle `P1 CANON -> P2 VAR -> P3 TRANS`.
- `M` — cycle apply policy (`PROFILE`, `MATERIALIZE`, `MATERIALIZE+BPM`).
- `Enter` — apply according to the selected policy.
- Repeated accepted `G` requests reroll the same musical identity through a bounded
  session attempt stream rather than changing the selected Genre/Variant/P-level tuple.

While PLAY is active, full Genre generation is prepared off the sounding bar and
publishes at the next real `BAR_START`; the current bar is not stopped or mutated in
place. While stopped, generation commits immediately.

#### FEEL 2/2

FEEL owns timing and velocity only:

- profile: `STRAIGHT`, `SWING COMPAT`, `LAID BACK`, `PUSH/PULL`;
- swing;
- bounded feel amount;
- velocity variation;
- repeat cycle `1/2/4/8` bars;
- FEEL presets;
- `P` cycles the same shared P1/P2/P3 request selector.

FEEL does not choose notes, harmony, synth TYPE or timbre.

### P1 / P2 / P3

```text
P1 CANON   clearest identity / least transformation
P2 VAR     recognizable variation; boot/session default
P3 TRANS   stronger related transformation where vocabulary allows
```

P3 is not CHAOS. Drum `Alt+G` remains a separate explicit chaos command.

### SYNTH A / SYNTH B

Each synth page owns one three-state tab cycle:

```text
[N]KM  NOTES
N[K]M  KNOBS
NK[M]  MORE
```

`Tab` cycles these states. There are no separate runtime SOUND pages.

On NOTES, plain `G` outside NOTE ENTRY rerolls **only the selected synth voice** through
the active Genre/Variant/Rhythm/P-level/harmony identity. During PLAY the selected
lane publishes at the next bar boundary. Drums and the other synth remain unchanged.
Inside NOTE ENTRY, `G` remains a note key and does not trigger generation.

### DRUMS

On the main drum grid:

- `G` — drums-only strong generation at the current P-level;
- `Ctrl+G` — randomize the focused drum voice;
- `Alt+G` — explicit full-pattern CHAOS;
- `Ctrl+Alt+G` — explicit Stage 12 phrase audition/probe;
- `P` — cycle the shared P1/P2/P3 request level.

These commands are intentionally separate contracts.

### Phrase Core and Phrase -> Song

Phrase Core is the second SONG page. It has one visible `TO:` destination.

| Key | Action |
|---|---|
| `1..4` | Select Phrase `A/B/C/D` |
| `Up/Down` | Select capture/generation length `1/2/4/8` bars |
| `Left/Right` | Preview a saved Phrase bar |
| `Ctrl+Left/Right` | Move `TO:` by one row |
| `Ctrl+Up/Down` | Move `TO:` by eight rows |
| `R` / `Shift+R` | Next / previous capture role |
| `P` | Cycle derive parent |
| `Enter` | Capture current Song region as references |
| `D` | Derive parent into selected Phrase slot |
| `G` | Generate fresh connected `1/2/4/8B` material at `TO:` |
| `W` | INSERT saved Phrase before `TO:` and shift later rows |
| `Alt+W` | REPLACE Phrase lanes at `TO:` without row shift |
| `Backspace` / `Delete` | Clear selected Phrase |

Fresh Phrase generation is deliberately STOP-only. During PLAY it rejects with
`STOP PLAYBACK FOR PHRASE` instead of stopping/restarting transport behind the user's
back. Successful `G` or `W` advances `TO:` by the Phrase length.

Phrase slots use `REFERENCE VIEW / REF MUTABLE`: referenced pattern edits are visible
to Phrase without copying hidden note ownership into a second subsystem.

### Song

Song horizontal navigation forms one bounded strip across Song slots A/B:

- `Left/Right` moves `Synth A -> Synth B -> Drums`; crossing the outer track edge moves
  between edit Song slot A/B.
- `B` changes visible `PAT:A/B` assignment context without mutating Song cells.
- `Alt+B` flips the stored reference/selection bank.
- `Ctrl+B` selects playback Song slot A/B.
- `Q..I` assigns an existing slot in the visible pattern context.
- `G` generates material into a safe free slot and assigns the selected Song cell.
- double `G` materializes Synth A + Synth B + Drums for the current row as one logical mutation.

Copy-on-write generation must not silently overwrite patterns referenced elsewhere.

### PERFORMANCE TOOLS

Open the tools layer from MIDI KEYBOARD with `Tab`.

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
| `9` | Receiver `MONO/POLY` |
| `-` / `+` | Performance velocity `10..120` |

MONO/POLY is an external receiver-mode contract; on SEQTRAK Synth/DX targets it uses
the current receiver-mode MIDI control path. Held-note ownership remains bounded and
NoteOff cleanup remains target-scoped.

### MIDI Player and HUB MIDI

The realtime SMF workflow includes:

- physical-track mute mixer (`U`) and direct track mute hotkeys `1..9`;
- channel (`I`), structural (`S`) and performance (`D`) panels;
- `H` between MIDI Player and HUB MIDI;
- HUB MIDI `Up/Down` to select a projected layer;
- HUB MIDI plain `Left/Right` to change route immediately (`AUTO`, `CH1..CH10`), including during PLAY;
- HUB MIDI `Fn+Left/Right` to adjust selected physical-track level;
- HUB MIDI `Enter` or `1..9` to mute/unmute;
- HUB MIDI `S` for Solo;
- persisted per-file route overrides when file identity still matches.

Route changes do not require pausing playback and do not use Enter-to-commit. Stale
queued events from the previous route revision are rejected and held notes receive
scoped cleanup NoteOff events.

SEQTRAK-safe routing maps drum lanes to `CH1..CH7`, Synth 1 to `CH8`, Synth 2 to
`CH9`, and DX to `CH10`. RAW routing passes source channels through and therefore does
not accept explicit SEQTRAK destination overrides.

### UI and waveform HUD

The bottom performance HUD has one compositing owner. The optional waveform is drawn
beneath mute/activity digits, uses bounded visual auto-gain, and is cleared every
frame so partial-redraw pages do not accumulate stale waveform pixels. MIDI Player
uses the taller progress waveform introduced in the final 0.9.1 HUD fix.

The public theme cycle is `CARBON <-> CYBER`. Persisted compatibility styles may still
decode but are not part of the public theme cycle.

## Controls

### Global navigation

| Key | Action |
|---|---|
| `Alt+H` | Page-aware help |
| `Fn+M` | Workspace launcher |
| `Fn+Tab` / `Fn+Shift+Tab` | Next / previous workflow |
| `[` / `]` | Previous / next page inside workflow |
| `Fn+[` / `Fn+]` | Previous / next workflow |
| `Alt+[` / `Alt+]` | Previous / next pattern page |
| `Alt/Fn+1..0` | Direct page jump |
| `Space` | Active transport unless page owns it |
| `Alt+P` | MIDI Player |
| `Alt+V` | First GENERATE page (GENRE) |
| `Alt+W` | Waveform overlay except Phrase `Alt+W` REPLACE |
| `Alt+X` | LiveMix |
| `Alt+M` | Song mode |
| `Alt+\` | `CARBON <-> CYBER` |
| `1..0` | Track-mute fallback when page does not own the digit |
| `Esc` / `Backspace` / `` ` `` | Back/dismiss/previous page |
| `Ctrl+Alt+Backspace` | Panic active notes and reset project |

The active page gets first refusal before global fallbacks.

The canonical page-by-page reference is [`src/ui/docs/keys.md`](src/ui/docs/keys.md),
and `Alt+H` exposes the matching on-device help.

## Screenshots

| Firmware screen | Preview |
|:---|:---|
| **GENRE** | ![GENRE screen](docs/screenshots/genre.png) |
| **OVERVIEW / SEQUENCER HUB** | ![OVERVIEW screen](docs/screenshots/sequencer_hub.png) |
| **DRUMS** | ![DRUMS screen](docs/screenshots/drum_page_cyber.png) |
| **SYNTH A** | ![SYNTH A screen](docs/screenshots/synth_params.png) |
| **SYNTH A NOTES** | ![SYNTH A NOTES screen](docs/screenshots/pattern_edit.png) |
| **SONG** | ![SONG screen](docs/screenshots/song_page.png) |

## Requirements

- **Hardware:** M5Stack Cardputer ADV / ESP32-S3.
- **Memory profile:** PSRAM disabled.
- **Tooling:** `arduino-cli` plus dependencies installed by `scripts/install_arduino_deps.sh`.
- **Normal profile:** USB MIDI + CDC Serial via `scripts/build.sh`.
- **MIDI-only profile:** class-compliant USB MIDI via `scripts/build_seqtrak_midi_only.sh`.
- **Partition:** `huge_app` as selected by repository build scripts.

No external GPIO wiring is required for the standard release build.

## Build & flash

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

MIDI-only build:

```bash
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

## Quick release smoke

1. Boot and navigate all five workflows.
2. PLAY a pattern and press GENRE `G`; old bar must finish and the complete new material must appear at the next step 1.
3. Compare `P1/P2/P3` on the same Genre/Variant/Rhythm context.
4. Reroll Synth A and Synth B independently with plain `G` outside NOTE ENTRY.
5. Verify Drums `G`, `Ctrl+G`, `Alt+G` remain distinct.
6. In Phrase, verify `TO:`, `G`, `W`, `Alt+W` and `Ctrl+Arrow` destination movement.
7. Save, reboot and Load; verify synth TYPE/parameters, Song and Phrase state.
8. With SEQTRAK, smoke notes, route changes, MONO/POLY, velocity and cleanup.
9. Confirm waveform motion leaves no stale pixels and mute digits stay readable.

## Troubleshooting

### GENRE `G` changes mid-bar

That is a release failure while PLAY is active. Full Genre material must publish only
at the next `BAR_START`.

### Phrase `G` refuses while PLAY is active

Expected. Multi-row Phrase generation is intentionally STOP-only in 0.9.1.

### Phrase destination does not move with punctuation keys

Use `Ctrl+Arrow`. Cardputer's physical punctuation/arrow positions normalize to arrow
HID codes and are not reliable as independent raw comma/period controls on this page.

### HUB MIDI route does not change

Explicit route overrides require SEQTRAK routing mode. In RAW mode, source channels
pass through by design.

### Audio crackle or hangs under load

Record the exact SHA and `[PERF]` telemetry. Continuous underrun growth, watchdog reset,
or monotonic memory loss is a release-correctness defect.

## Contributing

- Keep changes narrow and testable.
- Use [`docs/PRODUCT_POSITIONING.md`](docs/PRODUCT_POSITIONING.md) when choosing between competing product directions.
- Keep GroovePuter fully usable as a standalone groovebox while improving external MIDI/DAW workflows.
- Do not revive legacy runtime `GENERATION`, `TEXTURE`, or standalone SOUND pages.
- Preserve Genre/Feel/generation-request/sound ownership boundaries.
- Preserve Phrase reference semantics until an explicit owned-event design is accepted.
- Preserve the existing transport, MIDI dispatcher and note-lifecycle owners.
- Keep SEQTRAK device-specific behavior in routing/profile/capability layers rather than the musical core.
- Prefer musical performance controls over duplicating general DAW automation/editing features.

## Credits

- Original inspiration: [urtubia/miniacid](https://github.com/urtubia/miniacid)
- Hardware: M5Stack Cardputer ADV
- References: TB-303 / TR-808 lineage

## License

MIT (`LICENSE`)
