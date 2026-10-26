# W1 Cardputer acceptance session

Status: current-candidate session NOT RUN. Historical user persistence cycles are retained in the New/Clear follow-up; current target is an experimental test candidate, not an accepted release.

- Source base: `64ae4b8bcf13fc165a04b2b79a80b7e8b3a1f71f` (#495)
- Repair source: `9e6c3ddf434267f91b4cfca2e1b9ea374f85976e`
- Required BIN SHA-256: `349b8ea4f6a273486d3a6cb8cebb51cbfa2efe3775cfbb53e6b03f0b286277c1`
- Existing host fault-injection suite, SDL build, ADV build: PASS
- Hardware evidence directory: [`g6-selection-fix`](g6-selection-fix/README.md)

This combines W1 device actions into one session. It does not replace host event hashes, fault injection, or later #496/G2/G4/G7 acceptance. Do not mark a row PASS without recording the actual operation and evidence.

## Before starting

Record date/operator/board revision; verify the installed BIN SHA from the manifest or upload log; keep the current known project and a copy of its SD contents. Start serial capture before the test. For cold starts, remove power fully before each power-on and record elapsed time until audible drums plus Synth A and Synth B are synchronized, within 180 seconds.

## A. G1 — three cold starts

| Run | Cold power cycle | Drums + Synth A + Synth B heard/synchronized ≤180s | Reboot/panic/underrun | Elapsed seconds | Evidence file | Result |
|---|---|---|---|---:|---|---|
| 1 |  |  |  |  |  |  |
| 2 |  |  |  |  |  |  |
| 3 |  |  |  |  |  |  |

G1 passes only if all three rows pass without external debugging.

## B. G3 — edit and Undo boundaries

Use one known project and preserve its pre-test canonical event hash and Song references.

| Case | Action | Expected result | Actual result/evidence | PASS/FAIL |
|---|---|---|---|---|
| Melody edit | Change one existing Melody event, then Undo | Canonical material and Song refs exactly return to baseline; no stuck notes |  |  |
| FORM | Begin a FORM edit then cancel/Undo | Original canonical material and Song refs remain |  |  |
| DEVELOP | Attempt DEVELOP then decline/cancel | Original accepted material remains; no unrelated Undo consumed |  |  |

Record pre/post canonical event hashes and Song-reference map. If these cannot be collected, mark G3 NOT VERIFIED rather than inferring equality from sound alone.

## C. G6 — positive persistence

Use a project containing Melody A and B at 8 bars, Song references/HGS, and non-default Synth A/B patch parameters. Record canonical event, Song-reference, HGS, and patch-parameter hashes before saving.

| Cycle | Save confirmed | Full cold reset | Load confirmed | A/B 8-bar hashes match | Song/HGS/patch hashes match | Unaccepted edit absent | Serial/video evidence | Result |
|---|---|---|---|---|---|---|---|---|
| 1 |  |  |  |  |  |  |  |  |
| 2 |  |  |  |  |  |  |  |  |
| 3 |  |  |  |  |  |  |  |  |

G6 positive persistence passes only when all three cold cycles match the saved canonical hashes and an unaccepted edit is not published. Include Save As/New/Clear behavior in the same session and record each actual result.

## D. G6 — negative storage cases

Run only after the positive project is safely backed up. For missing SD and an induced read/write failure, record how the failure was induced, user-visible result, before/after canonical hashes, and whether a false success was shown. The currently running canonical project must remain intact on failed Load.

| Case | Failure induced | False success? | Canonical/project hashes preserved? | Evidence | Result |
|---|---|---|---|---|---|
| SD absent |  |  |  |  |  |
| SD read/write error |  |  |  |  |  |

Power-cut atomicity stays UNVERIFIED unless deliberately tested and recorded. Do not count serial idle captures as Save/Load evidence.

## E. Runtime observations during the same session

Capture free internal heap, minimum free heap if available, largest contiguous block, CPU/block-time, stack high-water, underruns, panic/reboot at cold start, generation, 16-bar play, Undo, Save and Load. Required 30-minute working run and paired #495 measurements are still separate acceptance conditions if this combined session is shorter or idle.
## Current conditions

The current BIN is uploaded/hash verified, but its physical gate rows remain unfilled. A user-reported quiet TB303 Acid observation is under investigation; host identical-input comparisons did not reproduce a level loss in Classic Acid or the unchanged Acid filter. Do not freeze a final RC or mark G4 PASS until the actual hardware patch/context is resolved.

The operator has no card reader. A backup/hash acquisition route over the device is not implemented or verified yet. Negative SD cases therefore remain HOLD until the positive test project's files can be protected and compared. Do not require the user to overwrite older accepted work. Older test-save payload absence/recovery remains unresolved; the old same-project census had a filename-prefix filtering limitation, so its absence claims were not exhaustive.

This session is intended as one combined G1/G3/G6 and runtime recording. Keep all timing, hash and failure evidence; no score or gate is granted solely for successful boot or audible playback.
