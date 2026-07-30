# GroovePuter Prioritized Bug Backlog

Source baseline: `main` commit `bfbc63049cf79646217aa19c170b730e50ab3e4f`.

Execution rule: fix in order unless a task explicitly states that it can run in parallel. Every behavioral fix must add a deterministic test before or with the implementation.

Hardware assumption for device tasks:

- M5Stack Cardputer ADV, ESP32-S3;
- built-in ES8311 audio codec;
- built-in display and keyboard;
- SD card present for persistence tests;
- exact board package/FQBN must be recorded by PLAT-001 before firmware acceptance;
- GPIO assignments must come from the selected Cardputer ADV profile, not literal values spread across modules.

## Stage 0 — Stop hardware and data hazards

- [ ] [HW]-[001] Disable the GPIO21 LED/amplifier collision

Task Context  
`miniacid.ino` drives GPIO21 high as amplifier enable. `src/ui/led_manager.cpp` sends NeoPixel data on GPIO21. Until the actual Cardputer ADV LED pin and amplifier-enable behavior are verified, LED writes are unsafe.

Task DOD  
A single Cardputer ADV hardware profile defines amplifier enable, I2S, I2C, display, keyboard, SD and LED pins. `LedManager` never uses GPIO21 unless the profile explicitly proves it is the RGB data pin and not amplifier enable. Cold boot, beat LED mode and step-trigger LED mode produce stable audio for 10 minutes. Serial reports the selected pin map. No literal duplicate pin ownership remains outside the profile.

- [ ] [DATA]-[001] Make scene save report real storage failure

Task Context  
Scene/application save APIs can report success while the storage write failed. This makes every other persistence test untrustworthy.

Task DOD  
Every save/create/rename path propagates the exact storage result to UI and serial. A removed SD card, read-only/failing mock, full filesystem and invalid path all produce failure. The active scene name is not changed on failure. Tests assert that no success toast is emitted after failed persistence.

- [ ] [DATA]-[002] Replace destructive scene overwrite with transactional commit

Task Context  
The current path removes the old scene before writing the replacement and updates metadata separately.

Task DOD  
Scene save writes a temporary file, closes it, validates length and full parse or checksum, then atomically promotes it where the filesystem supports rename. The prior scene remains readable after injected failures at every write stage. Current-scene metadata is updated only after the scene commit succeeds. Recovery behavior for a leftover temp file is documented and tested.

- [ ] [DATA]-[003] Reject unsafe scene names

Task Context  
Scene name normalization does not reliably reject path separators and traversal tokens.

