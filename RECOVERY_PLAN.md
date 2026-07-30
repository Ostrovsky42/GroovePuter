# GroovePuter Recovery Plan

Status: audit baseline for `main` at commit `bfbc63049cf79646217aa19c170b730e50ab3e4f`.

## Objective

Restore one reproducible Cardputer ADV build, establish a safe real-time state boundary, and prepare a minimal MIDI-output vertical slice for Yamaha SEQTRAK without rewriting the existing instrument.

## Audit boundary

This audit is based on the repository source and documentation available on `main`. Hardware flashing and a clean-room compile were not completed during this pass, so buildability and runtime behavior remain acceptance gates rather than assumed facts.

## Executive diagnosis

GroovePuter is not an empty prototype. It already contains a substantial musical system: 96 PPQN timing, genre and groove generation, multiple synth engines, drums, song arrangement, scene persistence, MIDI-file import/routing, and a device UI.

The main problem is not missing product scope. It is an undefined hardware/build contract and an unsafe boundary between control-plane mutations and real-time audio processing.

Do not add SEQTRAK USB-MIDI directly to `miniacid.ino`. Recover the platform and concurrency foundations first, then extract output-independent pattern playback.

## Confirmed findings

### P0 — No canonical build contract

The documented commands and repository layout disagree:

- `README.md` instructs `./release.sh` and `./upload.sh`, while scripts live under `scripts/`.
- `agents_guide.md` instructs `./build.sh` and `./release.sh`, also absent from the root.
- `README.md` uses `esp32:esp32:esp32s3:CDCOnBoot=cdc`.
- `scripts/build.sh` uses `m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app`.
- `scripts/build.sh` requires a repository-local `./platform_sdl/bin/arduino-cli`.

These are materially different toolchains and board definitions. Until one is selected and pinned, a successful build on one workstation does not prove a reproducible release.

### P0 — Hardware target is contradictory

`README.md` names Cardputer ADV. `agents_guide.md` and `scripts/build.sh` describe the original Cardputer and a no-PSRAM release assumption. The firmware includes `esp_psram.h`, reports free PSRAM, manually configures the ADV ES8311 codec, and drives an explicit I2S path.

The project must state one of these contracts explicitly:

1. Cardputer ADV only; or
2. Cardputer ADV primary plus original Cardputer compatibility profile; or
3. Cardputer ADV running deliberately in DRAM-only compatibility mode.

Safe default for recovery: **Cardputer ADV only**, with PSRAM treated as optional until memory measurements justify enabling it.

### P0 — Shared engine state is not synchronized

`audioTask()` calls `MiniAcid::generateAudioBuffer()` from a FreeRTOS task pinned to core 1. The Arduino `loop()` and UI handlers directly call mutable `MiniAcid` methods. `AudioGuard::lock` and `unlock` are no-ops despite comments claiming protection.

This creates uncontrolled concurrent access to engine state. Adding USB MIDI would introduce another producer and make failures harder to reproduce.

Recovery direction:

- never hold a general mutex across DSP rendering;
- route control changes through a bounded command queue or immutable snapshot exchange;
- apply queued commands at a deterministic audio block boundary;
- use atomics only for isolated scalar telemetry/flags;
- keep SD, JSON, allocation, and logging outside the audio task.

### P1 — Diagnostic sequence counter increments twice

`audioTask()` increments `perfStats.seq` before and after updating statistics. Readers cannot rely on the sequence as a callback count, and a seqlock-style interpretation would also be incomplete because memory ordering is unspecified.

Choose one meaning and test it:

- callback counter: increment exactly once per completed block; or
- seqlock version: odd before mutation, even after mutation, using atomics and documented reader logic.

### P1 — Recorder is described but disabled

`CardputerAudioRecorder` is included and represented in the UI wiring, but construction is commented out with `DISABLING FOR CRASH DEBUGGING`.

Until recovered, documentation and UI must identify recording as unavailable/experimental. Re-enabling it belongs after baseline audio stability because SD writes and buffering can violate real-time constraints.

### P1 — `miniacid.ino` is an integration hotspot

The sketch owns board initialization, ES8311 register programming, I2S startup, audio task creation, heap diagnostics, engine/UI allocation, sample scanning, boot recovery, keyboard normalization, fallback shortcuts, and transport controls.

A framework rewrite is not justified. Introduce only narrow seams required for testing and MIDI:

- `HardwareProfile`: board pins, codec and USB capabilities;
- `AudioRuntime`: I2S/task lifecycle and audio-block execution;
- `InputRouter`: keyboard/UI commands to application commands;
- `MidiRuntime`: MIDI transport implementation, initially disabled;
- `GroovePuterApplication`: engine ownership and command dispatch.

### P1 — Existing MIDI is not yet SEQTRAK output

The repository contains MIDI-file import and channel-to-internal-track routing. That is valuable, but it is not proof of USB-MIDI device output, clock stability, or SEQTRAK recording compatibility.

Keep these concepts separate:

- MIDI file parsing/import;
- internal track routing;
- live MIDI output;
- transport clock/start/stop;
- device-specific `SeqtrakProfile` behavior.

## Recovery architecture

### State ownership

The audio runtime is the sole writer of real-time engine state used during rendering.

UI, keyboard, encoder, future USB-MIDI input, and Atlas pattern loading produce `EngineCommand` messages. The audio task drains a bounded number of commands at the start of each block and then renders without locks.

Large objects such as complete patterns or scenes should not be copied through a tiny command queue. Prepare them outside the audio task, publish an immutable snapshot/version, then swap at an agreed musical boundary.

### Output separation

Create an output-neutral sequencer boundary:

