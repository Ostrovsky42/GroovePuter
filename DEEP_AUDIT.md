# GroovePuter Deep Firmware Audit

Status: source audit of `main` at commit `bfbc63049cf79646217aa19c170b730e50ab3e4f`.

Target assumed for recovery: M5Stack Cardputer ADV.

## Scope and confidence

This is a second-pass firmware audit, not a product overview. It follows control and data flows through the build scripts, board initialization, FreeRTOS audio task, 96 PPQN scheduler, scene persistence, pattern paging, MIDI importer, sampler, UI and diagnostics.

Severity:

- **P0** — can corrupt data, race active DSP ownership, break core timing, hang, or make the selected hardware profile unsafe.
- **P1** — reproducible functional defect or major reliability problem.
- **P2** — latent correctness issue, misleading diagnostics, performance debt, or build/test deficiency.

Confidence:

- **Confirmed by source** — the defect follows directly from the current code.
- **Hardware confirmation required** — the code conflict is confirmed, but its exact electrical or audible consequence must be measured on Cardputer ADV.
- **Static-build confirmation required** — the source/toolchain contradiction is confirmed, but a clean build could not be executed in this audit environment.

## Corrections to the first audit

The first recovery note was too shallow and included one incorrect interpretation:

1. `perfStats.seq` is incremented twice intentionally as an odd/even seqlock protocol. The defect is not the double increment itself. The defect is that `volatile` fields are used instead of atomics with defined memory ordering, while readers and writers run on different cores.
2. The suspected 512-frame overflow in WAV rendering is not present because the current global audio block is also 512 frames.
3. Streaming scene loading parses into a separate static `Scene` and copies it into the active scene only after successful parsing. A syntax failure therefore does not partially mutate the active scene. The remaining load bug is stale/default state in fields that `clearSceneData()` does not reset.

## Executive diagnosis

The repository contains substantially more than documentation drift. The dominant defects cluster around four boundaries:

1. the firmware claims 96 PPQN, but musical events are evaluated only every 24 ticks;
2. UI, storage, paging and automation mutate the same engine that the audio task renders;
3. scene and page persistence silently lose or corrupt state;
4. the selected Cardputer ADV hardware profile is contradicted by build flags and by a GPIO21 amplifier/NeoPixel collision.

SEQTRAK output must not be added until the P0 items below are closed. Otherwise MIDI will expose unstable timing and add another concurrent state producer.

---

# Finding registry

## A. Build, release and platform contract

### GP-001 — README invokes scripts that do not exist at repository root

Severity: **P1**  
Confidence: **Confirmed by source**

`README.md` documents `./release.sh` and `./upload.sh`; `agents_guide.md` documents `./build.sh` and `./release.sh`. The files are under `scripts/`.

Impact: a clean checkout fails at the first documented command.

### GP-002 — README and scripts compile different boards

Severity: **P0**  
Confidence: **Confirmed by source; hardware validation required**

`README.md` uses `esp32:esp32:esp32s3:CDCOnBoot=cdc`. `scripts/build.sh` and release scripts use `m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app`.

Impact: pin definitions, USB mode, partitions, library conditionals, codec behavior and PSRAM policy are not reproducible.

### GP-003 — Build and upload can use different `arduino-cli` installations

Severity: **P1**  
Confidence: **Confirmed by source**

Build/release scripts call `./platform_sdl/bin/arduino-cli`; `scripts/upload.sh` calls global `arduino-cli`.

Impact: compile and upload can resolve different board packages and library versions.

### GP-004 — “Current source” upload path does not compile

Severity: **P1**  
Confidence: **Confirmed by source**

The non-prebuilt branch of `scripts/upload.sh` runs `arduino-cli upload` without first compiling and without selecting an explicit build directory.

Impact: it may fail or upload stale artifacts while claiming to use current sources.

### GP-005 — Release file checks test a non-empty variable, not file existence

Severity: **P2**  
Confidence: **Confirmed by source**

`scripts/release.sh` uses `if [ -n "$APP_BIN_SRC" ]`; the variable is always non-empty. `set -e` usually stops the script on `cp`, but the validation and messages are misleading.

