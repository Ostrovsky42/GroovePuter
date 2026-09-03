# 0.9.10 P1 Runtime Projection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Insert a fixed, immutable PATTERN runtime projection between mutable Scene PATTERN storage and Synth A/B playback without changing any accepted PATTERN behavior.

**Architecture:** Keep Scene `SynthPattern` as the only persistent PATTERN authority. Build a fixed page-resident projection bank on the control side, one 16-entry bank per Synth voice, and let AudioTask select only already-prepared projection entries using existing selection metadata. Preserve the existing quantized-generation pending owner by projecting its old audible snapshot into that same pending slot and releasing it at the existing exact `BAR_START` activation.

**Tech Stack:** C++17, ESP32-S3/Cardputer ADV, SDL host harness, existing `AudioMutationGate`/`AudioGuard`, existing quantized generation slot publication, Python source-contract tests, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-09-03-pattern-phrase-p1-runtime-projection-design.md`

## Global Constraints

- P1 must preserve the accepted P0 scheduler/lifetime characterization unchanged.
- Physical PATTERN remains 16 steps, 24 ticks per step, 384 ticks per bar.
- `gridSteps` remains a Synth scheduler no-op in P1.
- Scene `SynthPattern` remains the only persistent PATTERN authority.
- AudioTask must never copy `SynthPattern`/`SynthStep` event material or build a projection.
- AudioTask may only select already-prepared projection data and release an already-published generation overlay.
- `NO PATTERN` is a valid canonical empty source, never a reason to retain stale material.
- No heap-backed projection container, second queue, second scheduler, second audio task, second page cache, second mutation owner or second Undo owner.
- Generation while PLAY is active must not regain a long `AudioGuard` section.
- Cardputer ADV fixed-DRAM acceptance must record the exact P1 memory delta.
- PHRASE persistence, MAKE PHRASE, GRID semantics, cross-bar lifetime and UI source switching remain outside this implementation.

---

# What this work means in human terms

## What changes after P1

The firmware should **sound and behave the same** as before P1. That is deliberate.

The important change is internal:

```text
BEFORE
Scene SynthPattern
      |
      +--> scheduler reads mutable Pattern bytes directly

AFTER P1
Scene SynthPattern
      |
      +--> control-side immutable projection
                  |
                  +--> scheduler/playback reads projection only
```

For the musician, pressing PLAY on the same project must produce the same notes,
timing, retriggers, probability decisions, Song row changes and current legacy
TIE behavior. There should be no new menu and no new editing mode in P1.

For the architecture, the result is decisive: playback no longer needs to know
that its source is a mutable 16-step Scene object. That is the seam that later
allows PATTERN and PHRASE to become two alternative musical sources without
creating two schedulers.

## The destination after the later PATTERN → PHRASE line

P1 is foundation, not the final musician-facing feature. The intended end state
of the larger line is:

```text
                    SYNTH A / SYNTH B
                           |
                 exactly one source
                    /             \
              PATTERN            PHRASE
              1 bar              1/2/4/8 bars
              16 cells           explicit notes
                    \             /
                     same runtime playback
                             |
                  internal synth + MIDI
