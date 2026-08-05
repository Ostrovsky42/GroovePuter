# MIDI Player ↔ HUB · MIDI navigation

## Purpose

Provide a lossless MIDI Player → `HUB · MIDI` → MIDI Player round-trip while the existing SMF player service keeps transport, scheduling, file ownership and playback position.

`HUB · MIDI` is a compact performance overview, not a second mute table or channel inspector. It shows the audible layers in a Hub-like row layout, lets the user select and mute them, and returns to the unchanged Player with `H` or `Esc`.

The Hub never loads a file, reads SD, seeks, restarts, pauses or stops SMF playback. During a file load or reload it shows `SYNCING` and refuses commands against stale snapshots.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3).
- microSD card containing a Type-0 or Type-1 Standard MIDI File.
- USB-C data/power cable.
- Optional Yamaha SEQTRAK or another class-compliant USB MIDI target.

## Wiring

No GPIO or PORT.A wiring is required.

- Insert the microSD card into Cardputer-Adv.
- Power Cardputer-Adv through USB-C.
- For external MIDI, connect Cardputer-Adv USB-C to SEQTRAK or a host with a data-capable cable.
- This branch does not change PORT.A I2C, I2S audio, TinyUSB descriptors or endpoint wiring.

## Build / Flash steps

```bash
git fetch origin
git checkout agent/player-hub-navigation
python3 tests/test_usb_midi_source_regressions.py
python3 tests/test_smf_panel_completion_source_regressions.py
python3 tests/test_hub_midi_stage_1c_source_regressions.py
./tests/run_host_tests.sh
./scripts/build_seqtrak_midi_only.sh --warnings all
./scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-seqtrak-midi-only/GroovePuter.ino.elf
```

Flash the generated Cardputer-Adv firmware using the repository's normal serial or M5Launcher workflow.

## Controls

1. Open MIDI Player and load a MIDI file.
2. Start playback with `Space`.
3. Press `H` from any loaded Player panel.
4. `HUB · MIDI` opens on the layer corresponding to the Player's selected physical SMF track.
5. In `HUB · MIDI`:
   - `H`: return to the previous MIDI Player panel.
   - `Esc`/Back: return to the previous MIDI Player panel.
   - `Up/Down`: select the previous or next layer.
   - `Left/Right`: move by one six-row screen.
   - `Enter`: toggle the selected layer mute.
   - `1–9`: toggle analyzed audible layers 1–9 directly.
   - `A`: clear all local SMF mutes.
   - `Space`: display `MIDI TRANSPORT: PLAYER`; Hub does not control transport.

`P` and `M` remain unadvertised compatibility aliases for the exploratory branch. They are not part of the intended Player workflow.

## Screen layout

Each row is intentionally performance-oriented:

```text
1> BASS  FingerBass   [▂][▅][█][▃]  ON
2  DRUMS StandardKit  [█][█][▇][█]  MUTE
```

The actual display uses four compact outlined cells rather than text blocks:

- `1–9`: direct mute hotkey.
- `>`: selected layer.
- `BASS`, `DRUMS`, `CHORD`, `PAD`, `LEAD`, `MIDI`: inferred musical role.
- Bounded track name.
- Four cells: analyzed activity in bars 1–16, 17–32, 33–48 and 49–64.
- The top marker over one cell follows the currently playing 16-bar section.
- `ON` / `MUTE`: current physical-track mute state.

The four cells come from the existing load-time structural snapshot. They are not a realtime Note On meter and do not add scheduler callbacks, queues or cross-task ownership.

The selected-layer summary uses readable labels such as:

```text
TRACK 03  CH 2  2-BAR LOOP
```

Raw inspector fields such as `G16`, `SW50`, `N3.2` and `A40%` are intentionally not shown in Hub. Detailed inspection remains in MIDI Player.

## Expected behavior

