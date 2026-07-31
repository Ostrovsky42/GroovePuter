# USB MIDI Transport Sync Stage

## Purpose

Validate GroovePuter as a USB MIDI transport master without changing the accepted Pattern MIDI ownership model.

Expected transport flow:

```text
MiniAcid 96 PPQN phase
        |
        v
AudioTask render bracket
        |
        +--> ScheduledMidiTransportEventQueue
        |      Start / Clock / Stop
        |
        +--> ScheduledMusicalEventQueue
               Pattern NoteOn / NoteOff
                    |
                    v
              MidiDispatchTask
                    |
                    +--> UsbMidiOutput (notes)
                    +--> CardputerUsbMidiTransport (F8 / FA / FC)
                    |
                    v
                  TinyUSB
```

`MidiDispatchTask` remains the only task that writes USB MIDI/TinyUSB.

MIDI Clock is 24 PPQN. GroovePuter is 96 PPQN internally, therefore one external Clock pulse corresponds to four internal ticks. The publisher is re-anchored from the current sequencer/audio phase on every render block; there is no independent `millis()`, `delay()` or FreeRTOS clock loop.

## Hardware list

- M5Stack Cardputer-Adv / ESP32-S3, no PSRAM required.
- Yamaha SEQTRAK or a computer/DAW that can receive class-compliant USB MIDI.
- Data-capable USB-C cable appropriate for the receiver.
- Optional Linux computer for MIDI capture/inspection.

Firmware assumptions:

- M5Stack ESP32 Arduino core remains pinned to `3.2.2` in CI/tooling.
- Native ESP32-S3 USB runs in TinyUSB/USB-OTG mode with CDC-on-boot.
- Internal audio remains on the existing ES8311/I2S path.
- PORT.A I2C is unrelated to this stage and remains GPIO2 SDA / GPIO1 SCL.
- Scroll Unit/Wire ownership and display initialization order are unchanged.

## Wiring

Direct hardware acceptance path:

```text
Cardputer-Adv USB-C
        |
        | data-capable USB cable
        v
Yamaha SEQTRAK USB
```

For computer capture, connect Cardputer-Adv USB-C to the Linux host instead and select the GroovePuter/Cardputer MIDI input port in ALSA or the DAW.

Do not change PORT.A wiring for this test.

## Build / Flash

Run the repository gates first:

```bash
bash tests/run_host_tests.sh
```

SDL build:

```bash
bash scripts/build_sdl.sh
```

Cardputer-Adv build:

```bash
bash scripts/build.sh
```

The Cardputer build must keep:

```text
PSRAM=disabled
USBMode=default
CDCOnBoot=cdc
UploadMode=cdc
```

Flash with the existing upload path:

```bash
bash scripts/upload.sh
```

Do not update Arduino/M5Stack dependencies for this stage.

### Linux MIDI inspection

List ALSA sequencer ports:

```bash
aseqdump -l
```

Inspect decoded events:

```bash
aseqdump -p <client:port>
```

For raw MIDI bytes, `amidi` can include MIDI Clock when `-c` is supplied:

```bash
amidi -l
amidi -p hw:<card>,<device>,<subdevice> -d -c
```

Expected raw status bytes are:

```text
FA    Start
F8    Timing Clock
FC    Stop
```

`aseqdump` is convenient for event presence/order but does not expose a simple arrival-time column. ALSA sequencer events support real-time timestamps internally; for precise jitter measurement use a DAW/MIDI monitor that exposes timestamps or a separate measurement tool. No timestamp utility is added as a GroovePuter project dependency.

## Expected behavior

1. Boot with USB connected or disconnected; internal audio still works.
2. Press Play once:
   - one MIDI Start (`0xFA`) is emitted;
   - the first Clock pulse is aligned to the transport start;
   - Clock continues at 24 PPQN.
3. At 120 BPM the nominal Clock interval is about `20.833 ms`.
4. Change BPM during playback:
   - Clock cadence follows the new BPM on the shared audio timeline;
   - no new Start/Stop is emitted.
5. Navigate pages, redraw the UI, mute tracks or change patterns:
   - transport does not restart;
   - Clock phase does not reset.
6. Song row transitions, including 1B / 2B / 4B / 8B rows:
   - no Start;
   - no Stop;
   - no Clock phase reset or catch-up burst.
7. Press Stop once:
   - existing PatternPlayer note cleanup runs first;
   - one MIDI Stop (`0xFC`) follows as the transport boundary;
   - queued Clock events from the old transport generation are invalidated;
   - no new F8 packets are emitted after Stop.
