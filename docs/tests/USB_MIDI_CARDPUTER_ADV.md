# USB-MIDI Device — Cardputer ADV

## Purpose

Validate the first class-compliant USB-MIDI output slice on real M5Stack Cardputer-Adv hardware.

The tested path is:

```text
Cardputer keyboard matrix
→ PerformanceKeyboard
→ MusicalEventRouter
   ├── InternalSynthOutput → Synth A → built-in speaker / audio out
   └── UsbMidiOutput → native ESP32-S3 TinyUSB MIDI → external host
```

This slice sends only live performance-keyboard events for logical Synth A.
It does not send PatternPlayer events, drums, MIDI clock, transport, CC,
Program Change, Pitch Bend, SysEx, or MIDI input.

## Hardware list

- M5Stack Cardputer-Adv with Stamp-S3A / ESP32-S3FN8.
- USB-C data cable.
- Linux development computer with `arduino-cli` and ALSA utilities.
- Built-in speaker or 3.5 mm audio output for internal-audio comparison.
- Optional Yamaha SEQTRAK for the second acceptance stage.

Cardputer-Adv remains a DRAM-only target:

```text
PSRAM=disabled
PartitionScheme=huge_app
```

The firmware explicitly selects native USB-OTG/TinyUSB with CDC enabled so
Serial/upload and MIDI share the ESP32-S3 USB device connection.

## Wiring

### Computer test

```text
Cardputer-Adv USB-C
→ USB-C data cable
→ computer USB port
```

### SEQTRAK test

Perform only after the computer MIDI-monitor test passes:

```text
Cardputer-Adv USB-C
→ USB-C data cable
→ SEQTRAK USB-C
```

No GPIO wiring is required. PORT.A is unused. Its project invariant remains:

```text
SDA = GPIO2
SCL = GPIO1
```

## Build and flash

Install the pinned dependencies:

```bash
bash scripts/install_arduino_deps.sh
```

Compile with all warnings:

```bash
bash scripts/build.sh --warnings all
```

Flash current sources. Replace the port when needed:

```bash
bash scripts/upload.sh /dev/ttyACM0
```

Open Serial diagnostics:

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

The build and upload scripts use the same FQBN:

```text
m5stack:esp32:m5stack_cardputer:
PSRAM=disabled,
PartitionScheme=huge_app,
USBMode=default,
CDCOnBoot=cdc,
UploadMode=cdc
```

## Routing contract

The first spike has one immutable external route:

```text
PerformanceKeyboard
→ MusicalEventTarget::SynthA
→ zero-based channel 7
→ MIDI channel 8
→ SEQTRAK SYNTH 1
```

Internal Synth A remains active at the same time.

External USB output is deliberately monophonic:

```text
NoteOn new note:
1. send NoteOff for the previous active external note;
2. send NoteOn for the new note;
3. remember the new note.

NoteOff:
- send only when it matches the active external note.

AllNotesOff:
- send one NoteOff for the active external Synth A note;
- do not send CC 123;
- do not affect Synth B.
```

## Computer MIDI-monitor test

Install ALSA utilities when absent:

```bash
sudo apt-get update
sudo apt-get install -y alsa-utils
```

List MIDI clients and ports:

```bash
aconnect -l
aseqdump -l
```

Start monitoring the GroovePuter MIDI port:

```bash
aseqdump -p <client:port>
```

Use the exact client and port shown by `aseqdump -l`.

### Expected events

With transport stopped and NOTE mode ON:

1. Press `A`.
2. Hold `A`, then press `S`.
3. Release `S` while still holding `A`.
4. Release `A`.
5. Press a note and then press `X`.

Expected MIDI sequence:

```text
A down       → NoteOn  channel 8, C2, velocity > 0
S down       → NoteOff channel 8, C2
             → NoteOn  channel 8, next scale note
S up         → NoteOff channel 8, previous active note
             → NoteOn  channel 8, C2
A up         → NoteOff channel 8, C2
X Panic      → one NoteOff for the active external note
```

Exact pitches after `A` depend on the selected scale, but the display and MIDI
monitor must agree.

## Disconnect and reconnect test

