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

**Result commit:** `f718fbb0e1d81d3353741a86643b3c321d617e13` (local implementation commit; this log update follows it).

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

## Version 0.9.30 — Harmonic Follow

**Version:** 0.9.30

**Base commit:** `bdc439b14167313a22ee810a9b37b02aabd44a17`

**Result commit:** `7e39d1a9c600d8d54f85c16000fca4fc444f991a`

**Goal:** Verify that generated and arranged material follows existing harmonic context, keeps chord changes aligned to harmonic rhythm, and fails closed when the route is invalid or stale.

**Implemented:** Audited and verified the existing `SongHarmonicState` / `PreparedGraphHarmony` route rather than adding a second harmony model. `HarmonicRhythmPlan` owns event timing; Graph route and role pitch policies constrain candidate notes; projection is staged before transactional commit. Playback snapshots validate exact Song participants and Pattern versions. Documented the authority, stale-context behavior, accepted-material boundary and real-time limit in `HARMONIC_FOLLOW.md` and AD-009. No production code or storage schema changed.

**Not implemented:** Live chord-aware macro TENSION. The sequencer macro currently receives project key/scale only. Song Graph state lives behind the storage owner, so reading it in the callback would violate the no-I/O real-time contract. A bounded coherent publication into the playback owner is needed before this behavior can be added safely. This is explicitly deferred.

**Tests added:** None; the existing focused suites were sufficient to verify the current behavior.

**Tests passed:** `bash tests/run_hgs_tests.sh` (exit 0; Graph builder, role policy, projection, Song state, persistence, storage placement/version and live snapshot tests); `bash tests/run_performance_macro_engine_tests.sh` (exit 0; unknown harmony leaves pitches unchanged); focused P1R source firewall, semantic seam, Graph materialization, deterministic repeat, ASan and UBSan gates within `bash tests/run_0_9_9_phrase_p1r_tests.sh`.

**Tests failed / pending:** The aggregate P1R runner exited 134 in its separate `run_0_9_9_m1_p1_tests.sh` subtest at `test_0_9_9_m1_p1_multibar_phrase.cpp:80`: it expects `SparseCall` for Electro, while the current Electro profile selects `AngularCell` (enum value 16); the following literal motif expectation is stale too. The same failure reproduces when that runner is executed at the exact baseline commit `28918d9b78e9a527ce2b5ce201a465af5126f76a`. The affected test/source/script are unchanged from that baseline. No fix was made to this unrelated old test contract. The canonical full host suite is not rerun here; its latest complete pass is the 0.9.29 record. No firmware or SDL build was run because this milestone changed documentation only.

**Memory delta:** 0 B source/runtime delta; documentation only. No new firmware measurement was made.

**Firmware size delta:** Not measured for 0.9.30. The last firmware evidence is the 0.9.29 build and does not include any 0.9.30 firmware change because there was none.

**Known risks:** Live macro TENSION does not follow chord changes. Musical quality is not established by pitch-set and routing tests. The older M1-P1 test runner remains red at the exact baseline due stale literal expectations.

**Hardware status:** `NOT TESTED`

**Decision:** `GO` for the verified existing harmonic-generation path; live chord-aware macro TENSION is `DEFERRED`. The baseline M1-P1 test failure is recorded and is not represented as passed.

## Version 0.9.31 — Intelligent Arrangement

**Version:** 0.9.31

**Base commit:** `885ce13507e7d7f93740ee84eb5fd15ee1e64c81`

**Result commit:** `084b7a78dcabf5cffc6c4767e0748edd740207d5`

**Goal:** Extend the existing FORM workflow with bounded section-role starting curves while keeping Song rows as the editable source of truth.

**Implemented:** Reused `EnergyCurve` and the existing transactional Song mutation/Undo path. Added transient INTRO, MAIN, DEVELOPMENT, BREAK and OUTRO presets on keys 4–8; existing BUILD, DROP and WAVE remain on 1–3. Applied arrangements materialize as ordinary Song rows without schema changes. Added validation of fixed-capacity curve count, bar length and energy before source reads or mutation, plus atomic refusal tests. The chosen preset/genre stays visible after the toast and curve edits display CUSTOM. SDL smoke exposed and verified the label fix.

