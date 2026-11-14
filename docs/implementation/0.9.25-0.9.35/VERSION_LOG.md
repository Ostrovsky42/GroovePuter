# Milestone log

## Version 0.9.25 — Foundation Audit & Contract Freeze

**Version:** 0.9.25  
**Base commit:** `28918d9b78e9a527ce2b5ce201a465af5126f76a`  
**Result commit:** `793fdbcf110a85ecf73fdfbf2dac7818b3128652`  
**Goal:** Verify and record the actual `feature/0.9.24` baseline, freeze existing ownership/storage contracts, and establish repeatable host, SDL and firmware evidence.  
**Implemented:** Baseline, architecture, MIDI, resource, test and user journey audit documents; SDL screenshots; corrected stale 0.9.24 UK Garage press-alternation claims. No production behavior changed.  
**Not implemented:** 0.9.26 routing/lifecycle additions and all later roadmap features. Live MIDI route rebinding remains deferred.  
**Tests added:** None. Existing test suite and scripts were reused; only test comments were corrected to match verified H2 behavior.  
**Tests passed:** Full local `bash tests/run_host_tests.sh` with `TMPDIR=build/tmp-host-suite` (exit 0); local SDL build and limited keyboard-driven UI scenario; exact-baseline local FS1B firmware build, link-map proof and DRAM gate; `git diff --check`; exact-head remote Cardputer ADV, MIDI Targets and Memory workflows.  
**Tests failed / pending:** Two earlier local host attempts failed (runner shell parse error during a concurrent script edit; later transient `ENOSPC` compiling an SDL-linked target); the stable rerun passed. The separate remote Core workflow host job remained `in_progress` at the latest check.  
**Memory delta:** 0 B source delta from baseline. Local fixed global DRAM 187,612 B / 191,488 B; 3,876 B headroom, same as exact-head CI.  
**Firmware size delta:** No firmware code change. Local FS1B program-storage report 1,742,066 B; exact-head CI reported 1,742,082 B (16 B difference). Local app-only BIN 1,742,224 B.  
**Known risks:** Provisional DRAM exception with only 3,876 B headroom; unfinished release-profile build fingerprint; route lifecycle test gaps; user-facing documentation is stale; no physical audio/MIDI acceptance.  
**Hardware status:** `NOT TESTED`  
**Decision:** `GO`

## Version 0.9.26 — MIDI Routing & Note Safety

**Version:** 0.9.26
**Base commit:** `ef5b19a96ed665c8ca33a984b86c3aad9a011624`
**Result commit:** `197ff2a8a92cecc886f019a9dc206dcd59c446b8`
**Goal:** Prevent locally forgotten USB MIDI notes from remaining stuck at a receiver after the whole MIDI transport disconnects and reconnects.
**Implemented:** On the overall mounted-to-unmounted transition, capture the active ownership table's channel mask before clearing local note state. On reconnect, send the existing bounded CC123 channel cleanup for those channels. Added a fake transport regression covering a PatternPlayer note on channel 7 and proving unrelated channels receive no cleanup. Runtime route snapshots remain frozen after startup; profile changes do not silently rebind active lanes.
**Not implemented:** Per-endpoint Tee loss/rejoin traces, integrated dispatcher overflow traces, and a live route-rebind transaction remain open. This change covers overall transport loss; it does not claim endpoint-specific USB cleanup when DIN keeps the Tee transport mounted.
**Tests added:** `tests/test_usb_midi_disconnect_cleanup.cpp`, registered in `tests/run_host_tests.sh`. The regression failed before the production fix and passed after it.
**Tests passed:** Targeted disconnect regression; complete `bash tests/run_host_tests.sh` (exit 0); Cardputer ADV FS1B build with `--warnings all`; FS1B map proof; `git diff --check`.
**Tests failed:** None in the final run.
**Memory delta:** Global DRAM 0 B versus baseline: 187,612 B / 191,488 B, 3,876 B headroom.
**Firmware size delta:** Arduino program report +64 B (1,742,130 B versus 1,742,066 B baseline); app-only BIN +64 B (1,742,288 B versus 1,742,224 B baseline). App BIN SHA-256: `9bc8336e836456b9a7cb24c2c1a08ec9f515ecb5e911859d7e048d4bc0fb26cb`.
**Known risks:** Recovery uses channel-level CC123 because the disconnected receiver's note state is unknown; another sender sharing that channel may also be silenced. Per-endpoint Tee loss/rejoin remains unaudited. Host and firmware build evidence do not establish device behavior.
**Hardware status:** `NOT TESTED`
**Decision:** `GO`

## Version 0.9.27 — MIDI Device Profiles

**Version:** 0.9.27

**Base commit:** `a4bd5dc737ad54de40f8477034cdfe498c43d393`

