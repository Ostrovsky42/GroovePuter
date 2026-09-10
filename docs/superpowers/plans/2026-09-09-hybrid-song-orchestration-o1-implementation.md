# Hybrid Song Orchestration O1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement O1A SongCell semantics and O1B explicit MAKE UNIQUE while preserving the current 8-byte `SongPosition`, bounded Undo model, persistence compatibility, live Song causality, Pattern/Phrase liveness, and Cardputer ADV DRAM budget.

**Architecture:** Replace raw Song-cell integer interpretation with a 2-byte semantic `SongCell` authority that represents EMPTY, REST, or MATERIAL without increasing `Song` size. Persistence continues to serialize ordinary pattern references in `a/b/drums/voice` and stores REST/occurrence metadata in an optional packed `rel` field. O1B then adds explicit copy-on-write material forking using existing global reference/liveness rules and a bounded fixed-value Undo receipt.

**Tech Stack:** C++17 embedded firmware, ArduinoJson 7.4.2, host C++ tests, Python source-regression tests, GitHub Actions, Cardputer ADV ESP32-S3 builds, SDL host build.

**Spec:** `docs/superpowers/specs/2026-09-09-hybrid-song-orchestration-o1-design.md`

## Global Constraints

- `sizeof(SongPosition)` must remain exactly 8 bytes.
- `sizeof(Song)` must not increase from the pre-O1 exact baseline.
- The canonical Undo payload capacity remains `1536` bytes.
- Do not add `NEW IDEA`, `VAR`, `DEV`, `RETURN`, or form-generation behavior.
- Do not assign new physical keyboard shortcuts in O1.
- `REST` is a deliberate silent occurrence; `EMPTY` is unresolved/unassigned.
- REST must be silent, must not count toward Pattern/Phrase liveness, must block merge/auto-fill fallback, and must preserve Song length.
- Shared material identity remains reference-based.
- Explicit MAKE UNIQUE must never mutate the shared source material.
- Explicit MAKE UNIQUE must clear reclaimable Song-generated ownership from the forked destination.
- Any PREPARE failure must leave Scene bytes unchanged.
- All accepted persistent changes continue through the existing bounded Undo / Scene revision discipline.

---

## File Structure

### New files

- `src/state/song_cell.h` — 2-byte SongCell value type, semantic predicates, raw storage codec, packed row relation codec helpers.
- `src/state/material_fork.h` — O1B shared/unique query, MAKE UNIQUE preparation, fixed receipt shape, bounded commit helpers.
- `tests/test_song_cell_o1.cpp` — representation, transformation, persistence helper, liveness-semantics tests that do not need the full UI.
- `tests/test_material_fork_o1.cpp` — explicit fork and Undo tests.
- `tests/test_song_cell_o1_source_regressions.py` — forbids raw Song semantic interpretation after migration and locks no-growth / persistence contracts.

### Existing files to modify

- `scenes.h` — replace `SongPosition::patterns[]` storage with `SongCell cells[]`, add no-growth asserts, expose SceneManager semantic cell accessors.
- `scenes.cpp` — semantic setters/getters, merge/alternate/insert/delete/trim logic, document JSON codec.
- `src/state/song_edit.h` — use SongCell API for editing and row transforms.
- `src/phrase/phrase_song_insert.h` — EMPTY vs REST occupancy semantics.
- `src/phrase/phrase_core.h` — Phrase write/preview/reference-view checks use SongCell material accessors.
- `src/dsp/generated_phrase_song.h` — generated Phrase placement and liveness verification use semantic SongCell accessors.
- `src/dsp/song_pattern_materializer.h` — reference counting and materialization writeback consume only MATERIAL refs.
- `src/generation/migration/live_song_arrangement_activation.h` — semantic SongPosition equality and audible-track mask.
- `src/state/undo_receipts.h` — sameSong comparison consumes semantic cells; O1B receipt declaration may live in `material_fork.h` if it remains isolated.
- `src/ui/pages/song_page.cpp` — rendering/paste/generation reads semantic cells; no new orchestration keys.
- `src/ui/pages/song_page.h` — detached Song PREPARE/COMMIT remains canonical.
- `src/ui/pages/phrase_page.cpp` — accepted Phrase liveness and occupancy display use material accessors.
- `scenes.h` streaming writer section — write legacy refs plus optional packed `rel`.
- existing relevant tests and CI workflow source lists — include the new focused O1 tests.

---

### Task 1: Add the 2-byte SongCell authority