### GP-006 — Desktop Makefile omits current synth implementation units

Severity: **P1**  
Confidence: **Static-build confirmation required**

`platform_sdl/Makefile` compiles `miniacid_engine.cpp`, which uses `SwappableSynthVoice`, SID, AY and OPL2, but the source list omits the corresponding implementation `.cpp` files.

Impact: the host build is expected to fail at link time or exercise an obsolete source set.

### GP-007 — No automated compile or test status exists for current `main`

Severity: **P1**  
Confidence: **Confirmed by repository status**

No GitHub Actions workflow run or commit status is associated with `bfbc6304...`. Existing “tests” are mainly manual documents and UI test pages.

### GP-008 — Upload port parsing is Unix-only

Severity: **P2**  
Confidence: **Confirmed by source**

`scripts/upload.sh` only accepts port arguments matching `/dev/*`; Windows COM ports and other valid Arduino CLI port identifiers are ignored.

---

## B. Cardputer ADV hardware and audio runtime

### GP-009 — GPIO21 is used both as amplifier enable and NeoPixel data

Severity: **P0**  
Confidence: **Source-confirmed conflict; exact hardware effect requires Cardputer ADV test**

`miniacid.ino` sets GPIO21 high as `PA_EN`. `LedManager::setLedColor()` sends WS2812 data with `neopixelWrite(21, ...)`.

Impact: LED beat/step activity can modulate the amplifier-enable line, producing clicks, dropouts, mute/unmute behavior or unstable audio power control.

Required action: move all pins into one Cardputer ADV hardware profile and disable LED writes until the actual ADV RGB pin is verified.

### GP-010 — Audio pins are ADV-specific while the build selects original Cardputer

Severity: **P0**  
Confidence: **Confirmed by source; hardware validation required**

`AudioOutI2S` hardcodes BCLK 41, LRCLK 43 and DOUT 42 and the sketch manually configures ES8311. The selected FQBN is `m5stack_cardputer` with original-Cardputer comments.

### GP-011 — Audio warm-up comment is wrong by roughly an order of magnitude

Severity: **P2**  
Confidence: **Confirmed by source**

The code says 32 blocks are approximately 90 ms at 44.1 kHz/128. The active configuration is 22.05 kHz/512, making 32 blocks approximately 743 ms.

Impact: boot/audio-start latency is misdiagnosed and hardware timing assumptions are stale.

### GP-012 — I2S write timeout exceeds the audio deadline by more than four times

Severity: **P1**  
Confidence: **Confirmed by source**

A 512-frame block at 22.05 kHz is approximately 23.2 ms. `i2s_channel_write` can block for 100 ms, followed by a 10 ms task delay after failure.

Impact: one failure can create a long audible gap and cascading deadline misses.

### GP-013 — DMA buffering is configured for high latency

Severity: **P2**  
Confidence: **Configuration confirmed; effective latency requires measurement**

Eight descriptors of 512 frames create substantial buffering relative to interactive keyboard/MIDI use. This may be acceptable for playback but is a poor default for live input without measurement.

### GP-014 — PSRAM capability threshold does not match requested allocations

Severity: **P1**  
Confidence: **Confirmed by source**

`MiniAcid::init()` treats more than 512 KiB free PSRAM as “high performance”, then requests an 8-second looper, a 2 MiB sampler pool and multiple delay buffers. Allocation return values are not propagated into a visible capability state.

Impact: features can silently be unavailable or fall back into internal DRAM.

### GP-015 — Audio recorder remains intentionally disabled after crashes

Severity: **P1**  
Confidence: **Confirmed by source**

Construction is commented with `DISABLING FOR CRASH DEBUGGING`, while recorder types and UI wiring remain present.

---

## C. 96 PPQN timing, swing and song scheduling

### GP-016 — 96 PPQN event scheduling is functionally broken

Severity: **P0**  
Confidence: **Confirmed by source**