**Result commit:** `545a749b41cb002b12f4d806858fce119281c207`

**Goal:** Make external MIDI enablement selectable independently from the external device profile, while keeping internal audio available.

**Implemented:** Added a Project → MIDI `External MIDI` setting with `INTERNAL` and `INT + MIDI` modes. It composes with the existing persisted settings without changing schema v2; both profile and output mode are next-boot settings. The output master gate is applied to the shared `UsbMidiOutput` owner and outbound transport lifecycle events, covering generated notes, Performance, SMF and clock paths. MIDI profile selection does not rewrite musical material. Added help text and SDL evidence for preview/save and profile independence.

**Not implemented:** Runtime route rebinding, immediate output-mode changes without reboot, and physical MIDI endpoint validation.

**Tests added:** Output-mode label and next-boot settings unit tests; output-destination source regressions; registered in `tests/run_host_tests.sh`.

**Tests passed:** Targeted output-destination runner; sequential MIDI device-profile/UI aggregate; SDL build; SDL keyboard scenario covering output-mode preview/save and profile independence; Cardputer ADV FS1B firmware build with `--warnings all` and link proof. Full `bash tests/run_host_tests.sh` ran through its final `test_ui_view_continuity_loop_tab: OK` marker; the terminal wrapper closed before its numeric exit code was captured.

**Tests failed / pending:** No failure marker appeared in the full host log. The wrapper's exact exit code was not preserved, so this evidence is recorded as completion through the final test marker rather than an asserted numeric exit code. SDL used dummy audio; no audio-quality claim. No hardware tests were run.

**Memory delta:** 0 B global DRAM versus baseline: 187,612 B / 191,488 B; 3,876 B headroom.

**Firmware size delta:** Arduino program-storage report +1,056 B versus baseline (1,743,122 B); app-only BIN +1,056 B (1,743,280 B). BIN SHA-256: `223c8858cd36c390a66e9bb03a40d3c6d08be4e61cd66552c4c784a84fd37daa`.

**Known risks:** Mode is applied at next boot because the MIDI route snapshot is frozen at startup. Host mocks and firmware compilation do not prove Cardputer USB behavior or sound. The full-suite terminal wrapper's exit code was not retained.

**Hardware status:** `NOT TESTED`

**Decision:** `GO`

## Version 0.9.29 — Performance Interaction & Small-Screen UX

**Version:** 0.9.29

**Base commit:** `efa5434562fa70303a75c5097a44f7d7e3911702`

**Result commit:** Pending local milestone commit.

**Goal:** Expose the four transient musical macros in Performance and apply them to Pattern/Phrase/Song playback through the existing playback and output owners.

**Implemented:** Added a seventh MACRO tab with TENSION, ENERGY, SPACE, MUTATION, per-value reset and RESET ALL. Values are session-only. `MiniAcid` publishes them as one packed 32-bit atomic snapshot and reads one coherent state per sequencer tick. A bounded per-event transform runs before the existing `RuntimeSynthPlaybackState`; internal synthesis and Pattern MIDI use the same transformed event. Pattern next-onset search is limited to 16 events and Phrase search to 128, only when TENSION is active. Added a mock-MIDI Pattern integration test, a Phrase runtime source-preservation test, seven-tab UI coverage and a corrected Song-row characterization: the existing `applySongPositionSelection()` hard barrier releases both internal and MIDI note ownership.

**Not implemented:** Macro commit/materialization, per-loop mutation, chord-aware TENSION, new MIDI CC control, or a musical-quality claim. Performance keyboard comfort and audio/MIDI behavior on Cardputer remain owner checks at 0.9.35.

**Tests added:** Expanded `tests/test_performance_macro_engine.cpp`, `tests/test_midi_perform_ui_polish.cpp`, `tests/test_pattern_phrase_p0_runtime.cpp` and `tests/test_p3_phrase_relative_onset.cpp`; updated source-regression expectations for the seventh tab and existing panic behavior.

**Tests passed:** `bash tests/run_performance_macro_engine_tests.sh`; `bash tests/run_midi_perform_ui_polish_tests.sh`; Performance Instrument, Looper and WaveMorph source regressions; `bash tests/run_pattern_phrase_p3_tests.sh`; full SDL app build; SDL keyboard smoke for macro values/reset, Play/Stop, Song row advance, page return, Scene Save/Load and 240×135 readability; default Cardputer ADV build and DRAM gate; FS1B build with candidate-only linker-map proof and DRAM gate; canonical host suite `env TMPDIR=/tmp/grooveputer-0929-host-tmp bash tests/run_host_tests.sh` (exit 0); `env TMPDIR=/tmp/grooveputer-0929-host-tmp bash tests/run_pattern_phrase_p2_p0_compatibility.sh` (GCC repeat, Clang parity, ASan and UBSan all PASS); target ELF disassembly confirms direct packed 32-bit load/store for macro snapshots; `git diff --check`.

