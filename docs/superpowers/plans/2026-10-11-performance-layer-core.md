# Performance Layer Core Implementation Plan

> **For agentic workers:** Execute this plan task-by-task with TDD. Keep each slice independently reviewable and continue through the remaining roadmap phases after this Core plan.

**Goal:** Add the controller-neutral Performance Core and close external MIDI note ownership correctly without requiring new hardware.

**Architecture:** Fixed-size semantic intents sit between adapters and musical owners. The existing MIDI dispatcher and bounded UI queue remain; their external note identity is extended through the single `PerformanceKeyboard`, which becomes the source of truth for held-note ownership and scoped release.

**Tech Stack:** C++17, fixed arrays, existing host-test runners, Cardputer Arduino/SDL build systems.

**Spec:** `docs/superpowers/specs/2026-10-11-performance-layer-core-design.md`

## Global Constraints

- No dynamic allocation or strings in realtime-reachable production paths.
- Keep one authoritative `PerformanceKeyboard` and existing `ProjectTransportTimeline`.
- Keep all external NoteOff/disconnect cleanup source/session scoped.
- RAW MIDI preserves exact pitch and velocity.
- Existing Cardputer behavior and legacy chord/arp engines remain compatible.

## Review Focus

- Two sources hold one pitch; one source releases or disconnects while the other remains held.
- A detached session's delayed NoteOff arrives after reconnect.
- Queue overflow recovery occurs while Cardputer notes and patterns remain active.
- MIDI channel-specific same-note ownership and All Notes Off remain scoped.
- MIDI pitch extrema 0 and 127 remain unchanged in RAW.
- Sustain and queue release reserve remain reliable under flood.

---

### Task 1: Semantic input identity and intent

**Files:**
- Create: `src/input/performance_intent.h`
- Create: `tests/test_performance_intent.cpp`
- Modify: `tests/run_host_tests.sh`

**Interfaces:**
- Produces fixed-size `PerformanceInputIdentity` and `PerformanceIntent` types.
- Identity contains adapter source ID, session generation, channel, and original note.
- Intents initially cover PlayNote, ReleaseNote, Hold, and ReleaseHold.

- [x] Write tests asserting distinct identities for source, generation, channel, and note.
- [x] Run the focused test and observe the missing contract before implementation.
- [x] Implement enums/value structs with bounded integer fields and no heap.
- [x] Run focused and host tests.

### Task 2: Carry identity through MIDI ingress and bounded queue

**Files:**
- Modify: `src/midi/midi_input_dispatcher.h`
- Modify: `src/midi/external_note_queue.h`
- Modify: `tests/test_c9_midi_input_dispatcher.cpp`
- Modify: `tests/test_external_note_queue.cpp`

**Interfaces:**
- `MidiExternalNoteSink::externalNoteOn/Off` receive a `PerformanceInputIdentity`.
- `ExternalNoteQueue::Event` stores the same identity in fixed fields.
- The MIDI adapter maps USB/UART and current generation into stable Core source IDs.

- [x] Add regressions for identity preservation and stale generation isolation.
- [x] Observe expected RED.
- [x] Carry identity on NoteOn, NoteOff, source cleanup, and recovery without changing direct Synth A/B/Drums routing.
- [x] Run focused C9 and queue tests plus host suite.

### Task 3: Source-aware PerformanceKeyboard ownership

**Files:**
- Modify: `src/input/performance_keyboard.h`
- Modify: `src/input/performance_keyboard.cpp`
- Modify: `GroovePuter.ino`
- Modify: `tests/test_performance_keyboard_external.cpp`

**Interfaces:**
- Add identity-aware external note methods and scoped release.
- Retain legacy overloads as a single legacy external source.
- Deduplicate active output pitches across multiple owners; only the last matching release emits NoteOff.
- Keep the existing performance keyboard as the sole chord/arp/rhythm owner.

- [x] Add same-pitch cross-source tests, per-source disconnect tests, and note 0/127 RAW tests.
- [x] Observe expected RED.
- [x] Implement bounded held ownership and exact RAW path.
- [x] Run focused keyboard tests and full host suite.

### Task 4: Integrate and close Core acceptance

**Files:**
- Modify: `docs/0.9.14/*` only if the active acceptance record requires it; otherwise add/update the Performance Layer stage record.

- [x] Run the source/queue/keyboard focused suites and `tests/run_host_tests.sh`.
- [x] Build SDL and Cardputer firmware.
- [x] Record bounded contracts: input identity ≤8 bytes, external event queue 32 entries with 8 release-reserve entries, scene record 84 bytes. Latest Cardputer image: 1,479,810 bytes flash, 185,220 bytes static RAM after audio-owned Scene boundary application, identity-scoped sustain and scene shortcuts.
- [x] Review owner scope, legacy behavior, and allocation boundaries before continuing into Smart Play / Performance Control.

Hardware tests remain open: the environment probe on 2026-10-11 found no Cardputer serial device and no ALSA MIDI port (`amidi -l` is empty). The attached keyboard enumerates USB Audio and HID interfaces, with no USB MIDI Streaming interface, so it cannot exercise the class-compliant MIDI ingress. Standalone hands-on UX, hot disconnect/reconnect, controller flood, and measured input-to-output latency are not claimed as passed.
