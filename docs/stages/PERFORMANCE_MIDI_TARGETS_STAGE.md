# Performance MIDI Targets: Synth A / Synth B / Drums

## Purpose

Allow the Cardputer-Adv performance keyboard to select one of three fixed live targets without changing PatternPlayer routing:

- `SYNTH A` -> USB-MIDI channel 8;
- `SYNTH B` -> USB-MIDI channel 9;
- `DRUMS` -> USB-MIDI channel 10.

Changing target sends a target-scoped `AllNotesOff` to the old lane before the new target becomes active. This prevents stuck notes. `DRUMS` is USB-MIDI-only; it does not alias to an internal GroovePuter synth voice.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3);
- data-capable USB-C cable;
- Yamaha SEQTRAK or another USB host/class-compliant MIDI monitor;
- optional second USB connection or powered hub when serial monitoring and MIDI capture are required together.

## Wiring

Connect the Cardputer-Adv USB-C port to the MIDI host/device path used for the existing GroovePuter USB-MIDI test.

No PORT.A wiring is used. GPIO2/GPIO1 and the shared `Wire` I2C bus are not involved in this test.

## Build / Flash

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh
```

Use the repository-pinned M5Stack Arduino core and the existing TinyUSB build options. The firmware requires `USBMode=default` with CDC enabled, as enforced by the build scripts.

## Controls

Open the `PERFORM` page.

```text
\       cycle SYNTH A -> SYNTH B -> DRUMS -> SYNTH A
N       toggle NOTE mode
X       panic the selected live target
, / .   previous / next scale
- / =   octave down / up
```

The screen displays the selected target and external MIDI channel.

## Expected behavior

### Screen

```text
TARGET:SYNTH A  MIDI CH:8
TARGET:SYNTH B  MIDI CH:9
TARGET:DRUMS    MIDI CH:10
```

`DRUMS` also displays `LIVE USB MIDI (EXTERNAL)`.

### MIDI monitor / SEQTRAK

- notes played on `SYNTH A` arrive on channel 8;
- notes played on `SYNTH B` arrive on channel 9;
- notes played on `DRUMS` arrive on channel 10;
- pressing `\` while a note is held releases the old channel before any note is sent to the new channel;
- `X` releases only the selected live target;
- PatternPlayer Synth A/B remain on channels 8/9.

### Internal audio

- `SYNTH A` plays internal voice A and USB-MIDI channel 8;
- `SYNTH B` plays internal voice B and USB-MIDI channel 9;
- `DRUMS` sends USB MIDI only and does not trigger internal Synth A.

## Troubleshooting

- **No target change:** confirm the `PERFORM` page is active and press the plain backslash key without Alt/Ctrl.
- **No USB MIDI:** use a data-capable cable and verify the device enumerates with the repository TinyUSB build settings.
- **Wrong destination on SEQTRAK:** verify receiving tracks are configured for MIDI channels 8, 9 and 10.
- **Notes blocked while transport runs:** this is intentional in the current performance ownership contract; stop PatternPlayer before live playing.
- **Stuck note after reconnect:** press `X`; disconnect/reconnect also clears local lane ownership and stale notes are not replayed.

## Acceptance checklist

- [ ] `bash tests/run_host_tests.sh` succeeds.
- [ ] `bash scripts/build.sh --warnings all` succeeds for Cardputer-Adv.
- [ ] `\` cycles A -> B -> D -> A on screen.
- [ ] target switch while holding a note produces NoteOff/AllNotesOff on the old target.
- [ ] Synth A is received on MIDI channel 8.
- [ ] Synth B is received on MIDI channel 9.
- [ ] Drums is received on MIDI channel 10.
- [ ] Drums does not trigger internal Synth A.
- [ ] PatternPlayer Synth A/B still play on channels 8/9.
- [ ] `X` clears the selected live lane without stopping the other PatternPlayer lane.