**Not implemented:** Persisted section-role metadata, independent motif variation, per-role genre variants, or an arrangement engine separate from FORM. The proposed contours only select participation from existing tracks; their musical usefulness awaits owner listening.

**Tests added:** Expanded `tests/test_energy_curve.cpp` for all named contours and malformed-curve atomicity; expanded `tests/test_song_form_genre_source_regressions.py` for keyboard range and persistent labels.

**Tests passed:** Focused EnergyCurve test, including AddressSanitizer and UndefinedBehaviorSanitizer runs; FORM source regression; canonical `bash tests/run_host_tests.sh` exit 0 at the pre-role-extension revision; full SDL application build and focused interactive FORM smoke after the final UI change; default Cardputer ADV firmware build and FS1B candidate firmware build with `--warnings all`; FS1B candidate-only FatFs link-map proof and static DRAM gate; `git diff --check`.

**Tests failed / pending:** The canonical host suite was not rerun after the final role-preset/UI extension; the focused domain and source-regression tests passed after it. Full-suite rerun is pending at the 0.9.35 integration gate. SDL used dummy audio; hardware audio, physical keyboard comfort, USB MIDI and musical listening remain untested by design.

**Memory delta:** 0 B static DRAM versus 0.9.29 records: default profile 187,604 B with 3,884 B headroom; FS1B 187,612 B with 3,876 B headroom against the recorded provisional gate. Stack high-water mark and runtime heap remain unmeasured.

**Firmware size delta:** Program-storage report +392 B versus 0.9.29 in both profiles. Final default report: 1,721,130 B; FS1B report: 1,745,602 B. FS1B app-only BIN is 1,745,760 B (+400 B versus the 0.9.29 FS1B app BIN). Final artifact fingerprints and merged-image sizes are in `RESOURCE_BUDGET.md`.

**Known risks:** Role contours control track participation only and have no listening acceptance. SDL UI smoke is limited to SONG → FORM and does not prove physical keyboard ergonomics. The static DRAM gate remains provisional with under 4 KiB headroom. The full host suite needs a final integrated rerun.

**Hardware status:** `NOT TESTED`

**Decision:** `GO` for the existing FORM-based arrangement milestone; motif development and owner listening remain open for 0.9.35.

## Version 0.9.32 — Expressive MIDI and performance controls

**Version:** 0.9.32

**Base commit:** `084b7a78dcabf5cffc6c4767e0748edd740207d5`

**Result commit:** `1d833b8ec029e906d0f6716c2809a4ac4d3fec43`

**Goal:** Verify that transient Pattern expression, selected CC output and MIDI profile routes remain predictable through the existing MIDI owners.

**Implemented:** Added a pure wrap-safe 20 ms controller retry deadline with explicit first-attempt state; the dispatcher stores one attempted bit per allowed controller and synth. The Synth MIDI page now displays the applied profile's Performance channel and identifies the existing Custom CH08/09 fallback. Pattern MIDI continues to use the transformed event duration through `RuntimeSynthPlaybackState` for Note Off. Added profile/channel, UI-label, limiter and Note Off host coverage. Added `midi_companion_settings.cpp` to the canonical SDL application source list because the profile-aware screen uses its existing route sanitizer.

**Not implemented:** Automatic macro-to-CC mapping, live route rebinding, persisted macros, Pitch Bend, aftertouch, motion/gesture support, or any new device profile. No musical listening acceptance is claimed.

**Tests added:** First-attempt/20 ms/wrap deadline checks; fake CC74 traces for SEQTRAK, General MIDI, Generic MIDI and Custom fallback; unsupported CC suppression; Synth MIDI profile/channel label and 240×135 fit checks; Pattern macro velocity/duration projection and source preservation; Note Off one subtick before and exactly at the transformed deadline. Follow-up review regressions cover USB-only tee reconnect with DIN mounted, FORM source-mode copying, repeated-source playback, and FORM-generated project reload.

