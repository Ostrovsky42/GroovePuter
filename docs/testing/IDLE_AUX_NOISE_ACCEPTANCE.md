# Cardputer ADV idle AUX noise acceptance

## Purpose

Verify that the Cardputer ADV audio path reaches true digital silence after audible tails finish, Tape AGE does not create a permanent idle noise bed, and any remaining periodic AUX click can be correlated with an explicit I2S write diagnostic.

## Hardware list

- M5Stack Cardputer ADV;
- 3.5 mm TRS AUX cable;
- Yamaha SEQTRAK AUDIO IN or another known-good line/headphone input;
- USB-C data cable for flashing and Serial diagnostics;
- headphones are useful for the isolation check.

## Wiring

Connect the Cardputer ADV 3.5 mm audio output to the SEQTRAK AUDIO IN with the normal TRS AUX cable. No PORT.A/I2C wiring is used.

For the ground-noise isolation step, disconnect USB-C after boot and run the Cardputer ADV from its battery while leaving AUX connected.

Cardputer ADV codec pins remain ES8311 I2C GPIO8/9 and I2S BCLK/WS/DOUT GPIO41/43/42. GPIO21 is intentionally not driven as a Cardputer ADV amplifier-enable pin.

## Build / Flash steps

```bash
git fetch origin
git switch agent/20260807-05-idle-audio-aux-noise-fix
git reset --hard origin/agent/20260807-05-idle-audio-aux-noise-fix
bash tests/run_idle_audio_aux_noise_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

- normal playback is unchanged;
- Pause/Stop does not disable or reinitialize I2S/ES8311;
- synth live-note release tails remain audible and decay naturally;
- Tape AGE colours audible material and tails, but no longer creates a standalone noise bed over exact digital silence;
- the final Cardputer I2S path collapses only `-1/0/+1` PCM LSB to zero; all samples with magnitude `>=2` are unchanged;
- the legacy PA setup call produces no GPIO side effect on Cardputer ADV and GPIO21 is not physically driven;
- a failed/partial I2S write emits `err` plus actual/expected byte counts, while the existing audio-underrun counter remains authoritative.

## Troubleshooting

### Hiss remains after Pause

Set Tape OFF, wait one second after Pause, then compare USB-powered operation with battery-only operation. If the hiss remains unchanged on battery with this firmware, treat it as an analog AUX/input-gain noise-floor problem rather than DSP noise.

### Periodic click remains

Keep Serial open. If an audible click coincides with:

```text
[AudioOutI2S] write err=... bytes=.../...
[I2S] Write Timeout / Error
```

the next fix belongs to realtime scheduling/DMA starvation. If there is no matching I2S diagnostic, isolate the analog path by testing Cardputer output directly with headphones and by removing shared USB ground where possible.

### Audio is cut too early

Test a live Synth A/B note while transport is stopped. This change never mutes the codec and never bypasses synth release processing; only the final `-1/0/+1` LSB PCM floor is collapsed to zero.

## Acceptance checklist

- [ ] `bash tests/run_idle_audio_aux_noise_tests.sh` passes;
- [ ] Cardputer ADV normal build passes;
- [ ] fixed DRAM budget passes;
- [ ] SEQTRAK MIDI-only build passes;
- [ ] SDL build passes;
- [ ] normal Song/Pattern playback sounds unchanged;
- [ ] Tape OFF + Pause becomes silent after release tails;
- [ ] Tape ON + Pause does not leave a permanent AGE hiss bed after tails;
- [ ] live keyboard works while transport is stopped;
- [ ] no new pop occurs on Pause/Play;
- [ ] GPIO21 is not driven by GroovePuter audio setup;
- [ ] any remaining click is either correlated with I2S diagnostics or reproducibly classified as analog/ground-path noise.
