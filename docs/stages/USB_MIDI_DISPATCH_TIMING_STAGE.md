# USB-MIDI dispatch timing repair

## Status

Production integration is implemented on:

```text
branch: fix/usb-midi-dispatch-timing
base: b97783942b81032049fc060b75f171c1de11bc5d
includes: hardware-accepted Song playhead repair from PR #10
```

The PR must remain draft until direct Cardputer-Adv → Yamaha SEQTRAK timing acceptance is recorded.

## Purpose

Remove the audible unevenness caused by dispatching PatternPlayer USB-MIDI from the Arduino UI loop.

Old path:

```text
sample-timeline event
→ untimed MusicalEvent queue
→ Arduino loop / display cadence
→ TinyUSB
```

Implemented path:

```text
MiniAcid per-sample renderer
→ MusicalEventQueue timing facade
→ ScheduledMusicalEventQueue
→ MidiDispatchTask
→ UsbMidiOutput
→ TinyUSB
```

The change is timing-only. It does not add drums, MIDI Clock, Program Change, BLE-MIDI, a channel settings page, or a new Song renderer.

## Hardware list

- M5Stack Cardputer-Adv, ESP32-S3, no PSRAM required.
- Yamaha SEQTRAK.
- USB-C data cable supporting device data, not a charge-only cable.
- Optional Linux computer for `aseqdump` timing capture.

## Wiring

```text
Cardputer-Adv native USB-C data port
→ Yamaha SEQTRAK USB host/device-compatible MIDI connection
```

No PORT.A, GPIO, I2C, or external level shifting is used.

Internal sound remains on the Cardputer-Adv ES8311 / I2S path. An AUX cable is optional for the separate audio workflow and is not required for USB-MIDI validation.

## Toolchain assumptions

Use the repository-pinned M5Stack ESP32 core `3.2.2` and this TinyUSB composite configuration:

```text
USBMode=default
CDCOnBoot=cdc
UploadMode=cdc
PSRAM=disabled
```

Existing routes remain fixed:

```text
Live keyboard / Synth A → MIDI channel 8
PatternPlayer / Synth A → MIDI channel 8
PatternPlayer / Synth B → MIDI channel 9
```

## Implementation

### Render-time timestamps

`MiniAcid` already publishes normalized PatternPlayer events from inside its per-sample render loop. The existing engine API remains unchanged:

```cpp
patternEventQueue_->tryPush(event);
```

`src/input/musical_event_queue.h` is now a compatibility facade over `ScheduledMusicalEventQueue`. `AudioTask` brackets every production render block with:

```cpp
beginMidiRenderBlock(...);
generateAudioBuffer(...);
endMidiRenderBlock();
```

At the exact event publication point, the facade reads MiniAcid sequencer phase and converts it to a bounded `frameOffset` for the current block. The transport-start discontinuity at tick 383 is explicitly normalized so the first step maps to frame zero rather than one PPQN tick late.

Scheduled entries use:

```cpp
struct ScheduledMusicalEvent {
    MusicalEvent event;
    uint32_t blockSequence;
    uint16_t frameOffset;
    uint32_t generation;
    uint32_t publicationSequence;
};
```

Required ordering is preserved:

```text
lower blockSequence first
within a block: lower frameOffset first
same frameOffset: publication order
replacement: old NoteOff before new NoteOn
```

The queue is fixed-size:

```text
128 storage slots
127 usable scheduled events
no per-event allocation
single AudioTask producer
single MidiDispatchTask consumer
```

### Playback anchors

After each rendered block, `AudioTask` publishes the block sequence and the render-start microsecond anchor. The dispatcher applies one documented audio-block output-latency compensation:

```text
kOutputLatencyUs = one 512-frame block
```

This constant shifts all external MIDI by the same amount. It does not change relative note spacing.

### Single USB owner

`MidiDispatchTask` is the only task allowed to mutate `UsbMidiOutput` or write TinyUSB packets.

```text
AudioTask / PatternPlayer
→ scheduled fixed queue
                         \
                          → MidiDispatchTask → UsbMidiOutput → TinyUSB
                         /
Arduino loop / live keys
→ bounded control queue
```

The Arduino loop still routes live events immediately to `InternalSynthOutput`; only external USB delivery is queued. The old `drainPatternMusicalEvents()` UI-loop path has been removed.

### Lifecycle and failure recovery

`AllNotesOff` is a target-scoped generation barrier:

1. increment the Synth A or Synth B generation;
2. request a target-scoped PatternPlayer panic;
3. discard queued events from older generations before USB delivery;
4. allow following events to use the next generation.

A dropped critical event also invalidates only its target. A dropped NoteOn is counted but does not destructively panic an unrelated active note.

Offline WAV rendering remains synchronous on the control task. Events outside the active `AudioTask` render bracket are suppressed, so WAV export cannot accumulate a render-speed USB-MIDI burst. Critical suppressed events still request final target cleanup.

### Diagnostics

Every five seconds the dispatcher prints a bounded summary:

```text
[MIDI-DISPATCH] sched=<depth> live=<depth> sent=<pattern>/<live>
late=<count> maxLateUs=<value> stale=<count> badFrame=<count>
drop=<noteOn>/<critical> liveDrop=<noteOn>/<critical>
suppressed=<count> panic=<pattern>/<live>
```

