# Performance Keyboard MONO / POLY acceptance

## Purpose

Verify that Cardputer-Adv PERFORM keeps the existing internal monophonic Synth A/B behavior while adding true per-key polyphonic output for external MIDI.

`MONO` is the compatibility/default mode: Synth A/B drive the internal synth and USB MIDI with last-note priority.

`POLY` is manual external-MIDI polyphony: every held performance key owns an independent NoteOn/NoteOff on the selected external synth channel. Internal Synth A/B do not consume these manual POLY events. ARP, Chord, Strum, Ratchet and Euclidean tools keep their existing generated-note behavior.

## Hardware list

- M5Stack Cardputer-Adv.
- Yamaha SEQTRAK or another USB-MIDI receiver that can play chords on one MIDI channel.
- The same USB cable/adapter already used for GroovePuter -> SEQTRAK MIDI testing.

## Wiring

No GPIO, I2C or audio wiring changes are required.

Use the existing GroovePuter USB-MIDI connection to the external receiver. Performance targets retain the existing channel map:

- Synth A -> MIDI CH8.
- Synth B -> MIDI CH9.
- DX -> MIDI CH10.
- Drums -> native independent MIDI CH1..7 lanes.

## Build / Flash

From the repository root:

```bash
./tests/run_host_tests.sh
./scripts/build.sh
```

For the SEQTRAK MIDI-only profile, also run:

```bash
./scripts/build_seqtrak_midi_only.sh
```

Flash with the project's normal Cardputer-Adv upload command/script.

## Expected behavior

1. Open `PERFORM` and keep NOTE mode enabled.
2. Press `Tab` to open `PERFORMANCE TOOLS`.
3. Tool `9` shows `VOICE MONO` by default.
4. In `MONO`, hold `A`, then press `S`: only the last note is active; releasing `S` restores `A`.
5. Press `9`. The toast shows `VOICE: POLY / EXT MIDI`; the screen shows `POLY` / `EXT MIDI ONLY`.
6. Select Synth A, Synth B or DX and hold two or three performance keys at once. The external receiver must sound all held notes together.
7. Release only the middle key. Only that note must stop; the other held notes must continue.
8. Release the remaining keys in any order. Every note must receive its matching NoteOff; no stuck note may remain.
9. Hold multiple notes and change `POLY -> MONO`, change target, disable NOTE mode, or press `X` Panic. All external POLY notes must stop.
10. In POLY on Synth A/B, the Cardputer internal Synth A/B must not sound the manual chord. This is intentional: internal voices remain monophonic.
11. Drums remain independent native lanes and are not changed by MONO/POLY.
12. Enabling ARP/Chord/Strum/Ratchet/Euclidean continues to use the existing transformed/generated note path rather than stacking a second manual-poly ownership model.

## Troubleshooting

- If only one external note sounds in `POLY`, confirm the PERFORM chip says `POLY` and that the external receiver/patch is itself polyphonic.
- If Synth A/B are silent on the Cardputer while `POLY` is selected, that is expected. Switch to `MONO` for internal live synth playback.
- If a note remains stuck after release, press `X` Panic and capture serial output plus the exact target/channel and key sequence. This is an acceptance failure.
- If `9` changes a global command instead of voice mode, confirm the `PERFORMANCE TOOLS` overlay is open with `Tab`.
- If USB MIDI is not ready, resolve the existing endpoint/cable/SEQTRAK routing issue first; POLY does not change USB enumeration or wiring.

## Acceptance checklist

- [ ] `MONO` is the boot/default voice mode.
- [ ] Existing MONO last-note-priority behavior is unchanged.
- [ ] `Tab -> 9` toggles `MONO <-> POLY` without leaving PERFORM.
- [ ] UI clearly marks POLY as external MIDI only.
- [ ] Two simultaneous keys produce two simultaneous external notes.
- [ ] Three simultaneous keys produce three simultaneous external notes.
- [ ] Releasing a non-last/middle key stops only that exact note.
- [ ] Matrix reconciliation/key-up recovery does not retrigger unrelated held notes.
- [ ] Synth A -> CH8, Synth B -> CH9, DX -> CH10 remain unchanged.
- [ ] Internal Synth A/B stay monophonic and ignore manual POLY events.
- [ ] Target change, mode change, NOTE off and Panic clean all POLY notes.
- [ ] Drums CH1..7 behavior is unchanged.
- [ ] ARP/Chord/Strum/Ratchet/Euclidean behavior is unchanged.
- [ ] No stuck MIDI notes after rapid multi-key press/release sequences.
- [ ] Host tests pass, except any explicitly documented pre-existing base failure.
- [ ] Cardputer-Adv normal build passes.
- [ ] SEQTRAK MIDI-only build passes.