- Player → Hub → Player does not issue load, seek, restart, play, pause, stop or reanchor commands.
- Repeated physical `H` works in both directions even when Cardputer reports its hardware/Fn `meta` bit.
- `Esc` returns to the same Player session and panel.
- The loaded filename, current tick/bar, playback state, mute mask and selected physical track remain in their existing runtime owners.
- Moving the Hub selection updates the existing generation-aware selected physical track.
- Player restores its previous subpanel and channel-inspector scroll after the round-trip, even if the lazy page cache evicts the Player object.
- The saved Player UI view is restored only when its captured SMF generation still matches the active session.
- Hub reads structural, track and mute snapshots on demand. It does not retain a runtime snapshot cache.
- During load/reload, Hub displays `SYNCING`, `WAITING FOR CURRENT SMF SESSION` and `PLAYER TRANSPORT KEEPS RUNNING`.
- Muting uses the existing generation-aware physical-track mute state. Pending owned notes are released through the existing scheduled SMF dispatcher queue, and muted layers do not emit new Note On events.
- A non-draining USB host may still produce the accepted `USB WAIT` parking behavior. This branch does not alter it.

## Runtime memory measurement

Use the existing setup log point in both unmodified `dev` and this branch:

```text
[after-critical-dsp-buffers] freeInt=... largInt=... free8=... larg8=...
```

Record `freeInt` and `largInt` from the same boot stage. Report `largInt` as `largest`.

```text
metric     dev before     branch after     delta
freeInt    __________     __________       __________
largest    __________     __________       __________
```

The branch intentionally does not adjust the provisional fixed-DRAM gate. If the ELF `.dram0` total changes, report the byte delta.

## Troubleshooting

- Player `H` opens global Help: verify the flashed head includes the Player first-refusal fix and that an SMF session is loaded.
- Hub `H` does not return: verify the flashed head includes the Hub first-refusal handler before the generic `meta` guard.
- Hub shows `SYNCING`: a load/reload is active or one of the projected snapshots does not yet match the current session generation.
- Hub shows `NO MIDI LAYERS`: the current file has no analyzed audible layer, or no file is loaded. Return with `H` or `Esc` and inspect the Player state.
- The four cells do not blink on every note: this is expected. They show the analyzed 64-bar arrangement shape, not realtime MIDI activity.
- Player returns to the default panel instead of its previous subpanel: verify the active session generation did not change during the transition.
- A muted note remains sounding: verify `takePendingReleaseTrack()` is drained by the existing scheduled SMF queue and that no direct USB writer was introduced.
- External output parks at `USB WAIT`: verify the host is draining the MIDI endpoint. Parking is intentional.

## Acceptance checklist

- [ ] `python3 tests/test_usb_midi_source_regressions.py` passes.
- [ ] `python3 tests/test_smf_panel_completion_source_regressions.py` passes.
- [ ] `python3 tests/test_hub_midi_stage_1c_source_regressions.py` passes.
- [ ] `./tests/run_host_tests.sh` passes.
- [ ] SEQTRAK MIDI-only build passes.
- [ ] Real ELF passes the unchanged provisional fixed-DRAM gate.
- [ ] Load a seven-track SMF and start playback.
- [ ] Press `H`; `HUB · MIDI` opens and global Help does not.
- [ ] Confirm rows show hotkey, selected marker, role, bounded name, four-cell arrangement shape and `ON`/`MUTE` without clipping.
- [ ] Confirm the section marker moves when playback crosses bars 16, 32 and 48.
- [ ] Use `Left/Right` on a file with more than six audible layers.
- [ ] Toggle layers with `1–7` and `Enter`; muted notes release and no new Note On is emitted for muted layers.
- [ ] Press `H` again; Player returns with the same file, position, selected track, mute state, subpanel and scroll.
- [ ] Repeat the test with `Esc` as the return key.
- [ ] Repeat Player → Hub → Player during playback without reload, stall, reanchor, drift or stuck notes.
- [ ] Reload/change a file and confirm Hub shows `SYNCING` instead of old layer rows.
- [ ] Confirm `ep=busy/stalled=0/0` on a normally draining host.
- [ ] Confirm Hub contains no SD read, file load, SMF transport, scheduler or direct TinyUSB path.
- [ ] Confirm `logs/`, Song, scene codecs, pattern sources, USB descriptors, RX drain and clock queue are unchanged.