```

The firmware should ultimately feel like this:

1. **PATTERN stays fast and simple.** It remains the immediate one-bar 16-step
   sequencer for groove construction. Existing projects remain valid.
2. **GRID stops pretending to be phrase length.** `GRID 8/16/32` becomes an
   editor resolution/snap/zoom decision. Changing GRID does not move already
   existing notes and does not change musical duration by itself.
3. **LENGTH becomes the multi-bar decision.** A PHRASE can explicitly be 1, 2,
   4 or 8 bars long.
4. **MAKE PHRASE is one-way.** The current PATTERN is copied into a new PHRASE
   representation. From that moment PATTERN and PHRASE are not live-linked.
5. **PATTERN xor PHRASE.** A Synth voice plays exactly one authoritative source
   at a time. There is no hidden merge and no double storage that must stay in
   sync.
6. **Long notes become real notes.** In PHRASE, a note has explicit start and
   duration, so it can naturally sustain across bar boundaries. Legacy PATTERN
   TIE remains compatibility behavior rather than becoming the PHRASE model.
7. **One playback path.** PATTERN and future PHRASE both project into runtime
   events consumed by the same scheduler/output path. Internal Synth A/B and
   USB/DIN MIDI do not gain competing owners.
8. **One global Undo owner.** Future PHRASE actions become additional payload
   kinds in the accepted owner; no second Undo stack appears.
9. **The UI exposes musician decisions, not engineering abstractions.** The
   user should see PATTERN/PHRASE, GRID, LENGTH and MAKE PHRASE — not runtime
   slot numbers, mutation gates or projection state.

That is the product target. P1 succeeds if it creates the runtime boundary needed
for that destination while remaining behavior-neutral today.

---

# File structure for P1

The implementation should keep the new concept small and inspectable.

**Create:**

- `src/dsp/pattern_runtime_projection.h`
  - fixed POD runtime event representation;
  - source identity;
  - page-resident bank constants/mapping;
  - pure PATTERN → projection builder;
  - canonical empty builder.
- `tests/test_pattern_phrase_p1_projection.cpp`
  - pure projection/mapping tests with no full engine dependency.
- `tests/test_pattern_phrase_p1_runtime.cpp`
  - engine-level timing, Song, generation and page-settlement acceptance.
- `tests/test_pattern_phrase_p1_source_contract.py`
  - static prohibitions and ownership checks.
- `tests/run_pattern_phrase_p1_projection.sh`
  - focused deterministic host runner.
- `.github/workflows/0_9_10_pattern_phrase_p1_projection.yml`
  - exact-head focused CI.

**Modify narrowly:**

- `src/dsp/miniacid_engine.h`
  - own fixed resident projection banks;
  - expose bounded refresh/access helpers.
- `src/dsp/miniacid_engine.cpp`
  - initialize/rebuild/refresh projection state;
  - route Synth playback reads through projection.
- `src/generation/migration/quantized_generation_undo_owner_impl.h`
  - include prepared old-audible Synth projection in the existing pending
    generation activation slot;
  - return projection material through the existing pending owner.
- `src/ui/pages/pattern_edit_page.h`
  - settle the affected resident projection inside the existing bounded Pattern
    COMMIT `audio_guard_` section.
- `src/ui/pages/pattern_edit_page.cpp`
  - add short post-generation projection settlement for quantized Synth G and
    generation Undo/Redo paths where required.
- `src/ui/pages/genre_page.cpp`
  - add short post-generation projection settlement after the existing long
    generation work has completed; do not wrap generation itself in AudioGuard.
- `src/ui/miniacid_display.cpp`
  - rebuild resident projection bank inside existing guarded page settlement
    before the new page identity is exposed.
- existing Core/DRAM scripts only where needed to include the new focused gate.

Do not move unrelated engine code or redesign Pattern paging during P1.

---

### Task 0: Freeze the P0 regression prerequisite

**Files:**
- Existing dependency: `tests/run_pattern_phrase_p0_characterization.sh`
- Existing dependency: `tests/test_pattern_phrase_p0_source_contract.py`
- Existing dependency: `tests/test_pattern_phrase_p0_runtime.cpp`

**Interfaces:**
- Consumes: accepted P0 characterization from PR #426.
- Produces: an implementation base on which the unchanged P0 runner can execute.

- [ ] **Step 1: Verify the accepted P0 head and focused CI are final**

Record the exact accepted P0 HEAD in the P1 implementation session before any
production edit. Do not silently copy an older `c02d9ae...` checkpoint.

- [ ] **Step 2: Make P0 available on the P1 implementation base**

Preferred order:

```text
finish/freeze PR #426
        -> merge test/documentation-only P0
        -> update P1 from the resulting main