**Tests passed:** Targeted `test_cutoff_sweep`, `test_usb_midi_cutoff` and `test_tee_midi_transport` builds/runs; `bash tests/run_midi_performance_route_binding_0_9_7_tests.sh`; `bash tests/run_midi_perform_ui_polish_tests.sh`; `bash tests/run_cardputer_prune_tests.sh` including FORM playback and persistence regressions; canonical `env TMPDIR=/tmp/grooveputer-0932-host-tmp bash tests/run_host_tests.sh` after the integration review fixes (exit 0; `/tmp/gp-0932-review-host-suite.log`); `bash tests/run_pattern_phrase_p2_p0_compatibility.sh` (GCC deterministic repeat, ASan and UBSan PASS; Clang `SKIP (clang++ unavailable)`; `/tmp/gp-0932-review-pattern.log`); `make -C platform_sdl clean all CXX=g++`; default Cardputer ADV `bash scripts/build.sh --warnings all`; FS1B `env TMPDIR=/tmp bash scripts/build_cardputer_dynbuffers.sh --warnings all` with candidate-only FatFs map proof; `git diff --check`.

**Tests failed / pending:** Early canonical host-suite attempts exposed missing `midi_companion_settings.cpp` links in SDL source consumers. The fix was made at the source of truth, `platform_sdl/Makefile`; the standard SDL build and final canonical suite then passed. During test development, a direct `processSequencerEvents()` check did not release a note because production checks deadlines in the audio per-sample loop. The integration test now drives the existing `releaseDue()`/consumer path at the exact projected deadline; no production note-lifecycle change was needed. The full host suite also prints the existing SDL fixture fallback message when no saved Scene is present; the affected tests pass. Independent integration review additionally found and fixed the tee CC reconnect and FORM steps-override issues in commits `c345560e` and `1d833b8e`; their targeted regressions and the full canonical host suite pass. Clang parity was not run because `clang++` is unavailable; the updated Pattern/Phrase runner reports `SKIP`. Physical hardware, audible quality and external device behavior remain untested by design.

**Memory delta:** Default static globals 187,628 B, +24 B versus 0.9.31; FS1B 187,636 B, +24 B. Headroom to the project's provisional 191,488 B gate is 3,860 B and 3,852 B respectively. Default `.dram0.data` / `.dram0.bss`: 44,732 B / 142,896 B; FS1B: 44,732 B / 142,904 B. Stack high-water and runtime heap were not measured.

**Firmware size delta:** Program-storage report grew by 1,516 B versus 0.9.31 in the default profile to 1,722,646 B and by 1,508 B in FS1B to 1,747,110 B. App-only BIN is 1,722,800 B default (+1,520 B) and 1,747,264 B FS1B (+1,504 B). Merged images are 4,194,304 B in both builds. Review fixes added 428 B of reported program storage and 8 B of static globals compared with the pre-review .32 build. Artifact SHA-256 values are in `RESOURCE_BUDGET.md`.

**Known risks:** Independent review findings for USB reconnect CC state and FORM STEPS overrides are fixed in the follow-up integration changes and covered by mock transport, FORM unit, playback and persistence regressions. The static DRAM gate remains provisional with under 4 KiB headroom. Default FatFs is not the SD-safe candidate; only the FS1B map proves candidate FatFs and excludes stock FatFs. FORM preserves an explicit loop range, so a user may need to expand it to hear inserted rows. Macro sound and physical control feel remain owner listening checks. No Cardputer, USB MIDI, DIN MIDI or SEQTRAK test was performed.

**Hardware status:** `NOT TESTED`

**Decision:** `GO` for the host-validated 0.9.32 engineering milestone. Hardware remains `NOT TESTED` by design.

## Version 0.9.33 — MIDI export

**Version:** 0.9.33

**Base commit:** `7ff9451718652033eb12f8c5d8c5180ca1866b0f` (0.9.32 route-change safety closure).

**Result commit:** `3f492197d460a5e59664d0fefb3be0519902c0df`.

**Goal:** Export the selected committed Song as deterministic, structurally valid SMF Type 1 without touching realtime ownership or persisting transient Performance state.

