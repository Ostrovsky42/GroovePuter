# Phrase Bank QWERTYUI P1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an eight-slot QWERTYUI Phrase Bank for Synth A/B while preserving the existing Pattern/Phrase playback owner, musical-boundary activation path, and tight Cardputer ADV memory budget.

**Architecture:** Phrase Bank is a control/state layer over the existing `ActiveMaterial {slot, kind}` plus M4 pending-material prepare/activate path. It must not allocate eight `RuntimeSynthEventBuffer`s per voice. PLAY and EDIT are separate per-voice slot identities; playback continues to use the existing `currentPhrase_[voice]` buffer and existing Pattern/Phrase lifetime owner. Slot material is prepared into the already-bounded NEXT buffer before activation.

**Tech Stack:** C++17 firmware, existing `MiniAcid` Pattern/Phrase runtime, Cardputer keyboard UI, Python/C++ host regression tests, GitHub Actions.

**Spec:** user-approved Phrase Bank QWERTYUI checkpoint in project conversation; authoritative base `fcd0d77da5ed6ef38419547477ab26e77ec6ff26`.

## Global Constraints

- Branch: `feature/20260909-phrase-bank-qwertyui-p1`, created from exact integration SHA `fcd0d77da5ed6ef38419547477ab26e77ec6ff26`.
- Eight physical slots map exactly `Q W E R T Y U I` → `0..7`.
- Synth A and Synth B keep independent PLAY and EDIT slot identities.
- PLAYING and EDITING are separate state; selecting an edit slot must not change sounding material.
- Playback remains owned by the existing Pattern/Phrase sequenced runtime and `MusicalEventRouter` path.
- `MAKE PHRASE` remains one-way Pattern → Phrase materialization and targets only the selected EDIT slot.
- No new sequencer, note owner, scheduler, MIDI router, direct synth calls, GF2 behavior, USB/MIDI work, Scene/Chain/Song mode, copy, rename, or auto-follow.
- Do not allocate `8 × 2` resident `RuntimeSynthEventBuffer`s. Current ABI is 1284 bytes per buffer; the naive bank would consume 20,544 bytes before other state.
- Quantized PLAY switching uses the existing M4 pending-material prepare/activate musical-boundary path; no second scheduling authority.
- Re-selecting the currently sounding slot must be idempotent and must not retrigger playback.
- Hardware PASS is out of scope for CI; final firmware still requires Cardputer ADV hardware acceptance.

---

### Task 1: Characterize Phrase Bank state and memory boundary

**Files:**
- Create: `tests/test_phrase_bank_qwertyui_p1.py`
- Create after RED: `src/phrase/phrase_bank_state.h`

**Interfaces:**
- Produces `PhraseBankState`, `kPhraseBankSlotCount`, key/slot mapping helpers, and per-voice PLAY/EDIT selection with no event buffers.
- Consumes no DSP or UI owner.

- [ ] **Step 1: Write the failing test**
  - Assert exactly eight slots and QWERTYUI mapping.
  - Assert independent per-voice PLAY and EDIT identities.
  - Assert changing EDIT does not change PLAY.
  - Assert invalid slot/voice inputs are rejected without mutation.
  - Assert the state header contains no `RuntimeSynthEventBuffer` resident array and its state remains bounded to scalar slot metadata.

- [ ] **Step 2: Run test to verify it fails**
  - Run `python3 tests/test_phrase_bank_qwertyui_p1.py`.
  - Expected RED: missing `src/phrase/phrase_bank_state.h` / missing Phrase Bank API.

- [ ] **Step 3: Write minimal implementation**
  - Add a header-only fixed-size state type containing two `uint8_t` arrays (`playingSlot[2]`, `editingSlot[2]`) and exact QWERTYUI mapping helpers.
  - Do not store musical event data in this type.

- [ ] **Step 4: Run test to verify it passes**
  - Run the focused test and existing Pattern/Phrase host regressions.

### Task 2: Bind bank identity to existing material authority

**Files:**
- Modify only the smallest existing Pattern/Phrase control file(s) needed after exact-source inspection.
- Test: extend `tests/test_phrase_bank_qwertyui_p1.py` or add a focused C++ harness.

**Interfaces:**
- Consumes existing `MiniAcid::activeMaterial`, `stagePendingMaterial`, `activatePendingMaterial`, `makePhrase`, and `currentPhraseBuffer` APIs.
- Produces control-side operations: choose EDIT slot; stage PLAY slot through existing NEXT; materialize Pattern into EDIT slot.

- [ ] **Step 1: Add RED ownership tests**
  - PLAY slot must follow `ActiveMaterial.slot` only after the existing activation boundary.
  - EDIT selection must not mutate `ActiveMaterial` or `currentPhrase_`.
  - Selecting the already-playing slot is a no-op.
  - Synth A/B operations must not cross-mutate.

- [ ] **Step 2: Verify RED is for missing bank control only**

- [ ] **Step 3: Implement the minimum adapter**
  - Reuse the existing pending buffer and activation path.
  - Do not introduce a second runtime event owner.

- [ ] **Step 4: Verify focused GREEN plus P3 lifetime regressions**

### Task 3: QWERTYUI Phrase Page interaction and causality

**Files:**
- Modify: `src/ui/pages/phrase_page.h`
- Modify: `src/ui/pages/phrase_page.cpp`
- Test: focused UI source/host regression.

**Interfaces:**
- Consumes Phrase Bank control operations from Task 2.
- Produces deterministic `Q..I` selection and visible PLAY/EDIT identities.

- [ ] **Step 1: Add RED UI tests**
  - QWERTYUI maps only on the Phrase page and only to slots 0..7.
  - The screen explicitly exposes `PLAY <key>` and `EDIT <key>`.
  - Editing a non-playing slot is possible without switching playback.

- [ ] **Step 2: Verify RED**

- [ ] **Step 3: Implement minimal UI**
  - Reuse existing Phrase page keyboard handling and draw primitives.
  - Keep all current event cursor/edit behavior intact.

- [ ] **Step 4: Verify focused GREEN and P3-U1 causality regressions**

### Task 4: Exact-SHA verification and memory gate

**Files:**
- No production changes unless a verified regression requires a bounded fix.

- [ ] **Step 1: Run Phrase Bank focused gate.**
- [ ] **Step 2: Run P3 Phrase lifetime and P3-U1 UI gates.**
- [ ] **Step 3: Run Cardputer ADV product build.**
- [ ] **Step 4: Compare `.data + .bss` and DRAM budget against the base SHA.**
- [ ] **Step 5: Record exact final SHA and keep hardware status PENDING until flashed and exercised on Cardputer ADV.**
