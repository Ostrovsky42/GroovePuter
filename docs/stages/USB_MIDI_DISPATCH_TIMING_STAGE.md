# USB-MIDI dispatch timing repair

## Status

Implementation branch rebased onto the hardware-accepted Song repair:

```text
base: b97783942b81032049fc060b75f171c1de11bc5d
includes: merged Song playhead repair from PR #10
```

The Song contract for correct `1B / 2B / 4B / 8B` row duration and lifecycle phase resets is now part of the implementation baseline and must remain unchanged.

## Purpose

Remove the perceptible unevenness observed when GroovePuter PatternPlayer drives Yamaha SEQTRAK over USB-MIDI.

The merged Stage 1 path is functionally correct but timing information is lost between the sample timeline and TinyUSB:

```text
sample-accurate PatternPlayer event
→ MusicalEvent without timestamp
→ fixed queue
→ Arduino loop
→ TinyUSB
```

The Arduino loop and display redraws are not a musical clock. In addition, a 512-frame audio block is rendered faster than realtime, so several events generated at different sample offsets can become an untimed USB burst.

This stage repairs timing only. It must not add drums, MIDI Clock, Program Change, BLE-MIDI, or a general MIDI settings page.

## Hardware assumptions

- M5Stack Cardputer-Adv, ESP32-S3, no PSRAM.
- Yamaha SEQTRAK connected through the Cardputer native USB-C data port.
- Repository-pinned M5Stack ESP32 core `3.2.2`.
- TinyUSB CDC + MIDI composite:

```text
USBMode=default
CDCOnBoot=cdc
UploadMode=cdc
```

- Existing internal audio remains on ES8311 / I2S1.
- No GPIO or PORT.A wiring is used.

## Current confirmed behavior

```text
Live keyboard / Synth A → MIDI channel 8
PatternPlayer / Synth A → MIDI channel 8
PatternPlayer / Synth B → MIDI channel 9
Song rows advance with the merged 1B / 2B / 4B / 8B contract
```

Direct SEQTRAK playback works, but the PatternPlayer stream feels uneven. This observation is the reproduction basis for the stage.

## Scope

Implement only:

- timestamped PatternPlayer MIDI queue entries;
- preservation of event order and relative sample offsets inside each audio block;
- a dedicated low-latency MIDI dispatch owner outside the UI loop;
- one serialized owner for `UsbMidiOutput` and TinyUSB writes;
- bounded queues with no per-event heap allocation;
- target-scoped overflow recovery;
- timing diagnostics suitable for Linux `aseqdump` and Cardputer serial summaries;
- host tests, SDL build, Cardputer build, and physical SEQTRAK acceptance.

## Explicitly out of scope

- drum MIDI;
- MIDI Clock, Start, Stop, or Continue;
- Song-specific external renderer;
- Program Change, Bank Select, CC, Pitch Bend, Aftertouch, or SysEx;
- dynamic MIDI channel UI;
- live target Synth A/B selector;
- BLE-MIDI;
- MIDI input;
- scene persistence changes;
- Atlas, genre, Pattern, or Song data-model changes.

## Timing contract

Do not add timing fields to the normalized `MusicalEvent` unless every source requires them. Use a queue wrapper:

```cpp
struct ScheduledMusicalEvent {
    MusicalEvent event;
    uint32_t blockSequence;
    uint16_t frameOffset;
    uint32_t generation;
    uint32_t publicationSequence;
};
```

Required invariants:

```text
frameOffset < kBlockFrames
same block: lower frameOffset dispatches first
same frameOffset: original publication order is preserved
cross-block order is monotonic by blockSequence
NoteOff before replacement NoteOn remains ordered
AllNotesOff is never delayed behind stale notes from an invalid lifecycle
stale generation events are discarded before reaching UsbMidiOutput
```

The implementation may use an absolute microsecond deadline internally, but the source of truth remains the audio sample position.

## Dispatch ownership

TinyUSB and mutable `UsbMidiOutput` state must have one task owner.

Target architecture:

```text
AudioTask / PatternPlayer
→ timestamped fixed queue
                         \
                          → MidiDispatchTask → UsbMidiOutput → TinyUSB
                         /
Arduino loop / live keys
→ bounded live-event handoff

Arduino loop / live keys
→ MusicalEventRouter → InternalSynthOutput
```

The internal Synth A response to live input remains immediate. Only the external USB sink is asynchronous.

Do not call TinyUSB from:

- AudioTask;
- DSP render code;
- display/UI rendering;
- ISR callbacks.

## Scheduler policy

A dedicated task alone is insufficient because events generated within one 512-frame render block already lose their spacing. The scheduler must preserve the relative time represented by `frameOffset`.

Implementation order:

1. Define `ScheduledMusicalEvent`, generation semantics, ordering, and host tests.
2. Capture PatternPlayer event offsets during `generateAudioBuffer()`.
3. Publish a monotonic block sequence and timing anchor from AudioTask.
4. Convert queued offsets to dispatch deadlines outside DSP.
5. Dispatch from one task without depending on `loop()` frequency.
6. Move live/control USB delivery to a bounded handoff owned by the same task.
7. Add lifecycle invalidation, overflow recovery, diagnostics, and hardware acceptance.