**Implemented:** Added bounded SMF writer, immutable Song export snapshot/projection, pattern and Melody material traversal, role/channel projection, track metadata for omitted/non-SMF material, hidden scratch/temp file assembly, production-parser readback, deterministic unique naming and final rename only after validation. Added Project → Scenes → Export MIDI; it refuses while playback is active and tells the user transient macros are not exported. No project schema change.

**Not implemented:** Performance-macro capture, audio rendering of Voice/drum FX, MIDI CC reconstruction from runtime state, asynchronous progress UI, or physical SD/device behavior verification.

**Tests added:** Host writer fixture and independent parser; committed-Song projection tests for reverse order, STEPS override, Melody repetition, routing, tempo, event pairing and omission warnings; fake-storage short-write/close/readback/validation/rename failures; repeat-export byte identity; independent file parser; Project UI source contract; canonical host-suite integration.

**Tests passed:** Targeted writer/domain/file-export tests, both independent parser fixtures, repeat-export/failure cleanup checks, export UI source regression, SDL application build, 240×135 SDL hint rendering check, default Cardputer firmware build, FS1B Cardputer firmware build and DRAM budget/map check; `git diff --check`. Canonical `env TMPDIR=/tmp bash tests/run_host_tests.sh` completed with exit 0 at integration HEAD `db29299fb130ad23bd5be6693f29242044e75e01`; log: `/tmp/gp-0933-stable-host-suite.log`. The run included documentation-only worktree edits; production source and `tests/` were clean.

**Tests failed:** No product test failures in the stable run. An earlier rerun was invalidated after the runner file was edited while executing; it is excluded from the result. The final `.35` candidate still needs the complete suite on its exact clean SHA.

**Memory delta:** Static DRAM unchanged from `.32`: default 187,628 B (3,860 B headroom); FS1B 187,636 B (3,852 B headroom) against the provisional 191,488 B gate. Runtime heap and stack high-water were not measured.

**Firmware size delta:** Program storage +12,504 B default to 1,735,150 B, and +12,484 B FS1B to 1,759,594 B. App-only BIN +12,512 B default to 1,735,312 B, and +12,480 B FS1B to 1,759,744 B. Exact build SHA and artifact hashes are in `RESOURCE_BUDGET.md`.

**Known risks:** UI export synchronously traverses Song/material and may block for a large Song; no real SD failure or power-loss behavior was measured. The static DRAM gate remains provisional with under 4 KiB headroom. Physical audio, keyboard, USB MIDI, SEQTRAK and musical quality remain untested.

**Hardware status:** `NOT TESTED`

**Decision:** `GO` for the host-validated `.33` engineering milestone. Hardware remains `NOT TESTED`; do not use this milestone's firmware as an intermediate hardware checkpoint.

## Version 0.9.34 — Productization

**Version:** 0.9.34

**Base commit:** `0d973ffcae96f056bb1605de2000a8bac787d2f7` (0.9.33 package/docs integration).

**Result commit:** `fb300ff5c3e221902c3771d7af53973400073e4b` (the final commit
updates the stale docs regression expectations; productization code is in
`d3dd29f859a22b982d4af33c6d5da42e4b611857`).

**Goal:** Make the candidate build identifiable and its unpublished handoff package reproducible, traceable and recoverable.

**Implemented:** Firmware build identity records semantic version, full source SHA and clean/dirty state in the ELF and boot diagnostics. A tested resolver maps release tags and branches to deterministic versions. The packager can verify ELF identity and clean source SHA/tree, reject a mismatched rollback archive, validate its accepted FQBN and internal product hashes, and bundle rollback images, provenance, owner acceptance record and restore instructions. A clean-source package requires rollback material.

**Tests added:** Version resolver cases; generated build identity; ELF/manifest identity match; clean-tree and source-HEAD validation; rollback outer hash, internal image checksums, FQBN and package cleanup on failure; reproducible package archive fixture.

