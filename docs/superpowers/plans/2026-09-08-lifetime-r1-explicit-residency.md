# LIFETIME-R1 Explicit Residency Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use the repository's TDD/review workflow task-by-task. Do not batch unrelated residency changes. Every production change needs a focused RED proving the current lifetime defect before GREEN.

**Goal:** Convert GroovePuter from mostly implicit session residency to explicit, measured ownership lifetimes without weakening realtime behavior or musical concurrency.

**Architecture:** Keep the permanent musical realtime core fixed. Optional services acquire resources on demand and release them after their last legitimate responsibility. Material preparation remains control-side and fixed-size; AudioTask only consumes prepared state. Filesystem and cold scratch resources are phase-owned rather than permanently convenient.

**Tech Stack:** ESP32-S3/Cardputer ADV, Arduino-ESP32/ESP-IDF FreeRTOS/FatFs, C++ firmware, Python/C++ host contract tests, GitHub Actions, existing MEMORY-R1 FS1B build/evidence pipeline.

**Spec:** `docs/contracts/0_9_10_LIFETIME_R1_RESOURCE_OWNERSHIP.md`

**Authoritative base inspected:** `integration/20260908-0.9.10-memory-r1-product-closure @ a3e821b4f1bb2b118e7d13540518b249ef8b7721`.

## Global constraints

- Do not reconstruct work from the older MEMORY-R1 or Pattern/Phrase branch heads.
- No production semantic changes in L1.
- No allocator calls, task creation/destruction, filesystem I/O, or blocking in AudioTask/sequencer/bar-boundary publication.
- Do not reduce AudioTask, USB MIDI, SMF, sampler, or other stacks without hardware HWM evidence.
- Do not invent a sampler task if the current line no longer has one; historical 4096 B evidence is not current ownership evidence.
- Preserve accepted MEMORY-R1 FS1B dynamic-FatFs configuration and static DRAM gate.
- Preserve Pattern/Phrase lifetime semantics and all current MIDI/internal lifetime ownership.
- Prefer M2 per-synth pending material if measured peak-live-set evidence accepts +2568 B; do not choose M1 merely to win a headline RAM number.
- A forbidden overlap requires a product-semantic justification and a regression test.

---

## Task 1 — L1 authoritative resource census and diagnostic vocabulary

**Files:**
- Spec already created: `docs/contracts/0_9_10_LIFETIME_R1_RESOURCE_OWNERSHIP.md`
- Create: `src/diag/lifetime_census.h`
- Create: `tests/test_lifetime_r1_l1_contract.py`
- Create: `tests/run_lifetime_r1_tests.sh`
- Modify only compile-gated observation call sites where necessary after RED.

- [ ] Write source-contract RED requiring a default-off `GROOVEPUTER_DIAG_LIFETIME_CENSUS` diagnostic with internal-free, largest-internal, minimum-ever-internal and raw task HWM reporting. The test must reject an always-on diagnostic and reject a fake allocation-count field.
- [ ] RED also freezes known current source facts: AudioTask stack = 8192 B; SMF task stack = 6144 B; SMF boot remains deferred; SMF worker currently has no release API at the start of L1.
- [ ] Implement only the diagnostic header and default-off probes. No runtime behavior changes.
- [ ] Add named L1 observation points at boot/product-ready and existing control-side phase transitions only where this can compile to nothing when disabled.
- [ ] Run focused test, existing source-contract regressions, Cardputer FS1B build and static DRAM gate in CI.
- [ ] Record CI/build SHA and mark Task 1 complete only from actual evidence.

## Task 2 — L2 SMF post-use residency RED

**Files:**
- Modify: `tests/test_lifetime_r1_l1_contract.py` or create focused SMF lifetime test if current test organization warrants it.
- Inspect: `src/platform/cardputer_smf_player.{h,cpp}` and registry/UI callers.
- No production change until semantic teardown boundary is proven.

- [ ] Characterize all SMF states: Unloaded, loaded/paused, Armed/Project, Playing, transport blocked, EOF, Stop, file replacement.
- [ ] Determine which state must survive leaving the SMF page and which state may release worker/parser/file resources.
- [ ] RED the actual desired release boundary. The test must fail because current `taskLoop()` is unbounded and no release API exists.
- [ ] Prove whether replay of an already-loaded file requires retained `SdByteSource`, timing map capacity, stream merger state, or only the storage path/metadata.
- [ ] Ruling: do not equate `Stop` with `Unload` unless current UI/product semantics already treat them as equivalent.

## Task 3 — L2 SMF bounded worker lifetime GREEN

**Files:**
- Modify: `src/platform/cardputer_smf_player.h`
- Modify: `src/platform/cardputer_smf_player.cpp`
- Modify registry only if restart semantics require it.
- Extend focused lifetime tests.

- [ ] Add a control-side/task-side shutdown handshake; never externally delete a running worker while it owns stream/file state.
- [ ] Worker must close its file, invalidate scheduled events/panic as required, publish a safe state, clear/hand off its task handle without a race, then self-delete.
- [ ] A later legitimate SMF action must recreate the worker safely.
- [ ] Decide timing/parser capacity ownership explicitly. If retained capacity remains after worker exit, document it; if released, do it outside realtime and prove re-entry.
- [ ] Add before/create/after-release census markers behind the L1 diagnostic flag.
- [ ] Repeat start/load/stop-or-unload/restart ×50 in hardware evidence; free and largest must return to an explained plateau without ratchet.
- [ ] Preserve Project/SEQTRAK pause/relaunch semantics and transport-stall behavior.

