# Performance MIDI Targets: Synth A / Synth B

## Purpose

Provide a reliable live performance keyboard for Cardputer-Adv while preserving the accepted sample-timed PatternPlayer USB path.

Current user-visible live targets:

- `SYNTH A` -> USB-MIDI channel 8;
- `SYNTH B` -> USB-MIDI channel 9.

The `Drums` backend target remains in the event/dispatcher model for the next runtime-routing stage, but it is intentionally removed from the PERFORM target cycle for now. Hardware acceptance showed that `DRUMS -> channel 10` is not a valid SEQTRAK-native drum route. SEQTRAK-native Pattern/live drum routing must use the configured per-voice drum routes on channels 1..7 instead.

Changing target sends a target-scoped `AllNotesOff` to the old lane before the new target becomes active.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3);
- data-capable USB-C cable;
- Yamaha SEQTRAK or another USB host/class-compliant MIDI monitor.

## Wiring

Connect the Cardputer-Adv USB-C data port to the existing GroovePuter USB-MIDI host/device path.

No PORT.A wiring is used. GPIO2/GPIO1 and the shared `Wire` I2C bus are not involved.

## Build / Flash

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Use the repository-pinned M5Stack ESP32 core `3.2.2` and existing TinyUSB options.

## Controls

Open the `PERFORM` page.

```text
\        cycle SYNTH A <-> SYNTH B
N        toggle NOTE mode
X        panic selected live target
, / .    previous / next scale
- / =    octave down / up (-2 .. +2)
Fn+Tab   cycle PERFORM / PATTERN / ARRANGE workflow
1..0     global track mutes (unchanged)
```

Keyboard manuals:

```text
ASDFGHJKL   base manual
QWERTYUIOP  one octave above base
```

## Expected behavior

### Screen

```text
TARGET:SYNTH A  MIDI CH:8
TARGET:SYNTH B  MIDI CH:9
ROOT:C SCALE:<name> OCT:-2..+2
```

### MIDI monitor / SEQTRAK

- Synth A notes arrive on channel 8;
- Synth B notes arrive on channel 9;
- external MIDI notes follow the full `-2 .. +2` keyboard octave range;
- target changes release the previous live lane first;
- PatternPlayer Synth A/B remain on channels 8/9.

### Internal audio

The internal synth engines retain their established safe `24..71` note range in this stage. Wider performance notes are clamped symmetrically for internal NoteOn/NoteOff ownership, while USB MIDI keeps the wider note value. This prevents stuck internal notes without changing existing DSP/pattern bounds.

## Known pending work

- SEQTRAK-native drum routing using the eight `MidiOutputSettings` drum routes and channels 1..7;
- runtime profile application;
- MIDI settings UI;
- SMF realtime player and its playback modes.

The existing `MidiImporter` is still a 1/16-grid importer, not a faithful SMF player.

## Troubleshooting

- **1/2/3 mute tracks instead of changing pages:** expected. Plain digits are global mute shortcuts. Use `Fn+Tab` for workflow navigation.
- **No Drums target:** expected in this revision. The invalid fixed channel-10 drum shortcut was removed after hardware testing.
- **No USB MIDI:** verify a data-capable cable and TinyUSB enumeration.
- **Notes blocked while transport runs:** current ownership contract intentionally gives PatternPlayer the internal synth voices while transport is active.
- **Stuck note:** press `X`; target changes also issue scoped cleanup.

## Acceptance checklist

- [ ] `bash tests/run_host_tests.sh` succeeds.
- [ ] `bash scripts/build.sh --warnings all` succeeds for Cardputer-Adv.
- [ ] `\` cycles Synth A <-> Synth B.
- [ ] `-` and `=` reach `OCT:-2` and `OCT:+2`.
- [ ] external MIDI pitch follows every octave position.
- [ ] internal notes never remain stuck at octave extremes.
- [ ] Synth A is received on MIDI channel 8.
- [ ] Synth B is received on MIDI channel 9.
- [ ] plain `1..0` retain global mute behavior.
- [ ] `Fn+Tab` changes workflow pages.
- [ ] PatternPlayer Synth A/B still play on channels 8/9.