1. Hold and release a note normally.
2. Disconnect the USB cable.
3. Wait two seconds.
4. Reconnect the cable.
5. Reopen `aseqdump` if the client number changed.
6. Press a fresh note.

Expected:

- no reset or watchdog;
- no unbounded event queue;
- no stale note is replayed automatically;
- the next newly pressed note produces a valid NoteOn/NoteOff pair;
- internal Synth A remains usable while no USB host is connected.

## SEQTRAK test

Run only after all computer checks pass.

1. Stop GroovePuter transport.
2. Enable NOTE mode on PERFORM.
3. Connect Cardputer-Adv directly to SEQTRAK by USB-C data cable.
4. Select or monitor SEQTRAK SYNTH 1, which receives MIDI channel 8 in this profile.
5. Play both Cardputer keyboard rows.
6. Test scale changes and octave `-1` / `0`.
7. Test last-note priority and `X` Panic.
8. Start GroovePuter transport and confirm no clock, transport, or PatternPlayer notes are sent.

Expected:

- GroovePuter keys play SEQTRAK SYNTH 1;
- internal GroovePuter Synth A remains audible through its own audio path;
- replacing a held note does not leave the previous SEQTRAK note sounding;
- release of the final key stops the external note;
- `X` stops the current external live note;
- GroovePuter transport does not start or stop SEQTRAK;
- GroovePuter PatternPlayer is not routed to USB in this slice.

## Expected behavior

### USB disconnected

- GroovePuter boots and functions as a standalone groovebox.
- Internal live Synth A works.
- USB output drops events without blocking or allocating an unbounded queue.

### USB mounted

- PerformanceKeyboard events for Synth A are mirrored to MIDI channel 8.
- Internal Synth A continues receiving the same normalized events.
- Synth B and PatternPlayer are not sent to USB.
- MIDI event handling does not acquire `AudioMutationGate`.

### Transport running

- PerformanceKeyboard already blocks live NoteOn while PatternPlayer owns Synth A.
- Note keys remain consumed by NOTE mode and do not trigger legacy randomize/BPM commands.
- No MIDI clock, Start, Stop, Continue, or PatternPlayer notes are emitted.

## Serial validation

Watch the existing startup and `[PERF]` diagnostics.

Confirm:

- Serial remains available in the TinyUSB composite build;
- no USB initialization failure prevents the groovebox from booting;
- `underruns` does not continually increase;
- free internal heap does not decrease on every MIDI event;
- no watchdog, Guru Meditation, reset, or heap-corruption message appears.

## Troubleshooting

### No MIDI client appears

1. Confirm the cable supports data, not charging only.
2. Run `lsusb`, `aconnect -l`, and `aseqdump -l` again after reconnecting.
3. Confirm the build log contains:

```text
USBMode=default,CDCOnBoot=cdc,UploadMode=cdc
```

4. Confirm the firmware was rebuilt after switching from hardware CDC/JTAG mode.
5. Enter the ESP32-S3 bootloader manually if upload no longer finds the application CDC port.
6. Capture `dmesg --follow` during connection.

### Serial works but MIDI does not appear

- Confirm the Cardputer build compiled `USBMIDI.h` from the pinned M5Stack core.
- Confirm the native MIDI interface was constructed before TinyUSB started.
- Check the PR build logs for TinyUSB MIDI configuration errors.
- Do not add Yamaha-specific SysEx as a workaround.

### MIDI appears but no notes arrive

1. Stop GroovePuter transport.
2. Open PERFORM.
3. Set `NOTE MODE: ON`.
4. Press `X`, release all keys, and try again.
5. Confirm `aseqdump` is monitoring the correct client and port.

### Notes remain sounding externally

1. Press `X`.
2. Release all physical keys.
3. Disconnect and reconnect USB.
4. Record the exact key sequence and MIDI-monitor output.
5. Verify every replacement NoteOn is preceded by NoteOff for the old active note.

### Direct SEQTRAK connection does not enumerate

1. Reconfirm that the same firmware works with the computer MIDI monitor.
2. Try another USB-C data cable.
3. Reboot both devices before reconnecting.
4. Capture the computer-side USB descriptors for comparison.
5. Do not add clock, CC, SysEx, or MIDI input while diagnosing enumeration.