**Tests failed / pending:** An early full-suite invocation used a relative `TMPDIR`; a generated-melody test changed working directory and `std::filesystem::temp_directory_path()` then failed. Rerunning the unchanged suite with an absolute temporary path passed (exit 0). The P0-C test initially expected an internal note to survive Song row selection; that expectation failed on both this worktree and clean 0.9.28. `applySongPositionSelection()` already calls `hardBarrierPatternPlayback_()`, so the test was corrected to assert synchronous internal and MIDI cleanup; the complete P0 GCC/Clang/ASan/UBSan matrix now passes. No mandatory 0.9.29 host checks remain pending. SDL load logs the known nonfatal top-level `out` array warning; a separate output persistence observer captures that field, and the Scene load reports success. SDL dummy audio is not sound validation.

**Memory delta:** Fixed global DRAM is unchanged from baseline: default profile 187,604 B; FS1B 187,612 B; FS1B gate headroom 3,876 B against 191,488 B. The 32-bit macro state is part of the existing global `MiniAcid` object; no global DRAM increase is reported. Stack usage was not instrumented.

**Firmware size delta:** Local FS1B program storage is 1,745,210 B versus 1,742,066 B exact-baseline local build (+3,144 B). App-only BIN is 1,745,360 B versus 1,742,224 B (+3,136 B). Current FS1B BIN SHA-256: `e561dcfe3d52e70725a0c93863e908f4b86f322ed4d8da8a6d357f07a1545845`. Default-profile build reports 1,720,738 B program storage and 187,604 B globals; it is compile evidence, not the SD-safe candidate profile.

**Known risks:** TENSION remains a local next-onset heuristic; MUTATION is deterministic per event and project key, not a per-loop generator. No physical keyboard, internal audio, USB MIDI or SEQTRAK validation has occurred. The baseline static-DRAM policy remains provisional with under 4 KiB headroom.

**Hardware status:** `NOT TESTED`

**Decision:** `GO` for the 0.9.29 engineering milestone. Hardware remains `NOT TESTED` by design.

## Version 0.9.28 — Musical Macros Engine

**Version:** 0.9.28

**Base commit:** `80b0e44eebe813dbe5cd436ae03f32aecd84e998`

**Result commit:** `6d90711f86630f9de189e907faabdf4e9272152b`

**Goal:** Define a deterministic, bounded, reversible macro projection that preserves saved musical material.

**Implemented:** Added the fixed four-byte `TENSION / ENERGY / SPACE / MUTATION` state and a transient transformation over fixed-capacity phrase events. Tension accents weak stepwise approaches; Energy scales velocity and increases ghost-event probability; Space shortens gates and attenuates ghost probability; Mutation applies seeded one-scale-step changes only to weak in-scale events when key context is known. Strong-beat anchors, event count, onsets and source phrase remain unchanged. Invalid input is rejected transactionally; unknown harmony skips pitch mutation. No Scene/storage/Undo owner or Commit action was added. The transformer uses caller-provided unpublished scratch and is control-side only.

**Not implemented:** Live Performance UI/playback integration (0.9.29), chord-aware tension (0.9.30), and permanent macro-to-phrase materialization.

**Tests added:** `tests/test_performance_macro_engine.cpp` and `tests/run_performance_macro_engine_tests.sh`, registered in `tests/run_host_tests.sh`.

**Tests passed:** Final targeted macro runner exit 0 after the scratch-buffer, alias-rejection and four-byte-state changes; full `bash tests/run_host_tests.sh` exit 0, recorded in `build/host-suite-0928.exit`; `git diff --check`.

**Tests failed / pending:** The initial test-first compile failed because the macro API did not exist yet, then passed after implementation. No final test failures. No SDL macro flow or firmware build was run because this domain engine is not yet connected to the application. No hardware or listening tests were run.

**Memory delta:** Macro state is 4 bytes. No new static firmware state or binary delta is claimed: the engine is not referenced by the live application yet. It adds no full-phrase buffer to its own stack; the future caller must account for its scratch/output owner.

**Firmware size delta:** Not measured for 0.9.28; no production firmware build was required before live integration. The last linked candidate evidence remains 0.9.27 and must not be represented as containing macros.

**Known risks:** Tension is a local stepwise-approach heuristic, not chord-aware dissonance. Mutation follows project key/scale rather than a chord-specific context. Data-level tests do not establish musical quality. Caller must prepare the inactive destination off the audio callback and publish it at a safe boundary.

**Hardware status:** `NOT TESTED`

**Decision:** `GO`