```text
Pattern/Song/Generator
        |
        v
Scheduled musical events (tick, track, note, velocity, duration, control)
        |
        +--> InternalAudioOutput
        +--> MidiOutput
```

The 96 PPQN clock remains authoritative. Outputs consume scheduled events; they do not independently advance musical time.

### Minimal MIDI interfaces

The first interface should remain small:

- `begin()` / `end()`
- `sendNoteOn(channel, note, velocity)`
- `sendNoteOff(channel, note, velocity)`
- `sendControlChange(channel, control, value)`
- `sendClock()`
- `sendStart()` / `sendStop()` / `sendContinue()`
- `allNotesOff()`

Do not add SysEx abstractions before a verified SEQTRAK use case exists.

## Phased recovery

### Phase 0 — Preserve evidence

- Record current `main` commit and release version.
- Capture boot serial output, visible pages, audio output, heap, and reset reason on a known Cardputer ADV.
- Do not refactor until this baseline exists.

Exit gate: a short baseline report states what works and what does not on actual hardware.

### Phase 1 — Reproducible Cardputer ADV build

- Select the canonical board package and exact versions.
- Add a root-level wrapper or update all documentation to `scripts/...`; do not retain two competing workflows.
- Remove the mandatory repository-local `arduino-cli` path or document/provision it deterministically.
- Pin library dependencies.
- Add a compile-only CI job using the same command as local builds.
- Print firmware version, board profile, core/library versions, flash layout, PSRAM detection, and build flags at boot.

Exit gate: clean checkout builds with one documented command and CI uses the same command.

### Phase 2 — Hardware smoke baseline

- Flash Cardputer ADV.
- Verify ES8311 initialization, I2S audio, keyboard, display, SD access, scene load/save, transport, synth switching, drums, and at least one song loop.
- Measure free internal heap, largest block, stack high-water mark, audio render time, and underruns.

Exit gate: 30-minute playback smoke test without reset, watchdog, persistent crackle, or heap collapse.

### Phase 3 — Real-time state boundary

- Define `EngineCommand` and a bounded queue.
- Convert high-risk UI mutations first: start/stop, BPM, mute, pattern selection, note/pattern edits, engine switching.
- Drain commands before each rendered block.
- Establish snapshot publication for complete pattern/scene replacement.
- Correct and atomically publish performance statistics.
- Add overflow counters and deterministic queue-full behavior.

Exit gate: stress test rapid controls for 30 minutes with zero queue corruption, reset, or audio-task blocking.

### Phase 4 — Characterization tests

- Add host-testable tests for timing math, 96 PPQN advancement, swing mapping, pattern boundaries, note-off scheduling, song transitions, and scene compatibility.
- Add compile-time checks for fixed-size command/event structures.
- Add a fake output that records scheduled musical events for deterministic assertions.

Exit gate: current musical behavior is represented by tests before output extraction changes it.

### Phase 5 — Output-neutral PatternPlayer

- Extract event scheduling from direct synth/drum invocation only as far as required.
- Preserve internal audio as the default output.
- Feed a fake output in tests and compare event streams against known patterns.

Exit gate: internal audio still behaves as before and event-stream tests pass.

### Phase 6 — USB-MIDI proof of concept

- Confirm Cardputer ADV USB hardware mode and TinyUSB/Arduino-core compatibility under the canonical board package.
- Implement `MidiOutput` behind a build flag.
- Send a fixed note test, then clock/start/stop, then one 16-step pattern.
- Measure clock jitter without Serial logging in the real-time path.

Exit gate: a computer MIDI monitor and one external device receive correct notes and transport for 10 minutes without stuck notes.

### Phase 7 — SEQTRAK profile

- Document physical USB topology and power assumptions. Do not assume two USB device-mode ports can communicate directly.
- Verify whether Cardputer ADV can act as USB host; otherwise use DIN/TRS MIDI hardware or an external USB host bridge.
- Add channel mapping, note ranges, drum note mapping, clock policy, transport policy, and panic behavior.
- Record observed SEQTRAK behavior rather than guessing undocumented SysEx.

Exit gate: one GroovePuter pattern reliably plays or records into SEQTRAK with documented setup and limitations.

### Phase 8 — Atlas runtime patterns

- Define a compact, versioned runtime pattern schema.
- Convert Atlas data offline; do not parse large research CSVs in the audio path.
- Validate bounds, channels, note ranges, step counts, and event budgets before publication.
- Keep provenance/research metadata outside the real-time representation.

Exit gate: a selected Atlas pattern loads, previews, and plays internally and through the SEQTRAK profile.

## Non-goals during recovery

- No full application framework rewrite.
- No redesign of existing pages solely for cleanliness.
- No speculative SysEx implementation.
- No recorder reactivation before audio stability.
- No large Atlas ingestion pipeline on-device.
- No blocking mutex held while producing an audio block.

## Fast hardware acceptance checklist

- [ ] Device is confirmed as M5Stack Cardputer ADV.
- [ ] Build command, FQBN, board package version, and library versions are recorded.
- [ ] Boot completes and serial reports the selected hardware profile.
- [ ] Built-in display and keyboard respond.
- [ ] ES8311 output produces clean audio.
- [ ] Start/stop, BPM, mute, pattern changes, drums, and synth switching work.
- [ ] Scene save/load works from SD.
- [ ] No reset or watchdog occurs during 30 minutes of playback and control stress.
- [ ] Audio task reports no sustained deadline misses.
- [ ] Recorder remains explicitly disabled until separately accepted.

## Immediate decision

The next coding change should be Phase 1, not USB-MIDI: establish a canonical Cardputer ADV build and boot fingerprint. The next architectural change should then be the bounded command boundary between UI and audio.