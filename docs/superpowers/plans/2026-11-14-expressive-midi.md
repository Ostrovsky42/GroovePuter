# Expressive MIDI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete 0.9.32 by making MIDI expression route-aware, rate-bounded and host-verifiable through the existing event, controller and note-ownership systems.

**Architecture:** Keep the four macros on the current transient Pattern/Phrase event path and keep controller changes on `CutoffVoice` → MIDI dispatcher → `UsbMidiOutput`. Correct the synth MIDI screen from the applied profile route projection, make the dispatcher’s 20 ms limiter host-testable and wrap-safe, and extend fake transport/UI tests. No persisted state or new transport owner is introduced.

**Tech Stack:** C++17 embedded/host code, Arduino Cardputer ADV firmware, SDL UI, existing g++ host tests and fake `IMidiTransport`.

**Spec:** `docs/superpowers/specs/2026-11-14-expressive-midi-design.md`; roadmap requirements in the user-provided GroovePuter 0.9.25–0.9.35 implementation goal, §0.9.32.

## Global Constraints

- P0 includes velocity, note length, accent, gate, channel-aware expression and safe permitted CC.
- P1 includes bounded CC automation, expression curves, smoothing, rate limiting, deduplication and safe recovery.
- P2 includes Pitch Bend and aftertouch only when the route and receiver support are confirmed; otherwise defer.
- Real-time paths use no dynamic allocation, file I/O, blocking operations, USB waits or long computation in the audio callback.
- Device profile does not own or rewrite musical material; use the existing MIDI output and note ownership owners.
- Hardware status for this milestone is `NOT TESTED`; no Cardputer, SEQTRAK, sound or physical-keyboard claim is allowed.
- Existing project schema remains backward-compatible; this milestone adds no persisted field.

## Review Focus

- **Profile route mismatch:** SEQTRAK, GM, Generic and Custom fallback must show and emit the same channel; pin with UI label and fake transport assertions.
- **First send after long uptime:** an unattempted CC must be allowed even if `millis()` is above the signed half-range; pin `hasAttempted == false` behavior.
- **32-bit timer wrap:** a deadline that wraps must remain blocked until the exact 20 ms boundary; pin values immediately before and at it.
- **Endpoint/backpressure recovery:** refused CCs must remain retryable, delivered values dedupe, and reconnect invalidates receiver knowledge; retain and run existing fake transport regressions.
- **Shortened macro gate:** transformed velocity/duration must reach Pattern MIDI and the same note owner must issue its Note Off at the transformed deadline; pin in the existing P0 Pattern integration fixture.

---

### Task 1: Test and harden the CC pacing deadline

**Files:**
- Modify: `src/midi/cutoff_sweep.h`
- Modify: `src/platform/cardputer_usb_midi_transport.cpp`
- Test: `tests/test_cutoff_sweep.cpp`

**Interfaces:**
- Produces: `constexpr uint32_t GroovePuterMidi::kCutoffSendIntervalMs = 20` and `bool cutoffSendDeadlineReached(bool hasAttempted, uint32_t nowMs, uint32_t nextAttemptMs)`.
- The first call is allowed when `hasAttempted == false`; later calls use wrap-safe signed deadline comparison. Existing per-controller deadline storage remains fixed. A two-byte per-synth bit mask tracks which controller deadlines have been attempted.

- [ ] **Step 1: Add failing deadline tests** for immediate first attempt, not-ready immediately before deadline, ready exactly at deadline, immediate first attempt at a large uptime, and timer wrap (`attempt=UINT32_MAX-10`, deadline `9`, blocked at `8`, ready at `9`).
- [ ] **Step 2: Run the cutoff test and verify it fails** because the helper is not defined.

Run: `g++ -std=c++17 -Wall -Wextra -Werror -I. tests/test_cutoff_sweep.cpp -o build/host-tests/test_cutoff_sweep`

Expected: compilation fails on the missing `cutoffSendDeadlineReached` helper.

- [ ] **Step 3: Implement the deadline helper** in `cutoff_sweep.h`, and replace the dispatcher’s raw 20 ms comparison with it. Mark a controller attempted after each send attempt, including a refused attempt, and set its next deadline to `nowMs + kCutoffSendIntervalMs`.
- [ ] **Step 4: Run cutoff and CC transport tests.**

Run: `build/host-tests/test_cutoff_sweep`

Run: `g++ -std=c++17 -Wall -Wextra -Werror -I. tests/test_usb_midi_cutoff.cpp src/midi/usb_midi_output.cpp -o build/host-tests/test_usb_midi_cutoff`

