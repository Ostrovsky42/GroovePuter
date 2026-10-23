# Long Melody E0–E6 Readiness Plan

> Continue the user-authorized work natively in this worktree. Do not delegate.

**Goal:** Finish host-side E0–E6 preparation, build exact Cardputer diagnostic images, and perform the final device acceptance run when the board is attached.

**Frozen spec:** `/home/gg/.codex/attachments/b7ebe313-af43-49e3-90b1-26a1de9e1ffe/pasted-text-1.txt`, Long Melody Viability / Scaling / UX Gate, sections 5–12.

## Constraints

- Prove one 8-bar Melody object first. Do not open 16-bar support before E0–E6 pass.
- A bar is a local edit/navigation region; it does not own a separate Melody.
- Keep production UI redesign, per-bar routing, unbounded undo, a parallel engine, and 16-bar support deferred.
- Host tests and build preparation may proceed while the Cardputer is absent. Hardware E1–E6 interpretation remains gated on a reproducible E0 baseline.
- Use the no-census diagnostic image for E0 baseline; use the M2 census image only after that baseline capture.
- Keep diagnostic instrumentation and census compile-time/runtime-only. Require the candidate-only FatFs link proof for both images.

## Host-side work

- [x] Verify E0 telemetry source coverage and primary/backup Song-loaded state.
- [x] Repair and pass the memory-instrumentation source regression checks.
- [x] Build no-census E0 image and M2 census image from this worktree.
- [x] Verify the E0/M2 telemetry markers, static DRAM reports, and candidate-only FatFs link maps.
- [x] Add bounded onset movement and atomic bar-range extraction/replacement primitives with fail-closed boundary, collision, and capacity behavior.
- [x] Add local edit, bars 5–6 replacement, outside-range preservation, one-step Undo/Redo, and 100-cycle A/B/store churn tests.
- [x] Add explicit real-MiniAcid coverage for cross-bar gates and the 8→1 loop policy.
- [x] Run the existing P3/P3-U1 and focused Phrase editing/Undo suites.

## Device acceptance sequence

1. Capture the first and repeat cold boots with the newly installed source-tagged GEN-diagnostic no-census image (`build/long-melody-e0-gen-diagnostic/GroovePuter.ino.bin`, flashed hash `50d15ed9…`). Start Serial before physical Reset with G0 released. Capture SD mounted, Song loaded, idle/playing, Pattern A+B, page churn, and slot switching. Compare heap/largest-block plateaus, minimum heap, stack HWM, SD latency, underruns, and boot/panic state. Capture one G attempt at STOP and PLAY to resolve the reported GEN failure.
2. Only after E0 is reproducible, flash the source-tagged M2 image from `build/long-melody-e0-m2-source-diagnostic/GroovePuter.ino.bin` (prior flashed hash `545955f1…`). Confirm the `[CENSUS]` snapshots for A active, A active + pending A, A+B active, and M2 active + pending A+B; require four successful bounded allocations, no load failures, and stable free/largest heap through 40 reload/activation cycles. Confirm the uniquely named `/__gp_diag/lm-*.bin` file is removed. The source-tagged image is required for the later A/B source matrix.
3. On the same diagnostic image, run the agreed E2–E6 manual matrix: eight-bar cross-boundary playback and 8→1 loop; A/B Pattern/Melody combinations with NEXT and drums/FX; local edit/replace and Undo where the existing editor exposes the operation; remove/restore SD to verify working playback and honest save failure; repeat PLAY/STOP, page/slot navigation, NEXT prepare/cancel, save/load, and edit cycles.
4. Mark any hardware gate green only from captured serial/audio evidence. A successful firmware build or host test does not pass a device gate.

## Hardware diagnosis notes

- Open serial capture before physical reset and leave G0 released. If no telemetry appears, record the screen and boot mode before changing firmware.
- `GROOVEPUTER_MELODY_CENSUS=2` adds four 1,284-byte internal diagnostic buffers (two active stand-ins and two pending) to deliberately stress residency; the product's own Undo object and audio/filesystem state stay live. Interpret measured heap deltas with this extra diagnostic residency in mind.
- Do not use one free-heap reading as acceptance. Compare repeated runs and post-churn plateaus, largest contiguous block, minimum heap, task high-water marks, and audio underruns.

**Current evidence:** See `docs/testing/long-melody/2026-10-21-e0-e6-readiness.md`. Host-side preparation is complete. Cardputer is enumerated at `/dev/ttyACM0`; the latest source-tagged no-census E0 image (`50d15ed9…`) was flashed and hash-verified, and its 180-second idle log is stable, but no G attempt or cold boot was captured. The previous image `31e12c81…` ran Pattern PLAY/STOP without underruns; `2d97ac49…` yielded one clean boot and a same-boot post-churn plateau. Capture G diagnostics and repeated cold boots plus controlled slot switching on `50d15ed9…`; then continue the M2 image sequence for E1–E6.
