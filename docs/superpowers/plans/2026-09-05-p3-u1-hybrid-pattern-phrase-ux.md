# P3-U1 Hybrid Pattern / Phrase Product UX Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the existing Synth A/B NOTES page project and edit either the existing Pattern model or the already-implemented P3 runtime Phrase model, including real cross-bar duration editing, without adding a scheduler, lifetime owner, heap-backed event container, Scene ABI change, or DRAM-ceiling increase.

**Architecture:** Preserve `PatternEditPage` as the single NOTES surface and leave its Pattern path behavior-compatible. Add a small bounded Phrase edit domain that prepares a complete `RuntimeSynthEventBuffer`, validates it, and atomically replaces the per-voice engine buffer under the existing `AudioGuard`; `RuntimeSynthPlaybackState` remains the only sounding-lifetime owner. Reuse the single existing 1536-byte `UndoOwner` slot with a distinct runtime-Phrase receipt path that does not mark the Scene dirty, because P3 Phrase material is not represented in the current Scene codec.

**Tech Stack:** C++17, bounded fixed arrays, existing MiniAcid engine/P3 runtime, existing UI/IGfx/Cardputer input, existing UndoOwner, SDL host build, GitHub Actions, Arduino Cardputer ADV build scripts.

**Spec:** User-approved checkpoint `0.9.10 — P3-U1 HYBRID PATTERN / PHRASE PRODUCT UX`; frozen P3 hardware evidence in `docs/testing/0_9_10_P3_PHRASE_DRAM_CHARACTERIZATION.md`.

## Global Constraints

- Exact implementation base: `020582e19933e0c83bc62ad7a05bced2ceab2b23`.
- Phrase capacity remains exactly 128 events.
- Phrase lengths remain exactly 1/2/4/8 bars; 96 PPQN, 384 ticks/bar, 16 subticks/tick.
- Editing grids are exactly 1/8=48 ticks, 1/16=24 ticks, 1/32=12 ticks.
- One Phrase note is one `PhraseRuntime::RuntimeSynthEvent`; continuation is visual only.
- `Alt+Right` extends one grid quantum; `Alt+Left` shortens one grid quantum; no key-repeat ownership.
- Source identity is `MiniAcid::SequencedSource` per voice; no UI-owned duplicate boolean.
- UI must never directly mutate the live Phrase buffer; PREPARE -> VALIDATE -> atomic COMMIT is mandatory.
- No new realtime heap allocation, realtime sorting, scheduler, transport, PPQN, lifetime owner, Phrase capacity, Undo stack, or Scene ABI.
- Existing DRAM ceiling remains 191488; inherited base fixed DRAM is 192904 (+1416 debt).
- P3 runtime Phrase remains session-only unless a separately versioned persistence checkpoint is implemented.
- Pattern behavior is a firewall: existing note/accent/slide/FX/copy/paste/selection/bank/pattern/generation/Undo/Song/transport/live/MIDI behavior must remain compatible.

---

### Task 1: Source-aware NOTES boundary

**Files:**
- Modify: `src/ui/pages/pattern_edit_page.h`
- Modify: `src/ui/pages/pattern_edit_page.cpp`
- Create: `tests/test_p3_u1_source_projection.py`
- Create: `tests/run_pattern_phrase_p3_u1_tests.sh`
- Create: `.github/workflows/0_9_10_pattern_phrase_p3_u1_hybrid_pattern_phrase_ux.yml`

**Interfaces:**
- Consumes: `MiniAcid::currentSequencedSource(int)`.
- Produces: a single Pattern/Phrase dispatch boundary in `PatternEditPage` without a second source flag.

- [ ] Write a source-regression RED proving Pattern and Phrase dispatch are selected from `MiniAcid::SequencedSource`, including A/B independence.
- [ ] Push only the test/workflow change and observe CI fail because no Phrase NOTES projection exists.
- [ ] Add the minimal source-aware draw/event dispatch while leaving the Pattern branch delegated to the retained implementation.
- [ ] Re-run the focused gate and commit GREEN.

### Task 2: Read-only Phrase projection

**Files:**
- Create: `src/ui/phrase_notes_projection.h`
- Create: `src/ui/phrase_notes_projection.cpp`
- Create: `tests/test_p3_u1_phrase_projection.cpp`
- Modify: `tests/run_pattern_phrase_p3_u1_tests.sh`

**Interfaces:**
- Consumes: `const RuntimeSynthEventBuffer&`, cursor tick, visible bar window.
- Produces: fixed-size visual span descriptors containing onset tick, end tick/subtick, note, cursor coverage, and bar-boundary position; no generated musical events.

- [ ] RED: event start=360 duration=96 ticks yields one onset/span crossing bar boundary 384 and end=456.
- [ ] Observe RED.
- [ ] Implement a pure read-only projection over the existing event buffer; continuation cells/spans are render data only.
- [ ] GREEN and commit.