**Files:**
- Create: `src/state/song_cell.h`
- Modify: `scenes.h`
- Test: `tests/test_song_cell_o1.cpp`

**Interfaces:**
- Produces:
  - `GroovePuterSong::SongCell`
  - `SongCell::empty()`
  - `SongCell::rest()`
  - `SongCell::material(int patternRef)`
  - `bool SongCell::isEmpty() const`
  - `bool SongCell::isRest() const`
  - `bool SongCell::hasMaterial() const`
  - `int SongCell::patternRef() const`
  - `int16_t SongCell::rawValue() const`
  - `static SongCell SongCell::fromRaw(int16_t raw)`
- `SongPosition` becomes `SongCell cells[kTrackCount]` and remains 8 bytes.

- [ ] **Step 1: Write the failing representation test**

```cpp
#include <cassert>
#include "../scenes.h"
#include "../src/state/song_cell.h"

int main() {
  using GroovePuterSong::SongCell;

  static_assert(sizeof(SongCell) == sizeof(int16_t));
  static_assert(sizeof(SongPosition) == 8);

  const SongCell empty = SongCell::empty();
  assert(empty.isEmpty());
  assert(!empty.isRest());
  assert(!empty.hasMaterial());
  assert(empty.patternRef() == -1);

  const SongCell rest = SongCell::rest();
  assert(!rest.isEmpty());
  assert(rest.isRest());
  assert(!rest.hasMaterial());
  assert(rest.patternRef() == -1);

  const SongCell p0 = SongCell::material(0);
  const SongCell p255 = SongCell::material(255);
  assert(p0.hasMaterial() && p0.patternRef() == 0);
  assert(p255.hasMaterial() && p255.patternRef() == 255);

  assert(SongCell::fromRaw(-1).isEmpty());
  assert(SongCell::fromRaw(-2).isRest());
  assert(SongCell::fromRaw(12).hasMaterial());
  assert(SongCell::fromRaw(9999).isEmpty());
}
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```bash
g++ -std=c++17 -I. tests/test_song_cell_o1.cpp -o /tmp/test_song_cell_o1 && /tmp/test_song_cell_o1
```

Expected: compile failure because `src/state/song_cell.h` and `SongCell` do not exist.

- [ ] **Step 3: Implement minimal SongCell**

Create `src/state/song_cell.h` with this exact semantic envelope:

```cpp
#pragma once
#ifndef GROOVEPUTER_STATE_SONG_CELL_H
#define GROOVEPUTER_STATE_SONG_CELL_H

#include <cstdint>

namespace GroovePuterSong {

class SongCell {
 public:
  static constexpr int16_t kEmptyRaw = -1;
  static constexpr int16_t kRestRaw = -2;
  static constexpr int kMinPatternRef = 0;
  static constexpr int kMaxPatternRef = 255;

  constexpr SongCell() : raw_(kEmptyRaw) {}

  static constexpr SongCell empty() { return SongCell(kEmptyRaw); }
  static constexpr SongCell rest() { return SongCell(kRestRaw); }

  static constexpr SongCell material(int patternRef) {
    return patternRef >= kMinPatternRef && patternRef <= kMaxPatternRef
        ? SongCell(static_cast<int16_t>(patternRef))
        : empty();
  }

  static constexpr SongCell fromRaw(int16_t raw) {
    return raw == kEmptyRaw || raw == kRestRaw ||
                   (raw >= kMinPatternRef && raw <= kMaxPatternRef)
        ? SongCell(raw)
        : empty();
  }

  constexpr bool isEmpty() const { return raw_ == kEmptyRaw; }
  constexpr bool isRest() const { return raw_ == kRestRaw; }
  constexpr bool hasMaterial() const {
    return raw_ >= kMinPatternRef && raw_ <= kMaxPatternRef;
  }
  constexpr int patternRef() const { return hasMaterial() ? raw_ : -1; }
  constexpr int16_t rawValue() const { return raw_; }

  friend constexpr bool operator==(SongCell lhs, SongCell rhs) {
    return lhs.raw_ == rhs.raw_;
  }
  friend constexpr bool operator!=(SongCell lhs, SongCell rhs) {
    return !(lhs == rhs);
  }

 private:
  explicit constexpr SongCell(int16_t raw) : raw_(raw) {}
  int16_t raw_;
};

static_assert(sizeof(SongCell) == sizeof(int16_t),
              "O1 SongCell must remain exactly two bytes");

}  // namespace GroovePuterSong

