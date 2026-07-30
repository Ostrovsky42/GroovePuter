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

Use the repository-pinned M5Stack ESP32 core 3.2.2. A build made with core 3.2.5 is not an acceptance result for this PR.

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
- Retrig produces explicit retriggered NoteOff/NoteOn pairs when the physical note is not shared by another logical owner.
- Stop releases both pattern lanes.
- Muting Synth A releases only PatternPlayer Synth A.
- Muting Synth B releases only PatternPlayer Synth B.
- Pattern, Song-row, or scene changes do not leave stale notes.
- Internal Synth A and Synth B continue to sound at the same time.
- Live NOTE panic does not cancel PatternPlayer Synth A when both own the same channel and pitch.
- Offline WAV rendering does not enqueue or replay render-generated MIDI events.

## Collision test: live and Pattern Synth A

1. Put C4 on a sustained Synth A pattern step and start transport.
2. In NOTE mode, hold the same C4 from the Cardputer keyboard.
3. Press live Panic or release the live C4 while the pattern note remains active.
4. Observe MIDI channel 8 and listen to the external instrument.

Expected result:

```text
first logical owner  -> one physical NoteOn C4
second logical owner -> no duplicate physical NoteOn
live release/Panic   -> no physical NoteOff while Pattern Synth A owns C4
final pattern release -> one physical NoteOff C4
```

## Queue-pressure recovery test

TinyUSB queue rejection is covered deterministically by host tests. On hardware, exercise rapid replacements and retrigs at the highest practical BPM, then Stop on an empty following step. The previous pitch must not continue sounding. Capture `aseqdump` output if a stale note occurs.

## Offline WAV render test

1. Start a Synth A/B pattern and confirm external MIDI activity.
2. Trigger the existing WAV export workflow.
3. Keep `aseqdump` running during and after the export.

Expected result:

- no render-speed NoteOn/NoteOff burst;
- no queue-overflow burst after export;
- the external pattern voices end through target-scoped cleanup;
- normal realtime MIDI resumes only after the next transport start.

## Timing stress test

Use maximum practical BPM and maximum Synth A/B retrig while navigating between pages and forcing normal UI redraws. Check timestamped `aseqdump` output for large event batches. Small USB jitter is acceptable in Stage 1; audible burst delivery or lost final NoteOff is not.

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
5. Stop, mute each synth independently, and change patterns/scenes while listening for stale notes.
6. Repeat the same-pitch collision test with SYNTH 1.

## Troubleshooting

- No USB device: verify the cable carries data and rebuild with pinned core 3.2.2.
- CDC works but MIDI is absent: verify `USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`.
- Only one synth responds: confirm channel 8/9 receive configuration on the monitor or SEQTRAK.
- Pattern C4 stops when live C4 is released: capture the exact channel/note event sequence; do not merge.
- Stuck note after replacement or Stop: capture the final `aseqdump` events and the periodic underrun line; do not merge.
- MIDI burst after WAV export: record timestamps and do not merge.
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
- [ ] Pattern A C4 + live C4 + live Panic keeps Pattern A C4 sounding
- [ ] rapid failed/retried replacements leave no stale previous pitch
- [ ] WAV render emits no realtime MIDI burst
- [ ] max retrig and high BPM remain usable during UI redraws
- [ ] pattern change leaves no stuck note
- [ ] scene change leaves no stuck note
- [ ] direct SEQTRAK test succeeds
- [ ] internal Synth A and Synth B remain audible
- [ ] no reboot, watchdog, heap corruption or continual underrun growth

## Known limitations

- routes are fixed; channel settings UI is a later stage
- 303 slide is translated as old-note NoteOff followed by new-note NoteOn
- when live and Pattern Synth A share the same channel and pitch, the first physical NoteOn velocity remains in effect until the final logical owner releases it
- the audio-to-control ring has 64 storage slots and 63 usable event positions
- Stage 1 does not add a timestamp scheduler; the timing stress test is therefore part of the merge gate
- drums, MIDI clock, Start/Stop messages, Song-specific rendering and BLE-MIDI are out of scope
