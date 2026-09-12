# M-WORKING Remaining Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the seven remaining 0.9.11 M-WORKING RED witnesses without moving Accepted truth implicitly, without rebinding Working on playback-only source changes, and without broad low-level setter rewrites.

**Architecture:** Keep `Scene` as ACCEPTED authority, `WorkingMaterialStorage[2]` as session Working, and the resident runtime event bank as the audible Pattern publication. Every user edit is PREPARE -> VALIDATE -> COMMIT. Manual target changes are guarded above the low-level playback/runtime setters; acceptance/discard/undo/migration/recovery use explicit domain operations rather than implicit Scene mutation.

**Tech Stack:** C++17 host tests, Python source-contract tests, GitHub Actions, ESP32/Cardputer PlatformIO build.

**Spec:** Existing M-WORKING contracts in `tests/test_0_9_11_m_working.cpp`, `tests/test_0_9_11_m_working_a2b_mwk.cpp`, `tests/test_0_9_11_m_working_manual_pattern_edit.cpp`, and A2-B Material identity contracts.

## Global Constraints

- Canonical starting SHA is `18bb102bc0630880beab884b28b5b0ae1d21f3a5` with `6 GREEN, 7 RED, 0 STOP`.
- `MaterialId`, not physical address alone, owns identity; ABA/address reuse must fail closed.
- Playback-only Pattern/Phrase source switching must never clear, accept, or rebind retained Working.
- Do not globally redirect `editSynthPattern()` or low-level Pattern setters; generation, Atlas, Song/NEXT and runtime paths retain their existing authority.
- No production change without a genuine failing test on the immediately preceding test-only commit.
- Every GREEN promotion requires exact-head CI and no `M-WORKING STOP`.
- Hardware smoke is separate from host/build evidence and may not be claimed without a physical Cardputer run.

---

### Task 1: MW-L manual target-switch guard

**Files:** create focused host/source-contract tests; modify `src/dsp/miniacid_engine.h/.cpp` only for a bounded manual-retarget API; modify `src/ui/pages/sequencer_hub_page.cpp` and `src/ui/pages/pattern_edit_page_legacy.h` only at manual pattern/bank target actions.

**Interface:** `tryManual303TargetSwitch(voice, bank, pattern)` (or equivalent bounded API) returns false when modified exact-bound Working would be orphaned by a different target, true for same target or clean Working. Low-level `set303PatternIndex`/`set303BankIndex` remain unguarded for non-manual runtime owners.

- [ ] Write RED covering modified refusal, same-target success, clean-target success, source-toggle independence, other-voice independence, and UI callers.
- [ ] Observe exact RED before production.
- [ ] Implement PREPARE -> VALIDATE -> COMMIT manual retarget with no partial bank/pattern movement on refusal.
- [ ] Route only manual Hub/Pattern-editor target actions through it.
- [ ] Promote MW-L to GREEN; require broad count `7 GREEN, 6 RED, 0 STOP`.

### Task 2: MW-A + MW-C remaining Pattern edit transaction surfaces

**Files:** focused Pattern-edit transaction tests; `miniacid_engine.h/.cpp`; UI source-contract tests/callers as needed.

**Interface:** bounded Working mutations for note clear/add, octave, accent, slide and step FX surfaces actually exposed by current UI. Each mutation clones Accepted/current exact-bound Working, changes candidate, publishes candidate to runtime bank, then stores identity-bound Working. Scene/revision stay unchanged.

- [ ] RED every currently user-reachable legacy Pattern mutation that still writes Scene.
- [ ] Observe RED with Accepted A unchanged expectation and audible runtime B expectation.
- [ ] Implement common candidate publish/store helper and the minimal Working mutation wrappers.
- [ ] Route user edit surfaces only; leave generation/randomization authority untouched.
- [ ] Promote MW-A and MW-C; require `9 GREEN, 4 RED, 0 STOP`.

### Task 3: MW-H Pattern -> Melody migration from Working B

**Files:** focused migration host test; `miniacid_engine.cpp` around `makePhrase`/Pattern projection.

**Interface:** `makePhrase()` projects exact-bound Working Pattern when present, otherwise Accepted Pattern, without accepting B into Scene first.

- [ ] RED: Accepted note 60, Working note 67, `makePhrase()` must yield Melody 67 while Accepted remains 60.
- [ ] Observe RED.
- [ ] Change only projection-source selection.
- [ ] Verify source round-trip and identity gates remain GREEN.
- [ ] Promote MW-H; require `10 GREEN, 3 RED, 0 STOP`.

### Task 4: MW-E explicit discard/accept lifecycle

**Files:** lifecycle host tests; `working_material_storage.h` only if a bounded clear primitive is required; `miniacid_engine.h/.cpp` for domain operations.

**Interfaces:** `discardWorking303Pattern(voice)` clears only exact-bound Working and republishes Accepted A to the audible Pattern bank; explicit accept/promote commits exact-bound Working B to Accepted using the canonical Scene mutation/revision boundary, republishes, then clears Working.

- [ ] RED discard and explicit accept semantics, including foreign identity fail-closed behavior.
- [ ] Observe RED.
- [ ] Add bounded storage clear if needed and explicit domain operations.
- [ ] Verify discard never mutates Accepted and accept never happens implicitly.
- [ ] Promote MW-E; broad target `11 GREEN, 2 RED, 0 STOP` (accept is lifecycle support even if not a separate broad ID).

### Task 5: MW-D Working-owned Pattern Undo

**Files:** focused Undo test; undo receipt helpers and `miniacid_engine` Pattern Working edit integration only.

**Interface:** Pattern edit receipts exchange Working candidates/session state. Undo/redo must not mutate Accepted Scene or scene revision before explicit accept.

- [ ] RED an edit B followed by Undo/Redo while Accepted A and revision remain fixed.
- [ ] Observe RED.
- [ ] Add/route a bounded Working Pattern undo receipt.
- [ ] Verify Melody RuntimePhrase undo is unchanged.
- [ ] Promote MW-D; require `12 GREEN, 1 RED, 0 STOP`.

### Task 6: MW-F recovery/persistence isolation

**Files:** focused recovery host test and persistence source-contract test; autosave/recovery boundary only if current serialization reads session Working.

**Interface:** autosave/recovery serializes Accepted Scene only until explicit accept; unaccepted Working is session-only and disappears across restart.

- [ ] RED Accepted A + Working B -> recovery reload must equal A.
- [ ] Observe RED.
- [ ] Remove any implicit Working-to-Scene leakage at autosave/recovery boundary; do not add a second persistence owner.
- [ ] Promote MW-F; require `13 GREEN, 0 RED, 0 STOP` and runner status 0.

### Task 7: MW-N exact-head embedded build/memory gate

**Files:** reuse existing Cardputer build/memory workflows and evidence scripts; add only missing measurement wiring if necessary.

- [ ] Build exact final SHA for Cardputer ADV.
- [ ] Record ELF/hash and DRAM/static-memory evidence against the established budget.
- [ ] Run full host/runtime/ownership suite on the same SHA.
- [ ] Preserve artifacts and exact provenance.
- [ ] Prepare a short physical smoke checklist for Pattern edit -> retarget refusal -> discard/accept -> make Phrase -> Undo/recovery; do not claim the physical smoke until run on hardware.

## Self-review

Coverage: MW-L, MW-A/C, MW-H, MW-E, MW-D, MW-F and final MW-N are each isolated into a reviewable RED->GREEN wave. MW-B/G/I/J/K/O and A2-B/source-round-trip are preservation gates, not implementation work. Low-level setters, generation semantics and Song/NEXT runtime retarget remain out of manual-guard ownership.
