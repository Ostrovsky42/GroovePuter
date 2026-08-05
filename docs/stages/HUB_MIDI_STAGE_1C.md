# Stage 1C — HUB · MIDI Projection

## Purpose

Project the already loaded MIDI Player session into Sequencer Hub without creating a second SMF transport, scheduler, file loader, ownership domain, or USB writer.

`HUB · MIDI` reads the existing player, structural-inspector, physical-track and mute snapshots. It may select and locally mute an existing physical SMF track through the accepted mute state. MIDI Player remains the only place that loads files and controls playback position.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3).
- microSD card with a Standard MIDI File Type 0 or Type 1.
- USB-C data/power cable.
- Optional Yamaha SEQTRAK or another USB MIDI target.

## Wiring

No GPIO or PORT.A wiring is required.

- Insert the microSD card into Cardputer-Adv.
- Power Cardputer-Adv through USB-C.
- For external playback, connect Cardputer-Adv USB-C to the host or MIDI target using a data-capable cable.
- Stage 1C does not change PORT.A I2C, analog audio wiring, or the TinyUSB single-writer path.

## Build / Flash steps

```bash
git fetch origin
git checkout agent/hub-midi-stage-1c
./tests/run_host_tests.sh
./scripts/build.sh --warnings all
./scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

Flash the generated Cardputer-Adv firmware with the repository's normal M5Launcher or serial flashing workflow.

## Controls

1. Load and start a MIDI file in MIDI Player.
2. Open Sequencer Hub.
3. Press `M` while Hub is in overview mode to switch between `HUB · INTERNAL` and `HUB · MIDI`.
4. In `HUB · MIDI`:
   - `Up/Down`: select a visible MIDI layer.
   - `Enter`: toggle local mute for the selected physical SMF track.
   - `1–9`: toggle the corresponding analyzed audible layer.
   - `A`: clear all local SMF track mutes.
   - `Space`: does not control transport; it shows `MIDI TRANSPORT: PLAYER`.
   - `M`, `B`, or Back: return to the internal Hub overview.

## Expected behavior

- The header shows the existing player state, bar and filename.
- Up to six of the first nine audible physical SMF tracks are visible at once.
- Each row keeps the logical slot and physical SMF track number separate.
- Rows show local mute state, inferred role and compact GM-derived label.
- The selected layer shows grid, swing, proven loop, Note On density and active-note density from the existing Stage 1B snapshot.
- Muting a row uses the same ownership-safe cleanup as MIDI Player; other layers continue.
- Returning to `HUB · INTERNAL` restores the existing sequencer Hub behavior.
- Opening `HUB · MIDI` does not reopen the SD file, seek, restart, play, pause or stop the SMF service.
- No file is loaded: the screen shows `NO MIDI LAYERS · LOAD IN PLAYER`.

## Troubleshooting

- `NO MIDI LAYERS`: load a MIDI file in MIDI Player and wait for the existing index/analysis pass to finish.
- `M` does nothing: return to the Hub overview; MIDI projection intentionally does not replace detail editing.
- Space does not start playback: this is intentional. Use MIDI Player for transport.
- A row label differs from the SMF TrackName: fixed DRAM stores physical track, channel and Program Change; the displayed label is derived from the retained GM program.
- A mute does not affect the external device: Stage 1C is local SMF dispatch mute, not SEQTRAK CC23/CC24 remote mute.
- External target is silent: verify a data-capable USB cable, target channels and MIDI routing profile.
- UI shows stale content after loading another file: leave and reopen `HUB · MIDI`; report the file, player state and visible physical track numbers if the snapshot does not update.

## Acceptance checklist

- [ ] `./tests/run_host_tests.sh` passes.
- [ ] SDL desktop build passes with both Hub translation units linked exactly once.
- [ ] Cardputer-Adv firmware build passes.
- [ ] Real ELF passes the provisional `191488 B` fixed-DRAM gate from #70; this is not yet a safety-derived permanent ceiling.
- [ ] Load a Type-1 file containing a conductor track and at least seven audible tracks.
- [ ] `M` switches Internal ↔ MIDI only from Hub overview.
- [ ] MIDI rows show logical slot and physical SMF track number separately.
- [ ] `Up/Down` scrolls through more than six layers without changing playback.
- [ ] `Enter` mutes and restores the selected layer without stuck notes.
- [ ] `1–9` invokes the same existing local physical-track mute state.
- [ ] `A` restores all locally muted SMF layers.
- [ ] Space shows `MIDI TRANSPORT: PLAYER` and does not start, stop, seek or restart playback.
- [ ] Returning to Internal preserves the existing Hub sequencer controls.
- [ ] Source regression rejects SD/file loading, scheduler ownership, transport commands and direct TinyUSB calls in the Hub MIDI projection.
- [ ] No new global snapshot cache, scheduler, ownership registry or MIDI service is introduced.
- [ ] No changes to Song/project codecs, scene persistence, Pattern/PERFORM ownership, internal synth/drum routing or remote SEQTRAK mute.
