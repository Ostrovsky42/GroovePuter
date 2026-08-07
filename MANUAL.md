# GroovePuter Manual (Current Firmware)

This manual describes user-visible behavior in the current `dev_0.9` firmware. [`PLAN.md`](PLAN.md) is the roadmap; deferred pages, arranger work and experimental genre behavior are not documented as available functions.

For the exhaustive key map, use [`src/ui/docs/keys.md`](src/ui/docs/keys.md) and [`docs/keys_sheet.md`](docs/keys_sheet.md).

## Quick keys

| Key | Action |
|---|---|
| `Space` | Play/stop the active transport |
| `Arrows` | Move cursor, selection, or list focus |
| `Enter` | Confirm/apply/toggle |
| `Tab` | Change section; on PERFORM, open PERFORMANCE TOOLS |
| `Alt/Fn+1..0` | Direct page jump where mapped by the current key map |
| `Alt+[` / `Alt+]` | Previous/next detailed page |
| `Fn+Tab` | Next workflow |
| `Fn+Shift+Tab` | Previous workflow |
| `Alt+P` | Open MIDI Player |
| `Alt+W` | Toggle waveform overlay |
| `Alt+\` | Toggle `CARBON ↔ CYBER` |
| `Esc` | Back/dismiss |

Page-local commands have priority over global fallbacks.

## 1. Core concept

GroovePuter separates four responsibilities:

- `GENRE`: musical language and constraints;
- `GENERATION`: creation or mutation of notes and drum hits in the page that owns the action;
- `FEEL`: timing of existing material;
- `TEXTURE`: sound coloration and effects.

```text
GENRE != FEEL != GENERATION != TEXTURE
```

There is no release guarantee that each responsibility has a separate reachable main page. Song owns its cell/row generation actions; Genre owns its explicit Apply policy. Compatibility page IDs and persisted fields may remain even when a legacy page is not reachable from the current workflow.

## 2. Sound engines

Synth A and Synth B can select:

- `TB303`
- `SID`
- `AY` / YM2149
- `SH101`
- `SN76489`
- `WAVEMORPH`

OPL2 is not selectable. Its legacy persisted value remains decode-only and is normalized to `TB303` when an older Scene is loaded.

Until the 0.9 persistence gate is closed, do not assume that TYPE and every visible parameter survive Save/reboot/Load; follow `docs/releases/0_9_FINAL_ACCEPTANCE.md`.

## 3. Current workflow pages

The current runtime includes these supported surfaces:

- `Genre`: style, texture/profile selection and explicit Apply policy;
- `Pattern Edit A/B`: note, accent, slide, probability and selection editing;
- `Synth Params A/B`: select and edit the current synth engine;
- `Drum Sequencer`: drum grid and selection editing;
- `Song`: arrangements A/B, selection, generation, copy/paste, markers, reverse and loop;
- `Sequencer Hub`: compact pattern overview; HUB MIDI appears from a loaded MIDI Player session;
- `Feel/Texture`: timing and sound-macro controls where exposed by the current workflow;
- `Groove Lab`: mode/flavor and corridor preview;
- `Project`: Scene storage, MIDI import and settings;
- `PERFORM`: live keyboard, targets and PERFORMANCE TOOLS;
- `MIDI Player`: realtime SMF playback, inspection, mute and routing.

Legacy `Tape`, standalone `Generator` and compatibility page identifiers are not guaranteed main-workflow destinations in 0.9. Persisted Tape state and source files do not by themselves prove user-visible reachability.

## 4. Pattern identity and project lifecycle

A pattern address is:

```text
page + bank + slot
```

Examples: `1A1`, `2B7`.

Each project owns its pattern-page namespace. Save As copies the source project's pages into the new namespace. New starts with its own pages. Clear removes `.gpp`, `.tmp` and `.bak` only for the selected project. The binary pattern format remains version 3.

Use `docs/tests/PROJECT_PATTERN_STORAGE_CARDPUTER_ADV.md` for the release smoke.

## 5. Song workflow

### Assignment and editing

- `Q..I`: assign existing slot `1..8` in the selected bank; this does not regenerate content;
- `Ctrl+1..8`: select Song pattern page `1..8`;
- `B`: flip bank `A/B` for the selected cell or selection;
- `Alt+B`: edit Song slot A/B;
- `Ctrl+B`: play Song slot A/B;
- `X`: split compare;
- `Alt+X`: LiveMix on/off;
- `Bksp` or `Tab`: clear selected Song cell/area;
- `Ctrl+C` / `Ctrl+V`: copy/paste selection;
- `Ctrl+Z`: undo the last supported Song edit.

### Song generation

`G` generates a new pattern for the selected Song cell. The material is written into a safe free slot and the cell is assigned only after generation succeeds.

Double-tap `G` prepares Synth A, Synth B and Drums for the current row and commits the row as one logical mutation only after destination validation.

Generation uses copy-on-write. Generating one cell must not modify the old pattern or another Song cell that still references it.

`NO EMPTY PATTERN SLOTS` and `GENERATION FAILED` leave the Song assignment and destination banks unchanged.

Detailed navigation is in [`docs/SONG_PAGE_QUICKSTART.md`](docs/SONG_PAGE_QUICKSTART.md).

## 6. PERFORM and PERFORMANCE TOOLS

### Live keyboard

- `N`: NOTE mode on/off;
- `ASDFGHJKL`: lower scale-aware manual;
- `QWERTYUIOP`: upper manual;
- `\`: cycle target;
- `,` / `.`: previous/next scale;
- `-` / `=`: octave down/up;
- `X`: panic the live-owned target.

Targets include Synth A, Synth B, DX and native drums. Native drums use MIDI `CH1..CH7`; Synth A uses `CH8`, Synth B `CH9`, and DX `CH10` for SEQTRAK performance output.

While Pattern/Song transport owns Synth A, performance note keys remain consumed and do not emit competing notes.

### Tools layer

Press `Tab`, then:

| Key | Tool | Effect |
|---|---|---|
| `1` | ARPEGGIATOR | Enable/disable held-note arpeggiation |
| `2` | DIRECTION | Change arpeggiator direction |
| `3` | CHORD | Select chord expansion mode |
| `4` | MEMORY | Capture/clear held chord memory |
| `5` | STRUM | Change strum delay |
| `6` | RATCHET | Change repeated-hit count |
| `7` | EUCLIDEAN | Change pulses in a 16-step mask |
| `8` | ROTATE | Rotate the Euclidean mask |

Use `Shift+1..8` to cycle adjustable values backward. Generated events use the same event router and MIDI dispatcher as normal performance notes.

## 7. MIDI Player

Open with `Alt+P`, choose a MIDI file and press `Enter`.

### Playback and routing

- `Space`: MIDI Player play/pause;
- `G`: GroovePuter Pattern/Song transport;
- `M`: RAW / SEQTRAK-safe routing;
- `T`: original-file/project tempo mode;
- `C`: internal/SEQTRAK clock-source control;
- `Left/Right`: seek one bar; hold Shift for four bars;
- `Up/Down`: adjust the active tempo source where permitted.

RAW routing preserves source channels and ignores HUB MIDI per-track overrides. SEQTRAK-safe routing uses `CH1..CH7` for drums, `CH8` for Synth 1, `CH9` for Synth 2 and `CH10` for DX.

### Player panels

- `U`: open the physical-track mute mixer;
  - arrows select a physical SMF track;
  - `Enter` or `K` toggles the selected track;
  - `A` unmutes all tracks;
- `I`: channel inspector;
- `S`: structural inspector;
- `D`: performance/throughput panel;
- `Alt+W`: waveform overlay.

Direct `1..9` mute shortcuts are not documented as reliable for 0.9 until the final Cardputer ADV acceptance confirms them. Use the `U` mixer as the supported path.

### Player ↔ HUB MIDI

With a file loaded, press `H` to open HUB MIDI. Press `H` or `Esc` to return. Loaded-file generation, selected physical track, Player panel and scroll state are preserved.

HUB MIDI displays physical tracks and structural roles. Use arrows to select a physical track, `Enter` to toggle mute and `A` to unmute all. Direct numeric mute shortcuts remain acceptance-dependent.

For a per-track output override:

1. Pause/stop MIDI playback.
2. Use SEQTRAK-safe routing, not RAW.
3. Select a physical track.
4. Press `C`.
5. Choose `AUTO` or `CH1..CH10` with `Left/Right`.
6. Press `Enter` to commit, or `Esc` to cancel.

HUB MIDI does not own playback, scheduling, TinyUSB or note lifecycle. Confirmed route overrides use bounded CRC-protected per-file profiles. A changed, stale or corrupt file profile starts at `AUTO`.

## 8. Session persistence and themes

The UI session stores:

- active page;
- last page in each workflow;
- master volume;
- visual style;
- waveform-overlay state.

MIDI route profiles are persistent but separate from UI session state and Scene Save/Load.

The global theme shortcut cycles `CARBON ↔ CYBER`. `AMBER` remains for legacy compatibility and specialized old page code; it is not part of the normal global cycle.

After successful Scene Save, the status dirty `*` clears. A successful persistent Scene mutation adds `*`; browse/preview without Apply must not.

## 9. Safety and troubleshooting

- Monitor `[PERF]`; `underruns` must not continuously increase during normal playback and navigation.
- A failed Song generation or Apply transaction must not leave partial data or increment the revision.
- After Stop, page changes, Song changes, MIDI mutes, route changes or HUB navigation, verify no stuck internal or external notes remain.
- The fixed-DRAM CI gate is a regression budget, not hardware runtime acceptance.
- A TYPE or sixth-parameter persistence mismatch is a release blocker, not an expected limitation.

## 10. Documentation index

- [`README.md`](README.md): capabilities and build entry points;
- [`PLAN.md`](PLAN.md): roadmap and deferred scope;
- [`src/ui/docs/keys.md`](src/ui/docs/keys.md): page-by-page key map;
- [`docs/SONG_PAGE_QUICKSTART.md`](docs/SONG_PAGE_QUICKSTART.md): Song operations;
- [`docs/GROOVE_LAB.md`](docs/GROOVE_LAB.md): mode/flavor/corridors;
- [`docs/MIDI_IMPORT_GUIDE.md`](docs/MIDI_IMPORT_GUIDE.md): MIDI import;
- [`docs/releases/PRE_0_9_RELEASE_GATE.md`](docs/releases/PRE_0_9_RELEASE_GATE.md): release blockers;
- [`docs/releases/0_9_FINAL_ACCEPTANCE.md`](docs/releases/0_9_FINAL_ACCEPTANCE.md): exact final validation record.
