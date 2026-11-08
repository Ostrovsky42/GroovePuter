# Long Melody E0–E6 Readiness — 2026-10-21

Latest update — 2026-10-23: the user accepted the Cyber A/B STEPS/source/slot checks, eight-bar loop, and combined A+B/drums/FX/NEXT/GO/Song interaction run. Persistence failed after reboot/manual Load Project (bars 2–8 missing). The [persistence audit](2026-10-23-persistence-audit.md) reproduced and corrected missing explicit-load hydration, stale resident pages on Save/autosave, incomplete Save As, and New/Clear session/file lifecycle errors. Software checks pass; persistence diagnostic image `6cb56545…` was flashed with a verified write hash. The E5 failure on `b93f21f1…` was followed by user acceptance of Save/Save As/Reset/Load on corrected `6cb56545…`; memory/audio trace acceptance and external MIDI remain open. The installed FS1B persistence image has fixed DRAM 190,240 B. Earlier dated evidence below remains historical.

Status: **host-side preparation complete; E0 repeatability partial; M2 residency census passed; hardware E1–E6 matrix incomplete**. A 2026-10-23 capture on diagnostic image `50d15ed9…` recorded 44 G refusals, all `Busy`, on both Synth voices and across genres. The user then confirmed on hardware that the queue frees; this is expected pre-boundary queue occupancy, not a genre failure or stuck queue. Runtime image `12241952…` with `GEN BUSY: WAIT` was flashed and the user confirmed the feedback works; the post-flash monitor showed `integrity=1` and `underruns=0`. E0 still lacks a captured cold-boot/repeatability run on the current runtime diagnostic image. The M2 image completed its four-buffer/40-cycle idle census and a B Melody PLAY/STOP session; the full remaining musical E1–E6 matrix has not run. See [`2026-10-22-e0-hardware-run.md`](2026-10-22-e0-hardware-run.md) and [`2026-10-22-m2-hardware-run.md`](2026-10-22-m2-hardware-run.md).

The first M2 PLAY/STOP window used the internal synth and showed no panic, underrun, or heap-integrity error, but the installed image did not report each voice's source. A source-tagged diagnostic image was then flashed. It captured A Steps/B Melody during PLAY and a stable 180-second post-activity idle plateau, still without underrun or heap-integrity error. The user accepted that internal-audio check and later resumed E0 testing. A Melody, dual Melody, and external MIDI remain unverified by captured evidence.

The exact firmware builds and static/link-map evidence are recorded in [`2026-10-21-e0-baseline.md`](2026-10-21-e0-baseline.md).

The 2026-10-22 interactive [SDL smoke](2026-10-22-sdl-smoke.md) passed Synth A/B G at STOP, Synth B G at PLAY, 1→2→4→8-bar Melody length, a bar-8 edit, rejection of truncation, and MATERIAL generation at STOP/PLAY on free targets. A rapid second Synth G before the next bar reproduced the generic `GEN FAILED`; the diagnostic capture showed Busy, and the follow-up image now displays `GEN BUSY: WAIT`, confirmed on the Cardputer. The full SDL UI acceptance remains red because a held unsaved Melody yields an unclear `PHRASE TARGET CHANGED` on an otherwise FREE destination, and Phrase Bank causality round trips were not exercised.

The subsequent [Synth note-display review](2026-10-23-synth-note-display-review.md) corrected current-slot rendering in RETRO/AMBER, Song WAIT/FAIL handling, the STOP marker, and Held Melody→STEPS display identity. Recording-Gfx checks for both voices/all styles, Unified Song slots, Working edits/retarget, SDL build, and the FS1B runtime build pass. The new candidate `b93f21f1…` was flashed on 2026-10-23 with a verified write hash; it is now the installed image. Hardware acceptance of the display corrections is pending.

## Gate status

