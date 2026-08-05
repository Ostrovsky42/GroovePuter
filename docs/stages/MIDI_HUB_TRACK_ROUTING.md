# MIDI Hub per-track SEQTRAK routing

## Purpose

Allow one physical SMF track selected in `HUB MIDI` to override its automatic SEQTRAK destination without editing the MIDI file.

The override is applied at the existing SMF scheduling boundary. The Hub does not write USB MIDI, own Note On/Off state, read the SD card or change transport timing.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3).
- Yamaha SEQTRAK.
- microSD card containing a Type-0 or Type-1 Standard MIDI File.
- Data-capable USB-C MIDI connection between Cardputer-Adv and SEQTRAK.

## Wiring

No GPIO or PORT.A wiring is required.

- Insert the microSD card into Cardputer-Adv.
- Connect Cardputer-Adv to SEQTRAK with the same data-capable USB MIDI setup used by MIDI Player.
- Do not add a separate 5 V connection between the devices.
- PORT.A I2C, I2S audio, USB descriptors, MIDI RX and clock wiring are unchanged.

## Destination map

```text
AUTO  existing GM/SEQTRAK routing
CH1   KICK   fixed note 60
CH2   SNARE  fixed note 60
CH3   CLAP   fixed note 60
CH4   HAT-C  fixed note 60
CH5   HAT-O  fixed note 60
CH6   PERC   fixed note 60
CH7   CYM    fixed note 60
CH8   SYN1   original note pitch
CH9   SYN2   original note pitch
CH10  DX     original note pitch
```

Overrides are stored by physical SMF track index. They reset to `AUTO` when another MIDI file is loaded or after reboot. They are active only in SEQTRAK routing mode; RAW routing remains unchanged.

## Build / Flash steps

```bash
git fetch origin
git switch agent/midi-hub-track-routing
git pull --ff-only origin agent/midi-hub-track-routing

python3 tests/test_hub_midi_stage_1c_source_regressions.py
./tests/run_host_tests.sh
./scripts/build_seqtrak_midi_only.sh --warnings all
./scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-seqtrak-midi-only/GroovePuter.ino.elf
```

Flash the generated Cardputer-Adv firmware with the repository's normal serial workflow.

## Controls

1. Load a MIDI file in MIDI Player.
2. Use SEQTRAK routing, not RAW.
3. Stop or pause playback.
4. Press `H` to open `HUB MIDI`.
5. Select a layer with `Up/Down`.
6. Press `C` to edit its output route.
7. Use `Left/Right` to choose `AUTO` or `CH1..CH10`.
8. Press `Enter` to apply.
9. Press `Esc` while editing to cancel. Press `H` to leave Hub immediately.

Outside route edit mode, `Left/Right` still pages through layers and `Enter` still toggles mute.

## Expected behavior

- The bottom band shows source and destination, for example `CH2>AUTO` or `CH2>CH8`.
- While editing it shows a readable destination such as `ROUTE CH8 SYN1`.
- Pressing `C` during playback or while armed shows `PAUSE MIDI FIRST` and makes no change.
- Pressing `C` in RAW routing shows `SEQTRAK ROUTING REQUIRED`.
- After applying a route, the next playback sends that physical track to the selected SEQTRAK channel.
- CH1-CH7 output note 60 as required by SEQTRAK drum tracks. CH8-CH10 preserve note pitch.
- Mute state, file position, tempo mode and Player/Hub navigation remain unchanged.

## Troubleshooting

- Route is not audible: verify Player routing is SEQTRAK rather than RAW and the selected SEQTRAK track is not muted.
- `PAUSE MIDI FIRST`: pause or stop the MIDI file before editing. Live route mutation is intentionally blocked so queued notes cannot retain the old destination.
- Route returns to `AUTO`: loading another MIDI file starts a new SMF session and clears per-track overrides by design.
- Drum melody collapses to one pitch: CH1-CH7 are fixed-note SEQTRAK drum destinations. Use CH8-CH10 for pitched material.
- Track shows `MULTI`: the source track contains more than one MIDI channel; the override still applies to the complete physical track.
- Stuck note after testing: press Player panic and verify the branch contains no direct MIDI writes from the Hub page.

## Acceptance checklist

- [ ] `python3 tests/test_hub_midi_stage_1c_source_regressions.py` passes.
- [ ] `./tests/run_host_tests.sh` passes.
- [ ] SDL build passes.
- [ ] Cardputer-Adv normal build passes.
- [ ] Cardputer-Adv SEQTRAK MIDI-only build passes.
- [ ] Fixed DRAM remains inside the unchanged budget.
- [ ] Load a file with at least three physical note tracks.
- [ ] Pause or stop, select a track, press `C`, choose CH8, and apply with `Enter`.
- [ ] Footer changes from `>AUTO` to `>CH8`.
- [ ] Playback sends only that physical track to SEQTRAK SYN1.
- [ ] Repeat with CH10 and verify DX receives the original pitches.
- [ ] Route a drum track to CH1-CH7 and verify SEQTRAK receives fixed note 60.
- [ ] Set the route back to `AUTO` and verify the original routing returns.
- [ ] While playing, `C` shows `PAUSE MIDI FIRST` and output continues unchanged.
- [ ] In RAW mode, `C` shows `SEQTRAK ROUTING REQUIRED` and RAW output remains unchanged.
- [ ] `Esc` cancels an edit; `H` exits Hub; mute and page navigation still work outside edit mode.
- [ ] Load another file and verify all routes begin at `AUTO`.
- [ ] No changes exist in TinyUSB calls, dispatcher ownership, scheduler timing, lookahead, clock queue, RX handling, scenes or pattern sources.