```

Do not duplicate a second divergent copy of the characterization tests in P1.

- [ ] **Step 3: Run P0 before production edits**

Run:

```bash
tests/run_pattern_phrase_p0_characterization.sh
```

Expected final line:

```text
PATTERN/PHRASE P0 characterization: PASS
```

- [ ] **Step 4: Commit only the base integration if the branch needs it**

No P1 production code belongs in this prerequisite commit.

---

### Task 1: Define the pure fixed runtime projection

**Files:**
- Create: `src/dsp/pattern_runtime_projection.h`
- Create: `tests/test_pattern_phrase_p1_projection.cpp`

**Interfaces:**
- Consumes: `SynthPattern`, `SynthStep`, `kBankCount`, `Bank<SynthPattern>::kPatterns`.
- Produces:
  - `PatternRuntimeStep`
  - `PatternRuntimeSourceKind`
  - `PatternRuntimeSource`
  - `PatternRuntimeProjection`
  - `kPatternRuntimeEntriesPerVoice`
  - `patternRuntimeIndex(int bank, int slot)`
  - `projectSynthPattern(...)`
  - `makeEmptyPatternRuntimeProjection(...)`

The new header should define fixed value types with no owning pointers and no
heap containers. The intended API is:

```cpp
#pragma once

#include <cstdint>
#include <type_traits>

#include "../../scenes.h"

enum class PatternRuntimeSourceKind : uint8_t {
  Empty = 0,
  Pattern,
};

struct PatternRuntimeStep {
  int8_t note = -1;
  bool slide = false;
  bool accent = false;
  bool ghost = false;
  uint8_t velocity = 100;
  int8_t timing = 0;
  uint8_t fx = 0;
  uint8_t fxParam = 0;
  uint8_t probability = 100;
};

struct PatternRuntimeSource {
  PatternRuntimeSourceKind kind = PatternRuntimeSourceKind::Empty;
  int8_t voice = -1;
  int8_t page = -1;
  int8_t bank = -1;
  int8_t slot = -1;
};

struct PatternRuntimeProjection {
  static constexpr int kSteps = SynthPattern::kSteps;
  PatternRuntimeSource source{};
  PatternRuntimeStep steps[kSteps]{};
};

static constexpr int kPatternRuntimeEntriesPerVoice =
    kBankCount * Bank<SynthPattern>::kPatterns;

int patternRuntimeIndex(int bank, int slot);
PatternRuntimeProjection projectSynthPattern(
    const SynthPattern& pattern,
    int voice,
    int page,
    int bank,
    int slot);
PatternRuntimeProjection makeEmptyPatternRuntimeProjection(int voice);

static_assert(std::is_trivially_copyable<PatternRuntimeStep>::value,
              "runtime Pattern step must stay trivially copyable");
static_assert(std::is_trivially_copyable<PatternRuntimeProjection>::value,
              "runtime Pattern projection must stay trivially copyable");
```

Implement the pure functions in the same header as `inline` functions so the
first slice does not add build-system ownership.

- [ ] **Step 1: Write the failing field-copy test**

Construct one `SynthPattern` whose step 5 has distinct values for all fields,
project it, mutate the source afterwards and assert the projection did not
change.

- [ ] **Step 2: Run the isolated test and confirm RED**

Compile only the test plus the new header dependency. Expected failure before
implementation: missing projection types/functions.

- [ ] **Step 3: Implement the fixed POD representation and exact field copy**

Copy all nine current playback fields. Do not copy padding bytes with `memcpy`
as the semantic contract; assign fields explicitly.

- [ ] **Step 4: Add bank-index and canonical-empty tests**

Required mapping:

```text
bank 0 slot 0 -> 0
bank 0 slot 7 -> 7
bank 1 slot 0 -> 8
bank 1 slot 7 -> 15
```

Invalid bank/slot must return `-1`.

- [ ] **Step 5: Run the isolated tests GREEN**

- [ ] **Step 6: Commit**

```bash
git add src/dsp/pattern_runtime_projection.h tests/test_pattern_phrase_p1_projection.cpp
git commit -m "feat: add fixed Pattern runtime projection"
```

---

### Task 2: Give MiniAcid a fixed resident projection bank

**Files:**
- Modify: `src/dsp/miniacid_engine.h`
- Modify: `src/dsp/miniacid_engine.cpp`
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`

**Interfaces:**
- Consumes: Task 1 projection types.
- Produces these bounded engine helpers:

```cpp
void rebuildPatternRuntimeProjectionBank(int pageIndex);
void refreshPatternRuntimeProjection(int voiceIndex,
                                     int bankIndex,
                                     int patternIndex,
                                     int pageIndex);
const PatternRuntimeProjection& activePatternRuntimeProjection(
    int voiceIndex) const;
const PatternRuntimeProjection& residentPatternRuntimeProjection(
    int voiceIndex,
    int bankIndex,
    int patternIndex) const;
```