### Task 3: Bounded Phrase mutation owner

**Files:**
- Create: `src/phrase/runtime_phrase_edit.h`
- Create: `src/phrase/runtime_phrase_edit.cpp`
- Modify: `src/dsp/miniacid_engine.h`
- Modify: `src/dsp/miniacid_engine.cpp`
- Create: `tests/test_p3_u1_phrase_mutation.cpp`
- Modify: `tests/run_pattern_phrase_p3_u1_tests.sh`

**Interfaces:**
- Produces: `RuntimePhraseEdit::validate(...)`, bounded prepare helpers, and `MiniAcid::commitPreparedPhrase(...)` that replaces one complete per-voice buffer in one assignment.
- Validation rejects count>128, invalid length, zero duration, start outside length, and event end beyond phrase length.

- [ ] RED invalid count/duration/end/voice cases and no-live-state-change on rejection.
- [ ] Observe RED.
- [ ] Implement pure validation/preparation and complete-buffer commit; no heap allocation and no live partial writes.
- [ ] GREEN and commit.

### Task 4: Insert/delete and deterministic event lookup

**Files:**
- Modify: `src/phrase/runtime_phrase_edit.h`
- Modify: `src/phrase/runtime_phrase_edit.cpp`
- Modify: `src/ui/pages/pattern_edit_page.h`
- Modify: `src/ui/pages/pattern_edit_page.cpp`
- Modify: `tests/test_p3_u1_phrase_mutation.cpp`

**Interfaces:**
- Produces: snapped insert, bounded delete, and `eventCoveringTick(...)`.
- Cursor coverage rule: among covering events choose the latest onset; equal-onset ties choose the lower buffer index, matching the runtime first-match rule at equal start ticks while respecting monophonic preemption for later onsets.

- [ ] RED empty insert: count+1, snapped `startTick`, one event, one-grid default duration.
- [ ] RED capacity 128 rejects event #129 with byte-identical retained candidate.
- [ ] RED cursor-inside-span selects the covering event and empty cursor selects none.
- [ ] Implement bounded in-place insertion/deletion and deterministic coverage lookup.
- [ ] GREEN and commit.

### Task 5: Duration resize and grid semantics

**Files:**
- Modify: `src/phrase/runtime_phrase_edit.h`
- Modify: `src/phrase/runtime_phrase_edit.cpp`
- Modify: `src/ui/pages/pattern_edit_page.h`
- Modify: `src/ui/pages/pattern_edit_page.cpp`
- Modify: `tests/test_p3_u1_phrase_mutation.cpp`

**Interfaces:**
- Produces: grid quantum mapping {48,24,12} ticks and resize by exactly one quantum.

- [ ] RED 1/16 event start=360 duration=24, `Alt+Right` x3 -> duration=96/end=456/count unchanged; one `Alt+Left` ->72.
- [ ] RED shrink clamps at one current grid quantum and cursor with no covering event is a no-op.
- [ ] RED grid change never mutates stored `startTick` or `durationSubticks`.
- [ ] Implement discrete key-down resize before retained Pattern Alt-arrow routing.
- [ ] GREEN and commit.

### Task 6: Phrase rendering on the 240x135 NOTES surface

**Files:**
- Modify: `src/ui/pages/pattern_edit_page.cpp`
- Modify: `src/ui/pages/pattern_edit_page.h`
- Modify: `src/ui/phrase_notes_projection.*`
- Create/modify: `tests/test_p3_u1_source_projection.py`

**Interfaces:**
- Phrase renderer shows source, length, grid, onset, continuation span, cursor, bar boundary, and playhead using the same NOTES page.

- [ ] RED static/host assertions for required source/length/grid status and use of read-only span projection.
- [ ] Render a compact Phrase timeline rather than a piano roll; Pattern draw functions remain untouched.
- [ ] GREEN and commit.

### Task 7: Phrase length PREPARE -> VALIDATE -> COMMIT

**Files:**
- Modify: `src/phrase/runtime_phrase_edit.*`
- Modify: `src/ui/pages/pattern_edit_page.*`
- Modify: `tests/test_p3_u1_phrase_mutation.cpp`

- [ ] RED valid lengths 1/2/4/8 accepted; 0/3/5 rejected without mutation.
- [ ] RED shrink rejects if any event end exceeds target length.
- [ ] Implement length mutation on a prepared copy and commit only after full validation.
- [ ] GREEN and commit.

### Task 8: Explicit MAKE PHRASE conversion

**Files:**
- Modify: `src/dsp/miniacid_engine.h`
- Modify: `src/dsp/miniacid_engine.cpp`
- Modify: `src/ui/pages/pattern_edit_page.*`
- Create: `tests/test_p3_u1_make_phrase.cpp`
- Modify: `tests/run_pattern_phrase_p3_u1_tests.sh`

