# GroovePuter Manual (Current Firmware)

This manual describes user-visible behavior in the current firmware. [`PLAN.md`](PLAN.md) is the roadmap; planned Phrase slots, retrospective capture, patch libraries, and other deferred work are not documented here as available functions.

For the exhaustive key map, use [`src/ui/docs/keys.md`](src/ui/docs/keys.md) and [`docs/keys_sheet.md`](docs/keys_sheet.md).

## Quick keys

| Key | Action |
|---|---|
| `Space` | Play/stop the active transport |
| `Arrows` | Move cursor, selection, or list focus |
| `Enter` | Confirm/apply/toggle |
| `Tab` | Change section; on PERFORM, open PERFORMANCE TOOLS |
| `Alt/Fn+1..0` | Direct page jump |
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

* `GENRE`: musical language and constraints;
* `GENERATOR`: creation or mutation of notes and drum hits;
* `FEEL`: timing of existing material;
* `TEXTURE`: sound coloration and effects.

```text
GENRE != FEEL != GENERATOR != TEXTURE
```

## 2. Sound engines

Synth A and Synth B can select:

* `TB303`
* `SID`
* `AY` / YM2149
* `SH101`
* `SN76489`
* `WAVEMORPH`

OPL2 is not selectable. Its legacy persisted value remains decode-only and is normalized to `TB303` when an older Scene is loaded.

## 3. Main pages

* `Genre`: style, texture, preset, and apply policy.
* `Pattern Edit A/B`: note, accent, slide, probability, and selection editing.
* `Synth Params A/B`: select and edit the current synth engine.
* `Drum Sequencer`: drum grid and selection editing.
* `Song`: two arrangements (`A/B`), split compare, live mix, generation, copy/paste, markers, reverse, and loop.
* `Sequencer Hub`: compact pattern overview; HUB MIDI appears when entered from a loaded MIDI Player session.
* `Feel/Texture`: timing and sound macros.
* `Generator`: generative parameters.
* `Groove Lab`: mode/flavor and corridor preview.
* `Project`: Scene storage, MIDI import, and settings.
* `Tape`: loop/FX performance page.
* `PERFORM`: live keyboard, targets, and PERFORMANCE TOOLS.
* `MIDI Player`: realtime SMF playback, inspection, mute, and routing.

## 4. Song workflow

### Assignment and editing

* `Q..I`: assign existing pattern `1..8` in the selected bank. This does not regenerate pattern content.
* `Ctrl+1..8`: select Song pattern page `1..8`.
* `B`: flip bank `A/B` for the selected cell or selection.
* `Alt+B`: edit Song slot A/B.
* `Ctrl+B`: play Song slot A/B.
* `X`: split compare.
* `Alt+X`: LiveMix on/off.
* `Bksp` or `Tab`: clear selected Song cell/area.
* `Ctrl+C` / `Ctrl+V`: copy/paste selection.
* `Ctrl+Z`: undo the last supported Song edit.

### Song generation

`G`

Generate a new pattern for the selected Song cell. The generated material is written into a safe free pattern slot and the cell is assigned to that slot only after generation succeeds.

Double-tap `G`

Generate Synth A, Synth B, and Drums for the current Song row. Destination slots are checked and all material is prepared before anything is committed. The row is one logical Scene mutation.

`Q..I`

Assign an already existing pattern without regenerating its content.

Generation uses copy-on-write. Generating one Song cell does not modify the old pattern or other Song cells that still reference it.

`NO EMPTY PATTERN SLOTS` means generation changed nothing. Free an unused, unreferenced slot or switch to another pattern page/bank, then retry.

`GENERATION FAILED` also leaves the Song row and destination banks unchanged.

Detailed Song navigation is in [`docs/SONG_PAGE_QUICKSTART.md`](docs/SONG_PAGE_QUICKSTART.md).

## 5. PERFORM and PERFORMANCE TOOLS

### Live keyboard