`generateAudioBuffer()` increments `currentTick_` at 96 PPQN, but calls `advanceTick()` only when `currentTick_ % 24 == 0`. `advanceTick()` then evaluates events whose target is `nominalTick + swingDelay + step.timing`.

Most non-zero swing or microtiming offsets are not multiples of 24, so the equality is never evaluated at their target tick and the events are dropped rather than delayed.

Impact:

- swing can make notes disappear;
- per-step timing values can make notes disappear;
- the project reports 96 PPQN without actually dispatching events at 96 PPQN resolution.

Required correction: evaluate scheduled events on every PPQN tick, while advancing the logical 16-step playhead only at 24-tick boundaries.

### GP-017 — Applying FEEL can reset fractional clock phase during playback

Severity: **P1**  
Confidence: **Confirmed by source**

The FEEL application path resets `tickPhaseAccum_`. Changing feel while playing can create a phase discontinuity instead of a boundary-safe update.

### GP-018 — Song pause sentinel `-2` cannot survive the public song API

Severity: **P1**  
Confidence: **Confirmed by source**

Playback contains logic for a `-2` rehearsal/pause pattern value. `setSongPattern()` and song getters pass values through `clampSongPatternIndex()`, which maps values below `-1` to `-1`.

Impact: the pause state is indistinguishable from an empty track.

### GP-019 — Offline song render ignores multi-bar pattern length

Severity: **P1**  
Confidence: **Confirmed by source; function appears currently unreferenced by UI**

`renderProjectToWav()` uses `songLength() * 16` steps. It does not multiply by `feel.patternBars`, so 2/4/8-bar pattern cycles render short.

### GP-020 — Offline render can race the hardware audio task

Severity: **P1**  
Confidence: **Confirmed by architecture; latent until function is called on device**

`renderProjectToWav()` invokes `generateAudioBuffer()` synchronously on the caller while the permanent FreeRTOS audio task still owns and renders the same `MiniAcid`. Toggling `playing` is not task suspension or ownership transfer.

### GP-021 — Changing BPM writes timing and delay state cross-core

Severity: **P1**  
Confidence: **Confirmed by source**

UI calls `setBpm()`, which changes clock increment and both delay instances while the audio task reads/processes them.

---

## D. Real-time ownership and concurrency

### GP-022 — `AudioGuard` is a no-op around shared mutable DSP

Severity: **P0**  
Confidence: **Confirmed by source**

The audio task renders `MiniAcid` on core 1. Arduino `loop()` and UI pages directly mutate transport, patterns, mutes, engine objects, scenes, paging state and FX. The installed guard has empty lock/unlock functions.

### GP-023 — Synth switching allocates and destroys voices concurrently with rendering

Severity: **P0**  
Confidence: **Confirmed by source**

`setSynthEngine()` can call `SwappableSynthVoice::setEngineType()` or `setState()`. These allocate `unique_ptr` engines, replace `current_`, reset `next_`, and eventually destroy the previous voice. `process()` concurrently dereferences and moves the same pointers.

Impact: use-after-free, corrupted crossfade state, heap races, aborts and rare audio crashes.

### GP-024 — Drum-engine automation can allocate in the audio task

Severity: **P0**  
Confidence: **Confirmed by source**

A drum automation lane can switch the drum engine from the sequencer path. `setDrumEngine()` constructs and replaces heap-owned drum synth objects.

Impact: unbounded allocator/destructor work in the real-time task and possible heap failure.

### GP-025 — Performance telemetry uses a non-atomic pseudo-seqlock

Severity: **P1**  
Confidence: **Confirmed by source**

The odd/even `seq` protocol is conceptually a seqlock, but `seq` and payload fields are `volatile`, not atomic. There are no release/acquire fences around publication and reading.

Impact: C++ data races and potentially torn or reordered telemetry snapshots.

### GP-026 — I2S failures do not update `audioUnderruns`

Severity: **P1**  
Confidence: **Confirmed by source**

The audio task logs I2S timeout/error but does not increment the counter used by the adaptive FX safety logic.

Impact: diagnostics and automatic overload mitigation do not observe the most direct output failure.

### GP-027 — Waveform double buffer does not protect the reader lifetime

