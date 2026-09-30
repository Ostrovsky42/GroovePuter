# PML-C — allow replacement of Pattern slots (engine, session-only)

Status: **engine implemented and host-tested; no UI, no persistence.** Contract: `docs/pml/PML_B_REUSE_CONTRACT.md`.
Action name for the future UI: **"Allow replacement"** (not "free"): content stays; availability depends on the other holders.
Tags: **[OBSERVED]** measured or read from code; **[OPEN]**.

## What exists

* `ReuseMarks` (`src/state/reuse_marks.h`): per current page, slot mask + content token (64 bit) + MaterialId (32 bit) per slot. **200 B** (static_assert <= 208), no heap.
  Dropped on scene load, new scene, and page change. After a wipe that bypasses the engine a mark is inert: token and id no longer match, the verdict is `Edited`.
* `slotContentToken` (`src/state/slot_content_token.h`): every editable lane (Synth A/B, drum voices, automation, groove). Any edit changes it; identical rewrites do not; it survives save/load (PML-B).
* `SlotReuse` (`src/dsp/slot_reuse.h`): `mark`, `unmark`, `verify`, `usable`, `findRun`, `preview`, `reclaim`. Nothing in it erases anything; `reclaim` runs only inside the guarded generation commit.
* Holders that exclude reuse (protection beats the mark): Song row in either song, Phrase Bank reference, CURRENT (synth or drum selector), Working material, queued NEXT, Melody descriptor, slot range of the live generation Undo receipt.
* Generation (`generate`, `generateCycle`): `findRun` takes a run that is **free now first** and reaches for marked slots only when none exists; the commit-time safety check re-verifies token, MaterialId and holders;
  `applyPreparedPersistent` empties the three patterns **and both descriptors** of each used slot (no stale MaterialId describes new content) and ends the mark.
* Preview (`SlotReuse::preview`): longest run now and after the permitted replacement, and per-slot verdicts/holders, so a UI can say "TAKE 4B: yes; GROW 8B: no" with the holder that blocks.

## Host results (`tests/run_0_9_14_pml_c_tests.sh`, in CI)

* Marking refuses protected slots (Song, CURRENT, Undo receipt) and slots that need no mark; unmark works.
* Every holder excludes reuse: Song (both songs), Phrase Bank, CURRENT, NEXT, Melody descriptor, Undo receipt.
* A manual edit revokes the mark (verdict `Edited`), content untouched, no resurrection after reverting the edit.
* When nothing is free a marked run is used (first-fit), both descriptors reclaimed, mark consumed, every other slot untouched; `TAKE 4B` preview "not now / after the replacement: yes", `GROW 8B` impossible.
* A run that is free now is preferred; marks stay until needed.
* Refusals (edited mark in the run, no run, unsupported length) change no slot.
* **Undo gate:** a real Song Undo receipt (snapshot referencing the orphan slots) is live; after the replacing generation the single Undo slot belongs to the generation, the Song receipt cannot be read back, and the
  number of Song / Phrase Bank references that point at empty slots is unchanged before the replacement, after it and after Undo. Undo of the replacing generation restores the rows and leaves the replaced slots empty;
  the old content does not come back (as the contract states).
* Save/load after replacement: the page reloads with the new content; no old slot token remains on the page.
* Sensitivity check: three deliberate breakages each make the suite fail (no token/id check; descriptors not cleared; holders ignored).

## Not in this slice / boundaries

* No UI, no persistence of marks (a cold boot drops them: fails closed), no Melody slots, no freeing of slots held by the cycle receipt, no Undo that restores replaced content.
* **PML-C does not make repeated GROW possible by itself**: slots held by the live receipt stay protected, and 8 consecutive usable slots are needed.
* Replacement content is not durable until the page is saved (generation publishes to RAM).
* Runtime memory (stack/heap under use) is not measured.

## Firmware (FS1B dynamic-FatFs build, `scripts/build_cardputer_dynbuffers.sh`) [OBSERVED]

* Build PASS. Static DRAM (`.dram0.data + .dram0.bss`): **189792 B before, 189984 B after: +192 B** (`.data` unchanged, `.bss` +192). Budget 191488 B, headroom **1696 B -> 1504 B**.
  The estimate in the PML-B contract (about 256 B) was high; `ReuseMarks` itself is 200 B and the measured delta is 192 B after layout.
* The budget check prints `policy: provisional exception; threshold-rule items 5-7 pending`: the gate passes under a provisional exception, it is not an unconditional pass.
* Not measured: stack high-water or heap use while the feature is exercised on the device.
