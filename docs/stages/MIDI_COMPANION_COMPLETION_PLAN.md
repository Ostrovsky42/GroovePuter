# MIDI Companion completion integration plan

## Current status

The accepted MIDI companion line now includes:

```text
PERFORM A/B/DX/DRUMS
PatternPlayer Synth A/B
sample-timed Pattern dispatcher
MIDI Clock / Start / Stop
realtime SMF playback
```

This stage completes the missing PatternPlayer Drums path while keeping the accepted single-dispatcher architecture.

## Accepted dependency chain

```text
PR #8 sample-timed MIDI dispatcher
-> MIDI companion routing/settings foundation
-> transport sync
-> realtime SMF player
-> stage-ready player/PERFORM UI
-> Pattern Drums + SEQTRAK target cleanup
```

All Pattern Drums work must reuse the accepted queue deadlines, generation invalidation, urgent cleanup, and single TinyUSB task ownership. It must not introduce a parallel MIDI scheduler.

## Integrated runtime model

### Live targets

```text
PERFORM Synth A -> CH8
PERFORM Synth B -> CH9
PERFORM DX      -> CH10
PERFORM Drums   -> native CH1..7
```

### Pattern targets

```text
Pattern Synth A -> CH8
Pattern Synth B -> CH9
Pattern Drums:
  Kick       -> CH1
  Snare      -> CH2
  Clap       -> CH3
  Closed Hat -> CH4
  Open Hat   -> CH5
  Mid Tom    -> CH6
  Rim        -> CH6
  High Tom   -> CH7
```

Pattern drum NoteOn events are published from the actual internal drum trigger API, so base hits, retrig, flam and roll share one timing source with internal audio. The events enter the same `MusicalEventQueue` used by Pattern Synth A/B and receive AudioTask block/frame timestamps.

Pattern drum NoteOff uses a fixed-size gate state inside `MidiDispatchTask`. The deadline is derived from the sample-timed NoteOn position. Retriggers extend the gate; there is no `millis()` gate, no FreeRTOS gate task and no second sequencer.

### SEQTRAK playable targets

The routing model keeps distinct playable destinations:

```text
CH1..7  DRUMS
CH8     SYNTH 1
CH9     SYNTH 2
CH10    DX
CH11    SAMPLER
```

DX is not the fallback bucket for unrelated melodic/texture material. SMF SEQTRAK mode keeps source CH3 explicit on DX and sends additional melodic source channels to SAMPLER until per-track custom routing exists.

FX remain a future control domain:

```text
TRACK FX
DELAY SEND
REVERB SEND
MASTER FX
```

They are not playable `MusicalEventTarget` note destinations.

## Remaining MIDI Companion work

### Global Cardputer storage adapter

- bind `MidiOutputSettings` to the accepted dispatcher;
- use NVS/Preferences or the selected global device store;
- distinguish missing, corrupt, and backend error;
- do not modify scene JSON.

### MIDI settings UI

Minimum useful surface:

```text
USB MIDI       ON/OFF
PROFILE        SEQTRAK / GENERAL MIDI / CUSTOM
LIVE TARGET    SYNTH A / SYNTH B / DX / DRUMS
SYNTH A CH     1..16
SYNTH B CH     1..16
PATTERN A/B    ON/OFF
DRUM OUT       ON/OFF
DRUM ROUTES    explicit rows
DRUM GATE      1..500 ms
STATUS         OFF / WAIT / READY
PANIC
```

The currently accepted v1 Pattern drum gate remains 80 ms until this settings binding is implemented.

Use partial redraws. Do not redraw the full display for dispatcher counters.

### Song controls and diagnostics

- expose Song mode, slot A/B, and position without a second MIDI renderer;
- continue to use active PatternPlayer events;
- show bounded aggregate diagnostics only;
- verify 1B/2B/4B/8B row transitions through A/B/Drums external routes.

### SMF routing follow-up

- per-track CUSTOM destination editing;
- track mute/solo;
- optional A-B loop;
- preserve explicit SYNTH1/SYNTH2/DX/SAMPLER roles.

## Hardware acceptance for Pattern Drums stage

- live Synth A/B/DX/Drums remain unchanged;
- Pattern Synth A remains external CH8;
- Pattern Synth B remains external CH9;
- all eight internal drum voices reach the configured native SEQTRAK destinations;
- simultaneous kick and hat;
- Mid Tom + Rim shared CH6 ownership;
- retrig, flam and roll remain ordered;
- final drum NoteOff follows the newest retrigger gate;
- Song rows preserve 1B/2B/4B/8B durations;
- route/lifecycle cleanup releases old Pattern Drums ownership;
- Panic leaves no active notes;
- reconnect performs no stale replay;
- UI navigation does not change MIDI timing;
- internal synths and drums remain audible;
- no reset, watchdog, heap collapse, or audio-underrun regression.

Detailed procedure: `docs/stages/PATTERN_DRUM_MIDI_STAGE.md`.

## Explicitly out of scope for this stage

- adding SAMPLER to the live PERFORM target cycle;
- runtime user editing of the 80 ms Pattern drum gate;
- Track FX / Delay / Reverb / Master FX control;
- CC, Program Change output, Pitch Bend, Aftertouch, SysEx;
- BLE-MIDI;
- MIDI input/slave mode;
- scene schema changes;
- a separate Song MIDI renderer.