Run: `build/host-tests/test_usb_midi_cutoff`

Expected: both exit 0; existing sweep, allowlist, dedupe, refusal and reconnect assertions remain green.

- [ ] **Step 5: Commit** as `fix: make cutoff CC pacing wrap safe`.

---

### Task 2: Display the applied Performance MIDI route

**Files:**
- Modify: `src/ui/pages/synth_midi_page.cpp`
- Modify: `src/ui/pages/synth_midi_page.h`
- Modify: `tests/test_midi_perform_ui_polish.cpp`
- Modify: `tests/run_midi_perform_ui_polish_tests.sh` (link the route-settings implementation used by the new profile test)

**Interfaces:**
- Consumes: `midiDeviceProfileRuntime().settings()` and `projectMidiPerformanceRoutes()`.
- Produces: a route label for the selected synth: `SEQTRAK CH08/09`, `GM CH01/02`, `GENERIC CH01/02`, and `CUSTOM CH08/09` (the existing Cardputer fallback when Custom has no full Performance table).
- Profile labels remain short enough for the 240×135 layout. Display only; profile remains next-boot state and is not rebound by this page.

- [ ] **Step 1: Add failing UI assertions** that construct `SynthMidiPage`, draw it with `RecordingGfx`, and check each profile's Synth A and Synth B route text plus `bodyFits()`.
- [ ] **Step 2: Run the focused UI test and verify it fails** on Generic/GM because the page currently prints the SEQTRAK route.

Run: `bash tests/run_midi_perform_ui_polish_tests.sh`

Expected: assertion failure for the Generic/GM channel label.

- [ ] **Step 3: Derive the route label from the applied profile** and `projectMidiPerformanceRoutes()`. For the incomplete Custom profile use the existing fallback channel constants (`kSynthAChannel`, `kSynthBChannel`) and label it `CUSTOM`.
- [ ] **Step 4: Run the focused UI test** and confirm all labels fit.

Run: `bash tests/run_midi_perform_ui_polish_tests.sh`

Expected: `MIDI/PERFORM UI polish gate: PASS`.

- [ ] **Step 5: Commit** as `fix: show active MIDI profile channels`.

---

### Task 3: Prove CC routing follows the selected profile

**Files:**
- Modify: `tests/test_midi_performance_route_binding_0_9_7.cpp`
- Modify: `src/input/musical_event.h`

**Interfaces:**
- Reuse: existing profile startup route publication and `UsbMidiOutput::handleMusicalEvent()`.
- Clarify: `MusicalEventType::ControlChange` accepts only the seven user-selected controllers in `cutoff_sweep.h`; unsupported controllers are discarded. The comment must not claim CC74 is the only permitted control.

- [ ] **Step 1: Add fake transport characterization traces** for CC74 on Synth A and Synth B under SEQTRAK, General MIDI, Generic MIDI and Custom fallback, asserting profile channels (7/8 or 0/1) and rejection of unsupported CC5. The Custom fixture also verifies its explicitly supplied fallback channels.
- [ ] **Step 2: Run the profile route suite** to characterize current route behavior before changing documentation.

Run: `bash tests/run_midi_performance_route_binding_0_9_7_tests.sh`

Expected: the new traces pass on current route code; they close missing coverage for the existing contract.

- [ ] **Step 3: Correct the stale `MusicalEvent` comment** to name the allowlisted user-selected controls instead of claiming CC74 is the only permitted control. Do not add output routing code if current `generatedChannel()` satisfies the characterization traces.
- [ ] **Step 4: Rerun the profile route and cutoff suites.**

Run: `bash tests/run_midi_performance_route_binding_0_9_7_tests.sh`

Run: `build/host-tests/test_usb_midi_cutoff`

Expected: both pass; the allowed CC set and profile channel behavior are explicit.

- [ ] **Step 5: Commit** as `test: cover expressive CC profile routing`.

---

### Task 4: Characterize the macro gate through the Pattern MIDI Note Off

**Files:**
- Modify: `tests/test_pattern_phrase_p0_runtime.cpp`

**Interfaces:**
- Reuse: `RuntimeFixture`, `testPatternMacroTransformsSharedEvent`, `runtimeDeadline()`, and the fixture's recording USB transport.
- Expected behavior: transformed Note On velocity matches the macro candidate; playback deadline equals onset subtick plus transformed `durationSubticks`; advancing to that deadline emits exactly one matching Note Off.

- [ ] **Step 1: Extend the existing Pattern macro integration test** to assert the transformed runtime deadline and Note Off at the shortened gate boundary.
- [ ] **Step 2: Run the focused P0 compatibility script** to characterize whether the current shared event path preserves the transformed duration through Note Off.

