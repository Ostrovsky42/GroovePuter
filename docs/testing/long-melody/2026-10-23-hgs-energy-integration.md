# HGS / Energy FORM / accepted Long Melody integration

Candidate branch: `integration/20261023-long-melody-hgs-energy`.

## Provenance

- HGS / advanced Melody grid / published Pattern harmony: `4f9bc63a`.
- Energy FORM lineage: `50ed4048`, merged as `a9ef1251`.
- Accepted 8-bar runtime, note display and persistence: `ec6bba6a`.
- Existing HGS popup work imported from a snapshot of its working tree; original worktree unchanged. Enter on KEY opens SCALE, arrows change tonic, H opens harmony strategy.

Energy tip `6d107e8b` also contains an alternative Performance implementation. It was not merged: the integration retains the evolved HGS Performance/ARP ownership model. FORM transitions were merged from the preceding coherent lineage instead. The optional tip memory patch was not required by the measured budget.

## Merge resolutions and compatibility fixes

- Project cleanup retains both Song harmonic storage cleanup and Melody storage cleanup.
- Failed generation retains HGS diagnostics; an occupied queue reports GEN BUSY.
- Runtime instrumentation anchors on the synth migration call, preserving the subsequent HGS harmonic publication.
- Added the required alias-safe include guard to genre_harmony_selection.h; pre-flash regression test found the missing guard.

## Verification

PASS: fresh Pattern/Phrase P3 suite, including project copy/load/New/Clear and note-display truth; full HGS suite; Smart Play suite; Performance boundary UI; MIDI/Performance UI polish; selection popup; Energy curve; memory runtime source closure and pre-flash source checks.

Cardputer runtime build with candidate-only dynamic FatFs and PSRAM disabled: PASS. Fixed DRAM 189132 bytes / provisional limit 191488, headroom 2356 bytes; flash 1632550 bytes. This is a build budget result, not a hardware runtime-memory acceptance.

SDL build: PASS. Focused interactive inspection on X display :1: SCALE popup renders; choosing Phrygian applies it; Right changes C to C# while retaining the chosen scale; H opens the separate GENRE/AUTO/GRAPH popup. Evidence: `/tmp/gp-integrated-sdl-20261023/` (genre.png, scale-list.png, scale-applied-key-changed.png, harmony-list.png). Dummy audio was used; this does not validate sound or physical MIDI.

General host runner `/tmp/gp-integration-host.log` passed its prefix through generated Melody and Stage 12, then stopped at stale GENRE presentation assertions inherited from the earlier UI. The two source tests were updated to require the current footer and retain the requested-profile tempo invariant. Production behavior was unchanged. The failed stage and remaining tail were then run separately:

- Stage 13/14/15 host matrix (GCC, Clang, ASan/UBSan): PASS, `/tmp/gp-integration-generation13.log`.
- Unified Song ownership/playback/display: PASS, `/tmp/gp-integration-unified.log`.
- Melody slot navigation: PASS, `/tmp/gp-integration-melody-nav.log`.
- TEMPO UI, new empty Melody, chord editing/playback, Synth MORE restore: PASS, `/tmp/gp-integration-ui-tail.log`; linked against the fresh P3 engine archive.
- Synth engine switch ASan/UBSan: PASS, `/tmp/gp-integration-engine-switch.log`.
- Both 8-bar Melody voices across separate OS processes, including ignoring unaccepted edits: PASS, `/tmp/gp-integration-process-restart.log`; linked against the fresh P3 engine archive.

Thus the host verification was completed in a prefix plus resumed stages, not a second uninterrupted invocation of the umbrella runner.

Final runtime binary SHA-256: `72334e3986929ac98089a742928ea9773bbb591d00792e5c748d3fd6fbea78c3`. Build log: `/tmp/gp-integration-cardputer-final.log`. Integration merge: `89989c02` (only tests/report updated afterward).

## Device acceptance — closed by user

The merged diagnostic image `72334e39…` was flashed successfully on 2026-10-23. Upload exited 0, flash hash verification succeeded, and `/dev/ttyACM0` returned. The bounded monitor is `logs/serial-60s-20261023-172650.log`; initial samples had intact heap and zero audio underruns.

After flashing, the requested grouped checks covered both 8-bar Melody voices and generation, separate scale/harmony selection, and Save → Reset → Load. The user reported “все ок” and explicitly closed acceptance. This closes the user-facing acceptance of the merged version; no repeat of the same acceptance checklist is required.

External MIDI, SD failure recovery, controlled cold-boot repeatability and post-STOP heap plateau have not received separate new evidence in this acceptance. These remain technical validation backlog and are not represented as verified by the user's general acceptance. The original E0–E6 evidence goal is therefore not marked fully complete.
