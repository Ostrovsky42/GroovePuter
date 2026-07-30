# PatternPlayer USB-MIDI Output Stage

## Purpose

Extend the merged live-keyboard USB-MIDI path so GroovePuter pattern playback can drive Yamaha SEQTRAK or another class-compliant MIDI target while internal Synth A and Synth B continue to sound.

```text
PatternPlayer
→ MusicalEventRouter
   ├── InternalSynthOutput
   └── UsbMidiOutput
       → CardputerUsbMidiTransport
       → USB-MIDI
```

This stage starts from the squash merge of PR #6:

```text
eb1842dfb0b022890de010efe83086ac330bf62d
```

## Hardware assumptions

- M5Stack Cardputer-Adv, ESP32-S3, no PSRAM.
- Native USB-C data connection.
- Pinned M5Stack ESP32 core `3.2.2`.
- TinyUSB device mode with CDC + MIDI composite.
- Build options:

```text
USBMode=default
CDCOnBoot=cdc
UploadMode=cdc
PSRAM=disabled
PartitionScheme=huge_app
```

No GPIO or PORT.A wiring is required for USB-MIDI.

## Scope

Implement only:

- `MusicalEventSource::PatternPlayer` output;
- `MusicalEventTarget::SynthA` and `SynthB`;
- fixed route configuration:
  - Synth A → internal channel index 7 → MIDI channel 8;
  - Synth B → internal channel index 8 → MIDI channel 9;
- explicit NoteOn, NoteOff and AllNotesOff/Panic lifecycle;
- independent fixed-size MIDI ownership lanes for:
  - PerformanceKeyboard / SynthA;
  - PatternPlayer / SynthA;
  - PatternPlayer / SynthB;
- transport Stop cleanup;
- pattern/scene lifecycle cleanup where required;
- internal and external playback at the same time;
- host tests, SDL build, Cardputer-Adv build and hardware acceptance documentation.

## Architecture constraints

- PatternPlayer must publish normalized events through `MusicalEventRouter`.
- PatternPlayer must not call `UsbMidiOutput` or TinyUSB directly.
- TinyUSB headers remain isolated in the Cardputer-specific transport.
- No heap allocation, file I/O, JSON parsing or blocking waits in the playback path.
- MIDI channel values are stored internally as `0..15`.
- Existing non-MIDI Pattern, Song, Atlas, generators and DSP behavior must remain unchanged.
- PerformanceKeyboard ownership must not cancel PatternPlayer ownership.
- Synth A PatternPlayer ownership must not cancel Synth B ownership.

## Note ownership

Use a fixed bounded lane model keyed by source and target. Do not replace it with one global active note.

Required lanes:

```text
PerformanceKeyboard / SynthA / channel 8
PatternPlayer       / SynthA / channel 8
PatternPlayer       / SynthB / channel 9
```

Each current synth pattern lane may remain monophonic.

For a replacement note on one lane:

```text
NoteOff previous active note
NoteOn new note
```

A NoteOff from one lane must not release another lane.

## Gate, tie and slide policy

- Normal notes require a scheduled NoteOff based on the existing musical transport timeline.
- Do not use `delay()` or unrelated wall-clock timers.
- A tied note must not be retriggered unnecessarily.
- A changed pitch during tie/slide may be represented as ordered NoteOff + NoteOn.
- MIDI pitch bend and full 303 slide emulation are out of scope.
- Any loss of internal slide semantics on the external MIDI path must be documented.

## Out of scope

- drums;
- route/channel settings UI;
- MIDI Clock, Start, Stop or Continue messages;
- Song-specific MIDI rendering;
- Standard MIDI File playback;
- CC, Program Change, Pitch Bend, Aftertouch or SysEx;
- MIDI input;
- BLE-MIDI;
- persistent MIDI settings;
- changes to Atlas or genre generators.

## Build and flash

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

## Expected behavior

- Existing live keyboard USB-MIDI continues to work.
- Starting Pattern playback emits Synth A pattern notes on MIDI channel 8.
- Synth B pattern notes emit on MIDI channel 9.
- Internal Synth A and Synth B remain audible.
- Performance live-note ownership remains independent from pattern ownership.
- Stop and pattern changes do not leave external notes active.

## Host acceptance checklist

- [ ] PatternPlayer Synth A NoteOn reaches the Synth A pattern lane.
- [ ] PatternPlayer Synth B NoteOn reaches the Synth B pattern lane.
- [ ] Synth A and Synth B use different MIDI channels.
- [ ] PerformanceKeyboard lane does not cancel PatternPlayer Synth A.
- [ ] PatternPlayer Synth A does not cancel PatternPlayer Synth B.
- [ ] Replacement sends ordered NoteOff then NoteOn.
- [ ] Gate end sends NoteOff.
- [ ] Transport Stop releases all PatternPlayer-owned notes.
- [ ] Pattern change releases obsolete notes.
- [ ] Scene lifecycle does not leave stale notes.
- [ ] USB disable clears owned notes safely.
- [ ] Disconnect clears local ownership.
- [ ] Reconnect does not replay stale notes.
- [ ] Internal sinks still receive PatternPlayer events.
- [ ] No dynamic allocation is introduced in the playback path.

## Cardputer-Adv / SEQTRAK acceptance checklist

- [ ] Cardputer-Adv boots without reset or watchdog.
- [ ] Internal Synth A and SEQTRAK MIDI channel 8 sound together.
- [ ] Internal Synth B and SEQTRAK MIDI channel 9 sound together.
- [ ] Stopping GroovePuter stops external pattern notes.
- [ ] Changing pattern creates no stuck note.
- [ ] Switching workflow mode creates no stuck note.
- [ ] Live keyboard still works after pattern playback.
- [ ] Serial CDC remains available.
- [ ] No continual underrun growth is observed.

## Troubleshooting

- Verify the core is exactly M5Stack ESP32 `3.2.2`.
- Verify the USB cable carries data.
- Verify Synth A listens on MIDI channel 8 and Synth B on MIDI channel 9.
- Confirm build and upload use the same TinyUSB FQBN options.
- Use a MIDI monitor before diagnosing SEQTRAK routing.
- A working live keyboard with missing pattern events indicates the PatternPlayer event publication path, not the TinyUSB transport.