**Tests passed:** `python3 tests/test_build_identity.py`; `python3 tests/test_release_version_resolution.py`; `python3 tests/test_hardware_candidate_packaging.py` (9 tests); `python3 tests/test_global_help_source_regressions.py`; `bash tests/test_release_firmware_reproducibility.sh`; canonical `env TMPDIR=/tmp bash tests/run_host_tests.sh` (exit 0 at exact clean HEAD `fb300ff5c3e221902c3771d7af53973400073e4b`, log `/tmp/gp-0934-host-suite-fb300ff5.log`); `git diff --check`. Release-profile FS1B firmware built with `--warnings all` and passed the project DRAM gate. Clean-source package and all bundled checksums passed.

**Tests failed:** The first host-suite attempt on `d3dd29f8` stopped at a stale assertion expecting the 0.9.22 Quickstart title after the `.33` docs update. Updated the regression test to current internal-line documentation; the full suite then passed on `fb300ff5`.

**Memory delta:** Identity adds 0 B static DRAM in the like-for-like FS1B `CDCOnBoot=cdc` comparison to `.33`: 187,636 B. Exploratory release profile `CDCOnBoot=default`: 188,060 B, 3,428 B headroom against the provisional 191,488 B gate. The profiles differ, so this is not attributed to identity.

**Firmware size delta:** Like-for-like CDC-on exploratory build: +116 B program storage, +128 B app BIN. Release profile: 1,808,354 B program storage and 1,808,512 B app BIN; it is a different build profile and has no exact `.33` release-profile comparison.

**Known risks:** Firmware artifact and clean package are tied to code commit `d3dd29f8`; the final result commit `fb300ff5` changes only the docs source-regression expectation. The `.35` artifact must be rebuilt and repackaged from its exact clean SHA. M5Launcher and M5Burner compatibility remain unverified.

**Hardware status:** `NOT TESTED`

**Decision:** `GO` for 0.9.34 productization, `HOST-VALIDATED / HARDWARE-UNVERIFIED`. `.35` still requires a clean integrated candidate and rerun of every acceptance lane.

## Version 0.9.35 — Integration closure

**Version:** `0.9.35-rc+615ac96`

**Base commit:** `a11b601b9e5645ab3bf4938f7f5cb32294c6578e`.

**Result / artifact-producing commit:** `615ac9642f1103ce1f9d4fcddcb083710b171db4` (clean source tree). This report update is documentation-only and follows that tested source SHA.

**Goal:** Close the software integration gates and prepare one unpublished Cardputer ADV candidate for the owner's first hardware acceptance.

**Implemented:** Added `testRouterTeeDisconnectAndFrozenRouteIntegration` in `tests/test_usb_midi_pending_release.cpp`. The deterministic `MusicalEventRouter` → `UsbMidiOutput` → USB-primary/DIN-secondary tee trace covers shared same-pitch Pattern/Performance ownership, USB loss with DIN mounted, final NoteOff delivered through DIN, frozen startup routing after a later snapshot publication, USB CC123 cleanup before post-reconnect NoteOn, section-like transition ordering, and zero owned notes after teardown. No production runtime code changed in this integration commit.

**Not implemented / deferred:** Hardware-specific active-playback profile rebinding; the UI keeps route changes unavailable during playback and applies stopped edits after restart. Real Cardputer dispatcher scheduling, physical USB enumeration, M5Launcher installation and M5Burner acceptance remain unverified. No new musical features were added.

**Tests added:** The USB/DIN integration trace above; existing export/parser, project compatibility, FORM/Undo, and MIDI lifecycle fixtures remain part of the canonical host suite.

**Tests passed:**