Severity: **P1**  
Confidence: **Confirmed by source**

The audio task publishes an index, then alternates two buffers. UI receives a reference but does not pin or copy the selected generation. After two audio blocks, the referenced buffer can be rewritten while drawing.

### GP-028 — LED event payload is raced behind an atomic flag

Severity: **P1**  
Confidence: **Confirmed by source**

`ledPulsePending_` is atomic, but `ledPulse_` is not. Audio can update brightness/duration while UI consumes the struct.

### GP-029 — Global engine construction can access SD before board setup

Severity: **P1**  
Confidence: **High static confidence; boot confirmation required**

A global static `MiniAcid` is constructed before `setup()`. Its `SceneManager`/paging construction path can call `PatternPagingService::ensureDirectory()`, touching global SD before normal board/SD initialization.

Impact: static-initialization-order dependence, silent directory failure or pre-setup crash.

### GP-030 — UI page churn allocates and frees large object graphs during playback

Severity: **P1**  
Confidence: **Confirmed by source**

Lazy page loading aggressively destroys and recreates pages based on free DRAM. This occurs while the audio task is running and shares the same heap.

Impact: fragmentation, allocation latency and failure exactly during navigation stress.

### GP-031 — Cross-core scalar reads are not snapshots

Severity: **P2**  
Confidence: **Confirmed by source**

UI reads `currentTick_`, step state, song state, CPU values and other mutable scalars without an atomic snapshot contract.

---

## E. Scene persistence and data integrity

The active Cardputer path uses the streaming writer/parser because the ArduinoJson fallback is disabled for DRAM reasons. Therefore the streaming format is the release-critical format.

### GP-032 — Drum velocity and timing are not saved

Severity: **P0**  
Confidence: **Confirmed by source**

The streaming writer stores hit, accent, probability and FX arrays, but omits each drum step's `velocity` and `timing`.

Impact: Save → Load changes the groove and dynamics.

### GP-033 — Synth ghost, velocity and timing are not saved

Severity: **P0**  
Confidence: **Confirmed by source**

The streaming writer stores note, slide, accent, probability and FX, but omits `ghost`, `velocity` and `timing`.

### GP-034 — Global swing percentage and swing mask are not saved

Severity: **P0**  
Confidence: **Confirmed by source**

The FEEL object saves grid, timebase, bars and texture flags, but not `swingPct` or `swingMask`.

### GP-035 — Generator parameters are omitted by the active streaming writer

Severity: **P1**  
Confidence: **Confirmed by source**

The ArduinoJson document builder serializes `generatorParams`; the streaming writer does not. The two serializers therefore do not round-trip the same scene.

### GP-036 — Active song slot is not serialized

Severity: **P1**  
Confidence: **Confirmed by source search**

`activeSongSlot` exists and is used by the editor/playback APIs, but is not written by the scene serializers.

### GP-037 — Clearing a synth pattern leaves stale metadata

Severity: **P1**  
Confidence: **Confirmed by source**

The clear helper resets note/accent/slide but does not reset ghost, velocity or timing.

Impact: a visually cleared pattern can retain dynamics/timing that reappear when notes are added.

### GP-038 — Static temporary load scene can retain omitted optional fields

Severity: **P1**  
Confidence: **Confirmed by source**

The streaming loader reuses static `s_tempLoadScene`. `clearSceneData()` does not reset every optional field, including several genre, vocal, sampler, active-slot and other state categories.

Impact: loading an older/sparse scene after a richer scene can inherit values from the previous load instead of documented defaults.

### GP-039 — JSON string escaping is incomplete

Severity: **P1**  
Confidence: **Confirmed by source**

The streaming writer escapes quote and backslash but not newline, carriage return, tab or other JSON control bytes.

Impact: a custom phrase can make the saved scene invalid JSON.

### GP-040 — Save/create APIs can report success after storage failure

Severity: **P0**  
Confidence: **Confirmed by source**

`saveSceneAs()` and related application methods do not reliably propagate storage write failure and can return success unconditionally.

Impact: UI tells the user a project was saved when it was not.

