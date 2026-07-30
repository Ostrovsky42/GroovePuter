# GroovePuter Recovery Backlog

Ordered for execution. Each task is intended to be independently reviewable and testable.

## Stage 0 — Baseline and platform contract

- [ ] REC-001 Capture current Cardputer ADV runtime baseline

Task Context  
Use `main` commit `bfbc63049cf79646217aa19c170b730e50ab3e4f` on a confirmed M5Stack Cardputer ADV. Record the exact hardware revision, USB connection, SD card presence, build command, FQBN, board package and library versions. Capture complete boot serial output and test display, keyboard, audio, transport, drums, both synth slots, scene load/save, and song playback. Do not change source behavior in this task.

Task DOD  
A short `docs/recovery/BASELINE.md` contains the environment, serial log summary, working features, failures, reset reason, free internal heap, largest free block, PSRAM report, and a 10-minute playback result. Any failure includes exact reproduction steps.

- [ ] REC-002 Declare the supported hardware profile

Task Context  
Resolve the contradiction between Cardputer ADV in `README.md`, original Cardputer/no-PSRAM assumptions in `agents_guide.md` and `scripts/build.sh`, and the ADV-specific ES8311 setup in `miniacid.ino`. Use Cardputer ADV as the recovery default. Preserve the possibility of a future original Cardputer profile, but do not claim it works without hardware validation.

Task DOD  
A single document states the supported board, ESP32 variant, flash/PSRAM policy, display, keyboard, ES8311 codec, I2S pins, SD assumptions, USB role assumptions, and voltage/power expectations. Unsupported profiles are explicitly marked untested.

- [ ] REC-003 Choose and pin the canonical Arduino build toolchain

Task Context  
Select one board package/FQBN that correctly supports Cardputer ADV and the current M5Unified/M5Cardputer audio path. Pin `arduino-cli`, board package, and library versions. The selected configuration must support the direct ESP-IDF I2S driver used by `AudioOutI2S` and the ADV ES8311 initialization.

Task DOD  
A version-controlled manifest or setup script installs exact dependencies from a clean environment. The canonical FQBN is declared once and reused by local and CI builds. Rationale for PSRAM and partition options is documented.

- [ ] REC-004 Normalize build and upload entry points

Task Context  
README currently references root scripts that do not exist, while scripts live under `scripts/`. `scripts/build.sh` also assumes `./platform_sdl/bin/arduino-cli`. Introduce one unambiguous workflow. Prefer small root wrappers calling versioned scripts, or update every reference to `scripts/...`; do not retain competing commands.

Task DOD  
From a clean checkout, one documented command builds and one documented command uploads. Scripts fail fast with actionable messages when `arduino-cli`, board packages, libraries, or the serial port are missing. `README.md` and `agents_guide.md` match the implementation exactly.

- [ ] REC-005 Add firmware build fingerprint

Task Context  
Runtime reports are currently insufficient to distinguish incompatible board packages and flags. Add a compact boot fingerprint outside the audio loop.

Task DOD  
Serial prints firmware version/commit, build date, hardware profile, FQBN-equivalent profile name, Arduino ESP32 core version where available, M5 library versions where available, flash size/layout, PSRAM presence/free bytes, sample rate, block size, and USB mode. Output is stable enough to paste into bug reports.

- [ ] REC-006 Add compile-only CI using the canonical command

Task Context  
CI must execute the same build path as local developers. Do not create a second hidden toolchain in YAML.

Task DOD  
A GitHub Actions workflow installs pinned dependencies and runs the canonical build command on pushes and pull requests. Build artifacts or size reports are retained. A deliberate compile error makes CI fail.

- [ ] REC-007 Clean obsolete build documentation and duplicate scripts

Task Context  
After the canonical build is established, remove stale commands, duplicated setup instructions, and unused build wrappers. Preserve historical notes only when clearly marked archival.

Task DOD  
Repository search finds no active instructions for an unsupported FQBN or nonexistent root command. Build documentation has one source of truth.

## Stage 1 — Audio and hardware smoke stability

- [ ] AUD-001 Characterize direct I2S and ES8311 startup

Task Context  
`miniacid.ino` starts M5Cardputer Speaker, ends it, then manually writes ES8311 registers before `AudioOutI2S` starts in another task. Verify the exact required sequence on Cardputer ADV and remove only demonstrably redundant operations.

Task DOD  
A documented test proves cold boot, warm reset, and repeated start/stop produce audio. Register writes are checked for failure and failures are visible on serial/screen. No silent boot is accepted as success.

- [ ] AUD-002 Add audio deadline and underrun telemetry

Task Context  
Current telemetry updates shared non-atomic fields and increments `stats.seq` twice. Define the statistics contract and keep instrumentation cheap.

