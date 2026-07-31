# PERFORM DX + Native SEQTRAK Drum Channels Stage

## Purpose

Extend the existing Cardputer-Adv PERFORM page without changing GroovePuter transport or PatternPlayer timing:

```text
PERFORM target cycle
SYNTH A -> SYNTH B -> DX -> DRUMS -> SYNTH A
```

SEQTRAK-native live routing in this stage:

```text
SYNTH A  -> MIDI CH8
SYNTH B  -> MIDI CH9
DX       -> MIDI CH10

DRUMS
A  KICK   -> MIDI CH1, note 60
S  SNARE  -> MIDI CH2, note 60
D  CLAP   -> MIDI CH3, note 60
F  HAT 1  -> MIDI CH4, note 60
G  HAT 2  -> MIDI CH5, note 60
H  PERC 1 -> MIDI CH6, note 60
J  PERC 2 -> MIDI CH7, note 60
```

DX and DRUMS are USB-MIDI-only targets. They must never alias to internal Synth A/B.

## Hardware list

- M5Stack Cardputer-Adv / ESP32-S3.
- Yamaha SEQTRAK.
- Data-capable USB-C cable.
- Optional computer with MIDI monitor for channel verification.

Firmware assumptions:

- M5Stack ESP32 Arduino core pinned to `3.2.2`.
- `PSRAM=disabled`.
- Native ESP32-S3 USB uses `USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`.
- Existing internal audio, PatternPlayer MIDI and MIDI Clock/Start/Stop remain unchanged.
- PORT.A I2C remains GPIO2 SDA / GPIO1 SCL and is unrelated to this test.

## Wiring

Direct SEQTRAK test:

```text
Cardputer-Adv USB-C
        |
        | data-capable USB cable
        v
Yamaha SEQTRAK USB
```

No PORT.A wiring is required.

## Build / Flash

Run host regressions:

```bash
bash tests/run_host_tests.sh
```

Build SDL target:

```bash
bash scripts/build_sdl.sh
```

Build Cardputer-Adv firmware:

```bash
bash scripts/build.sh
```

Flash:

```bash
bash scripts/upload.sh
```

Do not update the pinned Arduino/M5Stack dependencies for this stage.

## Expected behavior

### Target selector

On PERFORM, press `\\` repeatedly:

```text
SYNTH A -> SYNTH B -> DX -> DRUMS -> SYNTH A
```

Changing target sends target-scoped cleanup for the previous live target before switching.

### DX

With target `DX`:

- screen shows `MIDI CH:10`;
- NOTE keys send NoteOn/NoteOff to SEQTRAK channel 10;
- GroovePuter internal Synth A/B must not sound from those DX key presses;
- `X` releases only the live DX lane;
- PatternPlayer Synth A/B ownership remains unchanged.

### Drums

With target `DRUMS`:

- screen shows `MIDI CH:1-7`;
- only `A/S/D/F/G/H/J` are active drum pads;
- all pads send MIDI note 60;
- physical MIDI channel chooses the SEQTRAK drum track;
- multiple pads can remain held simultaneously because each channel owns an independent live lane;
- releasing one pad sends NoteOff only for that pad/channel;
- `X` sends one target-scoped Drums panic that releases every active CH1..7 drum lane.

Expected mapping:

```text
A -> CH1 KICK
S -> CH2 SNARE
D -> CH3 CLAP
F -> CH4 HAT 1
G -> CH5 HAT 2
H -> CH6 PERC 1
J -> CH7 PERC 2
```

Keys in the other NOTE row are consumed while DRUMS is selected but do not emit an invalid drum channel.

### Transport interaction

Existing behavior remains:

- live performance input is blocked while GroovePuter PatternPlayer transport is running;
- MIDI Clock stays 24 PPQN;
- Start/Stop behavior is unchanged;
- PatternPlayer Synth A/B remain on CH8/CH9;
- this stage does not add PatternPlayer drum MIDI output.

## Troubleshooting

### DX triggers internal Synth A

This is a regression. `InternalSynthOutput` must reject both `MusicalEventTarget::Dx` and `MusicalEventTarget::Drums`.

### DRUMS all play one SEQTRAK track

Verify the outgoing channel for each pad. The expected zero-based USB channels are `0..6`, displayed externally as MIDI CH1..7. Note number remains 60 for every pad.

### A drum pad hangs

Press `X` while DRUMS is selected. The scoped Drums panic must release all seven native live lanes. Check `liveDrop`/`panic` diagnostics if this occurred during heavy input.

### DX note hangs after queue overflow

The live control queue has a dedicated DX overflow bit. `MidiDispatchTask` must convert it to a scoped DX AllNotesOff recovery.

### Pattern MIDI or Clock changed

This stage must not alter PatternPlayer scheduling or transport timing. Re-run the existing Pattern MIDI and transport acceptance checks before merging.

## Acceptance checklist

### Screen / controls

- [ ] PERFORM target cycle is A -> B -> DX -> DRUMS -> A.
- [ ] DX displays MIDI CH10.
- [ ] DRUMS displays MIDI CH1-7.
- [ ] `X` clears the currently selected live target without stuck notes.

### DX hardware

- [ ] DX notes reach SEQTRAK DX / MIDI CH10.
- [ ] DX key presses do not trigger internal Synth A or Synth B.
- [ ] changing DX -> DRUMS while holding a note leaves no DX note hanging.

### Drum hardware

- [ ] `A` reaches CH1 / KICK.
- [ ] `S` reaches CH2 / SNARE.
- [ ] `D` reaches CH3 / CLAP.
- [ ] `F` reaches CH4 / HAT 1.
- [ ] `G` reaches CH5 / HAT 2.
- [ ] `H` reaches CH6 / PERC 1.
- [ ] `J` reaches CH7 / PERC 2.
- [ ] two or more drum pads can overlap without one replacing the other.
- [ ] releasing one drum pad does not release another held pad.
- [ ] DRUMS panic clears every active CH1..7 live lane.

### Regression

- [ ] internal GroovePuter Synth A/B still work.
- [ ] PatternPlayer Synth A remains MIDI CH8.
- [ ] PatternPlayer Synth B remains MIDI CH9.
- [ ] MIDI Clock/Start/Stop remains stable.
- [ ] no reset/watchdog/audio-underrun regression.
- [ ] disconnect/reconnect does not replay stale notes.

Hardware pass criterion:

```text
A/B melodic targets unchanged
+ DX plays CH10 only
+ A..J drum pads reach CH1..7 independently
+ no stuck notes
+ transport timing unchanged
```