A constant output-latency compensation is allowed only as a documented runtime constant. It must not alter relative spacing between events.

## Realtime constraints

- No `new`, `malloc`, STL containers, JSON, file I/O, or Serial printing per event.
- AudioTask publication must never block.
- MidiDispatchTask may sleep until the next deadline but must not busy-spin continuously.
- Queue overflow must remain observable and must end in target-scoped cleanup.
- UI redraw must not delay PatternPlayer MIDI dispatch.
- Internal audio must not gain underruns as a result of MIDI scheduling.

## Song mode

Internal Song mode already exists and is toggled with:

```text
Alt + M
```

PatternPlayer MIDI follows the active Synth A/B patterns as Song rows change because events are emitted from the same active engine timeline.

The merged Song repair guarantees correct row durations and phase resets. This stage must preserve that behavior while invalidating stale scheduled MIDI events from the previous lifecycle generation at row transitions.

This timing stage does not add an external SEQTRAK Song mode, MIDI Clock, or transport synchronization. A visible PERFORM-page Song toggle and live target A/B selector belong to the following performance-controls stage.

## Build and flash

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Use only M5Stack ESP32 core `3.2.2`.

## Linux timing capture

```bash
aconnect -l
aseqdump -l
aseqdump -p <client:port>
```

Capture these cases:

```text
120 BPM, plain 1/16 notes
maximum practical BPM
maximum Synth A retrig
maximum Synth B retrig
both synths active
page navigation and redraw during playback
Song mode row changes
Stop during a sustained note
```

## Expected behavior

- Channel 8 and channel 9 routes remain unchanged.
- Note spacing follows the PatternPlayer timeline instead of Arduino loop cadence.
- UI redraw does not create audible event batches.
- Retrig remains ordered and does not collapse into a burst.
- Stop, mute, pattern change, scene change, and Song-row change leave no stale note.
- Song row duration remains correct for `1B / 2B / 4B / 8B`.
- Internal audio remains continuous.
- Live keyboard remains responsive while transport is stopped.
- No old event is replayed after USB reconnect.

## Host tests

- [ ] Events in one block dispatch by ascending `frameOffset`.
- [ ] Equal-offset events preserve publication order.
- [ ] Cross-block ordering is monotonic.
- [ ] Generation comparison rejects stale lifecycle events.
- [ ] Replacement remains `NoteOff old → NoteOn new`.
- [ ] TIE does not add an unnecessary NoteOn.
- [ ] Retrig preserves scheduled intervals.
- [ ] Late dispatch is measured, not silently ignored.
- [ ] Lifecycle Panic invalidates stale scheduled events.
- [ ] Queue overflow produces target-scoped cleanup.
- [ ] Live and Pattern Synth A same-note ownership remains correct.
- [ ] TinyUSB is called only by the dispatch owner.
- [ ] No dynamic allocation exists in the realtime path.
- [ ] Existing Pattern, Song, scene, and WAV-render regressions remain green.

## Cardputer-Adv acceptance checklist

- [ ] Firmware boots without reset or watchdog.
- [ ] Internal Synth A/B and drums remain audible.
- [ ] SEQTRAK receives Synth A on channel 8.
- [ ] SEQTRAK receives Synth B on channel 9.
- [ ] Plain 1/16 pattern sounds even at 120 BPM.
- [ ] High BPM does not create audible batches.
- [ ] Maximum retrig remains regular enough for performance use.
- [ ] Navigating pages does not change MIDI rhythm.
- [ ] Song row transitions do not create a burst or stuck note.
- [ ] Song `1B / 2B / 4B / 8B` row duration remains correct.
- [ ] Stop releases both external synth notes.
- [ ] Same-pitch live/pattern ownership still passes.
- [ ] USB reconnect does not replay stale notes.
- [ ] No continual increase in audio underruns.

## Troubleshooting

### Pattern still sounds uneven

Capture timestamped `aseqdump` output and compare event deltas with the intended step interval. Check scheduler lateness counters and UI draw duration. Do not compensate by adding arbitrary delays to NoteOn or NoteOff.

### Events are evenly spaced but consistently early or late

This is constant latency, not jitter. Adjust only the documented output-latency compensation after relative timing is verified.

### Live notes work but PatternPlayer is late

Check block anchors, `frameOffset`, queue sequence ordering, and dispatch-task wakeups. Do not move TinyUSB calls back into AudioTask.

### Internal audio underruns increase

Lower the dispatch task priority or reduce diagnostic work. Do not reduce I2S safety buffers without a separate hardware measurement.

### Song changes leave a stale note

Verify that the Song-row lifecycle generation invalidates queued events from the previous row before the next row is scheduled. Do not alter the merged Song bar counter to solve MIDI cleanup.

## Acceptance gate

This stage is complete only when direct SEQTRAK playback no longer feels tied to UI-loop timing and the timestamp capture shows no large redraw-related event batches. A constant USB/audio latency offset may remain documented; audible unevenness may not.