Task DOD  
`seq` has one documented meaning. Published statistics are race-safe. Counters include rendered blocks, maximum/average DSP time, deadline misses, I2S write failures, and queue overflows. Telemetry can be sampled by UI without blocking audio.

- [ ] AUD-003 Add audio task stack and heap watermarks

Task Context  
The audio task uses an 8192-byte stack and the sketch mixes static and heap allocations. Establish measured margins before adding MIDI.

Task DOD  
Serial diagnostics report audio task stack high-water mark and internal heap/largest block at boot and after a controlled stress run. Acceptance thresholds are documented. No allocation is added to the steady-state render loop.

- [ ] AUD-004 Run 30-minute hardware playback smoke test

Task Context  
Exercise internal audio before concurrency refactoring: song loop, both synth slots, drums, engine switching, tempo changes, mutes, scene operations, and UI navigation.

Task DOD  
`docs/recovery/AUDIO_SMOKE.md` records test steps and results. There are no resets, watchdogs, persistent crackle, stuck transport, heap collapse, or sustained deadline misses.

- [ ] AUD-005 Clean disabled or misleading audio features

Task Context  
The recorder is constructed nowhere because it was disabled during crash debugging, yet code and product documentation can imply availability.

Task DOD  
Recorder UI/documentation explicitly says unavailable or experimental, or the feature is hidden. Dead debug comments and duplicated audio initialization comments are removed without re-enabling recording.

## Stage 2 — Real-time concurrency boundary

- [ ] RT-001 Inventory all mutable `MiniAcid` access sites

Task Context  
Identify every access from audio task, Arduino loop, UI pages, encoder, scene storage, sampler, and future MIDI seams. Classify reads/writes, whether they can allocate/block, and whether they affect active DSP state.

Task DOD  
`docs/recovery/STATE_OWNERSHIP.md` contains a concrete table of call sites and assigns a single owner or synchronization strategy to each state category. No important mutation remains unclassified.

- [ ] RT-002 Define fixed-size `EngineCommand`

Task Context  
UI and input producers need a bounded, allocation-free way to request real-time state changes. Start with transport, BPM, mute, parameter adjustment, pattern selection, and engine switching.

Task DOD  
A trivially copyable command type has explicit payloads, size assertions, validation, and no pointers to temporary data. Command semantics and application boundary are documented.

- [ ] RT-003 Implement bounded control command queue

Task Context  
Use a FreeRTOS queue or a proven bounded SPSC/MPSC design appropriate to the actual producers. The audio task must never block waiting for a command. Queue-full behavior must be deterministic.

Task DOD  
Producers can enqueue commands without touching active DSP state. Audio drains a bounded number before rendering each block. Queue overflow increments telemetry and follows a documented drop/coalescing policy. No general mutex surrounds `generateAudioBuffer()`.

- [ ] RT-004 Route transport, BPM, mute and volume through commands

Task Context  
Convert the simplest high-frequency direct calls from `loop()` and UI first. Preserve visible behavior.

Task DOD  
Keyboard, button, encoder, and UI controls no longer directly mutate these active fields. Hardware acceptance confirms controls remain responsive and audio remains stable under rapid input.

- [ ] RT-005 Route pattern edits and synth-engine switching safely

Task Context  
Pattern structures and engine switches can involve multi-field updates. Apply commands or immutable snapshots at deterministic block or musical boundaries.

Task DOD  
No partial pattern update can be observed by rendering. Engine switching preserves existing click-free behavior. Stress tests cover repeated edits and switches during playback.

- [ ] RT-006 Implement immutable scene/pattern publication

Task Context  
Do not push large scene or pattern blobs through the small command queue. Prepare and validate outside audio, then publish a versioned immutable snapshot and swap ownership at a safe boundary.

Task DOD  
Scene load and complete pattern replacement perform no SD, JSON, allocation, or blocking I/O in the audio task. Failed validation leaves the active snapshot unchanged.

- [ ] RT-007 Replace no-op `AudioGuard`

Task Context  
The current no-op guard falsely communicates safety. After command/snapshot migration, remove it or repurpose it only for genuinely protected non-real-time UI snapshots.

Task DOD  
There is no empty lock/unlock implementation presented as synchronization. Documentation accurately describes the final state model.

- [ ] RT-008 Add concurrency stress test and cleanup

Task Context  
Rapidly exercise BPM, start/stop, mutes, engine switching, pattern edits, page changes, and scene requests while playing.

Task DOD  
A 30-minute hardware stress run has no reset, watchdog, corrupted pattern, stuck note, persistent crackle, or unexplained queue overflow. Temporary migration paths and duplicate direct mutations are removed.

## Stage 3 — Characterization and host tests

- [ ] TST-001 Establish host-testable core boundary