### GP-041 — Scene replacement is not power-loss safe

Severity: **P0**  
Confidence: **Confirmed by source**

The storage path removes the previous file before writing the replacement, updates current-scene metadata separately, and verifies mainly by file size. There is no temp-file + fsync/close + parse/CRC + atomic rename protocol.

Impact: SD error or power loss can destroy the last known-good scene and leave the current-scene pointer inconsistent.

### GP-042 — Scene names allow path separators and traversal tokens

Severity: **P0**  
Confidence: **Confirmed by source**

Name normalization does not robustly reject `/`, `\` or `..` path components before constructing SD paths.

Impact: unintended file overwrite/removal outside the scene directory.

### GP-043 — Two scene serializers encode different schemas

Severity: **P1**  
Confidence: **Confirmed by source**

The ArduinoJson builder and streaming writer differ in field coverage and placement. Only one is practical on constrained hardware, but both remain authoritative-looking implementations.

### GP-044 — Track volume setter accepts unbounded values

Severity: **P2**  
Confidence: **Confirmed by source**

`setTrackVolume()` stores arbitrary float values. Corrupt JSON or a UI bug can inject excessive or negative gain.

---

## F. Pattern paging

### GP-045 — Page files are raw C++ struct dumps with no schema header

Severity: **P0**  
Confidence: **Confirmed by source**

Paging writes arrays of structs directly to files. There is no magic, version, declared size, checksum, endianness or migration policy.

Impact: compiler padding or struct evolution silently changes the file format.

### GP-046 — A page save is split across three non-transactional files

Severity: **P0**  
Confidence: **Confirmed by source**

Drums, synth A and synth B are saved separately. Interruption can produce a page assembled from different generations.

### GP-047 — Failed page load mutates the active scene anyway

Severity: **P0**  
Confidence: **Confirmed by source**

Missing/corrupt components are zeroed independently. Callers set the target page even when load returns false.

Impact: navigation can destructively replace current data with partial/zero data.

### GP-048 — `memset(0)` does not create an empty synth pattern

Severity: **P0**  
Confidence: **Confirmed by source**

An empty synth step uses note `-1` and meaningful defaults for velocity/probability. Zeroing creates note 0 with zeroed metadata, not a valid clean rest pattern.

### GP-049 — `SceneManager::setPage()` ignores save/load results and weakly validates page index

Severity: **P1**  
Confidence: **Confirmed by source**

The old page can fail to save, the new page can fail to load, and state still advances. Negative values are checked in some paths, but the complete valid page range is not enforced consistently.

### GP-050 — UI paging duplicates service logic and repeats the same failure policy

Severity: **P1**  
Confidence: **Confirmed by source**

`MiniAcidDisplay::handlePaging_()` saves/loads directly, ignores save failure and sets the current page on load failure.

### GP-051 — Only 12 of 16 pattern pages are reachable through global shortcuts

Severity: **P1**  
Confidence: **Confirmed by source**

Pattern paging defines 16 pages. Alt+[ / Alt+] wraps using UI `kPageCount = 12`, so pages 12–15 are unreachable through that path.

### GP-052 — Global “first free pattern” search repeatedly inspects local/current patterns

Severity: **P1**  
Confidence: **Confirmed by source**

The search loops global pattern IDs but calls accessors that clamp to local 0–7 indices in the current bank/page.

Impact: MIDI import destination selection can overwrite or reject the wrong pattern.

---

## G. MIDI importer and routing

### GP-053 — Partial import is cached even after parse failure

Severity: **P1**  
Confidence: **Confirmed by source**

`importFile()` can persist the modified page after parsing has already returned an error; page-save success is also not reliably propagated.

### GP-054 — Overwrite mode clears all following patterns before import length is known

Severity: **P0**  
Confidence: **Confirmed by source**

On the first routed note, overwrite logic clears from the target start through the maximum pattern range rather than only the patterns that the source will occupy.

Impact: importing one MIDI file can erase unrelated later patterns.

### GP-055 — Type-1 MIDI normalization depends on file track order

Severity: **P1**  
Confidence: **Confirmed by source**

Each track resets its absolute tick. The global first routed step is initialized by the first encountered routed event, not the earliest event across all tracks. A later track containing an earlier musical event can normalize negative and be dropped.

### GP-056 — Channel Pressure parsing desynchronizes the scanner

Severity: **P1**  
Confidence: **Confirmed by source**

The scan path recognizes status `0xD0` but does not consume its one data byte.

Impact: subsequent bytes are interpreted as statuses/events incorrectly.

### GP-057 — Four-byte VLQ with continuation is accepted

Severity: **P2**  
Confidence: **Confirmed by source**

`readVarLen()` stops after four bytes without rejecting a continuation bit in the fourth byte.

### GP-058 — Import metrics count events that were not imported

Severity: **P2**  
Confidence: **Confirmed by source**

`notesImported` can increment for unmapped drum notes or cases where overwrite policy leaves the destination unchanged.

### GP-059 — Track-name auto-routing guesses channel before note evidence

Severity: **P2**  
Confidence: **Confirmed by source**

For Type-1 files, track-name routing can associate names with a guessed channel based on traversal order rather than actual channel events.

### GP-060 — Import intentionally discards note duration and tempo-map semantics

Severity: **P2 / documented limitation**  
Confidence: **Confirmed by source**

Note-off, duration, tempo changes and time signatures are not represented in the step import model. This must be explicit in UI/docs rather than implied as general MIDI import.

---

## H. Sampler and WAV loading

### GP-061 — Sampler preload can loop forever when no sample is evictable

Severity: **P0**  
Confidence: **Confirmed by source**

While over capacity, `preload()` repeatedly calls `evictLRU()`. If all loaded slots have non-zero references, eviction makes no progress and the loop termination condition does not detect it.

Impact: main/UI task hang and watchdog reset.

### GP-062 — Sample eviction races audio acquisition and can free in-use PCM

Severity: **P0**  
Confidence: **Confirmed by source**

`evictLRU()` observes `refCount == 0`, then later clears/frees without an atomic claim/recheck. Audio can acquire the slot between those operations.

Impact: use-after-free in the audio task.

### GP-063 — Default reverse sample playback starts at frame zero

Severity: **P1**  
Confidence: **Confirmed by source**

When reverse is enabled and `endFrame == 0` means “use sample end”, `SamplerVoice::trigger()` initializes position to 0 because total frame count is not consulted until `process()`.

Impact: reverse playback ends immediately or plays incorrectly unless an explicit end frame is configured.

### GP-064 — RIFF chunk padding is ignored

Severity: **P1**  
Confidence: **Confirmed by source**

WAV chunks are word-aligned, but unknown/fmt chunk skipping does not add padding for odd chunk sizes.

Impact: valid WAV files with odd-sized metadata chunks can fail or parse garbage.

### GP-065 — Stereo fallback undercounts pool memory

Severity: **P1**  
Confidence: **Confirmed by source**

If stereo-to-mono shrink allocation fails, the original stereo-sized allocation is retained, but pool accounting uses mono frame bytes.

Impact: actual memory use can exceed the configured pool while accounting reports room.

### GP-066 — WAV loader accepts channel counts it cannot render correctly

Severity: **P1**  
Confidence: **Confirmed by source**

Only stereo is mixed down. Files with more than two channels are accepted as PCM16, but channel count is not retained in the playback view and samples are interpreted as mono frames.

### GP-067 — Sampler pad accessors do not bounds-check

Severity: **P2**  
Confidence: **Confirmed by source**

`DrumSamplerTrack::pad(int)` indexes the array directly. Current UI may clamp, but malformed state or future callers can access out of bounds.

---

## I. UI, display and diagnostics

### GP-068 — Splash path starts a display transaction and returns without ending it

Severity: **P1**  
Confidence: **Confirmed by source**

`MiniAcidDisplay::update()` calls `gfx_.startWrite()`. While the splash remains active it calls `flush()` and returns before `gfx_.endWrite()`.

Impact: unbalanced SPI/display transactions during boot.

### GP-069 — Display pushes the full 320×240 frame approximately 25 times per second

Severity: **P2**  
Confidence: **Confirmed by source**

Every UI update redraws and `pushImage()` transfers the complete RGB565 frame, even when little or nothing changed.

Impact: unnecessary SPI traffic, CPU load and contention with audio/SD; violates the intended partial-update performance rule.

### GP-070 — `drawRect()` can write out of bounds for fully off-screen rectangles

Severity: **P1**  
Confidence: **Confirmed by source; caller reachability requires test**

The function clamps maximum coordinates but can still directly index with invalid `x0/y0` when a rectangle lies entirely outside the frame.

### GP-071 — Debug CPU line draws stale/unformatted text

Severity: **P2**  
Confidence: **Confirmed by source**

The debug overlay reads CPU values but draws `buf` without formatting a CPU string after the DRAM line.

### GP-072 — Page labels and routing comments have drifted

Severity: **P2**  
Confidence: **Confirmed by source**

For example, a global shortcut logs “Groove Lab” for page 11 while page 11 constructs `ModePage`. This is a symptom of UI navigation being maintained in several places.

### GP-073 — Full UI object destruction can invalidate retained raw pointers

Severity: **P2**  
Confidence: **Risk confirmed by ownership model; concrete dangling caller requires dynamic test**

Pages are aggressively deleted while containers and dialogs use raw/non-owning component pointers in several interaction paths. A focused hardware/ASan host test is required.

---

# Priority order

The correct recovery order is now:

1. Disable GPIO21 LED writes and declare a single Cardputer ADV pin map.
2. Add characterization tests that expose the 96 PPQN dispatch failure, then fix dispatch on every PPQN tick.
3. Stop all direct cross-core engine mutation; introduce a bounded command/snapshot boundary.
4. Remove engine allocation/destruction from the audio task.
5. Make scene save/load round-trip every musical field and report storage errors truthfully.
6. Replace raw page dumps with a versioned, checksummed, transactional format.
7. Fix sampler eviction ownership before enabling sample-heavy workflows.
8. Repair MIDI importer destruction/parser bugs.
9. Establish one pinned build and compile-only CI.
10. Only then extract output-neutral scheduled events and add MIDI output.

# Mandatory reproduction tests

## Timing test

Create a one-step drum pattern with:

- step 1 hit enabled;
- `timing = 1` tick;
- no probability/ghost randomness.

Expected after fix: exactly one event at PPQN tick 1 relative to the nominal step. Current source behavior: the event is never evaluated at tick 1.

Repeat with swing delays from 1 to 23 ticks and assert no event is lost.

## Scene round-trip test

Populate non-default values for every drum/synth step field, FEEL swing/mask, generator parameters, active song slot, sampler pads, vocal values and both songs. Save through the Cardputer streaming writer, load through the evented parser and compare field-by-field.

Current source is expected to fail on the omitted fields listed above.

## Paging fault-injection test

Simulate:

- missing synth-B file;
- truncated drum file;
- write failure after the first component;
- power loss before commit.

The active page must remain unchanged on every failure. Current source mutates/zeroes partial state.

## Concurrency stress test

While playing, rapidly switch synth/drum engines, edit patterns, change BPM, change pages, load scenes and trigger LED events. Instrument command ownership, heap operations and audio deadlines. No direct mutation from UI is acceptable after recovery.

## Sampler ownership test

Fill the pool, hold handles for every slot, then request another preload. It must return a bounded failure rather than loop. Race acquire against eviction under ThreadSanitizer/host instrumentation or an equivalent deterministic interleaving test.

# Audit limitations

- No successful clean-room Arduino compile was claimed. The audit environment could inspect GitHub through the connector but could not clone dependencies through the local container network.
- No physical Cardputer ADV was available to measure codec startup, GPIO21 electrical behavior, DMA latency, stack margins or audible artifacts.
- Static findings marked “requires test” must be validated, but they should not be ignored: the conflicting code paths are already present.
- This audit intentionally does not recommend a framework rewrite. The fixes should remain small, ordered and test-driven.