| Gate | Host evidence | Hardware evidence |
|---|---|---|
| E0 baseline | E0 telemetry source tests and diagnostic builds pass; candidate-only FatFs map verified | PARTIAL: earlier BIN `2d97ac49…` produced one clean cold boot, Pattern PLAY/STOP, page churn, and a stable plateau. Current runtime image `12241952…` booted into stable telemetry (`sdMounted=1`, `songLoaded=1`, `integrity=1`, `underruns=0`); user confirmed the Busy toast and queue release. Capture cold boot/repeatability on the current image, then controlled slot switching and post-switch plateau. |
| E1 8-bar object | Fixed-capacity runtime buffer, Melody serialization, slot promotion/navigation, semantic round-trip and failure-path suites pass | PARTIAL: M2 four-buffer residency and 40-cycle idle census passed; B Melody PLAY/STOP captured, but real A+B Melody playback and failure paths remain |
| E2 cross-bar playback | Real MiniAcid proves an 8-bar phrase-relative timeline, no gate cut/retrigger at 4→5, multi-bar gate lifetime, and 8→1 release/retrigger policy | NOT RUN: internal audio and external MIDI continuity still need listening/trace evidence |
| E3 dual voice | P3 per-synth source/cardinality, FS2A current/next causality, and M1 lineage/lifecycle suites pass | PARTIAL: A Steps/B Melody PLAY and STOP recorded without underrun; A Melody, A+B Melody + drums/FX, same-boundary NEXT, switching, and controlled heap recovery remain |
| E4 local edit | Extract/rebase and replace a one-bar or bars-5–6 candidate; reject cross-boundary notes/collisions/capacity atomically; preserve outside events; test onset, duration, insert, delete, one-step Undo/Redo and 100 alternating A/B cycles | Core-only test evidence exists. A production range-selection/regeneration UI remains deferred by the frozen non-goals |
| E5 persistence | Full 8-bar A+B OS-process restart, explicit load, Save As/new/existing destination, canonical Save/recovery, New/Clear and failure cases pass after fixes | PARTIAL: user confirms corrected firmware restores both 8B voices after Save/Save As/Reset/Load. A later runtime capture has no persistence operation; SD removal/failure reporting still needs hardware |
| E6 churn | 100 cycles of A/B range edit, insert/delete, Undo/Redo and store round-trip pass; existing NEXT/slot/process lifecycle suites pass | NOT RUN: repeated page/slot/NEXT/save/load/PLAY/STOP endurance and post-warmup memory/audio plateau |

## Focused test results

Passed during this work:

- `tests/test_cardputer_memory_baseline_source_regressions.py`
- `tests/test_memory_r1_product_closure.py`
- `tests/test_melody_pending_census_source.py`
- `tests/run_pattern_phrase_p3_tests.sh`
- `tests/run_pattern_phrase_p3_u1_tests.sh`
- `tests/run_long_melody_range_edit.sh`
- UI Constitution U4B3, U4B4, U4B5, U4B7, U4B8, U4C1, U4C2, and U4D1 focused suites
- `git diff --check`

Focused host gates re-run on 2026-10-22 against the current dirty worktree:

- P3 and P3-U1 source/projection, duration/grid, per-synth routing, phrase-relative onset: PASS
- Long Melody bounded range edit and Undo: PASS
- Runtime diagnostic source/telemetry closure and memory source regressions: PASS
- MelodyStore format and transactional promotion: PASS
- H5 SDL process-restart restore: PASS
- FS2A current/next causality, M1 development lineage/lifecycle, M4 NEXT material, P2 lifecycle barriers: PASS

The broader SDL builds emitted existing C++ bit-field and `snprintf`/format warnings; all commands exited 0. This adds host evidence only and does not close the outstanding Cardputer interaction cases.

Previously passed on this worktree before the final range-extract tests: MelodyStore, transactional promotion, Melody slot navigation, C9 audible fail-closed and MAKE PHRASE boundary, P2 lifecycle/playback, FS2A, M1 lineage/lifecycle, and H5 SDL process restart. Existing compiler warnings are unrelated; no focused gate above failed after its relevant code changes.

`U4B5` had a stale source assertion that required `UNDO: PHRASE`; the current editor intentionally displays `UNDO: MATERIAL`/`REDO: MATERIAL`. The assertion was aligned to the shipped wording; the runtime Undo test and context-gating checks pass.

## Device session order

1. The runtime Busy-feedback image `12241952…` is installed and the user confirmed the toast works and the queue frees. Capture a cold boot with Serial already open, then controlled slot switching and a post-switch idle plateau. Proceed to the source-tagged M2 image after E0 repeatability is captured.
2. If E0 repeats cleanly, flash the source-tagged M2 image from `build/long-melody-e0-m2-source-diagnostic`. At about 20 seconds it should report the four residency snapshots, four successful 1,284-byte allocations, no allocation/read failures, 40 churn cycles, load timing, cleanup of its unique diagnostic SD file, and `sourceA/sourceB` for the musical tests.
3. On the M2 image, record A Melody/B Pattern, A Pattern/B Melody, A+B Melody, A+B with drums/FX, simultaneous NEXT requests, editing one voice while the other plays, slot/page changes, PLAY/STOP, SD removal/restore, save/reload, audio underruns, heap/largest-block plateau, and stack HWM.
4. For E2, use sustained notes over bar 4→5 and near bar 8→1. Listen on the internal synth and SEQTRAK MIDI; verify no unintended bar retrigger, no duplicate NoteOn/NoteOff, no stuck gate, and the documented loop-end release followed by the single bar-1 onset.

The E4 range operation is currently a bounded core capability with host coverage. The experiment does not add UI gestures for selecting/regenerating bars; that remains a later UX decision under the frozen non-goals.

The [2026-10-23 internal acceptance and monitor report](2026-10-23-internal-acceptance.md) records the accepted user checks, 178 clean integrity samples and 36 zero-underrun samples, plus the missing STOP/cold-boot/MIDI/SD evidence. Integration with HGS/Performance and Energy Transitions is now user-authorized.