Task DOD  
Only a documented safe character set is accepted. Empty names, `.`, `..`, `/`, `\`, control bytes and traversal-like names are rejected before any SD operation. Tests prove save/delete/rename cannot escape the scene directory.

- [ ] [CLEAN]-[001] Remove obsolete hazard paths after Stage 0

Task Context  
Once pin ownership and transactional save exist, stale literal pins, duplicate path construction and unconditional-success wrappers must not remain.

Task DOD  
Repository search finds one hardware profile and one scene commit implementation. Disabled compatibility code is deleted or explicitly archived. Existing behavior unrelated to these hazards is unchanged.

## Stage 1 — Repair the 96 PPQN scheduler

- [ ] [TIME]-[001] Add failing characterization tests for non-zero tick offsets

Task Context  
The clock increments at 96 PPQN, but the sequencer evaluates musical events only every 24 ticks. Tests must capture the defect before implementation changes.

Task DOD  
A host-testable scheduler fixture proves current failure for drum and synth events with timing offsets 1, 2, 7, 12 and 23 ticks and for equivalent swing offsets. Tests also cover zero offset and bar wrap. The tests fail against the current scheduling logic for the expected reason.

- [ ] [TIME]-[002] Dispatch scheduled events on every PPQN tick

Task Context  
`generateAudioBuffer()` calls `advanceTick()` only on ticks divisible by 24, while `processSequencerEvents()` expects exact offset ticks.

Task DOD  
Every 96-PPQN tick evaluates due events. Logical 16-step advancement, automation step changes and song-position changes remain restricted to their intended boundaries. Events with all supported positive/negative timing offsets and swing delays trigger exactly once. Long-run tick count and drift tests pass at 10, 40, 120, 180 and 250 BPM with 128/256/512-frame simulated blocks.

- [ ] [TIME]-[003] Apply FEEL changes at a safe musical boundary

Task Context  
Applying FEEL can reset fractional phase and cause a timing discontinuity while playing.

Task DOD  
Grid/timebase/swing changes use a documented boundary policy: immediate parameter publication without clock reset, next-step, or next-bar. The chosen policy is deterministic and tested. No code resets `tickPhaseAccum_` during ordinary live FEEL changes.

- [ ] [TIME]-[004] Restore a representable song pause state

Task Context  
Playback expects `-2` for pause/rehearsal, but the song API clamps all values below `-1` to `-1`.

Task DOD  
The song pattern type defines named constants for empty and pause. Set/get/serialize/load/UI operations preserve both values. Tests cover both song slots and every track. Pause behavior is documented and visibly distinct from an empty cell.

- [ ] [TIME]-[005] Correct offline render ownership and duration

Task Context  
Offline WAV rendering ignores multi-bar pattern length and can invoke the live engine concurrently with the hardware audio task.

Task DOD  
Rendering uses a dedicated engine snapshot or an explicit audio-runtime ownership transfer. The hardware audio task cannot render the same mutable engine concurrently. Duration includes pattern bars, song length and selected loop policy. A 2/4/8-bar fixture produces the exact expected frame count within one render block of final padding. Playback state is restored deterministically.

- [ ] [CLEAN]-[002] Delete duplicate step/tick advancement paths

Task Context  
After scheduler repair, legacy methods and comments that conflate PPQN ticks with 16th-note steps will cause regressions.

Task DOD  
There is one authoritative PPQN dispatch path and one explicit step-boundary path. Constants replace literal `24` and `384` where they express musical timing. Tests remain green and no stale “Stage 2 every 24 ticks” behavior remains.

## Stage 2 — Establish real-time ownership

- [ ] [RT]-[001] Inventory and classify every mutable engine access

Task Context  
UI, keyboard, encoder, paging, scene loading, MIDI importer, automation and audio all touch `MiniAcid` state.

Task DOD  
`docs/recovery/STATE_OWNERSHIP.md` lists every producer/caller, state category, current thread/core, allocation/blocking behavior and final ownership rule. Every direct mutation is assigned to command, immutable snapshot, non-real-time storage state or isolated atomic telemetry.

- [ ] [RT]-[002] Introduce a bounded allocation-free engine command queue

Task Context  
Transport, BPM, mutes, scalar parameters and engine-selection requests must not mutate active DSP from UI/core 0.

Task DOD  
A trivially-copyable `EngineCommand` and bounded queue exist. Producers never block the audio task. Audio drains a bounded count at the start of a block. Queue-full policy and counters are explicit. Unit tests cover order, overflow, coalescing where used and invalid payload rejection.

- [ ] [RT]-[003] Route transport, BPM, mutes and scalar parameters through commands

Task Context  
These are frequent cross-core mutations and provide the smallest vertical slice.

Task DOD  
Button, keyboard, encoder and UI handlers no longer call active DSP mutators directly for the migrated operations. A 30-minute rapid-control test produces no reset, stuck gate, corrupted state or unexplained queue overflow. Control latency is measured and documented.

- [ ] [RT]-[004] Publish patterns and scenes as validated snapshots

Task Context  
Complete pattern/scene replacement is too large for scalar commands and currently races rendering.

Task DOD  
Storage/parser/importer prepares and validates inactive state outside the audio task. Audio swaps an immutable/versioned snapshot at a documented block/step/bar boundary. Failed validation leaves the active state unchanged. No SD, JSON, heap allocation or blocking file operation occurs in the audio task.

- [ ] [RT]-[005] Remove heap allocation and destruction from engine switching

Task Context  
Synth switching allocates/replaces `unique_ptr` voices while rendering. Drum automation can allocate a new drum engine from the audio path.

Task DOD  
All supported synth and drum engine instances are preallocated or constructed outside real-time ownership and published safely. Audio switching performs bounded pointer/index/state changes only. A test allocator proves zero allocations during steady-state rendering and repeated engine switching. Existing click-free crossfade behavior is retained.

- [ ] [RT]-[006] Replace fake `AudioGuard` with the final ownership model

Task Context  
The no-op guard falsely communicates safety.

Task DOD  
No empty lock/unlock object is presented as synchronization. Pages use commands or snapshots. Read-only UI state is published through an explicit atomic/versioned telemetry snapshot. Documentation matches the implementation.

- [ ] [RT]-[007] Make performance telemetry race-safe and wire real failures

Task Context  
The double `seq` increment is an intended odd/even publication protocol, but it uses `volatile` rather than atomics. I2S failures do not increment the underrun/deadline counters used by FX safety.

Task DOD  
Telemetry publication uses atomics with documented acquire/release semantics or a copied snapshot behind a safe protocol. Counters distinguish rendered blocks, DSP deadline misses, I2S timeout/failure, queue overflow and dropped/coalesced commands. UI reads cannot spin forever. Fault injection proves FX safety observes output failures.

- [ ] [RT]-[008] Make waveform and LED event publication race-safe

Task Context  
The waveform reference can be rewritten after two blocks. LED payload fields race behind an atomic pending flag.

Task DOD  
Waveform UI receives a generation-stable copy/view with explicit lifetime. LED event publication uses a complete atomic/queue snapshot. Threaded host tests or deterministic interleavings show no torn payload. GPIO ownership from HW-001 remains enforced.

- [ ] [RT]-[009] Remove pre-setup SD access from static construction

Task Context  
Global engine construction can reach paging directory setup before board and SD initialization.

Task DOD  
Constructors perform no hardware/filesystem I/O. SD/paging initialization occurs explicitly after board/SD startup and returns a checked status. Cold boot and reset tests show deterministic initialization order.

- [ ] [CLEAN]-[003] Remove direct mutation and temporary migration paths

Task Context  
The command/snapshot migration must end with one ownership model, not two.

Task DOD  
Repository search and state-ownership review find no unapproved UI/storage mutation of active DSP. Temporary bypasses, duplicate fields and obsolete guard comments are removed.

## Stage 3 — Make scene persistence complete

- [ ] [SCENE]-[001] Define one versioned scene schema

Task Context  
The ArduinoJson builder and streaming writer encode different field sets and structures. The streaming path is the constrained-device release path.

Task DOD  
One schema version and field table cover every persistent field, defaults, bounds and migrations. One canonical writer and one canonical reader implement it. Alternative implementations are generated from the same schema or removed from release behavior.

- [ ] [SCENE]-[002] Add exhaustive Cardputer-path round-trip tests

Task Context  
Current saves omit drum velocity/timing, synth ghost/velocity/timing, swing percentage/mask, generator parameters and active song slot.

Task DOD  
A fixture sets every persistent field in all banks/patterns/steps, both songs, both song slots, FEEL, generator, sampler, vocal, tape, LED and engine state. Write through the streaming writer, read through the evented parser, compare field-by-field. No unsupported field is silently discarded.

- [ ] [SCENE]-[003] Reset all defaults before loading sparse or legacy scenes

Task Context  
The static temporary `Scene` is reused and `clearSceneData()` does not reset every optional field.

Task DOD  
There is one complete default initializer. Loading a sparse fixture after a fully populated fixture yields declared defaults, never values from the previous load. Malformed input remains transactional and leaves the active scene unchanged.

- [ ] [SCENE]-[004] Implement complete JSON escaping and bounds validation

Task Context  
The streaming writer does not escape all JSON control characters and several numeric fields accept unsafe ranges.

Task DOD  
Strings correctly escape quote, backslash, control characters and supported UTF-8 bytes. Readers validate note values, probabilities, timing, velocity, volumes, enum ranges, lengths and array counts. Fuzz/property tests cover malformed and boundary input without abort or active-state mutation.

- [ ] [SCENE]-[005] Make pattern clear operations reset the full step state

Task Context  
Synth clear leaves ghost, velocity and timing metadata behind; equivalent helpers must be checked for drums and automation.

Task DOD  
Clear uses canonical default constructors/initializers for every step and lane field. Tests clear a maximally populated pattern and compare it to a newly initialized empty pattern.

- [ ] [CLEAN]-[004] Remove duplicate serializers and stale compatibility branches

Task Context  
After schema migration, two divergent authoritative implementations are unsafe.

Task DOD  
One release writer/reader pair remains. Supported legacy fixtures are explicit. Disabled fallback comments and duplicated field mappings are removed.

## Stage 4 — Replace destructive pattern paging

- [ ] [PAGE]-[001] Define a versioned page cache format

Task Context  
Current paging stores raw C++ structs with compiler padding and no validation.

Task DOD  
The format has magic, version, page index, payload sizes, schema/ABI-independent encoding and checksum. Invalid version/size/checksum is rejected without mutating active state. Migration or cache invalidation policy is documented.

- [ ] [PAGE]-[002] Commit a complete page transactionally

Task Context  
Drum, synth A and synth B are independent files and can represent different generations.

Task DOD  
A page is committed as one validated file or as a manifest-led transaction with one generation ID. Injected interruption after every write leaves either the old complete page or the new complete page, never a mixture.

- [ ] [PAGE]-[003] Load into temporary state and swap only on success

Task Context  
Missing/corrupt components are zeroed directly and callers switch pages after failure.

Task DOD  
Page load populates a temporary validated object. Failure leaves current page and page index unchanged and produces a visible error. Missing page creation uses canonical empty-pattern defaults, never `memset(0)`.

- [ ] [PAGE]-[004] Unify page-count constants and navigation

Task Context  
Paging supports 16 pages while global shortcuts wrap at UI page count 12.

Task DOD  
Pattern-page count is defined once and distinct from UI-screen count. Every page 0–15 is reachable, displayed and bounds-checked. Tests cover wraparound and invalid requests.

- [ ] [PAGE]-[005] Fix global free-pattern search

Task Context  
The search loops global IDs but uses local/current bank accessors that clamp to 0–7.

Task DOD  
Global IDs are decoded into page, bank and local pattern before inspection. Tests cover occupied/free combinations across all pages and both banks and prove unrelated active page state is not mutated.

- [ ] [CLEAN]-[005] Remove duplicated paging orchestration

Task Context  
`SceneManager` and `MiniAcidDisplay` currently repeat save/load policy.

Task DOD  
One application service owns page transition, error handling and snapshot publication. UI only requests transition and displays result.

## Stage 5 — Repair sampler ownership and WAV parsing

- [ ] [SAMPLE]-[001] Make preload failure bounded when no slot is evictable

Task Context  
The pool-eviction loop can make no progress forever when every sample has a live reference.

Task DOD  
Eviction reports whether it freed bytes. Preload exits with a typed failure after bounded work when no candidate exists. A unit test holds every handle and verifies no loop/watchdog condition.

- [ ] [SAMPLE]-[002] Make slot acquisition and eviction mutually safe

Task Context  
Eviction can observe refcount zero and free a slot after audio acquires it.

Task DOD  
Slot lifecycle uses a safe state machine/CAS or equivalent ownership protocol. No new handle can be acquired after eviction claims a slot; claimed data is not freed until references are impossible. Deterministic race tests and sanitizer-supported host tests pass.

- [ ] [SAMPLE]-[003] Fix default reverse playback

Task Context  
Reverse with `endFrame == 0` starts at frame zero because trigger does not consult sample length.

Task DOD  
Trigger obtains the sample view/length before choosing reverse start position. Default reverse starts at `actualEnd - 1`, respects start/end bounds and plays the expected frames. Tests cover explicit and implicit end frame, loop and non-loop.

- [ ] [SAMPLE]-[004] Harden WAV RIFF parsing and memory accounting

Task Context  
Chunk padding is ignored, channel counts above two are accepted incorrectly, and stereo fallback can retain double-sized memory while accounting it as mono.

Task DOD  
Parser handles RIFF word padding, validates supported channel counts, checks arithmetic overflow and records actual allocated bytes. Mono/stereo fixtures with odd metadata chunks load correctly. Unsupported multichannel files fail clearly. Pool usage equals actual allocated memory.

- [ ] [SAMPLE]-[005] Bounds-check sampler pad access

Task Context  
Pad accessors directly index a fixed array.

Task DOD  
Invalid indices return a safe failure/reference policy without memory access. All callers handle failure. Tests cover negative and upper-bound indices.

- [ ] [CLEAN]-[006] Remove legacy unsafe sample APIs where unused

Task Context  
ID-based acquire/release APIs and duplicate ownership paths increase underflow/race risk.

Task DOD  
Audio uses handle-based ownership only. Unused legacy methods are removed or isolated with explicit tests. Refcount underflow is detected in debug/test builds.

## Stage 6 — Repair MIDI import before adding MIDI output

- [ ] [MIDI]-[001] Parse into an isolated import transaction

Task Context  
Current import mutates/cache-saves partial results and can persist after parser error.

Task DOD  
Parser writes only to a temporary import model. No page/pattern is changed until parsing, routing, bounds validation and destination planning all succeed. Failure leaves source state byte-for-byte unchanged.

- [ ] [MIDI]-[002] Limit overwrite clearing to the actual import footprint

Task Context  
Overwrite can clear every later pattern before source length is known.

Task DOD  
A planning pass determines exact destination patterns first. Only those patterns are replaced. A one-pattern import cannot modify any later unrelated pattern. Tests cover multi-track and multi-pattern imports near the final page.

- [ ] [MIDI]-[003] Fix Type-1 global timing normalization

Task Context  
Per-track parsing and first-encountered normalization make results depend on track order.

Task DOD  
Events from all tracks use a common absolute timeline and are sorted/stably merged before quantization. Reordering MTrk chunks in an equivalent file produces identical imported patterns.

- [ ] [MIDI]-[004] Fix scanner byte consumption and VLQ validation

Task Context  
Channel Pressure does not consume its data byte and malformed four-byte continuation VLQs are accepted.

Task DOD  
Every channel event consumes the MIDI-specified data length under running status. VLQs reject overflow/unterminated encodings. Corpus tests cover all channel statuses, SysEx/meta skipping, running status and malformed truncation without parser desync.

- [ ] [MIDI]-[005] Make routing and metrics truthful

Task Context  
Track-name routing guesses channels before note evidence and imported-note metrics include unmapped/no-op events.

Task DOD  
Routing is based on actual track/channel evidence with deterministic fallback. Metrics distinguish parsed, routed, imported, skipped-unmapped, skipped-policy and rejected events. UI describes that note duration/tempo-map semantics are not imported unless implemented.

- [ ] [MIDI]-[006] Commit import through the recovered page/snapshot service

Task Context  
Importer currently depends on unsafe paging and direct active-scene mutation.

Task DOD  
Successful import publishes through PAGE-003 and RT-004. Storage failure or snapshot rejection leaves active and cached data unchanged. Integration tests cover import into non-current page and rollback.

- [ ] [CLEAN]-[007] Remove partial-import and duplicate routing paths

Task Context  
After transactional import, old mutation code must not remain reachable.

Task DOD  
One parser/planner/commit pipeline remains. Obsolete caches, counters and overwrite loops are removed.

## Stage 7 — UI/display and diagnostics cleanup

- [ ] [UI]-[001] Balance every display transaction

Task Context  
Splash rendering returns after `startWrite()` without `endWrite()`.

Task DOD  
RAII or a single structured exit guarantees every started transaction ends exactly once. Splash, normal page, overlay and error paths are covered by a fake display transaction-count test and Cardputer ADV boot test.

- [ ] [UI]-[002] Replace unconditional full-screen redraw with dirty-region updates

Task Context  
The UI pushes a full 320×240 RGB565 frame approximately every 40 ms.

Task DOD  
Static frame, header, playhead, meters and changed cells use explicit dirty regions or a bounded partial-update strategy. SPI transactions are grouped. Idle UI performs no full-screen transfer. Hardware measurement records UI frame time and audio deadline behavior before/after.

- [ ] [UI]-[003] Harden display primitive clipping

Task Context  
`drawRect()` can index outside the frame for rectangles fully outside valid bounds.

Task DOD  
All primitives reject empty/off-screen regions before indexing. Host tests fuzz negative/oversized coordinates under AddressSanitizer. No out-of-bounds writes occur.

- [ ] [UI]-[004] Eliminate page heap churn during playback

Task Context  
Pages are destroyed/recreated based on low-heap thresholds while audio is active.

Task DOD  
Frequently used pages/components use a measured bounded allocation plan. Navigation during playback performs no unexpected large allocations or can fail visibly without corrupting state. Heap/largest-block measurements remain stable across 1,000 page transitions.

- [ ] [DIAG]-[001] Correct debug overlay values

Task Context  
The CPU line draws stale buffer contents and current statistics are incomplete.

Task DOD  
Overlay formats every displayed value from one race-safe snapshot. CPU ideal/actual, deadline misses, I2S failures, queue overflow, heap and stack watermark are independently labeled and tested.

- [ ] [UI]-[005] Centralize screen routing names and IDs

Task Context  
Screen indices, names and shortcut comments have drifted.

Task DOD  
One route table defines screen ID, title, constructor and shortcuts. Page 11 and every other route display/log the same name. Tests verify unique IDs and valid navigation targets.

- [ ] [CLEAN]-[008] Remove recorder claims and stale UI test artifacts

Task Context  
Recorder remains disabled and old test pages/patches can look like supported behavior.

Task DOD  
Unavailable features are hidden or explicitly experimental. `test_patch.diff` and obsolete manual scaffolding are removed or archived with rationale. Authoritative tests run in CI.

## Stage 8 — Reproducible build and release gate

- [ ] [PLAT]-[001] Pin one Cardputer ADV toolchain and board profile

Task Context  
README and scripts compile different boards; build and upload use different CLI binaries.

Task DOD  
A versioned setup manifest installs exact `arduino-cli`, ESP32/M5Stack board package and library versions. One FQBN/profile is declared once. Build fingerprint prints commit, board profile, core/library versions, flash/partition, PSRAM, sample rate, block size and USB mode.

- [ ] [PLAT]-[002] Normalize build, release and upload commands

Task Context  
Documented root commands do not exist, release checks are weak and upload may use stale build output.

Task DOD  
From a clean checkout, one command builds and one command uploads the just-built artifact. Scripts use the same CLI and FQBN, verify artifact existence, accept platform-valid port names and fail with actionable errors. README and agent guide exactly match.

- [ ] [PLAT]-[003] Repair the SDL host source list

Task Context  
The Makefile omits current synth implementation files and is not a reliable characterization harness.

Task DOD  
The host build includes the same platform-neutral implementation units needed by firmware. It links from clean state on Linux in CI. Hardware-only code remains behind explicit boundaries rather than silent source omission.

- [ ] [PLAT]-[004] Add compile, unit and artifact CI

Task Context  
Current `main` has no automated status.

Task DOD  
GitHub Actions installs pinned dependencies, runs host unit/sanitizer tests and the canonical firmware compile, reports binary/DRAM/flash sizes and retains artifacts. A deliberate test/compile failure fails the workflow.

- [ ] [PLAT]-[005] Run the Cardputer ADV acceptance suite

Task Context  
Static fixes are not accepted without real hardware.

Task DOD  
On a confirmed Cardputer ADV: boot, display, keyboard, SD, scene round-trip, page transition, both synths, all drums, sampler, song, FEEL/swing/microtiming and 30-minute control stress pass. Serial and screen behavior are recorded. No reset, watchdog, silent save failure, data loss, stuck note, persistent crackle or sustained deadline miss occurs.

- [ ] [CLEAN]-[009] Remove unsupported build paths and close recovery debt

Task Context  
After the canonical path is green, old FQBNs, local binary assumptions and stale release instructions must not remain active.

Task DOD  
Repository search finds one supported build source of truth. Unsupported original Cardputer behavior is explicitly untested or lives behind a separately validated profile. Recovery documents link to final evidence and remaining non-blocking limitations.

## SEQTRAK gate

USB-MIDI and `SeqtrakProfile` work may begin only after the following are complete:

- HW-001;
- TIME-001 through TIME-004;
- RT-001 through RT-009;
- SCENE-001 through SCENE-005;
- PAGE-001 through PAGE-005;
- SAMPLE-001 and SAMPLE-002;
- PLAT-001, PLAT-002 and PLAT-004.

Reason: before these gates, MIDI would inherit dropped microtimed events, unsafe cross-core state mutation, unreliable pattern data and an undefined USB/board profile.
