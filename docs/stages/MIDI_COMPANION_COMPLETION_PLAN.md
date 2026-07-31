# MIDI Companion completion integration plan

## Current status

This branch currently contains only the pure configuration foundation. It is
based on `main` after the merged Song playhead repair and intentionally does not
implement runtime integration before PR #8 is accepted.

## Required dependency

```text
PR #8 sample-timed MIDI dispatcher
-> hardware acceptance
-> squash merge to main
-> rebase this branch onto updated main
```

The integration stage must reuse PR #8 queues, deadlines, generation
invalidation, urgent cleanup, and single TinyUSB task ownership. It must not
introduce a parallel MIDI scheduler.

## Ordered integration commits after PR #8

### 1. Runtime settings adapter

- bind `MidiOutputSettings` to the accepted dispatcher;
- apply channel changes through generation invalidation;
- release old wire ownership before route replacement;
- keep settings global and outside `Scene`.

### 2. Live Synth A/B target

- select internal and external live target together;
- preserve immediate internal performance response;
- route external events through the dispatcher live/control queue;
- retain same-channel same-note ownership protection.

### 3. Drum event model

- add `MusicalEventTarget::Drums` or the accepted PR #8 equivalent;
- map the existing eight internal voices to `MidiDrumVoice`;
- preserve velocity, source order, deadline, and generation;
- keep internal drums active.

### 4. Drum gate scheduler

- emit percussive NoteOn without synth-style note-owner collapsing;
- schedule bounded NoteOff gates;
- preserve retrig, flam, roll, and simultaneous hits;
- use target-scoped cleanup on overflow.

### 5. Global Cardputer storage adapter

- implement `IMidiSettingsStorage` outside the pure codec;
- use NVS/Preferences or the selected global device store;
- distinguish missing, corrupt, and backend error;
- do not modify scene JSON.

### 6. MIDI settings UI

Minimum surface:

```text
USB MIDI       ON/OFF
PROFILE        SEQTRAK / GENERAL MIDI / CUSTOM
LIVE TARGET    SYNTH A / SYNTH B
LIVE CH        1..16
SYNTH A CH     1..16
SYNTH B CH     1..16
PATTERN A/B    ON/OFF
DRUM OUT       ON/OFF
DRUM ROUTES    8 explicit rows
DRUM GATE      1..500 ms
STATUS         OFF / WAIT / READY
PANIC
```

Use partial redraws. Do not redraw the full display for dispatcher counters.

### 7. Song controls and diagnostics

- expose Song mode, slot A/B, and position without a second MIDI renderer;
- continue to use active PatternPlayer events;
- show bounded aggregate diagnostics only;
- verify 1B/2B/4B/8B row transitions through external routes.

## Final hardware acceptance

- live Synth A and Synth B;
- Pattern Synth A on external channel 8 by default;
- Pattern Synth B on external channel 9 by default;
- same-note Live/Pattern ownership in both release orders;
- all eight drum voices produce configured external events;
- simultaneous kick and hat;
- retrig, flam, and roll remain ordered;
- Song rows preserve 1B/2B/4B/8B durations;
- route disable and channel change release old ownership;
- Panic leaves no active notes;
- reconnect performs no stale replay;
- UI navigation does not change MIDI timing;
- internal synths and drums remain audible;
- no reset, watchdog, heap collapse, or audio-underrun regression.

## Explicitly out of scope

- MIDI Clock, Start, Stop, Continue;
- SMF realtime playback;
- BLE-MIDI;
- MIDI input;
- CC, Program Change, Pitch Bend, Aftertouch, SysEx;
- scene schema changes;
- a separate Song MIDI renderer.
