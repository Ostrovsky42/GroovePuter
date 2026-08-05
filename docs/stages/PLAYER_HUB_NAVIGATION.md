# MIDI Player ↔ HUB · MIDI navigation

## Purpose

Provide a lossless MIDI Player → `HUB · MIDI` → MIDI Player round-trip while the existing SMF player service keeps transport, scheduling, file ownership and playback position.

The Hub remains a projected layer panel. It never loads a file, reads SD, seeks, restarts, pauses or stops SMF playback. While a file load or reload is changing the SMF session identity, the Hub shows `SYNCING` and refuses stale layer commands.

`U` and `I` remain the detailed MIDI Player tools. The Hub reuses them instead of duplicating a second mute table or channel inspector.

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
./tests/run_host_tests.sh
./scripts/build_seqtrak_midi_only.sh --warnings all
./scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-seqtrak-midi-only/GroovePuter.ino.elf
```

Flash the generated Cardputer-Adv firmware using the repository's normal serial or M5Launcher workflow.

## Controls

1. Open MIDI Player and load a MIDI file.
2. Start playback with `Space`.
3. Press `H` from the loaded Player, mute table, channel inspector, performance panel or structural inspector.
4. `HUB · MIDI` opens on the layer corresponding to the Player's selected physical SMF track.
5. In `HUB · MIDI`:
   - `Up/Down`: select the previous or next layer.
   - `Left/Right`: move by one six-row screen.
   - `Enter`: toggle the selected layer mute.
   - `1–9`: toggle analyzed audible layers 1–9.
   - `A`: clear all local SMF mutes.
   - `U`: return to MIDI Player and open its existing mute mixer on the selected physical track.
   - `I`: return to MIDI Player and open its existing channel inspector.
   - `P`: return to the previous MIDI Player panel.
   - `Back`/`B`: return to MIDI Player when Hub was opened from Player; otherwise return to internal Hub.
   - `M`: switch to internal Hub without changing SMF transport.
   - `Space`: display `MIDI TRANSPORT: PLAYER`; Hub does not control transport.

## Row format

A Hub row is compact and uses only already analyzed snapshots:

```text
1>02 ON  C10 ##. DRM Drums
```

- `1`: direct mute hotkey. Layers after 9 show `-` and remain accessible with navigation plus `Enter`.
- `>`: selected layer.
- `02`: physical SMF track number.
- `ON` / `MUT`: mute state.
- `C10`, `MIX` or `--`: one MIDI channel, multiple channels or unavailable channel metadata.
- `#..` to `###`: analyzed note-activity density from the structural load pass.
- `DRM`, `BAS`, `CHD`, `PAD`, `LED`, `OTH`: inferred structural role.
- Final text: bounded GM-family/role label.

The three-cell density is not a real-time Note On meter. It is stable structural metadata, so displaying it adds no scheduler callback, queue read or cross-task ownership.

## Expected behavior

- Player → Hub → Player does not issue load, seek, restart, play, pause, stop or reanchor commands.
- The loaded filename, current tick/bar, playback state, mute mask and selected physical track remain in their existing runtime owners.
- Moving the Hub selection updates the existing generation-aware selected physical track. Opening `U` therefore lands on the same layer in the Player mute table.
- `U` and `I` reuse the existing Player panels; Hub does not implement duplicate editors.
- Player restores its previous subpanel and channel-inspector scroll after a normal `P`/Back round-trip, even if the lazy page cache evicts the Player object.
- The saved Player UI view is restored only when its captured SMF generation still matches the active session. It is discarded after file change or reload.
- Hub reads structural, track and mute snapshots on demand. It does not retain a runtime snapshot cache.
- During load/reload, Hub displays `SYNCING`, `WAITING FOR CURRENT SMF SESSION` and `PLAYER TRANSPORT KEEPS RUNNING`.
- `SYNCING` clears only after structural, track and mute snapshot generations all match `smfSessionGeneration()`.
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

- `H` opens global help: the Player is still in the file browser. Load or return to the active file first.
- Hub shows `SYNCING`: a load/reload is active or one of the projected snapshots does not yet match the current session generation. Wait for the current load pass; do not display old layers.
- Hub shows `NO MIDI LAYERS`: the current file has no analyzed audible layer, or no file is loaded. Return with `P` and inspect the Player state.
- `U` opens a different row: verify the Hub selection changed the generation-aware `selectedTrack` and the file was not reloaded between pages.
- Player returns to the default panel instead of its previous subpanel: verify the active session generation did not change during the transition.
- A muted note remains sounding: verify `takePendingReleaseTrack()` is drained by the existing scheduled SMF queue and that no direct USB writer was introduced.
- External output parks at `USB WAIT`: verify the host is draining the MIDI endpoint. Parking is intentional and must not be replaced by another writer or pacing path.

## Acceptance checklist

- [ ] `python3 tests/test_usb_midi_source_regressions.py` passes.
- [ ] `python3 tests/test_smf_panel_completion_source_regressions.py` passes.
- [ ] `./tests/run_host_tests.sh` passes.
- [ ] SEQTRAK MIDI-only build passes.
- [ ] Real ELF passes the unchanged provisional fixed-DRAM gate.
- [ ] `.dram0` before/after totals and byte delta are reported.
- [ ] `freeInt` and `largest` are recorded from `[after-critical-dsp-buffers]` on both builds.
- [ ] Load a seven-track SMF and start playback.
- [ ] Select and scroll a non-first track in the Player.
- [ ] Press `H`; `HUB · MIDI` opens the corresponding physical layer.
- [ ] Confirm every row shows physical track, mute state, channel, density, role and bounded label without clipping.
- [ ] Use `Left/Right` on a file with more than six audible layers and confirm selection moves by one screen.
- [ ] Select a layer in Hub, press `U`, and confirm the Player mute mixer opens on the same physical track.
- [ ] Return to Hub, press `I`, and confirm the existing Player channel inspector opens without changing transport.
- [ ] Toggle layers with `1–7` and `Enter`; muted notes release and no new Note On is emitted for muted layers.
- [ ] Press `P` or Back; Player returns with the same file, position, selected track, mute state, subpanel and scroll.
- [ ] Repeat Player → Hub → Player during playback without transport stall, reanchor or position drift.
- [ ] Reload/change a file and confirm Hub shows `SYNCING` instead of old layer rows.
- [ ] Confirm `ep=busy/stalled=0/0` on a normally draining host.
- [ ] Confirm no stuck notes after repeated mute and navigation cycles.
- [ ] Confirm Hub contains no SD read, file load, SMF transport, scheduler or direct TinyUSB path.
- [ ] Confirm `logs/`, Song, scene codecs, pattern sources, USB descriptors, RX drain and clock queue are unchanged.