Task Context  
Extract only platform-neutral timing/event logic needed for tests. Do not port the full firmware or introduce a general framework.

Task DOD  
Tests compile without M5 hardware libraries for timing, pattern, song, and scheduling components. Embedded build behavior is unchanged.

- [ ] TST-002 Test 96 PPQN clock advancement

Task Context  
Cover BPM conversion, fractional phase accumulation, long-run drift, wraparound, and block-size variation.

Task DOD  
Deterministic tests verify expected tick counts and bounded drift over long simulated runs at representative and boundary BPM values.

- [ ] TST-003 Test swing and grid mapping

Task Context  
Characterize the existing FEEL behavior for 1/8, 1/16, 1/32, half/normal/double time, and supported swing range.

Task DOD  
Golden tests document current tick offsets and prevent accidental timing changes during PatternPlayer extraction.

- [ ] TST-004 Test pattern/song boundaries and note-offs

Task Context  
Cover pattern length, multi-bar transitions, song-slot changes, mute transitions, note duration, and panic/all-notes-off behavior.

Task DOD  
Tests prove no missing or duplicate boundary events and no indefinitely hanging notes in the recorded event stream.

- [ ] TST-005 Test scene backward compatibility

Task Context  
README claims safe loading of older scenes with optional fields. Create representative fixtures for supported historical schema variants.

Task DOD  
Fixtures load with explicit defaults, malformed scenes fail safely, and round-trip behavior is documented.

- [ ] TST-006 Remove duplicate test scaffolding and stale patches

Task Context  
Review repository artifacts such as `test_patch.diff` and any ad-hoc test files. Keep only reproducible tests and intentional fixtures.

Task DOD  
Obsolete patches and duplicated harnesses are removed or archived with rationale. CI runs the authoritative test set.

## Stage 4 — Output-neutral scheduling

- [ ] EVT-001 Define scheduled musical event model

Task Context  
Represent the minimum data required by internal audio and MIDI: tick/time, destination track, event kind, note/control, velocity/value, duration or explicit note-off, and ordering.

Task DOD  
The type is fixed-size or bounded, validated, documented, and covered by ordering tests. It does not contain device-specific SEQTRAK assumptions.

- [ ] EVT-002 Add fake event output

Task Context  
Create a host-test output that records events for assertions without DSP or hardware.

Task DOD  
Known patterns produce deterministic, inspectable event lists. Tests cover simultaneous events and stable ordering.

- [ ] EVT-003 Extract minimal `PatternPlayer`

Task Context  
Separate event scheduling from direct synth/drum calls only where needed. Keep 96 PPQN timing authoritative and preserve current generator/pattern/song models.

Task DOD  
`PatternPlayer` can feed the fake output and internal output. Existing device audio passes the smoke suite with no intended musical change.

- [ ] EVT-004 Implement `InternalAudioOutput`

Task Context  
Adapt scheduled events back to the existing synth and drum engines. Avoid redesigning DSP engines.

Task DOD  
All currently supported internal tracks play through the adapter, including mutes and engine switching. No extra allocation or blocking is introduced in rendering.

- [ ] EVT-005 Clean legacy direct scheduling paths

Task Context  
After parity is established, remove duplicate paths that independently advance patterns or trigger voices.

Task DOD  
There is one authoritative pattern/song advancement path. Golden event tests and hardware smoke tests remain green.

## Stage 5 — USB-MIDI proof of concept

- [ ] MIDI-001 Verify Cardputer ADV USB role and electrical topology

Task Context  
Determine from board hardware and the canonical core whether the exposed USB port can operate as USB device, USB host, or both in the intended power configuration. Explicitly evaluate direct Cardputer ADV ↔ SEQTRAK feasibility; two USB device endpoints cannot communicate without a host.

Task DOD  
`docs/midi/USB_TOPOLOGY.md` documents supported roles, cables/adapters, power direction, risks, and the selected proof-of-concept topology. Unsupported direct connections are clearly rejected.

- [ ] MIDI-002 Define minimal `MidiOutput` interface

Task Context  
Support note on/off, CC, clock, start, stop, continue, and all-notes-off. Avoid SysEx until a verified requirement exists.

Task DOD  
Interface and no-op/fake implementations compile in host tests. Channel and value bounds are validated.

- [ ] MIDI-003 Implement USB-MIDI transport behind a build flag

Task Context  
Use the USB stack compatible with the pinned board package. Initialization and callbacks must not block audio or mutate engine state directly.

Task DOD  
Firmware enumerates in the selected topology and a desktop MIDI monitor receives a fixed note-on/note-off test. Internal audio remains the default when the feature is disabled.

- [ ] MIDI-004 Emit notes from `PatternPlayer`

