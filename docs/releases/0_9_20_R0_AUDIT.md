# GroovePuter 0.9.20 — R0 Audit (working draft)

- Audit date: 2026-11-04
- Baseline branch: `genre/cell-source` (`fec3237c95c0e19f61e3958cee7d4cd23cecb483`)
- Local candidate branch: `release/0.9.20-minimal-ui-fix`
- Candidate code fix commit: `fbbf0105a8105a2869f12af691da1522fb4d5338`; the post-fix firmware evidence snapshot is `83ea592ed503221d78cbafc3bf4c8b3865bb250f`. The final full-tree SHA is recorded in the regenerated package provenance and exact-SHA CI run.
- Exact source SHA and dirty-state evidence must match the refreshed workflow-profile build and generated `provenance.txt` (`worktree_dirty=false`).
- Candidate tree: clean after commit in an isolated worktree; the user's active checkout remains untouched.

## Decision

**DEFER — release evidence is incomplete.** This is not a release approval. During repository-history review, the audit found that the candidate still contained the 808/909 kick phase conversion defect tracked by open PR #489. The PR's sanitizer regression reproduced the undefined float-to-uint32 conversion on the candidate; its narrow fix and test are now in `fbbf0105`. The firmware and review bundle must be rebuilt for the refreshed exact candidate. Product export scope and physical-device acceptance remain unverified.

## Repository and publication state

| Check | Result | Evidence |
|---|---|---|
| Remote branch points at the candidate SHA | PASS | GitHub API `commits/genre/cell-source` returned `fec3237c95c0e19f61e3958cee7d4cd23cecb483` |
| Exact-SHA GitHub Actions checks | IN PROGRESS for superseded SHA | Workflow run `41882445936` is pinned to `93541d89c1f733d661cbaf0bf12ad9d3e8201e0c`; it does not cover the current code commit `fbbf0105` or the eventual refreshed candidate. |
| PR from `genre/cell-source` | None found | GitHub API returned an empty list |
| `v0.9.20` GitHub Release | Not published | GitHub API returned 404 for the tag |
| Local candidate | PASS, code and evidence committed locally | Branch `release/0.9.20-minimal-ui-fix`; firmware evidence snapshot `83ea592ed503221d78cbafc3bf4c8b3865bb250f`; fix commit `fbbf0105a8105a2869f12af691da1522fb4d5338`. The exact full-tree release candidate must be taken from the regenerated provenance after the evidence refresh is committed. |
| Refreshed workflow-profile build | PASS, local/provisional | Clean tree `83ea592e`; BIN SHA-256 `4ab1bfafad54d7601a0b68b1d7e2c2472b29614d1b9db3db02c8170f256ad6fe` (1,726,784 bytes), ELF `695ad678afc43f917f06320d04b6b0fb2993ea895bd9fcf61c3c099d128c6a66`, merged BIN `393e97e178be0c81cc4fc09c4330c501396d77529f3604622b32fae0745f7972`. Two clean builds with fixed release `BUILD_PATH` and `SOURCE_DATE_EPOCH` produced matching hashes. Globals remain 186,236/191,488 B (5,252 B margin), policy provisional; warning occurrences 1,001. Logs: build transcript from first post-fix compile in the active session; `/tmp/gp-0.9.20-83ea-second-build.log` records the repeated build. |
| Review bundle | SUPERSEDED | Existing `build/grooveputer-0.9.20-candidate-NOT-A-RELEASE.zip` predates the kick-phase fix; regenerate it from the final candidate and keep `STATUS.txt` as NOT A RELEASE. |

## Focused host checks

| Command | Result |
|---|---|
| `bash tests/run_smart_play_tests.sh` | PASS, exit 0. All listed Partner phases through phase 10, note-range wire/engine, and included related targets passed. Compiler warnings include `LOG_DEBUG` redefinition / format-security and several `snprintf` truncation warnings. |
| `bash tests/run_step_settings_tests.sh` | PASS, exit 0. Drum settings and source checks passed. |
| `bash tests/run_melody_slot_navigation_tests.sh` | PASS, exit 0. |
| `bash tests/run_ui_navigation_tests.sh` | PASS in the final core gate. SDL navigation, melody editing/playback, Pattern Pair titles/playhead, tempo controls and MIDI import audible-path targets completed. |
| `bash tests/run_s1_synth_engine_switch_tests.sh` | PASS, exit 0. Switch/crossfade object consistency, 600 rapid switches, pre-render switch, and source regressions passed. |
| `bash tests/run_s1_synth_page_restore_tests.sh` | PASS, exit 0. Synth page restores the MORE tab before component construction. |
| `bash tests/test_release_identity_source.py` | PASS after the warning cleanup; artifact identity and portable install instructions remain aligned. |
| `test_drum_phase_wrap` against the pre-fix candidate | FAIL as expected under `-fsanitize=undefined,float-cast-overflow`: 808 reports an out-of-range value at `src/dsp/mini_drumvoices.cpp:399`; 909 reports the same at line 974. |
| `test_drum_phase_wrap` after `fbbf0105` | PASS for both 808 and 909 with `-fsanitize=undefined,float-cast-overflow`; the fix uses defined unsigned modular phase multiplication. |
| `bash tests/run_smart_play_tests.sh` | PASS after the warning cleanup; all listed Partner phases through phase 10 and pattern-range wire/engine targets passed. Log: `/tmp/grooveputer-0.9.20-smart-play-after-warning-cleanup.log`. |
| `bash tests/run_s1_synth_engine_switch_tests.sh` | PASS after warning cleanup; engine/object consistency and 600 rapid-switch scenarios passed. Log: `/tmp/grooveputer-0.9.20-s1-switch-after-warning-cleanup.log`. |
| `bash scripts/ci/run_core.sh` | All 46 groups closed; final S1 page-restore test and `::endgroup::` recorded. Log: `/tmp/gp-0.9.20-540a-run-core-final.log`. The run tested the exact source contents now committed at `be45c4e`; local evidence only, not exact-SHA remote CI. |