No Serial logging occurs per event.

## Build and flash

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Do not upgrade the M5Stack core while validating this PR.

## Linux timing capture

List MIDI ports:

```bash
aconnect -l
aseqdump -l
```

Capture GroovePuter output:

```bash
aseqdump -p <client:port>
```

Recommended cases:

```text
120 BPM, plain 1/16 notes
maximum practical BPM
maximum Synth A retrig
maximum Synth B retrig
both synths active
page navigation during playback
Song row transitions
Stop during a sustained note
```

For plain 1/16 notes, compare successive NoteOn deltas. They should follow the musical interval and should not collapse into redraw-related batches.

## Expected behavior

### Screen

- Existing GroovePuter pages and Song workflow remain available.
- Pattern, Song, scene, keyboard, and internal audio behavior remain unchanged.
- No new MIDI settings screen is added in this PR.

### Serial

- Boot reaches `setup-complete` without watchdog or reset.
- `[MIDI-DISPATCH]` appears approximately every five seconds.
- `badFrame` remains `0`.
- `drop` and `liveDrop` remain `0/0` in ordinary use.
- `late` may increment occasionally, but `maxLateUs` must not continually grow with page redraws.
- Audio underruns must not continually increase after enabling USB-MIDI playback.

### SEQTRAK

- Synth A arrives on channel 8.
- Synth B arrives on channel 9.
- Plain 1/16 notes sound even at 120 BPM.
- Retrigs preserve spacing instead of becoming an immediate burst.
- Page navigation does not alter PatternPlayer rhythm.
- Stop, mute, scene changes, pattern changes, Song row changes, and reconnect leave no stuck note.

## Automated acceptance checklist

- [x] Scheduled events carry block, frame, generation, and publication sequence.
- [x] Phase-derived frame offsets are covered by a host test.
- [x] Forced transport start maps the first event to frame zero.
- [x] Equal-block publication order is retained.
- [x] Lifecycle `AllNotesOff` invalidates older generations.
- [x] Critical queue overflow produces target-scoped cleanup.
- [x] Non-realtime/offline events are suppressed.
- [x] Live and Pattern same-note wire ownership tests remain present.
- [x] Failed replacement NoteOff recovery tests remain present.
- [x] TinyUSB ownership is restricted to `MidiDispatchTask` by source contract.
- [x] Host tests pass.
- [x] SDL build passes.
- [x] Cardputer-Adv firmware compiles with the pinned toolchain.

## Cardputer-Adv acceptance checklist

- [ ] Firmware boots without reset or watchdog.
- [ ] Internal Synth A/B and drums remain audible.
- [ ] SEQTRAK receives Synth A on channel 8.
- [ ] SEQTRAK receives Synth B on channel 9.
- [ ] Plain 1/16 Pattern sounds even at 120 BPM.
- [ ] High BPM does not create audible event batches.
- [ ] Maximum Synth A retrig remains regular.
- [ ] Maximum Synth B retrig remains regular.
- [ ] Navigating all pages does not change MIDI rhythm.
- [ ] Song row transitions do not create a burst or stuck note.
- [ ] Song `1B / 2B / 4B / 8B` row duration remains correct.
- [ ] Stop releases both external synth notes.
- [ ] Same-pitch live/Pattern ownership still passes.
- [ ] WAV export sends no render-speed or post-render MIDI burst.
- [ ] USB reconnect does not replay stale notes.
- [ ] Serial `badFrame`, queue drops, and continual underrun growth remain absent.

## Troubleshooting

### SEQTRAK does not see GroovePuter

- Confirm the cable supports USB data.
- Confirm the build uses `USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`.
- Reconnect USB after the firmware has booted.
- Check that live keyboard notes still reach the expected channel.

### Pattern still sounds uneven

Run `aseqdump` and compare NoteOn deltas. Record the corresponding `[MIDI-DISPATCH] late` and `maxLateUs` values. Do not add arbitrary delays inside MiniAcid or TinyUSB.

### Events are evenly spaced but consistently early or late

That is constant latency rather than jitter. Adjust only the documented `kOutputLatencyUs` after relative spacing is confirmed on hardware.

### `badFrame` or queue drops increase

Capture the full serial summary and the Pattern/BPM/retrig settings. Do not enlarge queues before identifying whether the producer is generating an invalid offset or the dispatcher is starved.

### WAV export produces external notes

The render must execute outside the active `AudioTask` block bracket. Confirm `suppressed` increases during export and no queued burst follows completion.

### Song changes leave a stale note

Check `stale` and Pattern panic counters. Song-row cleanup must invalidate the previous generation; do not change the merged Song bar counter to hide a MIDI lifecycle problem.

### Internal audio underruns increase

Capture `[PERF]` and `[MIDI-DISPATCH]` summaries. Do not move TinyUSB writes into `AudioTask`, increase task priority above audio, or add per-event logging.

## Merge gate

The PR may leave draft only after all Cardputer-Adv checklist items are confirmed against a real SEQTRAK. It must not be merged solely from automated CI because the blocking defect is perceptual and timing-dependent on the physical USB path.
