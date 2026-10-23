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

The general host suite is recorded separately in `/tmp/gp-integration-host.log`; its final status must be checked before declaring the candidate ready.

## Remaining hardware gates

This merged image has not been flashed. The user accepted the previous internal-audio and persistence image. External MIDI, SD failure recovery, current-image cold boot and post-STOP heap plateau remain separate hardware gates; integration does not close them.