## Format-truncation warning review

The latest firmware build reports 23 `-Wformat-truncation` occurrences. Three diagnostics across two UI formatting paths were real capacity risks and are fixed: phrase-selection status now has 64 bytes (compiler maximum was 56), and the chord toast has 48 bytes (compiler maximum was 47). The remaining diagnostics were traced to values constrained by existing model/UI invariants; GCC does not propagate several of those checks into `snprintf` range analysis:

| Call-site family | Source bound checked | Buffer implication |
|---|---|---|
| Pattern address | `PatternAddress::valid()` restricts page to 0–15, bank to 0–1, slot to 0–7; `scenes.h` constants confirm the limits. | Maximum valid label is `16B8`; smallest affected caller buffer is 8 bytes. |
| Note-entry octave, pair-play, sequencer labels | Entry octave shift is clamped to −2…2; active pair-play stores a validated slot; sequencer hub has 10 tracks and QWERTY pattern indices are a fixed finite map. | Current message buffers exceed the longest domain-valid strings. |
| Song rows and loop labels | `Song::kMaxPositions` is 128; selection bounds clamp cursor rows, and each grid renderer clamps `scroll_row_` before drawing. | Displayed rows and loop endpoints are 1…128; current 8–24 byte buffers are sufficient. |
| Form labels | Energy curves allow at most 8 sections and each section is 4 or 8 bars; song insert/source rows are clamped to the 128-position model. | Form labels stay below their 40/48 byte buffers. |
| Phrase labels and off-window counts | Synth phrase event count is capped at 128; note name and duration formatters have bounded output; visible-window counts cannot exceed the phrase event cap. | The 64 byte status and 8 byte counters fit their bounded results. |
| Workspace memory label | Remembered child page is checked against the selected workflow's page count before use. | Five child indices fit the 32 byte buffer. |

This review clears the remaining truncation warnings as bounded under current code paths. The only application warnings left are these 23 bounded truncation diagnostics; the repeated Arduino core macro redefinitions are upstream toolchain warnings. The firmware still builds with warnings enabled, and any future change to these limits must revisit the associated buffers.

## Source regression checks named by the release draft

Each command was run from the candidate source. The same checks were also run on `fa322ec4` (the candidate parent) to separate inherited failures from candidate-only differences.

| Check | Candidate | `v0.9.19` | R0 classification |
|---|---|---|---|
| `test_pattern_generation_owner_0_9_9_source.py` | FAIL: parser cannot find the old `if (keyF)` boundary | FAIL on `fa322ec4` for the same parser boundary | Inherited source-test drift: current handler uses `if (keyF && !ui_event.ctrl)`. A temporary parser-anchor update let all remaining assertions pass. Refresh the source check and retain the asserted ownership contract. |
| `test_phrase_ui_source_regressions.py` | FAIL: expects the exact unguarded waveform-toggle expression | FAIL on `fa322ec4` for the same expression | Inherited source-test drift: current global Alt+W includes the compile-time `GROOVEPUTER_ENABLE_WAVEFORM` guard and excludes Phrase Core. A temporary expectation update let remaining assertions pass. |
| `test_song_playhead_source_regressions.py` (includes SMF panel test) | FAIL in nested SMF panel check: stale arrangement/render tokens | FAIL on `fa322ec4` | Inherited source-test drift: current arrangement renderer is split into `sequencer_hub_page_midi.cpp`, derives occupancy, and uses current route/footer labels. Update assertions to the live rendering path and preserve coverage of all rendered rows and playhead. |
| `test_ui_layout_source_regressions.py` | FAIL, two source-layout assertions | FAIL, same two assertions | Inherited source-test drift: grid geometry lives in `pattern_edit_page_legacy.h`; synth, phrase-roll, and drum layouts use current bounds-relative expressions. Update ownership and geometry assertions; SDL captures show clean frame layout but do not prove every boundary. |
| `test_undo_0_9_9_r9_regression_source.py` | FAIL: expects `REDO: PATTERN` / `UNDO: PATTERN` | FAIL on `fa322ec4` | Inherited source-test drift: current shared feedback names the edits `STEPS` and `TAKE`. A temporary expectation update passed the remaining contract assertions. |
| `test_drum_g_undo_0_9_9_source.py` | FAIL: old plain Drum G handler boundary absent | FAIL on `fa322ec4` | Inherited source-test drift: handler is bounded by the current shared-realization selector comment. Updating only the boundary made the contract pass. |
| `test_performance_source_regressions.py` | FAIL: expects MIDI Player seek modifier `Shift` | FAIL on `fa322ec4` | Inherited source-test drift: current Player uses Ctrl+Left/Right for coarse seek; document this binding. A temporary source expectation update advanced to the current cleanup owner (`stopGeneratedOutput()` then `resetPulseClock(true)`). |
| `test_wavemorph_performance_source_regressions.py` | FAIL: expects all four knob key pairs to use coarse/fine step | FAIL on `fa322ec4` | Inherited source-test drift: three controls use coarse/fine; dual-osc cutoff is intentionally discrete, and the source selector is now PRESET. Refresh assertions to retain these distinctions. |