#endif
```

Change `SongPosition` to:

```cpp
struct SongPosition {
  static constexpr int kTrackCount = 4;
  GroovePuterSong::SongCell cells[kTrackCount]{};
};

static_assert(sizeof(SongPosition) == 8,
              "O1 SongPosition must remain exactly eight bytes");
```

Include `src/state/song_cell.h` before the declaration.

- [ ] **Step 4: Run the focused test and verify GREEN**

Run the same command. Expected: PASS, exit 0.

- [ ] **Step 5: Commit**

```bash
git add src/state/song_cell.h scenes.h tests/test_song_cell_o1.cpp
git commit -m "feat: add bounded SongCell semantics"
```

---

### Task 2: Add canonical Song cell accessors and migrate core Song transforms

**Files:**
- Modify: `scenes.h`
- Modify: `scenes.cpp`
- Modify: `src/state/song_edit.h`
- Test: `tests/test_song_cell_o1.cpp`

**Interfaces:**
- Consumes: `GroovePuterSong::SongCell`
- Produces SceneManager methods:
  - `SongCell songCell(int position, SongTrack track) const`
  - `SongCell songCellAtSlot(int slot, int position, SongTrack track) const`
  - `void setSongCell(int position, SongTrack track, SongCell cell)`
  - `void setSongRest(int position, SongTrack track)`
  - existing `songPattern*` methods remain compatibility views returning only MATERIAL refs.

- [ ] **Step 1: Add failing transform tests**

Extend `tests/test_song_cell_o1.cpp` with cases that prove:

```cpp
Song song{};
song.length = 3;
song.positions[0].cells[0] = SongCell::material(4);
song.positions[1].cells[0] = SongCell::rest();
song.positions[2].cells[0] = SongCell::empty();

GroovePuterUndo::SongEdit::insertRow(song, 1);
assert(song.positions[1].cells[0].isEmpty());
assert(song.positions[2].cells[0].isRest());

GroovePuterUndo::SongEdit::deleteRow(song, 1);
assert(song.positions[1].cells[0].isRest());
```

Add a small helper-level merge/trim characterization if the existing SceneManager construction fixture is available; otherwise add it to the existing SceneManager Song test file rather than inventing a new fake.

- [ ] **Step 2: Run focused tests and verify RED**

Expected: compile failures because `song_edit.h` still accesses `patterns[]`.

- [ ] **Step 3: Migrate `song_edit.h` to semantic cells**

Use `cells[]` and only `SongCell` predicates. The compatibility helper `patternAt()` must return `cell.patternRef()`. `setPattern()` must assign `SongCell::material(pattern)` for valid refs and `SongCell::empty()` for negative input. Inserted rows use default `SongPosition{}`.

- [ ] **Step 4: Migrate SceneManager core operations**

Implement semantic accessors and change:

```text
clearSong
setSongPattern
clearSongPattern
songPattern
songPatternAtSlot
mergeSongs
alternateSongs
insertSongRow
deleteSongRow
trimSongLength
```

Rules:

```text
merge: copy B only when A.isEmpty()
trim: MATERIAL or REST marks row used
padding/new row: EMPTY
songPattern*: REST and EMPTY both return -1
```

- [ ] **Step 5: Run focused tests and existing Song edit tests**

Run:

```bash
g++ -std=c++17 -I. tests/test_song_cell_o1.cpp -o /tmp/test_song_cell_o1 && /tmp/test_song_cell_o1
python3 tests/test_song_generation_source_regressions.py
python3 tests/test_song_phrase_edit_0_9_8_r4_source_regressions.py
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add scenes.h scenes.cpp src/state/song_edit.h tests/test_song_cell_o1.cpp
git commit -m "refactor: route Song edits through SongCell semantics"
```

---

### Task 3: Add backward-compatible persistence for REST

**Files:**
- Modify: `src/state/song_cell.h`
- Modify: `scenes.h`
- Modify: `scenes.cpp`
- Test: existing scene persistence tests
- Test: `tests/test_song_cell_o1.cpp`

**Interfaces:**
- Produces:
  - `uint16_t packSongRowSemantics(const SongPosition&)`
  - `void applyPackedSongRowSemantics(uint16_t packed, SongPosition&)`
- Initial 3-bit tag values:
  - `0 = Unspecified`
  - `1 = Rest`

- [ ] **Step 1: Add failing packed-codec tests**

```cpp
SongPosition row{};
row.cells[0] = SongCell::rest();
row.cells[1] = SongCell::material(7);
row.cells[2] = SongCell::empty();
row.cells[3] = SongCell::rest();