Run: `bash tests/run_pattern_phrase_p2_p0_compatibility.sh`

Expected: the new assertions pass if the existing shared event path is wired as documented; a failure is a P0 defect requiring a narrow production correction before continuing.

- [ ] **Step 3: If a P0 assertion fails, make only the narrow production correction required.** Preserve `RuntimeSynthPlaybackState` as the only owner and keep the source phrase unchanged. If behavior passes, make no production edit.
- [ ] **Step 4: Rerun the P0 compatibility script** and confirm GCC, Clang, ASan and UBSan gates pass.

Run: `bash tests/run_pattern_phrase_p2_p0_compatibility.sh`

Expected: all compatibility and sanitizer variants pass.

- [ ] **Step 5: Commit** as `test: verify macro MIDI gate release` (omit a separate commit if this test change is folded into Task 3 only when files overlap; otherwise commit independently).

---

### Task 5: Record 0.9.32 contracts and resource evidence

**Files:**
- Create: `docs/implementation/0.9.25-0.9.35/EXPRESSIVE_MIDI.md`
- Modify: `docs/implementation/0.9.25-0.9.35/ARCHITECTURE_DECISIONS.md`
- Modify: `docs/implementation/0.9.25-0.9.35/DEFERRED_ITEMS.md`
- Modify: `docs/implementation/0.9.25-0.9.35/RESOURCE_BUDGET.md`
- Modify: `docs/implementation/0.9.25-0.9.35/VERSION_LOG.md`
- Modify: `tests/run_rhythm_browser_engine_tests.sh` and `tests/run_drum_ctrl_backspace_tests.sh` (link the existing MIDI settings implementation required by the profile-aware synth MIDI page)

**Interfaces:**
- Record exactly which existing macro, gate, CC whitelist, profile route, dedupe, retry, reconnect and throttle behaviors were tested.
- Record pitch bend, aftertouch and motion/gesture input as deferred unless confirmed route support is found.
- The implementation/result commit SHA is written to `VERSION_LOG.md` only after the code commit exists.

- [ ] **Step 1: Run targeted final checks** for cutoff pacing, CC routing, UI labels and Pattern macro Note Off, then run `git diff --check`.
- [ ] **Step 2: Run the complete canonical host suite** with an absolute temporary directory.

Run: `mkdir -p /tmp/grooveputer-0932-host-tmp`

Run: `env TMPDIR=/tmp/grooveputer-0932-host-tmp bash tests/run_host_tests.sh`

Expected: exit 0; capture the exact final exit code and any known pre-existing failures.

- [ ] **Step 3: Build SDL and both firmware profiles** using existing project targets; record exact command/results and binary/DRAM deltas versus the 0.9.31 records.

Run: `make -C platform_sdl clean all CXX=g++`

Run: `bash scripts/build.sh --warnings all`

Run: `env TMPDIR=/tmp bash scripts/build_cardputer_dynbuffers.sh --warnings all`

Expected: all commands exit 0; the FS1B map contains candidate FatFs and excludes stock FatFs; no new static DRAM gate exception is introduced.

- [ ] **Step 4: Write the 0.9.32 report** with passed, failed and not-run tests separated; record `Hardware status: NOT TESTED` and `Decision: GO / NO-GO / DEFER` based only on evidence.
- [ ] **Step 5: Commit implementation and evidence** in separate local commits; then add the exact result SHA and commit the version log.

---

## Self-review

- **Spec coverage:** P0 velocity/accent and duration/gate are covered through the shared Pattern MIDI integration; channel-aware expression is covered through profile-routed CC traces; P1 automation, limiting, deduplication and recovery are covered by existing and extended cutoff tests. Pitch Bend, aftertouch and motion are explicitly P2/deferred. No storage, MIDI ownership or transport clock contract changes.
- **Input coverage:** profile-specific channel selection; unsupported CC; initial pacing after long uptime; wraparound deadline; endpoint refusal/reconnect; macro-shortened gate release are each assigned to a test step above.
- **Type consistency:** `cutoffSendDeadlineReached(bool, uint32_t, uint32_t)` and `kCutoffSendIntervalMs` are declared once in `cutoff_sweep.h` and consumed by the dispatcher. UI routing uses `MidiPerformanceRouteProjection` and the existing custom fallback constants.
- **Proportion:** only four production paths are touched if tests show a need: the dispatcher deadline check, the existing synth MIDI route label, the stale CC comment and the shared event test seam. No second expression engine or schema migration is planned.
