# HUB MIDI Solo + Drum Grid Labels — Cardputer ADV acceptance

## Purpose

Validate the release-polish changes for Cardputer ADV:

- HUB MIDI arrangement activity is rendered as compact centered squares while preserving the existing density/mute color ramp.
- `S` toggles Solo for the selected MIDI track without pausing or stopping the MIDI Player.
- leaving Solo restores the mute mask that existed before Solo.
- the drum sequencer shows step numbers `1..16` above the grid and semantic lane names at the left.

## Hardware list

- M5Stack Cardputer ADV.
- microSD card containing one known multi-track `.mid` file.
- USB-C cable for build/upload and Serial.
- Optional Yamaha SEQTRAK only if the existing SEQTRAK routing path is part of the smoke test.

## Wiring

No new wiring is required. Use the normal Cardputer ADV USB-C connection. This test does not change PORT.A/I2C, audio wiring, USB ownership, or MIDI routing.

## Build / Flash

From the repository root on branch `agent/20260807-02-hub-solo-drum-grid-polish`:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh
bash scripts/upload.sh /dev/ttyACM0
```

If the Cardputer enumerates on another serial device, pass that device to `scripts/upload.sh` instead.

## Expected behavior

### HUB MIDI

1. Load and start a multi-track MIDI file in MIDI Player.
2. Open HUB MIDI with the existing Player/Hub navigation.
3. Each of the 16 activity cells per visible track is a compact square centered vertically in its row. Density and muted-track colors remain the same as before.
4. Select a sounding track with `Up/Down` and press `S` while playback is running.
5. Playback continues from the same transport position. Other MIDI tracks become muted and the selected track remains audible. Toast: `MIDI SOLO TRK xx`.
6. Move `Up/Down` and press `S` to move Solo to another track without stopping playback.
7. Press `S` again on the currently soloed track. Toast: `MIDI SOLO OFF`; the exact pre-Solo mute state is restored.
8. `A`, numeric mute hotkeys and `Enter` keep their previous mute behavior; manual mute leaves Solo deterministically instead of retaining stale Solo state.

### Drum sequencer

1. Open the main DRUMS sequencer grid.
2. The top row reads `1 2 3 ... 16`, aligned to the 16 step columns.
3. The left column identifies the established eight internal voices as `KICK`, `SNARE`, `HAT1`, `HAT2`, `PERC1`, `PERC2`, `RIM`, `CLAP` for normal drum engines.
4. On the 606 engine, the existing special mapping remains visible as `CYM` / unavailable final lane.
5. Cursor movement, step toggling, accent toggling, playback highlight and existing pattern data are unchanged.

## Troubleshooting

- `MIDI SOLO UNAVAILABLE`: load a valid multi-track MIDI session and reopen HUB MIDI.
- `MIDI LAYERS: SYNCING`: wait for the current SMF session projection to settle; if it persists, reload the MIDI file and inspect Serial for the existing SMF load error.
- Solo seems to leave another track audible: verify the file is using the normal SMF track mute path and is not producing audio from an unrelated external source.
- Drum labels overlap the grid: confirm the flashed SHA is from this branch and the display is using the normal 240x135 Cardputer ADV layout.

## Acceptance checklist

- [ ] `bash tests/run_host_tests.sh` passes.
- [ ] `bash scripts/build.sh` passes for Cardputer ADV DRAM-only build.
- [ ] HUB MIDI cells are visually square, not full-row rectangles.
- [ ] Cell fill intensity/muted colors still reflect the existing activity logic.
- [ ] `S` Solo works during active playback with no pause, restart, seek, or clock reset.
- [ ] Moving Solo to another track works while playback continues.
- [ ] Second `S` on the soloed track restores the exact previous mute state.
- [ ] `A`, `1..9`, and `Enter` mute controls still work.
- [ ] DRUMS shows step numbers `1..16`.
- [ ] DRUMS shows semantic lane names at the left without covering step 1.
- [ ] Existing drum patterns, cursor movement, accent row and playback highlight are unchanged.
- [ ] Serial shows no new SMF/session-generation errors during the smoke test.