Engine state is fixed:

```cpp
PatternRuntimeProjection patternRuntimeBank_[NUM_303_VOICES]
                                            [kPatternRuntimeEntriesPerVoice]{};
PatternRuntimeProjection emptyPatternRuntime_[NUM_303_VOICES]{};
int8_t patternRuntimePage_ = -1;
```

No vector, `unique_ptr`, shared owner or per-Song-row allocation.

- [ ] **Step 1: Write a failing full-bank rebuild test**

Populate all 16 PATTERN addresses for Synth A and B with unique notes, call the
rebuild helper, then assert all 32 resident projections map to the correct
voice/bank/slot/page.

- [ ] **Step 2: Run the runtime test RED**

- [ ] **Step 3: Implement full-bank rebuild while AudioTask is inactive/guarded**

The helper reads Scene PATTERN material only on the control side. It must not be
called from `processSequencerEvents()` or `generateAudioBuffer()`.

- [ ] **Step 4: Implement single-entry refresh**

The function validates voice/bank/slot/page first. Invalid identity returns
without changing the resident entry.

- [ ] **Step 5: Initialize canonical empty projections for both voices**

Do this in normal engine initialization/reset settlement; no heap work.

- [ ] **Step 6: Run the focused runtime tests GREEN**

- [ ] **Step 7: Commit**

```bash
git add src/dsp/miniacid_engine.h src/dsp/miniacid_engine.cpp tests/test_pattern_phrase_p1_runtime.cpp
git commit -m "feat: own resident Pattern runtime bank"
```

---

### Task 3: Settle manual PATTERN edits into the resident bank

**Files:**
- Modify: `src/ui/pages/pattern_edit_page.h`
- Modify only if needed: `src/ui/pages/pattern_edit_page.cpp`
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`
- Extend: `tests/test_pattern_phrase_p1_source_contract.py`

**Interfaces:**
- Consumes: `MiniAcid::refreshPatternRuntimeProjection(...)`.
- Produces: one resident projection refresh in the same bounded audio exclusion
  that already publishes the persistent Pattern mutation.

The existing `commitPatternMutation()` already performs one bounded Scene write
inside `audio_guard_`. Extend that exact `apply` lambda so persistent Pattern and
derived runtime projection settle together:

```cpp
const auto apply = [&]() {
  GroovePuterUndo::restoreSynthPatternUndo(manager, prepared);
  mini_acid_.refreshPatternRuntimeProjection(
      voice_index_,
      manager.getCurrentBankIndex(voice_index_ + 1),
      manager.getCurrentSynthPatternIndex(voice_index_),
      mini_acid_.currentPageIndex());
};
```

Do not add another revision mark or Undo commit for the projection.

- [ ] **Step 1: Write a test that edits one step while the old projection is resident**

Assert Scene and projection both contain the new step after COMMIT, and that the
other 15 resident PATTERN entries plus the other Synth voice are unchanged.

- [ ] **Step 2: Run RED**

- [ ] **Step 3: Add the derived refresh to the existing bounded COMMIT**

- [ ] **Step 4: Cover paste, selection edit, rotate, clear, note entry, accent,
slide, FX and octave through the existing single mutation helper**

Do not add per-command projection code if all commands already converge through
`commitPatternMutation()`.

- [ ] **Step 5: Run GREEN and source-contract checks**

- [ ] **Step 6: Commit**

```bash
git add src/ui/pages/pattern_edit_page.h src/ui/pages/pattern_edit_page.cpp tests/test_pattern_phrase_p1_runtime.cpp tests/test_pattern_phrase_p1_source_contract.py
git commit -m "feat: settle Pattern edits into runtime projection"
```

---

### Task 4: Make runtime selection use prepared resident or empty projection

**Files:**
- Modify: `src/dsp/miniacid_engine.cpp`
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`

**Interfaces:**
- Consumes: existing `songPatternIndexForTrack()`, current bank indices and
  `patternRuntimeBank_`.