## Task 4 — Current sampler residency truth

**Files:**
- Inspect current `src/sampler/*`, AudioTask, sample store/loader, recording and streaming code.
- Tests/docs only until a current dedicated task or transient owner is proven.

- [ ] Prove whether a separate sampler I/O task exists on this line. Historical 4096 B task stack is not enough.
- [ ] Map fixed sample pages/cache, file handles, decode/load buffers, and any control-side temporary allocations.
- [ ] If there is no dedicated sampler task, close that historical optimization candidate explicitly and do not add one.
- [ ] If a task exists and is idle-resident, write RED for its true last-responsibility boundary before implementing lazy lifetime.

## Task 5 — Current FatFs/file-handle lifetime after FS1B

**Files:**
- Inspect accepted FS1B build scripts plus every long-lived `File`/filesystem owner in Scene, samples, SMF, autosave/recovery.
- Extend L1 resource matrix and tests.

- [ ] Treat stock `5*FIL+FATFS = 24,832 B` only as historical comparison. Record the compiler-measured dynamic FS1B structures from the current build evidence.
- [ ] Enumerate application `File` owners and their open/close points.
- [ ] Identify handles that remain open beyond their musical/system responsibility.
- [ ] RED one owner at a time; do not create a generic file pool or giant scratch owner.
- [ ] Verify repeated open/use/close churn against `free` and `largest` on hardware.

## Task 6 — L3 M2 peak-live-set census

**Files:**
- Reuse/extend existing `src/diag/melody_pending_census.h` rather than creating a competing pending-buffer instrument.
- Extend lifetime docs/tests; no Pattern/Phrase semantic redesign.

- [ ] Measure M1 (+1284 B shared pending) and M2 (+2568 B per-synth pending) under the same FS1B image and scenario.
- [ ] Required overlap: active A+B + pending A+B + normal MIDI + display + filesystem residency + sampler streaming only if musically valid concurrently + Undo/autosave state that truly overlaps.
- [ ] Capture free/largest/min-ever and task HWM at prepare, both-pending-ready, boundary activation and post-activation plateau.
- [ ] Prefer M2 if accepted; reject it only with evidence, not intuition.

## Task 7 — L3 ACTIVE/NEXT/OLD ownership

**Files:** current Pattern/Phrase runtime owner only; no second scheduler/publication owner.

- [ ] RED that control-side NEXT preparation does not alter audible ACTIVE material before the accepted boundary.
- [ ] RED that OLD remains readable only until the last legitimate realtime reader and is then reclaimable/reusable.
- [ ] Implement fixed per-synth pending ownership if M2 is accepted.
- [ ] Keep activation selection-only and allocation-free in AudioTask.
- [ ] Prove Stop/Pause/source transitions do not create stale OLD ownership or duplicate releases.

## Task 8 — L3 bounded disk-backed Melody transaction

**Files:** existing Scene/storage/material owners; exact paths determined after Task 7.

- [ ] Introduce compact `PATTERN | MELODY` material reference without embedding full Melody payloads in Scene.
- [ ] RED transactional promotion: failed write/validation leaves PATTERN unchanged.
- [ ] GREEN: prepare candidate -> write -> validate -> publish reference.
- [ ] Without SD, new promotion/load is rejected explicitly.
- [ ] If SD disappears after load, current working Melody remains playable/editable and becomes dirty/not-saved.
- [ ] All disk work remains outside realtime paths.

## Task 9 — L4 shared cold scratch only from proven exclusion

- [ ] Build overlap proof for save/load/recovery scratch owners.
- [ ] Use typed bounded shared storage only where intersections are proven empty.
- [ ] Reject a generic anonymous byte arena.
- [ ] Add control-flow/source-contract tests for the exclusion.

## Task 10 — L5 CPU lifetime census and evidence-backed idle work

- [ ] Instrument audio callback duration/max, active engine/sampler counts, FX active/tail/idle state and UI transfer timing with default-off diagnostics.
- [ ] Measure before changing DSP.
- [ ] Add tail-aware FX idle bypass only if measured waste exists; never cut audible tails.
- [ ] Separate synth object residency from DSP activity; do not reconstruct voices on mute.
- [ ] Consider display dirty/unchanged suppression only if transfer measurement shows it matters.

## Task 11 — Final acceptance

- [ ] Run focused LIFETIME-R1 tests and all inherited relevant host suites.
- [ ] Run accepted FS1B Cardputer build and static DRAM gate.
- [ ] Hardware scenarios: boot, music idle/busy, Pattern, Phrase, M2 prepare/activate, sample activity, SMF, autosave/recovery, load/save, repeated switching ×50.
- [ ] Produce final table: resource/phase, before, after, resident bytes saved, peak cost, largest before/after, CPU effect, musical consequence, evidence.
- [ ] Produce final lifetime graph with resources gained/released at each transition.
- [ ] Do not mark checkpoint GREEN while any unexplained `largest` ratchet, underrun, panic/reset, realtime allocation, stale material, SD-removal loss, or task-lifecycle race remains.