Task Context  
Map scheduled note events to MIDI without changing musical time ownership.

Task DOD  
A deterministic 16-step pattern appears correctly in a MIDI monitor for 10 minutes with matched note-ons/note-offs and no stuck notes.

- [ ] MIDI-005 Emit clock and transport

Task Context  
MIDI clock is 24 PPQN while GroovePuter timing is 96 PPQN, so emit one MIDI clock per four internal ticks. Define start/continue/stop semantics and reset position behavior.

Task DOD  
Tests verify 4:1 tick conversion and transport ordering. Measured output at several BPM values has documented average rate and jitter with debug logging disabled.

- [ ] MIDI-006 Add MIDI panic and disconnect recovery

Task Context  
External devices must not retain notes after stop, profile change, cable loss, or queue overflow.

Task DOD  
Stop and error paths send explicit note-offs/all-notes-off according to policy. Reconnect does not replay stale events.

- [ ] MIDI-007 Clean proof-of-concept diagnostics

Task Context  
Remove high-rate Serial output and temporary note generators once PatternPlayer drives MIDI.

Task DOD  
No logging or allocation occurs per MIDI clock/note event in release mode. Debug facilities are opt-in and bounded.

## Stage 6 — Yamaha SEQTRAK vertical slice

- [ ] SEQ-001 Capture SEQTRAK MIDI observation matrix

Task Context  
Test actual SEQTRAK behavior for channels, note ranges, drum note mapping, external clock, start/stop/continue, recording while externally clocked, and project/track state assumptions. Record firmware version and connection topology.

Task DOD  
`docs/midi/SEQTRAK_OBSERVATIONS.md` distinguishes official documentation, observed behavior, and unknowns. Each observation includes a reproducible test.

- [ ] SEQ-002 Implement versioned `SeqtrakProfile`

Task Context  
Keep device mapping outside generic scheduling and MIDI transport. Include channel assignment, drum note map, melodic ranges, clock policy, transport policy, and panic policy.

Task DOD  
Profile selection is explicit and persisted safely. Tests validate all mappings and reject unsupported values.

- [ ] SEQ-003 Play one GroovePuter pattern on SEQTRAK

Task Context  
Use a small known pattern with drums and one melodic track. Do not add Atlas data yet.

Task DOD  
The documented setup plays the expected rhythm and notes for 10 minutes without drift, stuck notes, reset, or audio-task deadline regression.

- [ ] SEQ-004 Validate recording into SEQTRAK

Task Context  
Determine what SEQTRAK records under external clock and how pattern boundaries are handled. Avoid assumptions about undocumented internal project APIs.

Task DOD  
A repeatable procedure records the test pattern or documents the exact limitation preventing it. The result includes timing comparison and required user actions.

- [ ] SEQ-005 Clean device-specific leakage

Task Context  
Review generic `PatternPlayer`, event, and MIDI layers for SEQTRAK-specific constants introduced during the slice.

Task DOD  
SEQTRAK mappings remain confined to the profile/device adapter. Generic layers retain reusable tests.

## Stage 7 — Atlas runtime integration

- [ ] ATL-001 Define compact runtime pattern schema

Task Context  
Atlas research CSVs and provenance metadata are not appropriate for direct parsing in the audio task. Define a versioned runtime representation for steps/events, tracks, tempo hints, swing/feel hints, length, and bounded metadata identifiers.

Task DOD  
Schema has explicit limits, validation rules, versioning, and examples. Worst-case RAM and event counts are calculated for Cardputer ADV.

- [ ] ATL-002 Build offline Atlas converter

Task Context  
Convert validated Atlas source data into the compact runtime format on a workstation. Preserve source IDs so provenance can be inspected outside the real-time payload.

Task DOD  
Converter is deterministic, reports rejected records with reasons, and produces a manifest with counts and hashes. No research CSV parsing is added to firmware.

- [ ] ATL-003 Add bounded runtime loader

Task Context  
Load and validate a selected runtime pattern outside the audio task, then publish it through the immutable snapshot path.

Task DOD  
Malformed or oversized input is rejected without altering active playback. Memory use and load time are reported.

- [ ] ATL-004 Play one Atlas pattern internally and through SEQTRAK

Task Context  
Use one verified pattern to prove the complete path before adding browsing/catalog UI.

Task DOD  
The same pattern produces equivalent scheduled events for internal audio and `SeqtrakProfile`, subject to documented voice/mapping differences.

- [ ] ATL-005 Remove duplicated pattern sources and temporary adapters

Task Context  
After the vertical slice is accepted, identify temporary hard-coded patterns, duplicate conversion logic, and stale schema drafts.

Task DOD  
One runtime schema and one converter are authoritative. Existing built-in patterns remain intact unless intentionally migrated with parity tests.