- Produces: `activePatternRuntimeProjection()` with no Scene PATTERN material
  dereference.

The accessor must preserve current source resolution order:

```text
pending generation projection, if valid for this voice/target
    else
Song/Pattern source metadata
    -> NO PATTERN => canonical empty
    -> valid local slot => resident fixed projection entry
```

At this task, pending generation may still temporarily use the old helper until
Task 7; write the accessor so the later overlay hook is one narrow replacement.

- [ ] **Step 1: Write PATTERN-mode selection tests**

Changing current bank/pattern must immediately select the matching already-
prepared entry without rebuilding it.

- [ ] **Step 2: Write Song `-1` test**

A row with no Synth PATTERN must resolve to the canonical empty projection even
if the previous row had audible notes.

- [ ] **Step 3: Run RED**

- [ ] **Step 4: Implement metadata-only selection**

Do not call `sceneManager_.getSynthPattern()` from the runtime accessor.

- [ ] **Step 5: Run GREEN**

- [ ] **Step 6: Commit**

```bash
git add src/dsp/miniacid_engine.cpp tests/test_pattern_phrase_p1_runtime.cpp
git commit -m "feat: select prepared Pattern runtime sources"
```

---

### Task 5: Rewire normal Synth playback to projection event material

**Files:**
- Modify: `src/dsp/miniacid_engine.cpp`
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`
- Extend: `tests/test_pattern_phrase_p1_source_contract.py`

**Interfaces:**
- Consumes: `activePatternRuntimeProjection(int)`.
- Produces: zero audio-rendering reads of mutable Scene Synth PATTERN event
  material.

Replace event-material reads in these exact paths:

```text
processSequencerEvents()  -> timing
triggerSynthStep_()       -> note/flags/velocity/fx/probability
 generateAudioBuffer()    -> Synth A/B retrigger source step