**Interfaces:**
- Produces: an engine-side helper that prepares the current Pattern using the existing `projectPatternToRuntimeEvents(...)` settings used by Pattern runtime publication; no duplicate projector.
- Conversion preserves the current Phrase length, replaces Phrase event material explicitly, leaves Pattern unchanged, and selects Phrase only as part of the explicit semantic operation.

- [ ] RED Pattern remains byte-identical, prepared Phrase matches authoritative projection, source change is explicit.
- [ ] RED non-empty Phrase refuses ordinary MAKE PHRASE; explicit overwrite path is deterministic.
- [ ] Reuse existing projection machinery; do not duplicate Pattern/TIE semantics.
- [ ] GREEN and commit.

### Task 9: Single-owner runtime Phrase Undo

**Files:**
- Modify: `src/state/bounded_undo_slot.h`
- Modify: `src/state/undo_owner.h`
- Modify: `src/state/undo_receipts.h`
- Modify: `src/ui/pages/pattern_edit_page.*`
- Create: `tests/test_p3_u1_phrase_undo.cpp`

**Interfaces:**
- Add a distinct runtime-Phrase Undo kind/receipt to the existing 1536-byte owner. Receipt contains voice identity, prior source identity, and one 1284-byte before-image; it fits without increasing `kUndoPayloadBytes`.
- Runtime-Phrase commit/toggle uses Scene revision only as an expiration token and does **not** call `markSceneMutated()`, because P3 Phrase is session-only.

- [ ] RED exact restoration for insert/delete/extend/shrink/length/MAKE PHRASE and no cross-kind decode.
- [ ] RED `sizeof(receipt)<=1536`, `kUndoPayloadBytes==1536`, Scene dirty state unchanged by runtime-only edits.
- [ ] Implement runtime commit/toggle methods in the same `UndoOwner`; no second stack or resident buffer.
- [ ] GREEN and commit.

### Task 10: Source/lifetime barriers and production cross-bar playback

**Files:**
- Modify: `src/dsp/miniacid_engine.cpp`
- Create: `tests/test_p3_u1_product_cross_bar.cpp`
- Create/modify: `tests/test_p3_u1_transport_barriers.cpp`
- Modify: `tests/run_pattern_phrase_p3_u1_tests.sh`

- [ ] RED UI/mutation-created event start=360 duration=96 -> NoteOn@360, no release@384, NoteOff@456.
- [ ] RED STOP, PAUSE, and source switch release an active sequenced note exactly once through existing `RuntimeSynthPlaybackState` ownership; live-note cleanup remains separately owned.
- [ ] Add only the necessary source-switch hard barrier using the existing lifetime owner.
- [ ] GREEN and commit.

### Task 11: Pattern firewall and documentation

**Files:**
- Modify: existing Pattern/editor/Undo/source regression runners only as needed to include the new gate.
- Modify: Synth NOTES help frame implementation.
- Create: `docs/contracts/0_9_10_P3_U1_HYBRID_PATTERN_PHRASE_UX.md` after controls are implemented.

- [ ] Run existing Pattern editor, P1/P2/P3, Undo, Song, MIDI Pattern, Performance/live, STOP/PAUSE regressions.
- [ ] For any suspected inherited failure, run the exact same command on `020582e1` and on new HEAD before classifying it.
- [ ] Document implemented SOURCE, MAKE PHRASE, LENGTH, GRID, Alt+Left/Right, session-only persistence, and limitations.
- [ ] Commit docs only after implementation is real.

### Task 12: SDL/Cardputer/MIDI-only and DRAM attribution

**Files:**
- No production changes unless a proven build defect requires a separate RED/GREEN slice.

- [ ] Build SDL target and run available integration tests.
- [ ] Build normal Cardputer ADV product; record `.data`, `.bss`, fixed total, and delta vs base fixed 192904.
- [ ] Build MIDI-only profile if still gated.
- [ ] Verify product contains no `p3-phrase-loaded`, `p3-cross-bar`, `p3-restart-1`, or `P3DramCharacterization` markers.
- [ ] Attribute any material new static DRAM by map/nm/size symbols; do not raise 191488 ceiling.

### Task 13: PR, CI, and hardware handoff

**Files:**
- Draft PR metadata and hardware handoff evidence only.

- [ ] Push final branch and open Draft PR against the current authoritative development target, preserving the P3 base ancestry.
- [ ] Record host/SDL/Cardputer/MIDI-only CI at exact HEAD.
- [ ] Freeze one exact hardware acceptance ELF: commit SHA, path, SHA256, fixed DRAM, profile.
- [ ] Provide exact upload and serial-monitor commands without rebuilding.
- [ ] Stop at physical hardware handoff. Hardware remains PENDING until user supplies real Cardputer results; do not mark Ready for merge before that gate.