- Focused trace compile/run with `-Wall -Wextra -Werror`.
- `env TMPDIR=/tmp bash tests/run_host_tests.sh`, exit 0 on clean exact SHA `615ac9642f1103ce1f9d4fcddcb083710b171db4`; log `/tmp/gp-0935-host-suite-615ac964.log`.
- Clean SDL build at the same source SHA. Interactive Xephyr run used dummy audio and verified: create/accept a Melody, extend it to two bars, add a bar-two note, refuse a shortening that would truncate that note, generate in STOP and PLAY, create a Material TAKE, apply FORM to insert 36 bars after the source block, save and reload the project, change and restore the external-output mode in Project settings, and export MIDI. Screenshots are under `/tmp/gp-0935-sdl-615ac964/xephyr-run/`.
- The SDL-exported `grooveputer_scene-Song1-2.mid` (12,737 bytes) passed `tests/parse_smf_export_file.py` with exit 0.
- Release-profile FS1B Cardputer build with `--warnings all`, exact source identity, candidate-only FatFs linker-map proof and DRAM gate; log `/tmp/gp-0935-fs1b-release-615ac964.log`.
- `tests/test_release_firmware_reproducibility.sh` built the same exact-version app image twice with matching SHA-256 `6a6c920b3cab4c241464b02209680962743211d4d592af6c6adfd5a315339567`; log `/tmp/gp-0935-fw-repro-615ac964.log`.
- Corrected clean-source package built twice from the exact source SHA with the `.16` rollback and hardware checklist. Both archives were byte-identical; final ZIP SHA-256 `b9810a1f98fa20a6d5f98ece295c52eaae0209eed79a1c050a7311bf09675731`. ZIP integrity and all 20 inner SHA-256 entries passed. Manifest/M5Burner addresses use the Cardputer board-specific bootloader offset `0x0` and record 8 MiB physical flash versus the 4 MiB merged-image span.
- The packaging regression test now covers board-specific bootloader offset `0x0`, physical flash capacity 8 MiB versus the 4 MiB merged-image span, FlashSize menu overrides, and both documented flash spans. `python3 tests/test_hardware_candidate_packaging.py` passes all 12 tests; `py_compile` and `git diff --check` pass.
- At the owner's request, the app-only image was flashed on the connected GroovePuter at `0x10000` using esptool v4.7.0. The device's bootloader at `0x0`, partition table at `0x8000`, and `boot_app0` at `0xe000` matched candidate hashes before writing. Esptool reported `Hash of data verified`; independent `verify_flash` returned `verify OK (digest matched)`, followed by USB re-enumeration. This does not verify main UI startup, sound or MIDI.

**Tests failed / stopped:** No product-gate failure. A duplicate full-suite rerun after packaging-only changes was stopped after 12 minutes during sanitizer compilation and is not counted; the complete host suite on the unchanged firmware source SHA `615ac964...` passed above. An initial `CDCOnBoot=cdc` compile with `0.0.0-dev` identity completed but is not a candidate build. A follow-up CDC-on compile with the RC identity was canceled after confirming that the release FQBN is `CDCOnBoot=default`; neither run is counted as candidate evidence.

**Memory delta:** Static DRAM is 188,060 B, unchanged from the `.34` release-profile build. Headroom is 3,428 B against the provisional 191,488 B gate. `.dram0.data` is 44,812 B and `.dram0.bss` is 143,248 B. Runtime heap and task stack high-water were not measured.

**Firmware size delta:** Program storage 1,808,354 B; app-only BIN 1,808,512 B; merged full image 4,194,304 B. App-only SHA-256 `6a6c920b3cab4c241464b02209680962743211d4d592af6c6adfd5a315339567`; merged SHA-256 `2d3d78a10baf44af9e966b42fd8d72326412d01dcef2cadd40daf360058d5c19`; ELF SHA-256 `4038a6671dc840e94677abe10dbd910151040cf861c3944e020cfacded27c003`.

**Known risks:** The static DRAM gate is provisional and has 3,428 B headroom; runtime heap and stack usage were not measured. The release build emits legacy compiler warnings (mostly repeated pinned-core macro redefinitions; also format-truncation and type-limits warnings), though it exits successfully with the DRAM gate passing. The host tee trace does not model the real Cardputer dispatcher task or USB enumeration. A full old-app readback attempt ended with a serial CRC error before the write; the verified `.16` rollback bundle remains available. Physical UI startup, audio, keyboard feel, SD behavior and MIDI/SEQTRAK interoperability still require owner testing.

**Hardware status:** `HARDWARE-UNVERIFIED` (app write/readback and USB re-enumeration observed; runtime acceptance pending).

**Decision:** `GO — READY_FOR_OWNER_HARDWARE_TEST`. Candidate bundle is local and unpublished. App-only firmware was flashed at the owner's request; no M5Burner/M5Launcher upload was performed, and no public release was created.
