# PatternPlayer USB-MIDI — Cardputer-Adv acceptance

## Purpose

Validate Stage 1 pattern output without changing GroovePuter's autonomous audio behavior:

```text
PatternPlayer Synth A -> USB MIDI channel 8
PatternPlayer Synth B -> USB MIDI channel 9
```

Live NOTE mode remains the Stage 0 route. Drums, MIDI clock and transport messages are not part of this test.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3, no PSRAM)
- data-capable USB-C cable; a charge-only cable will not work
- Linux computer with ALSA utilities
- Yamaha SEQTRAK for the second test stage
- headphones or speaker path for confirming internal GroovePuter audio

## Wiring

Computer test:

```text
Cardputer-Adv USB-C -> USB-C data cable -> Linux computer
```

SEQTRAK test:

```text
Cardputer-Adv USB-C -> USB-C data cable -> SEQTRAK USB-C
```

No GPIO, PORT.A or external power wiring is used.

## Build and flash

Use the repository-pinned M5Stack ESP32 core 3.2.2:

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

The required FQBN options are:

```text
USBMode=default,CDCOnBoot=cdc,UploadMode=cdc
```

## Linux MIDI monitor

```bash
aconnect -l
aseqdump -l
aseqdump -p <client:port>
```

Prepare different audible patterns for Synth A and Synth B, then start GroovePuter transport.

## Expected behavior

- Synth A pattern sends NoteOn/NoteOff on MIDI channel 8.
- Synth B pattern sends NoteOn/NoteOff on MIDI channel 9.
- Velocity follows each pattern step.
- A replacement pitch sends old NoteOff before new NoteOn.
- TIE extends the existing note and does not create a duplicate NoteOn.
- Retrig produces explicit retriggered NoteOff/NoteOn pairs.
- Stop releases both pattern lanes.
- Muting Synth A releases only PatternPlayer Synth A.
- Muting Synth B releases only PatternPlayer Synth B.
- Pattern, Song-row, or scene changes do not leave stale notes.
- Internal Synth A and Synth B continue to sound at the same time.
- Live NOTE panic does not cancel PatternPlayer lanes.

## Serial output

Normal boot should pass the existing USB stages:

```text
[BOOT-STAGE] 52 before USB MIDI sink
[BOOT-STAGE] 53 after USB MIDI sink
```

There is no production per-note Serial logging. Periodic performance output must not show continual underrun growth.

## SEQTRAK procedure

1. Complete the Linux MIDI-monitor test first.
2. Connect Cardputer-Adv directly to SEQTRAK with the same data cable.
3. Configure/observe SYNTH 1 on MIDI channel 8 and SYNTH 2 on MIDI channel 9.
4. Start a GroovePuter pattern containing distinct Synth A and Synth B notes.
5. Stop, mute each synth independently, and change patterns while listening for stale notes.

## Troubleshooting

- No USB device: verify the cable carries data and rebuild with pinned core 3.2.2.
- CDC works but MIDI is absent: verify `USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`.
- Only one synth responds: confirm channel 8/9 receive configuration on the monitor or SEQTRAK.
- Stuck note after Stop: capture the final `aseqdump` events and the periodic underrun line; do not merge.
- Internal audio disappears: treat as a regression; USB MIDI is an additive sink only.

## Acceptance checklist

- [ ] host-tests pass
- [ ] SDL build passes
- [ ] Cardputer-Adv build passes with M5Stack core 3.2.2
- [ ] Cardputer enumerates as CDC + class-compliant MIDI
- [ ] Synth A pattern appears on MIDI channel 8
- [ ] Synth B pattern appears on MIDI channel 9
- [ ] step velocity is preserved
- [ ] Stop releases both external synth notes
- [ ] Synth A mute does not cancel Synth B
- [ ] Synth B mute does not cancel Synth A
- [ ] pattern change leaves no stuck note
- [ ] direct SEQTRAK test succeeds
- [ ] internal Synth A and Synth B remain audible
- [ ] no reboot, watchdog, heap corruption or continual underrun growth

## Known limitations

- routes are fixed; channel settings UI is a later stage
- 303 slide is translated as old-note NoteOff followed by new-note NoteOn
- drums, MIDI clock, Start/Stop messages, Song-specific rendering and BLE-MIDI are out of scope