### SEQTRAK receives only the first TinyUSB FIFO

If pacing is active but only roughly 16 packets are accepted before every
write is rejected, test the MIDI-only USB profile. This distinguishes receiver
throughput from an embedded-host incompatibility with the normal CDC + MIDI
composite descriptor:

```bash
./scripts/build_seqtrak_midi_only.sh
./scripts/upload_seqtrak_midi_only.sh /dev/ttyACM0
```

The MIDI-only uploader defaults to `115200`, which is more reliable for the
Cardputer USB-Serial/JTAG ROM loader. The normal uploader keeps `921600`; use
`UPLOAD_SPEED=115200 ./scripts/upload.sh /dev/ttyACM0` if that link is noisy.

The MIDI-only firmware intentionally has no CDC serial monitor. Use the on-screen
USB diagnostics and reproduce the same track directly with SEQTRAK.

In this profile Arduino-ESP32 does not auto-start TinyUSB because
`ARDUINO_USB_CDC_ON_BOOT` is disabled. `CardputerUsbMidiTransport::begin()`
therefore starts `USB` explicitly after the global `USBMIDI` object has
registered its descriptor. A build that only changes `CDCOnBoot=default` but
does not call `USB.begin()` exposes neither MIDI nor CDC and is not a valid
SEQTRAK compatibility test.

The same profile does not call `Serial.begin()`: without USB CDC, Arduino maps
`Serial` to UART0 TX on GPIO43, which Cardputer-Adv uses for the ES8311 I2S word
select signal. Diagnostics remain available on screen; the normal composite
build retains CDC logging.

To restore the normal diagnostic build, enter the Cardputer-Adv download mode:

1. Set the side power switch to OFF.
2. Hold the top-side `G0` button.
3. Connect USB to the computer and power on, then release `G0`.
4. Find the bootloader port and run `./scripts/upload.sh /dev/ttyACM<N>`.

## Acceptance checklist

### Build

- [ ] Pinned dependencies install cleanly.
- [ ] Host tests pass.
- [ ] SDL build passes without TinyUSB headers.
- [ ] Cardputer-Adv build passes with all warnings.
- [ ] Build and upload scripts use the same TinyUSB FQBN.

### USB and Serial

- [ ] Cardputer enumerates as a class-compliant USB-MIDI device.
- [ ] CDC Serial diagnostics remain available.
- [ ] Current-source upload remains usable.
- [ ] Disconnect/reconnect does not reset the device.

### MIDI behavior

- [ ] `A` sends NoteOn on MIDI channel 8.
- [ ] Releasing `A` sends NoteOff for the same note.
- [ ] Velocity is non-zero for NoteOn.
- [ ] A replacement note sends old NoteOff before new NoteOn.
- [ ] Releasing an inactive held key sends no external NoteOff.
- [ ] Returning to a previous held key sends a clean replacement pair.
- [ ] `X` sends one target-scoped NoteOff.
- [ ] Repeated Panic sends no duplicate NoteOff.
- [ ] No CC 123 is emitted.
- [ ] No stuck external note remains.

### Scope boundaries

- [ ] Internal Synth A still plays.
- [ ] Synth B is not routed to USB.
- [ ] PatternPlayer is not routed to USB.
- [ ] Drums are not routed to USB.
- [ ] No MIDI clock or transport messages are emitted.
- [ ] No CC, Program Change, Pitch Bend, SysEx, or MIDI input was added.
- [ ] Scene/project schema is unchanged.

### Stability

- [ ] No watchdog or Guru Meditation occurs.
- [ ] No heap corruption occurs.
- [ ] Heap does not trend downward per note.
- [ ] Audio underruns do not continually increase during a 30-minute play/reconnect test.

### SEQTRAK

- [ ] Direct USB connection is recognized.
- [ ] Cardputer keys play SEQTRAK SYNTH 1 on channel 8.
- [ ] Both physical keyboard rows work.
- [ ] Scale and octave behavior match the GroovePuter display.
- [ ] Last-note replacement leaves no hanging SEQTRAK note.
- [ ] GroovePuter transport and patterns remain local in this slice.

Do not merge the USB-MIDI PR until the computer and SEQTRAK hardware sections
have both been physically validated.