The eight checks were refreshed in the isolated readiness branch to follow current owners and labels while retaining their behavioral/source contracts; all eight now pass on the candidate. This removes the stale-check blocker, but source assertions are not substitutes for physical behavior tests.

## Product journey and documentation

| Journey stage | Current evidence | Status |
|---|---|---|
| Discover and understand | README describes GroovePuter and links to v0.9.19 as latest published; identifies 0.9.20 as an unaccepted candidate | PARTIAL; no public 0.9.20 release page or demo package |
| Install | Local candidate ZIP contains workflow-profile images and a draft M5Launcher SD install/recovery guide; the package is marked NOT A RELEASE. The path has not been tried on hardware. | UNVERIFIED |
| Launch and make music | Quickstart explains the first session and marks Acid/House MATERIAL `D` as unsupported | PARTIAL; not verified by a first-time user on hardware |
| Edit 0.9.20 features | Manual/key map include current note-entry/step-settings bindings and identify Partner as session-only | PARTIAL; physical workflow remains unverified |
| Save and return | Quickstart and manual document project Save/Load | DOCUMENTED, not physically verified on the candidate |
| Export/share | No user-facing MIDI/audio file export workflow exists in the first-user docs or reviewed UI. WAV recorder implementations exist, but Cardputer construction/registration is commented out in `GroovePuter.ino`; the UI only stores the recorder pointer and has no start/stop call sites. SDL feeds samples to its recorder callback, but no user-facing recording start/stop action was found. Realtime MIDI routing to external gear/DAW is a separate workflow. | GAP / product contract unresolved |
| Support/recovery | Manual and on-device help exist; the candidate bundle now includes an M5Stack factory-restore recovery draft. Neither install nor recovery has been exercised on the candidate device. | PARTIAL |

## Release profile

The release workflow sets `FQBN=...CDCOnBoot=default...` and calls `build_cardputer_dynbuffers.sh`; `scripts/build.sh` defaults to `CDCOnBoot=cdc`. The workflow environment override is therefore material. The release-profile local build explicitly used the workflow FQBN and passed; a preceding build with the script default (`cdc`) is not the release-profile evidence. Both use dynamic FatFs FS1B. A source guard in `tests/test_release_identity_source.py` asserts the workflow product profile. The workflow uploads `.elf`, `.bin`, checksums, provenance, and a build log. It runs on `v*` tags, `release/**` branches, or manual dispatch; there is no exact-candidate run. Do not tag merely to trigger it.

## Physical acceptance

The candidate release record lists Partner ANSWER/LOOP, `X` silence, drum step settings, `Ctrl+A` copy, C#5/E5, USB roles, persistence and recovery. The available owner report is not linked here to a final image hash. All physical-device behavior remains **UNVERIFIED** for this audit.

## Next work

1. Review whether the 23 source-bounded format-truncation diagnostics should be silenced with clearer local range contracts; 978 repeated Arduino core macro redefinitions remain in the Arduino toolchain. No application warnings from other categories remain in the latest build.
2. Resolve the DRAM margin and provisional exception: current build has 5,252 B headroom against the provisional 191,488 B ceiling, while required hardware minima/reserves and threshold derivation remain open.
3. Close the first-user distribution gap: physically verify the M5Launcher SD install/recovery draft in [0_9_20_INSTALL_DRAFT.md](0_9_20_INSTALL_DRAFT.md), and decide whether realtime MIDI-to-DAW handoff meets the user's export requirement or native WAV/MIDI file export is required. WAV writer implementations exist, but there is no user-facing start/stop/export path; Cardputer recorder setup is disabled in `GroovePuter.ino`. No Launcher catalog entry or M5Burner package is prepared.
4. Refresh the release-profile image, provenance, checksums and NOT A RELEASE bundle for the post-fix candidate; then obtain exact-SHA remote CI evidence and physical acceptance on that exact BIN, recording device, profile, hash, test results and logs.
5. Prepare the final release decision packet. Keep the decision at DEFER; do not publish or tag without owner authorization.
