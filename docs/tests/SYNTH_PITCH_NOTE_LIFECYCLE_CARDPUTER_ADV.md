# Synth Pitch and Note Lifecycle — Cardputer ADV

## Purpose

Validate the first synth-engine repair stage before GroovePuter 0.9:

- AY pitch uses a separate 1.7734 MHz PSG clock instead of the 22.05 kHz host sample rate;
- SN76489 notes below the physical 109.35 Hz divider floor fold upward by octaves while preserving pitch class;
- SN76489 `Oct+` is root + one octave + two octaves upward;
- live notes outside C1..B4 release through the same clamp used by NoteOn.

This test does not cover synth persistence, TB303 VCA, distortion gain, SID articulation, aliasing, or knob acceleration. Those remain separate PRs.

## Hardware list

- M5Stack Cardputer-Adv with Stamp-S3A / ESP32-S3.
- USB-C data cable.
- Development computer with `arduino-cli`.
- Optional headphones or line recording path for easier pitch comparison.

The firmware audio contract remains mono, 22 050 Hz, block size 512.

## Wiring

No external wiring is required.

Use:

- the built-in Cardputer keyboard;
- the built-in ES8311 codec and speaker, or the normal headphone/recording path;
- USB-C for power, flashing, and Serial monitoring.

PORT.A is not used. Its invariant remains GPIO2 SDA / GPIO1 SCL.

## Build and flash

Run the complete host regression suite:

```bash
bash tests/run_host_tests.sh
```

Build Cardputer-Adv with all warnings:

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
```

Flash the branch. Replace the port when necessary:

```bash
bash scripts/upload.sh /dev/ttyACM0
```

Open Serial monitoring:

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

### AY chromatic pitch

1. Stop transport.
2. Select AY for Synth A.
3. Set Noise near zero and use a stable envelope.
4. Play:

```text
D#4 → E4 → F4 → F#4 → G4
```

Expected: five clearly distinct ascending pitches. None may repeat the previous pitch.

For a longer check, play C1 through B4 chromatically. Pitch quantization error is expected to remain within ±5 cents in the supported range.

### SN76489 lower register

1. Select SN76489 for Synth A.
2. Set `Stack = Uni`, Noise = 0.
3. Play:

```text
C1 → A1 → C2 → F#2 → A2
```

Expected:

- the sequence no longer collapses to one 109.35 Hz drone;
- pitch classes remain C, A, C, F#, A;
- unsupported low notes sound one or more octaves higher;
- C1 and C2 may intentionally resolve to the same playable C because both are octave-folded.

### SN76489 stack

Set `Stack = Oct+` and play A2, A3, then A4.

Expected: each note contains root, one octave up, and two octaves up. The old fixed 109.35 Hz lower drone must not appear.

### Out-of-range NoteOff

With transport stopped, trigger a live note above B4 or below C1 through a MIDI/performance input path, then release the same physical/MIDI key.

Expected:

- the sounding note is clamped to C1..B4 as before;
- releasing the original out-of-range key releases the voice;
- no stuck note remains;
- Panic is not required for normal release.

## Serial validation

No new continuous Serial telemetry is added by this PR.

Confirm:

- no assertion, watchdog, Guru Meditation, heap corruption, or reset;
- no continually increasing underrun count during the note tests;
- engine switching and note release produce no error log.

## Troubleshooting

### AY notes still repeat

Confirm the flashed firmware is from `agent/synth-pitch-note-lifecycle`, AY is selected, and the test is D#4 through G4 rather than notes beyond the supported C1..B4 range.

### SN low notes sound higher than their labels

This is intentional. The SN76489 cannot represent frequencies below 109.35 Hz. GroovePuter folds unsupported notes upward by whole octaves to preserve musical pitch class instead of collapsing them to one note.

### C1 and C2 sound the same on SN76489

This is expected under octave folding: both resolve to the first playable C. Compare C1 with A1 or F#2 to verify that different pitch classes remain distinct.

### Out-of-range note remains sounding

Record the exact MIDI note, synth engine, transport state, and input path. Press Panic once, then repeat after a reboot. A reproducible stuck note fails acceptance.

## Acceptance checklist

- [ ] `bash tests/run_host_tests.sh` passes.
- [ ] Cardputer-Adv build passes with `--warnings all`.
- [ ] AY D#4, E4, F4, F#4, and G4 are five distinct ascending pitches.
- [ ] AY C1..B4 has no adjacent collapsed semitones.
- [ ] SN C1, A1, C2, F#2, and A2 do not all sound as one drone.
- [ ] SN octave-folded notes preserve pitch class.
- [ ] SN `Oct+` contains upward octaves and no fixed lower drone.
- [ ] A NoteOn above B4 releases from the matching original NoteOff.
- [ ] A NoteOn below C1 releases from the matching original NoteOff.
- [ ] Normal in-range live note release is unchanged.
- [ ] Synth A and Synth B both pass the out-of-range release check.
- [ ] No reset, watchdog, heap corruption, or continuously growing underrun count occurs.
- [ ] Persistence, TB303, SID, FX, and UI behavior outside this PR remain unchanged.

Do not merge until the AY, SN76489, and out-of-range NoteOff checks have been physically validated on Cardputer-Adv.
