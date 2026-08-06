# MIDI Hub route persistence

## Purpose

Restore the `AUTO / CH1..CH10` routes assigned in `HUB MIDI` when the same MIDI file is opened again, including after Cardputer-Adv reboot.

The profile is accepted only when the normalized path, file size, retained physical-track count, and route-relevant semantic fingerprint all match. A changed or corrupt file starts with `AUTO` routes.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3).
- Yamaha SEQTRAK.
- microSD card with at least two Type-0 or Type-1 Standard MIDI Files.
- Data-capable USB-C MIDI connection between Cardputer-Adv and SEQTRAK.

## Wiring

No GPIO or PORT.A wiring is required.

- Insert the microSD card into Cardputer-Adv.
- Connect Cardputer-Adv to SEQTRAK through the existing USB MIDI setup.
- Do not add another 5 V connection between the devices.
- PORT.A I2C, I2S audio, USB descriptors, MIDI RX and clock wiring are unchanged.

## Storage model

- Sixteen fixed NVS slots under the existing `grooveputer` namespace.
- Each slot contains one 72-byte versioned record with CRC32.
- Saving replaces the newest record for the same normalized path, then a free/corrupt slot, then the oldest valid slot.
- The semantic fingerprint is accumulated during the existing first full SMF parse. No second SD traversal is added.
- Mutes, playback position, tempo, transpose and velocity are not stored by this stage.

## Build / Flash steps

```bash
git fetch origin
git switch agent/midi-hub-route-persistence
git pull --ff-only origin agent/midi-hub-route-persistence

python3 tests/test_smf_route_persistence_source_regressions.py
./tests/run_host_tests.sh
./scripts/build_seqtrak_midi_only.sh --warnings all
./scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-seqtrak-midi-only/GroovePuter.ino.elf
```

Flash the generated Cardputer-Adv firmware with the repository's normal serial workflow.

## Expected behavior

1. Load MIDI file A and stop or pause playback.
2. Open `HUB MIDI` with `H`.
3. Select a physical track, press `C`, assign `CH8`, and confirm with `Enter`.
4. Load MIDI file B. Its routes begin at `AUTO` unless it already has its own saved profile.
5. Load file A again. The same physical track shows `CH8` and plays through SEQTRAK SYN1.
6. Reboot Cardputer-Adv, load file A, and verify the route is restored again.
7. Replace file A with a musically changed MIDI at the same path. It must start at `AUTO`.

Serial shows one bounded status line per load/save:

```text
[SMF-ROUTE] load=<status> tracks=<n>
[SMF-ROUTE] save=<0|1> tracks=<n>
```

## Troubleshooting

- Route returns to `AUTO`: the file fingerprint, file size or physical-track count changed, the record was evicted from the sixteen-slot bank, or the stored record failed CRC.
- Route does not survive reboot: inspect `[SMF-ROUTE] save=0`; verify the NVS partition is writable and not exhausted by unrelated settings.
- A different file received a route: verify the exact branch head and run the profile host test; path and semantic identity must both match.
- `ROUTE SET / SAVE BUSY`: the bounded player command queue was full. Re-enter the same route after transport activity settles.
- `PAUSE MIDI FIRST`: persistence does not change the existing safety rule; route edits remain stopped/paused only.
- Stuck note: use Player panic. This stage does not change note ownership, scheduling or TinyUSB writes.

## Acceptance checklist

- [ ] `python3 tests/test_smf_route_persistence_source_regressions.py` passes.
- [ ] `./tests/run_host_tests.sh` passes.
- [ ] SDL build passes.
- [ ] Cardputer-Adv normal build passes.
- [ ] Cardputer-Adv SEQTRAK MIDI-only build passes.
- [ ] Fixed DRAM gate passes.
- [ ] Assign two different routes in file A and reload file A.
- [ ] Both routes restore without opening another editor or reassigning them.
- [ ] Reboot and verify file A restores again.
- [ ] Load file B and verify it keeps its independent route profile.
- [ ] Replace file A with changed note/channel/timing content at the same path and verify `AUTO`.
- [ ] Corrupt/missing profile storage does not prevent MIDI loading and results in `AUTO`.
- [ ] `RAW` still ignores overrides.
- [ ] Mutes, playback position, tempo mode and Player/Hub round-trip remain unchanged.
- [ ] No second SD traversal, UI-side NVS operation, scheduler change, lookahead change, TinyUSB change or note-ownership change exists.