const uint16_t packed = GroovePuterSong::packSongRowSemantics(row);
SongPosition decoded{};
decoded.cells[0] = SongCell::empty();
decoded.cells[1] = SongCell::material(7);
decoded.cells[2] = SongCell::empty();
decoded.cells[3] = SongCell::empty();
GroovePuterSong::applyPackedSongRowSemantics(packed, decoded);
assert(decoded.cells[0].isRest());
assert(decoded.cells[1].patternRef() == 7);
assert(decoded.cells[2].isEmpty());
assert(decoded.cells[3].isRest());
```

Also prove invalid/reserved tags sanitize to UNSPECIFIED without changing a valid MATERIAL ref.

- [ ] **Step 2: Run focused test and verify RED**

Expected: undefined codec functions.

- [ ] **Step 3: Implement the packed row codec**

Use 3 bits per track, tracks 0..3, 12 bits total. Encoding REST emits tag 1; all other current O1 cells emit 0. Decoding tag 1 converts only a non-material cell to REST; it must never overwrite a valid material ref. Reserved tags 2..7 are ignored as UNSPECIFIED in O1.

- [ ] **Step 4: Update document JSON writer/reader**

For each Song row:

- write `a/b/drums/voice` as `cell.patternRef()` so EMPTY and REST both persist `-1` in legacy fields;
- write optional `rel` only when packed value is nonzero;
- decode refs first;
- then, if integer `rel` is present in `0..4095`, apply the packed semantic tags;
- missing `rel` leaves legacy behavior unchanged.

Update both the ArduinoJson document writer in `scenes.cpp` and the streaming writer in `scenes.h` so their output semantics match.

- [ ] **Step 5: Add persistence regression cases**

Prove:

```text
legacy row without rel -> EMPTY remains EMPTY
REST -> save/load -> REST
MATERIAL 255 -> save/load -> 255
REST old-firmware view -> ref -1
invalid rel -> safe UNSPECIFIED/EMPTY behavior
```

- [ ] **Step 6: Run persistence tests**

Run the repository's existing scene persistence test commands plus `tests/test_song_cell_o1.cpp`. Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add src/state/song_cell.h scenes.h scenes.cpp tests/test_song_cell_o1.cpp tests
git commit -m "feat: persist Song REST semantics compatibly"
```

---

### Task 4: Migrate Phrase, liveness, live activation, UI reads, and semantic equality

**Files:**
- Modify: `src/phrase/phrase_song_insert.h`
- Modify: `src/phrase/phrase_core.h`
- Modify: `src/dsp/generated_phrase_song.h`
- Modify: `src/dsp/song_pattern_materializer.h`
- Modify: `src/generation/migration/live_song_arrangement_activation.h`
- Modify: `src/state/undo_receipts.h`
- Modify: `src/ui/pages/song_page.cpp`
- Modify: `src/ui/pages/phrase_page.cpp`
- Test: existing Phrase / liveness / live Song suites

**Interfaces:**
- All consumers must ask SongCell semantics instead of raw sign.
- Phrase non-overwrite occupancy: EMPTY available; REST/MATERIAL occupied.
- Liveness/reference count: MATERIAL only.
- Audible track mask: MATERIAL only.
- semantic equality: EMPTY and REST are different.

- [ ] **Step 1: Add failing REST boundary tests**

Add focused cases to the closest existing test suites:

```text
Phrase insert non-overwrite accepts EMPTY
Phrase insert non-overwrite rejects REST
Phrase insert non-overwrite rejects MATERIAL
REST does not increment global Pattern reference count
sameSongPosition(EMPTY, REST) == false
REST contributes no audible track mask
```

- [ ] **Step 2: Run focused tests and verify RED**

Expected: compile failures/raw semantics mismatches.

- [ ] **Step 3: Migrate Phrase insertion and PhraseCore**

Use `cell.isEmpty()` for availability and `cell.hasMaterial()` for pattern resolution. Explicit overwrite may replace REST; non-overwrite may not.

- [ ] **Step 4: Migrate materializer/reference counting**

Only `hasMaterial()` cells yield refs. REST must never contribute to liveness or allocator ownership.

- [ ] **Step 5: Migrate live Song activation**