* `N`: NOTE mode on/off.
* `ASDFGHJKL`: lower scale-aware manual.
* `QWERTYUIOP`: upper manual, one octave above the matching lower keys.
* `\`: cycle target.
* `,` / `.`: previous/next scale.
* `-` / `=`: octave down/up.
* `X`: panic the live-owned target.

Targets include Synth A, Synth B, DX, and native drums. Native drums use MIDI `CH1..CH7`; Synth A uses `CH8`, Synth B `CH9`, and DX `CH10` for SEQTRAK performance output.

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

Use `Shift+1..8` to cycle adjustable values backward. The generated events use the same event router and MIDI dispatcher as normal performance notes.

## 6. MIDI Player

Open with `Alt+P`, choose a MIDI file, and press `Enter`.

### Playback and routing

* `Space`: MIDI Player play/pause.
* `G`: GroovePuter Pattern/Song transport.
* `M`: toggle RAW / SEQTRAK-safe routing.
* `T`: toggle original-file/project tempo mode.
* `C`: internal/SEQTRAK clock-source control.
* `Left/Right`: seek one bar; hold Shift for four bars.
* `Up/Down`: adjust the active tempo source where permitted.

RAW routing preserves source MIDI channels and ignores HUB MIDI per-track overrides. SEQTRAK-safe routing uses `CH1..CH7` for drums, `CH8` for Synth 1, `CH9` for Synth 2, and `CH10` for DX.

### Player panels

* `U`: physical-track mute mixer.
  * Arrows select a physical SMF track.
  * `Enter` or `K` toggles the selected track.
  * `A` unmutes all tracks.
* `1..9`: toggle the first nine projected audible physical tracks without opening the mixer.
* `I`: channel inspector.
* `S`: structural inspector.
* `D`: performance/throughput panel.
* `Alt+W`: waveform overlay.

### Player ↔ HUB MIDI

With a file loaded, press `H` to open HUB MIDI. Press `H` or `Esc` to return. The current loaded-file generation, selected physical track, Player panel, and scroll state are preserved during this navigation.

HUB MIDI displays physical SMF tracks and structural roles. Arrows select a physical track; `Enter` or `1..9` toggles mute; `A` unmutes all.

For a per-track output override:

1. Pause/stop MIDI playback.
2. Use SEQTRAK-safe routing, not RAW.
3. Select a physical track.
4. Press `C`.
5. Choose `AUTO` or `CH1..CH10` with `Left/Right`.
6. Press `Enter` to commit, or `Esc` to cancel.

HUB MIDI does not own playback, scheduling, TinyUSB, or note lifecycle. Confirmed route overrides are stored in bounded CRC-protected per-file profiles. Reopening the same unchanged file, including after reboot, restores its `AUTO` / `CH1..CH10` assignments. A different, modified, stale, or corrupt file profile starts at `AUTO`.

## 7. Session persistence and themes

The UI session stores:

* active page;
* last page in each workflow;
* master volume;
* visual style;
* waveform-overlay state.

MIDI track-route profiles are persistent but separate from UI session state and Scene Save/Load. They are matched by normalized path, file size, physical-track count, and route-relevant semantic fingerprint.

The global theme shortcut cycles `CARBON ↔ CYBER`. `AMBER` remains only for legacy compatibility and specialized old page code; it is not part of the normal global theme cycle.

Scene Save/Load is separate from UI session persistence. After successful Save, the status-chrome dirty `*` clears. A successful persistent Scene mutation adds `*`.

## 8. Tape workflow

* `X`: smart REC/PLAY/DUB flow.
* `A`: capture.
* `S`: thicken.
* `D`: wash.
* `G`: loop mute.

See [`docs/TAPE_WORKFLOW.md`](docs/TAPE_WORKFLOW.md).

## 9. Safety and troubleshooting

* Monitor `[PERF]`; `underruns` must not continuously increase during normal playback and navigation.
* A Song generation error must not leave partial pattern data, a changed Song reference, or a dirty revision.
* After Stop, page changes, Song changes, MIDI mutes, or HUB navigation, verify there are no stuck notes.
* The fixed-DRAM CI gate is a regression budget, not a substitute for hardware runtime acceptance.

## 10. Documentation index

* [`README.md`](README.md): current capabilities and build entry points.
* [`PLAN.md`](PLAN.md): roadmap and deferred scope.
* [`src/ui/docs/keys.md`](src/ui/docs/keys.md): page-by-page key map.
* [`docs/SONG_PAGE_QUICKSTART.md`](docs/SONG_PAGE_QUICKSTART.md): Song operations.
* [`docs/GROOVE_LAB.md`](docs/GROOVE_LAB.md): mode/flavor/corridors.
* [`docs/MIDI_IMPORT_GUIDE.md`](docs/MIDI_IMPORT_GUIDE.md): MIDI import.
* [`docs/TAPE_WORKFLOW.md`](docs/TAPE_WORKFLOW.md): Tape page.
* [`docs/stages/`](docs/stages/): implementation and acceptance records.
