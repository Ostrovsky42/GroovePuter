# MIDI Player ↔ HUB MIDI arrangement view

## Purpose

Provide a lossless MIDI Player → `HUB MIDI` → MIDI Player round-trip and show the loaded SMF as a shared sixteen-segment arrangement map.

The Hub is read-only apart from the existing physical-track mute commands. It does not load files, read SD, seek, restart, pause, stop, schedule notes, write USB MIDI or own Note On/Off state.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3, 240×135 display).
- microSD card with a Type-0 or Type-1 Standard MIDI File.
- USB-C data/power cable.
- Optional Yamaha SEQTRAK or another class-compliant USB MIDI target.

## Wiring

No GPIO or PORT.A wiring is required.

- Insert the microSD card into Cardputer-Adv.
- Power Cardputer-Adv through USB-C.
- For external MIDI, use a data-capable USB-C connection to the host or SEQTRAK.
- This change does not touch PORT.A I2C, I2S audio, USB descriptors, RX drain or clock wiring.

## Build / Flash steps

```bash
git fetch origin
git switch agent/player-hub-navigation
git pull --ff-only origin agent/player-hub-navigation

python3 tests/test_usb_midi_source_regressions.py
python3 tests/test_smf_panel_completion_source_regressions.py
python3 tests/test_hub_midi_stage_1c_source_regressions.py
./tests/run_host_tests.sh
./scripts/build_seqtrak_midi_only.sh --warnings all
./scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-seqtrak-midi-only/GroovePuter.ino.elf
```

Flash the generated Cardputer-Adv firmware using the repository's normal serial workflow.

## Controls

1. Open MIDI Player and load a MIDI file.
2. Start playback with `Space`.
3. Press `H` from any loaded Player panel.
4. In `HUB MIDI`:
   - `H` or `Esc`/Back: return to the originating Player panel.
   - `Up/Down`: select a layer.
   - `Left/Right`: move by one seven-row page.
   - `Enter`: toggle the selected physical SMF track mute.
   - `1–9`: toggle the corresponding analyzed layer directly.
   - `A`: clear all local SMF mutes.
   - `Space`: show `MIDI TRANSPORT: PLAYER`; transport remains Player-owned.

`P` and `M` remain unadvertised compatibility aliases. The existing MIDI Player `U` inspector implementation is not changed by this visual commit.

## Screen layout

- Seven rows expand across the full 135-pixel height.
- Each row contains its hotkey, a bounded track/program label and sixteen activity cells.
- All rows share one horizontal time axis.
- A two-pixel vertical playhead is derived from `currentTick / endTick` and crosses the complete grid beneath the text overlays.
- The top overlay shows filename and `BAR current/total`.
- The bottom overlay shows selected channel, bounded note count, MIDI pitch range and key hints.
- Overlay bands use a solid near-black scrim because `IGfx` has no alpha blending; text is never drawn directly over activity cells.
- Muted rows use a uniformly dimmed palette. There is no `ON`/`MUTE` text.
- The selected row uses a subtle background shift and an accent hotkey. The same accent is used only for the playhead.

The sixteen cells are computed once during the existing SMF load scan. Saturating four-bit per-bar Note On counts are collected for a bounded 256-bar analysis window and proportionally mapped across all sixteen cells:

```text
segment = bar * 16 / analyzedBars
```

Therefore a 32-bar file and a 200-bar file both fill the same sixteen-cell width. Files longer than 256 analyzed bars are marked with `*` in the filename overlay; playback remains unchanged, but the arrangement map is intentionally partial rather than allocating unbounded DRAM.

## Rendering behavior

The page composes into the existing RGB565 back buffer. `CardputerDisplay::flush()` compares dirty tiles and sends only changed tile runs to the LCD. During steady playback this limits physical panel updates to the old/new playhead tiles and any overlay tiles whose bar text changed. Selection redraws two rows; mute redraws one row; page entry or scroll may redraw the complete grid.

No framebuffer, sprite, scheduler callback, MIDI queue or realtime activity meter is added.

## Expected behavior

- The entry and exit points of drums, bass, pads and leads align vertically and are visible at a glance.
- The playhead gives every layer one shared position reference.
- `H` and `Esc` return without file reload, transport reanchor or position reset.
- `1–7` and `Enter` retain the existing generation-aware mute semantics and owned-note cleanup.
- During load/reload, Hub displays `SYNCING` and refuses commands against stale snapshots.
- Player filename, position, mute mask, selected physical track, subpanel and scroll remain owned by their existing runtime state.

## Performance and memory measurement

Baseline before this visual commit:

```text
commit:        471b99fa
CI run:        31035929646
.dram0 total:  120712 bytes
.dram0.data:    25040 bytes
.dram0.bss:     95672 bytes
budget room:    70776 bytes
observed ui:    27537 us  (user hardware sample; capture matching uiPeak)
```

Use the same seven-track file, playback state and open Hub screen for at least 30 seconds before recording:

```text
metric      before         after          delta
freeInt     __________     __________     __________
largInt     __________     __________     __________
ui          27537 us       __________     __________
uiPeak      __________     __________     __________
```

Use the existing setup log for `freeInt` and `largInt`. Do not adjust the provisional DRAM gate; report the ELF `.dram0` delta.

## Troubleshooting

- `H` opens Help: verify an SMF session is loaded and the flashed head includes the Player first-refusal handler.
- Hub shows `SYNCING`: a load/reload is active or projected generations do not match the current session.
- Grid is empty: verify the file contains Note On events in the analyzed physical tracks.
- Filename ends in `*`: the file extends beyond the bounded 256-bar arrangement analysis window.
- Text is hard to read: verify on the physical Cardputer panel; the top and bottom bands must remain solid dark scrims.
- Playhead causes broad LCD traffic: verify `DirtyTileTracker` remains active and no forced full refresh was added.
- A muted note remains sounding: verify the existing pending track-release path is still drained by the scheduled SMF queue.

## Acceptance checklist

- [ ] `python3 tests/test_usb_midi_source_regressions.py` passes.
- [ ] `python3 tests/test_smf_panel_completion_source_regressions.py` passes.
- [ ] `python3 tests/test_hub_midi_stage_1c_source_regressions.py` passes.
- [ ] `./tests/run_host_tests.sh` passes.
- [ ] SDL build passes.
- [ ] Cardputer-Adv normal build passes.
- [ ] Cardputer-Adv SEQTRAK MIDI-only build passes.
- [ ] Real ELF passes the unchanged provisional DRAM gate and `.dram0` delta is reported.
- [ ] Load a seven-track file and open Hub during playback.
- [ ] Seven rows are readable; no arrow or `ON`/`MUTE` labels are shown.
- [ ] A 32-bar and a roughly 200-bar file each map across all sixteen segments.
- [ ] Layer entry/exit points and overlaps are visible at a glance.
- [ ] Playhead is vertically aligned across all rows at every position.
- [ ] Top filename/bar and bottom channel/note/range text remain legible over every cell color.
- [ ] `1–7` and `Enter` mute the expected physical layers and release owned notes.
- [ ] `H` and `Esc` return with the same file, playback position, selected layer and mute mask.
- [ ] `freeInt`, `largInt`, `ui` and `uiPeak` are recorded before and after on hardware.
- [ ] No changes exist in scheduler, lookahead, note ownership, USB transport, descriptors, RX drain, clock queue, Song, scenes or pattern sources.
