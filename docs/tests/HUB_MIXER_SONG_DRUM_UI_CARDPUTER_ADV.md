# Hub mixer, Song clipboard, and drum-grid UI — Cardputer ADV acceptance

## Purpose

Verify both Hub mixer paths, project persistence of internal track levels, reliable Song pattern input/copy-paste, and compact readable drum-grid labels.

## Hardware list

- M5Stack Cardputer ADV;
- USB-C data cable;
- headphones or powered speaker for internal mixer verification;
- optional Yamaha SEQTRAK or USB MIDI monitor for HUB MIDI level verification.

## Wiring

No external wiring is required for the internal Hub, Song, or drum-grid checks. For HUB MIDI, connect Cardputer ADV USB-C to the normal class-compliant MIDI target. PORT.A is not used by this test.

## Build / Flash steps

```bash
git fetch origin
git switch agent/20260807-04-hub-mixer-song-drum-fixes
git reset --hard origin/agent/20260807-04-hub-mixer-song-drum-fixes
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Record `git rev-parse HEAD`. Change the serial port only if Cardputer ADV enumerates on a different device.

## Expected behavior

### Internal Sequencer Hub

- `Up/Down` selects one internal track;
- `Fn+Left/Right` changes only that selected track by 5%;
- `-` / `=` remains a compatibility volume path;
- the Hub range remains `0..120%`, and engine volume `1.0` is displayed as `100%`;
- track volume is scene/project state, so a synth or drum lane saved at `0%` remains at `0%` after reboot and loading the same project.

### HUB MIDI

- `Up/Down` selects a physical SMF track;
- plain `Left/Right` keeps the existing route-edit behavior;
- `Fn+Left/Right` changes only the selected physical SMF track in 5% steps;
- the selected level is visible as `V0..V100` in the lower detail line;
- level changes are applied at queue-consumer time, so already scheduled future NoteOn events use the current level before USB dispatch;
- `0%` suppresses future NoteOn events for that track without changing mute/solo state;
- loading a new SMF resets MIDI track levels to `100%`;
- route persistence remains unchanged.

### Song

- physical `Q..I` assigns slots 1..8; physical `W` reliably assigns slot 2;
- copy/paste keeps the exact composite ID (`page + bank + slot`) instead of recomputing it;
- rectangular selection may reach the side PLAY/EDIT button visually, but copy/cut/paste/generate operate only on real A/B/DR data columns;
- copying `2A2` and pasting it to another Song cell displays `2A2`.

### Drum grid

Lane labels are fixed to three glyphs:

```text
KIK SNR HH1 HH2 PR1 PR2 RIM CLP
```

TR-606 keeps `CYM` / `---` for its engine-specific final lanes. Step headers are compact one-glyph visual indices:

```text
1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6
```

Internal step numbers remain 1..16; only the rendered labels are compacted.

## Troubleshooting

### Internal volume does not survive reboot

Confirm the project/scene was explicitly saved after the volume change and that the same project was loaded. The persisted field is `trackVolumes`; runtime global mute flags are a separate feature.

### HUB MIDI volume appears delayed

Confirm the test firmware contains `smf_track_level.h`. Scaling is intentionally applied in the queue consumer, after scheduling and before USB ownership, so a normal level change must not wait for the SMF lookahead window.

### `W` still does not assign slot 2

Check Serial `[KEY]` diagnostics. One physical W press must produce one HID letter event; the duplicate `KeysState::word` copy must be suppressed.

### Song area paste shifts identifiers

Record lane focus mode, source selection, and destination. The side PLAY/EDIT column must never be counted as clipboard data.

### Drum labels overlap

Record the visual style and a screen photo. Every semantic lane label is exactly three glyphs and the left label area is 20 px.

## Acceptance checklist

- [ ] `bash tests/run_host_tests.sh` passes;
- [ ] `bash scripts/build.sh --warnings all` passes;
- [ ] internal Hub `Fn+Left/Right` changes only the selected track;
- [ ] internal `1.0` volume reads `100%`, and `1.2` reads `120%`;
- [ ] save a project with at least one synth and one drum lane at `0%`;
- [ ] reboot/load preserves both saved `0%` levels;
- [ ] HUB MIDI plain `Left/Right` still edits route;
- [ ] HUB MIDI `Fn+Left/Right` changes only the selected physical SMF track;
- [ ] HUB MIDI `0%` silences future notes and returning to `100%` restores them;
- [ ] a newly loaded SMF starts every MIDI track at `100%`;
- [ ] physical Song `W` assigns slot 2 exactly once;
- [ ] single-cell and rectangular Song copy/paste preserve composite IDs;
- [ ] selection touching PLAY/EDIT never inserts a blank clipboard column;
- [ ] drum labels are three glyphs wide in Minimal, Retro and Amber styles;
- [ ] step headers are `1..9,0,1..6` without crowding after step 9;
- [ ] no audio underruns, stuck notes, page errors, or Song reference corruption appear in Serial.
