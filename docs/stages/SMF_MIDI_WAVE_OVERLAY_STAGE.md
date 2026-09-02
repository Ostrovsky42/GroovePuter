# SMF MIDI Wave Overlay Stage

## Purpose

Draw a small MIDI-reactive waveform directly over the current SMF progress track. The animation reacts to NoteOn events that were accepted by routing, track mute and the bounded SMF output queue. It is a musical activity display, not an audio oscilloscope of SEQTRAK output.

## Hardware list

- M5Stack Cardputer-Adv
- Yamaha SEQTRAK
- data-capable USB-C cable
- SD card with `.mid` files under `/midi`

## Wiring

```text
Cardputer-Adv USB-C <---- MIDI ----> Yamaha SEQTRAK USB-C
```

PORT.A is unused. If unrelated I2C hardware is attached, keep GPIO2 SDA / GPIO1 SCL on `Wire`.

## Build / Flash

```bash
bash tests/run_host_tests.sh
bash scripts/build_sdl.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

## Expected behavior

- the existing Tape PCM waveform is unchanged;
- the MIDI Player progress bar contains a thin animated wave;
- NoteOn velocity controls visual amplitude;
- note pitch changes visual frequency;
- chords and dense passages create stronger movement;
- Pause, Stop, Panic and muted tracks stop producing new impulses;
- the envelope decays to a flat center line when MIDI activity stops;
- Inspector, Track Mute, RAW/SEQTRAK routing and SEQ MASTER remain usable.

## Troubleshooting

### Progress moves but the wave stays flat

Confirm that the selected track is not muted and that the file's source channels are mapped in the selected RAW/SEQTRAK routing mode. Unmapped and muted NoteOn events intentionally do not animate the overlay.

### Wave moves before the audible note

Treat as a regression. Scheduling may look ahead, but the visual timeline releases an impulse only when `currentTick` reaches the queued event tick.

### Existing Tape waveform changed

Treat as a regression. This stage does not modify `MiniAcid::WaveformBuffer` or `WaveformVisualization`.

## Acceptance checklist

```text
[ ] Tape page PCM waveform is unchanged
[ ] MIDI progress bar remains readable
[ ] wave moves on accepted SMF NoteOn events
[ ] stronger velocity gives a larger impulse
[ ] different pitches visibly change wave density
[ ] muted track stops creating new impulses
[ ] unmapped SEQTRAK-safe channels do not animate
[ ] Pause/Stop/Panic decays to a flat line
[ ] Continue resumes without an artificial first impulse
[ ] Inspector and Track Mute controls remain usable
[ ] no full-screen redraw was added specifically for the wave
[ ] no new task, heap allocation or USB owner was added
[ ] host-tests green
[ ] SDL build green
[ ] Cardputer-Adv build green
```