`sameSongPosition` compares complete SongCell semantic values. `audibleTrackMaskFor()` uses material presence only. EMPTY->REST and REST->EMPTY therefore produce mutation causality without producing sound.

- [ ] **Step 6: Migrate Undo equality and UI reads**

`sameSong()` compares semantic cells. Song/Phrase UI reads use `patternRef()` or explicit EMPTY/REST predicates; no UI action for REST is added yet.

- [ ] **Step 7: Run inherited focused suites**

Run:

```bash
python3 tests/test_song_generation_source_regressions.py
python3 tests/test_song_phrase_edit_0_9_8_r4_source_regressions.py
python3 tests/test_phrase_ui_source_regressions.py
```

and the repository's C++ tests for PhraseCore, Song/Phrase liveness, Song pattern materializer, live Song arrangement, generated Phrase Song, and Undo.

Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add src/phrase src/dsp src/generation/migration src/state/undo_receipts.h src/ui/pages tests
git commit -m "refactor: make Song consumers REST-aware"
```

---

### Task 5: Lock O1A against raw semantic interpretation

**Files:**
- Create: `tests/test_song_cell_o1_source_regressions.py`
- Modify: relevant CI focused workflow or existing Core regressions source list

**Interfaces:**
- Produces a source-level guard that prevents reintroduction of raw Song semantic checks outside the SongCell implementation/codec boundary.

- [ ] **Step 1: Write failing source regression**

The test must require:

```text
sizeof(SongPosition) == 8 static assertion
SongCell source file exists
scenes.h SongPosition contains SongCell cells[]
no production occurrence of `.patterns[` on SongPosition-specific access paths
no production Song semantic check using `patterns[t] >= 0` or `< 0`
persistence contains optional `rel` write/read
Phrase non-overwrite uses semantic EMPTY predicate
live Song equality compares cells
```

Do not flag synth/drum bank `.patterns[]` usage; only SongPosition raw access is forbidden.

- [ ] **Step 2: Run source regression and verify RED before the final cleanup**

Expected: it identifies remaining raw SongPosition accesses.

- [ ] **Step 3: Remove each remaining raw SongPosition semantic consumer**

Do not mechanically replace all `.patterns[]` text; inspect each hit and migrate only Song occurrence semantics. Pattern-bank storage remains unchanged.

- [ ] **Step 4: Run source regression and inherited source suites**

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add tests/test_song_cell_o1_source_regressions.py .github src scenes.h scenes.cpp
git commit -m "test: lock O1A SongCell semantic boundary"
```

At this commit O1A is independently reviewable and must be taken through the full platform gate before O1B begins.

---

### Task 6: Implement explicit MAKE UNIQUE with bounded Undo

**Files:**
- Create: `src/state/material_fork.h`
- Create: `tests/test_material_fork_o1.cpp`
- Modify: `src/dsp/song_pattern_materializer.h` only where existing reference/ownership helpers should be reused or factored into stable helpers
- Modify: `src/state/undo_kind.h` only if a distinct `UndoKind::MaterialFork` is required by the existing enum discipline; otherwise use the repository-approved existing kind with exact payload-size discrimination
- Modify: UI only enough to expose status text if backend API needs it; do not add a physical shortcut

**Interfaces:**
- Produces:
  - `enum class MaterialForkStatus { InvalidSource, AlreadyUnique, NoDestination, Prepared, Committed, TargetChanged }`
  - `PreparedMaterialFork`
  - `MaterialForkUndoPayload`
  - `prepareMaterialFork(...)`
  - `commitPreparedMaterialFork(...)`
  - `undoMaterialFork(...)` / redo through the canonical owner pattern

- [ ] **Step 1: Write failing MAKE UNIQUE tests**

Cover:

```text
EMPTY -> InvalidSource and byte-identical Scene
REST -> InvalidSource and byte-identical Scene
unique material -> AlreadyUnique and byte-identical Scene
shared Song material -> new destination ref only for selected occurrence
Song+Phrase shared material -> Phrase stays on source
no safe destination -> zero mutation
Synth MaterialKind copied
forked generated material clears Song-generated reclaimable ownership
Undo restores selected occurrence and previous destination bytes
Redo reapplies forked state
receipt sizeof <= 1536
```

- [ ] **Step 2: Run the test and verify RED**

Expected: missing `material_fork.h` / undefined API.

- [ ] **Step 3: Implement PREPARE only**

`PreparedMaterialFork` contains fixed-value state only:

```text
pageIndex
songSlot
row
track
sourceRef
destinationRef
source MaterialKind when Synth
destination-before bytes
destination-after bytes
old SongCell
new SongCell
base Scene revision / live target evidence required by existing mutation discipline
```

PREPARE must not mutate Scene.

- [ ] **Step 4: Reuse existing global liveness/reference logic**

The shared query must include both Songs and Phrase references exactly as the current Song materializer allocator does. Do not create a second reference counter.

- [ ] **Step 5: Implement destination clone semantics**

For Synth:

```text
copy source SynthPattern/Melody-backed slot bytes according to current MaterialKind owner
copy MaterialKind descriptor
clear reclaimable Song-generated ownership from explicit fork destination
```

For Drums:

```text
copy complete DrumPatternSet
clear any Song-generated ownership marker used by the existing allocator
```

If the current generated ownership bit is embedded in per-pattern reserved state, centralize the clear operation in a helper shared with the materializer rather than duplicating bit knowledge.

- [ ] **Step 6: Implement bounded COMMIT and Undo receipt**

The receipt contains only:

```text
selected occurrence old cell
destination address
destination previous material bytes
previous MaterialKind when applicable
```

Use `static_assert(sizeof(MaterialForkUndoPayload) <= GroovePuterUndo::kUndoPayloadBytes)`.

COMMIT writes destination material/kind and selected SongCell as one logical persistent mutation through `undoOwner().commitPrepared(...)`. Failure before COMMIT leaves Scene unchanged.

- [ ] **Step 7: Run MAKE UNIQUE tests and inherited allocator/liveness/Undo tests**

Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add src/state/material_fork.h src/dsp/song_pattern_materializer.h src/state tests/test_material_fork_o1.cpp tests
git commit -m "feat: add explicit bounded material fork"
```

---

### Task 7: Exact-SHA verification and closure

**Files:**
- Modify documentation only if evidence recording is required by repository convention.

**Interfaces:**
- No new product interface. This task proves the exact commit.

- [ ] **Step 1: Capture exact HEAD SHA**

```bash
git rev-parse HEAD
```

Record it in the evidence report / PR comment.

- [ ] **Step 2: Run focused O1 tests**

Run all new `test_song_cell_o1*` and `test_material_fork_o1*` tests. Expected: PASS.

- [ ] **Step 3: Run inherited host/regression suites**

Required evidence:

```text
Core host regressions
Song generation regressions
Song/Phrase liveness
PhraseCore
Generated Phrase Song
Pattern/Phrase P3 lifetime
P3-U1 Hybrid Pattern Phrase UX
live Song arrangement
Undo ownership/revision suites
scene persistence
```

Expected: PASS.

- [ ] **Step 4: Run platform builds on the same SHA**

Required:

```text
Cardputer ADV compile
fixed DRAM gate
SDL build
SEQTRAK MIDI-only build
```

Expected: PASS.

- [ ] **Step 5: Verify no-growth evidence**

Confirm compile-time assertions and the Cardputer DRAM report show no increase caused by `Song`/`SongPosition` storage.

- [ ] **Step 6: Review diff against the frozen spec**

Explicitly verify:

```text
no NEW/VAR/DEV/RETURN implementation
no new keyboard shortcut
no separate relation array
no global Undo payload increase
REST != EMPTY everywhere relevant
MAKE UNIQUE source remains byte-identical for other users
```

- [ ] **Step 7: Commit evidence docs if needed and update PR**

Use the exact verified SHA in the PR comment. Do not call O1 complete until all required gates are GREEN on that same SHA.

---

## Plan Self-Review

### Spec coverage

- Song footprint and semantic owner: Task 1.
- Canonical accessors and transforms: Task 2.
- Backward-compatible persistence: Task 3.
- Phrase/liveness/live/Undo/UI boundaries: Task 4.
- Raw semantic access prohibition: Task 5.
- REUSE / explicit MAKE UNIQUE / bounded Undo / generated-owner clearing: Task 6.
- Exact-SHA host/platform/DRAM acceptance: Task 7.

### Type consistency

- `SongCell` is the only Song occurrence value type.
- `SongPosition` owns four `SongCell` values.
- compatibility `songPattern*()` methods expose only material refs and return `-1` for both EMPTY and REST.
- semantic consumers use `SongCell` predicates.
- persistence `rel` is metadata, never raw tagged ref persistence.

### Scope locks

The plan intentionally does not implement future musical relation labels, form generation, G4 Idea semantics, or keyboard mapping.