refreshSynthCaches()      -> audible projection when cache is meant to mirror sound
```

Do not modify drum runtime ownership in P1.

- [ ] **Step 1: Add a source-contract RED test**

The test should extract `processSequencerEvents`, `triggerSynthStep_` and the
Synth retrigger sections and reject `activeSynthPattern(` plus direct
`getCurrentSynthPattern(` / `getSynthPattern(` event-material reads there.

- [ ] **Step 2: Add behavioral runtime fixtures for all SynthStep fields**

At minimum cover:

```text
normal note
TIE (-2)
ghost
probability
velocity
accent
slide
timing
RETRIG + fxParam
```

- [ ] **Step 3: Run RED**

- [ ] **Step 4: Replace audio event-material reads with `PatternRuntimeStep`**

Do not change scheduler formulas, random calls, gate countdown rules or MIDI
publication ordering.

- [ ] **Step 5: Run P1 focused runtime test and P0 characterization**

Expected: P1 tests GREEN and P0 output byte-for-byte compatible where the P0
runner compares deterministic traces.

- [ ] **Step 6: Commit**

```bash
git add src/dsp/miniacid_engine.cpp tests/test_pattern_phrase_p1_runtime.cpp tests/test_pattern_phrase_p1_source_contract.py
git commit -m "feat: route Synth playback through runtime projection"
```

---

### Task 6: Prove exact Song BAR_START selection needs no projection build

**Files:**
- Modify only if necessary: `src/dsp/miniacid_engine.cpp`
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`
- Extend: `tests/test_pattern_phrase_p1_source_contract.py`

**Interfaces:**
- Consumes: existing `advanceSongBar_()` → `advanceSongPlayhead()` →
  `applySongPositionSelection()` metadata transition.
- Produces: exact-boundary selection of an already-prepared resident/empty
  projection.

The intended runtime behavior is deliberately simple:

```text
BAR_START
  -> existing Song row advancement changes source metadata
  -> activePatternRuntimeProjection() resolves new metadata
  -> new row step 0 reads an already-prepared entry
```

No new Song cursor or prefetch scheduler is allowed.

- [ ] **Step 1: Add a two-row Song test with distinct step-0 notes**

Assert the old row remains audible before boundary and the new row is selected
at the same existing boundary.

- [ ] **Step 2: Add `PATTERN -> NO PATTERN` and `NO PATTERN -> PATTERN` row tests**

These specifically prevent stale projection leakage.

- [ ] **Step 3: Add a source-contract prohibition**

Inside the audio boundary path, reject:

```text
projectSynthPattern(
refreshPatternRuntimeProjection(
rebuildPatternRuntimeProjectionBank(
memcpy of SynthPattern/SynthStep material
```

- [ ] **Step 4: Run RED, make the minimal selection-only integration, run GREEN**

- [ ] **Step 5: Commit**

```bash
git add src/dsp/miniacid_engine.cpp tests/test_pattern_phrase_p1_runtime.cpp tests/test_pattern_phrase_p1_source_contract.py
git commit -m "test: prove projection-only Song boundary selection"
```

---

### Task 7: Project the existing quantized-generation audible overlay

**Files:**
- Modify: `src/generation/migration/quantized_generation_undo_owner_impl.h`
- Modify: `src/dsp/miniacid_engine.cpp`
- Modify: `src/ui/pages/genre_page.cpp`
- Modify: `src/ui/pages/pattern_edit_page.cpp`
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`
- Extend: `tests/test_pattern_phrase_p1_source_contract.py`

**Interfaces:**
- Consumes: existing `PendingGeneration`, `fillAudibleActivationSnapshot()`,
  `armActivationSlot()`, `pendingAudibleActivation()` and exact BAR_START
  activation.
- Produces:

```cpp
const PatternRuntimeProjection* pendingAudibleSynthProjection(
    const MiniAcid& engine,
    int voice);
```

Extend `PendingGeneration` with fixed prepared runtime projections for the old
audible Synth material. The projection must be filled **before**
`armActivationSlot()` publishes that existing activation slot.

Do not create another global pending slot or atomic owner.

The generation ordering must remain:

```text
prepare candidate new material
capture old audible material
prepare old audible runtime projection in existing activation slot
arm existing activation slot
commit new persistent Scene PATTERN
short guarded resident-projection settlement
continue PLAY using old pending projection
BAR_START clears existing pending activation
playback falls through to resident new projection
```

- [ ] **Step 1: Write the pending-generation RED test**

While PLAY is active, generate a new step-0 note. Assert:

```text
before command            -> old projection audible
after persistent COMMIT   -> old pending projection still audible
exact BAR_START            -> new resident projection audible
```

- [ ] **Step 2: Add Synth-A-only and Synth-B-only pending cases**

The untouched voice must stay on its resident projection.

- [ ] **Step 3: Add cancellation/target-change tests**

Existing `CancelledTargetChanged`, `CancelledRevisionChanged` and explicit
cancel paths must not leave a stale pending projection selected.

- [ ] **Step 4: Fill runtime projection inside the existing activation slot**

Use the captured old `SynthPattern` bytes already present in generation PREPARE;
never recapture mutable Scene material from AudioTask.

- [ ] **Step 5: Add a short post-generation resident settlement**

PLAY generation must remain outside the long `AudioGuard`. After
`regenerateWithQuantizedCommit()` or `regenerateSynthWithQuantizedCommit()` has
returned successfully, use the page's existing `audio_guard_` only for the
bounded resident entry refresh. Do not wrap generation itself.

For a full generation refresh both Synth targets; for synth-only refresh only
the generated voice.

- [ ] **Step 6: Settle generation Undo/Redo into the resident bank**

Whenever an accepted generation Undo/Redo changes persistent Synth PATTERN
bytes, the matching resident projection must be refreshed in the caller's short
existing audio guard before stale resident material can become audible.

- [ ] **Step 7: Replace the final audio-side pending `SynthPattern` read**

`activePatternRuntimeProjection()` must ask the existing pending owner for
`pendingAudibleSynthProjection()` rather than
`pendingAudibleSynthPattern()`.

- [ ] **Step 8: Run focused generation regressions plus P0**

In addition to P1 tests, run existing generation source/regression suites that
protect PLAY generation from long `AudioGuard` ownership.

- [ ] **Step 9: Commit**

```bash
git add src/generation/migration/quantized_generation_undo_owner_impl.h src/dsp/miniacid_engine.cpp src/ui/pages/genre_page.cpp src/ui/pages/pattern_edit_page.cpp tests/test_pattern_phrase_p1_runtime.cpp tests/test_pattern_phrase_p1_source_contract.py
git commit -m "feat: project pending generation audible material"
```

---

### Task 8: Make page load and projection-bank publication one settlement

**Files:**
- Modify: `src/ui/miniacid_display.cpp`
- Modify if required: `src/dsp/miniacid_engine.cpp`
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`
- Extend: `tests/test_pattern_phrase_p1_source_contract.py`

**Interfaces:**
- Consumes: existing guarded `handlePaging_()` and
  `PatternPagingService::loadPage(...)`/creation flow.
- Produces: a resident projection bank whose `patternRuntimePage_` always matches
  playback-visible `currentPageIndex()` after successful page settlement.

Within the existing `withAudioGuard([&] { ... })` paging transaction, order
successful target settlement as:

```text
load/create target Scene resident page
-> mini_acid_.rebuildPatternRuntimeProjectionBank(target)
-> mini_acid_.setCurrentPage(target)
-> finish existing result/rollback logic
```

If load/create fails, keep both the prior page and its prior projection bank.

- [ ] **Step 1: Write old-page/new-page mixed-state RED test**

Use the same bank/slot with different notes on page A and B. The test must never
observe `currentPage=B` with page-A projection data.

- [ ] **Step 2: Write failed-load rollback test**

Assert page identity and runtime bank remain on the last accepted page.

- [ ] **Step 3: Implement guarded rebuild-before-page-publication**

Do not create a second Pattern page cache.

- [ ] **Step 4: Run GREEN**

- [ ] **Step 5: Commit**

```bash
git add src/ui/miniacid_display.cpp src/dsp/miniacid_engine.cpp tests/test_pattern_phrase_p1_runtime.cpp tests/test_pattern_phrase_p1_source_contract.py
git commit -m "feat: settle Pattern page projection atomically"
```

---

### Task 9: Build the focused P1 regression/source-contract runner

**Files:**
- Create: `tests/test_pattern_phrase_p1_source_contract.py`
- Create/complete: `tests/run_pattern_phrase_p1_projection.sh`
- Extend if needed: `tests/test_pattern_phrase_p1_runtime.cpp`

**Interfaces:**
- Consumes: P0 runner and all P1 tests.
- Produces: one local command that proves projection architecture and behavior.

The source contract must explicitly verify:

```text
projection storage is fixed capacity
PatternRuntimeProjection is not heap-backed
AudioTask playback sections contain no activeSynthPattern event-material read
AudioTask contains no project/rebuild/refresh call
NO PATTERN canonical empty path exists
Song boundary keeps existing traversal owner
existing quantized generation pending owner remains singular
no new Undo owner / queue / scheduler / audio task appears
```

The focused runner should follow the accepted P0 deterministic pattern:

```bash
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

tests/run_pattern_phrase_p0_characterization.sh
python3 tests/test_pattern_phrase_p1_source_contract.py
# compile/run P1 pure + runtime fixtures with GCC
# repeat deterministic runtime output
# run Clang parity when available
# run ASan
# run UBSan

echo "PATTERN/PHRASE P1 runtime projection: PASS"
```

Use the actual SDL source list extraction pattern already established by the P0
runner instead of maintaining a second handwritten source list.

- [ ] **Step 1: Write source-contract assertions first and run RED**

- [ ] **Step 2: Complete focused GCC runtime build and deterministic repeat**

- [ ] **Step 3: Add Clang, ASan and UBSan parity**

- [ ] **Step 4: Run the complete focused runner GREEN**

- [ ] **Step 5: Commit**

```bash
git add tests/test_pattern_phrase_p1_source_contract.py tests/test_pattern_phrase_p1_projection.cpp tests/test_pattern_phrase_p1_runtime.cpp tests/run_pattern_phrase_p1_projection.sh
git commit -m "test: add P1 runtime projection acceptance"
```

---

### Task 10: Add CI, fixed-DRAM evidence and final cleanup

**Files:**
- Create: `.github/workflows/0_9_10_pattern_phrase_p1_projection.yml`
- Modify only if required: existing Cardputer fixed-DRAM reporting scripts
- Update: `docs/superpowers/specs/2026-09-03-pattern-phrase-p1-runtime-projection-design.md`
- Update: this plan only for factual implementation notes discovered during TDD

**Interfaces:**
- Consumes: completed P1 implementation and focused runner.
- Produces: exact-head remote evidence and a reviewable P1 checkpoint.

- [ ] **Step 1: Add focused exact-head P1 workflow**

It must run `tests/run_pattern_phrase_p1_projection.sh` and not replace existing
Core/build gates.

- [ ] **Step 2: Run the complete required matrix**

Required evidence:

```text
P0 focused characterization     PASS
P1 focused projection runner    PASS
host Core tests                 PASS
SDL build                       PASS
Cardputer ADV build             PASS
Cardputer ADV fixed-DRAM gate   PASS
SEQTRAK MIDI-only build         PASS
```

- [ ] **Step 3: Record memory delta**

Capture at least:

```text
sizeof(PatternRuntimeStep)
sizeof(PatternRuntimeProjection)
resident bank bytes per voice
resident bank bytes total
generation projection delta
final fixed DRAM delta
```

Do not accept an unexplained DRAM regression merely because the binary builds.

- [ ] **Step 4: Run a final architecture grep/review**

Confirm:

```text
no duplicate active PATTERN material owner
no new runtime heap allocation
no stale activeSynthPattern read in audio event-material path
no projection-specific Scene revision or Undo receipt
no duplicated Song/page mapper
```

- [ ] **Step 5: Remove temporary diagnostics, dead helpers and duplicated tests**

Keep only diagnostics that are useful as permanent acceptance evidence.

- [ ] **Step 6: Update PR body with exact final HEAD and verification matrix**

Keep PR draft until the exact-head workflow is terminal GREEN.

- [ ] **Step 7: Commit cleanup/CI**

```bash
git add .github/workflows/0_9_10_pattern_phrase_p1_projection.yml docs/superpowers/specs/2026-09-03-pattern-phrase-p1-runtime-projection-design.md docs/superpowers/plans/2026-09-03-pattern-phrase-p1-runtime-projection.md
git commit -m "ci: gate P1 immutable runtime projection"
```

---

# Review checkpoints

The implementation should be reviewed at three semantic checkpoints rather than
as one large diff.

## Review A — projection exists but playback is still legacy

Expected state:

```text
fixed projection types        YES
resident bank                 YES
manual refresh                YES
playback still legacy         YES
behavior delta                NONE
```

This proves the derived model before making it authoritative for playback.

## Review B — projection is authoritative for normal PATTERN playback

Expected state:

```text
normal timing/trigger/retrig   projection
Song boundary                 prepared selection only
NO PATTERN                     canonical empty
pending generation             still fully characterized
P0                             GREEN
```

## Review C — generation/page settlement complete

Expected state:

```text
pending generation old side   prepared projection
BAR_START activation           release/select only
page settlement               bank + page identity together
fixed DRAM                     accepted
all required builds            GREEN
```

Only Review C is P1 completion.

---

# What must *not* be implemented in this plan

Do not use P1 as an excuse to implement the attractive later features early.
Specifically do not add:

```text
PhraseEvent startTick/durationTicks runtime execution
MAKE PHRASE
PATTERN/PHRASE toggle
GRID snap/zoom behavior
LENGTH 1/2/4/8 editor
cross-bar gate lifetime changes
new MIDI note-lifetime owner
Phrase persistence migration
Phrase Undo payload
new UI tabs or source badges
```

Those features become safer *because* P1 removes direct mutable PATTERN material
from the audio path. They should be separate checkpoints with their own causal
proofs.

---

# Definition of done

A musician loading the same project before and after P1 should not need to learn
anything new and should not hear a designed musical difference.

An engineer inspecting the runtime after P1 should be able to state, truthfully:

```text
The Scene owns PATTERN persistence.
The control plane projects PATTERN material.
The audio path consumes immutable projected event material.
Song only selects already-prepared material at the boundary.
Generation only releases/selects already-prepared material at the boundary.
NO PATTERN is explicit silence.
There is still one scheduler, one generation pending owner and one Undo owner.
```

That is the exact foundation required before PHRASE becomes a real second
musical source.