8. Disconnect/reconnect does not replay old Start or Clock packets. New current-phase Clock packets may resume while transport is still running, but stale packets are never replayed.

### Ordering and overflow contract

At the same `blockSequence + frameOffset` deadline, scheduled transport wins over scheduled musical events. Within transport traffic, lifecycle events have higher priority than Clock; the publisher inserts Start before the first Clock at transport start.

The already accepted PatternPlayer panic path is preserved. On Stop, target-scoped NoteOff/AllNotesOff cleanup may therefore reach the wire before the scheduled Stop event. This is intentional: note ownership is released first, then Stop closes the external transport lifecycle.

Clock overflow is lossy by design:

- Clock uses a fixed bounded SPSC queue.
- Clock cannot consume the final reserved lifecycle slots.
- a stale Clock more than 5 ms late is dropped instead of sent in a catch-up burst.
- Stop changes the transport generation, invalidating already queued Clock events.
- Start/Stop use reserved capacity; an extreme lifecycle-only overflow falls back to a fixed critical recovery mailbox and is counted in diagnostics.

No heap allocation is used by the realtime transport queue.

### Diagnostics

The existing aggregated line is extended approximately every five seconds:

```text
[MIDI-DISPATCH] ... clockSent=... clockLate=... clockDropped=... start=... stop=... transportFail=... transportDrop=... overflow=... recovery=...
```

There is no per-F8 Serial logging.

## Troubleshooting

### SEQTRAK/DAW receives notes but no Clock

- Confirm the receiver is configured to use external/USB MIDI clock.
- Verify Start/Clock with `aseqdump` or `amidi -d -c` on a computer.
- Check `[MIDI-DISPATCH]` counters:
  - `clockSent` should increase during playback;
  - `clockDropped` should not grow continuously under normal load;
  - `transportFail` usually indicates USB was not mounted when Start/Stop was attempted.

### Clock packets arrive in bursts

- Check `clockLate` and `clockDropped`.
- Confirm no debug build is printing per-sample/per-clock Serial output.
- Confirm the firmware is still using the single `MidiDispatchTask`; do not add USB writes to `loop()`, UI or DSP.
- At high BPM, old Clock events should be dropped rather than replayed.

### Stop occurs but a note hangs

- Confirm the existing Pattern MIDI tests still pass.
- Inspect `panic` and Pattern queue diagnostics.
- Re-test Synth A and Synth B separately; this stage must not change same-note wire ownership or failed NoteOff recovery.

### Reconnect starts the receiver unexpectedly

- Capture raw bytes after reconnect.
- A stale `FA` must not be replayed.
- If transport remains running, current-phase F8 may resume; stop and press Play again to deliver a fresh Start lifecycle.

### No USB MIDI device appears

- Use a known data cable.
- Keep `USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`.
- Keep the pinned M5Stack core version used by CI.
- Do not move TinyUSB initialization into `loop()` or the DSP engine.

## Acceptance checklist

Hardware acceptance completed on Cardputer-Adv -> Yamaha SEQTRAK after flashing this implementation.

- [x] Cardputer boots normally
- [x] internal audio works
- [x] Pattern MIDI Synth A/B still works
- [x] pressing Play produces one MIDI Start
- [x] playback produces stable MIDI Clock
- [x] Stop produces one MIDI Stop
- [x] no Clock packets continue after Stop
- [x] page navigation does not audibly alter tempo
- [x] Song row transitions do not restart external transport
- [x] Song 1B/2B/4B/8B timing remains correct
- [x] changing BPM changes external clock tempo
- [x] high BPM does not create packet bursts
- [x] reconnect does not replay stale Clock/Start packets
- [x] no stuck notes after Stop
- [x] no watchdog/reset/audio underrun regression

Observed during acceptance: transitions to some UI pages caused a brief CPU-load spike to approximately 90% and visible UI lag. This did not audibly alter tempo, interrupt MIDI Clock/Start/Stop, cause stuck notes, reset the device, or otherwise affect the transport acceptance criteria. Treat UI transition load as a separate performance follow-up rather than a transport-sync merge blocker.

Hardware pass criterion:

```text
Play
-> exactly one Start
-> stable 24 PPQN Clock
-> continuous Pattern/Song playback
-> exactly one Stop
-> silence/no Clock after Stop
```

Cardputer-Adv -> Yamaha SEQTRAK hardware acceptance passed.
