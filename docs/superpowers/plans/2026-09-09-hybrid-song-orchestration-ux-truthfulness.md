# Hybrid Song Orchestration UX Truthfulness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove misleading orchestration labels from current Song/PHRASE presentation without adding new musical semantics or changing generation behavior.

**Architecture:** Preserve current Song pattern-reference materialization, Phrase generation, placement, rollback, lifetime, Undo and keyboard ownership. Change only presentation vocabulary so the UI describes current implementation truthfully while future NEW/VAR/DEV/REST/RETURN semantics remain deferred.

**Tech Stack:** C++17 UI pages, Python source-regression tests, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-09-09-hybrid-song-orchestration-ux-design.md`

## Global Constraints

- Authoritative base is `fcd0d77da5ed6ef38419547477ab26e77ec6ff26`.
- No G0-G4 generation semantics in this checkpoint.
- No genre coefficient changes.
- No new Idea/Material owner.
- No new keyboard mappings.
- No runtime/storage/persistence changes.
- Existing Song `G`, double-`G`, Ctrl+G and Alt+G behavior remains unchanged.
- Existing PHRASE generation and placement behavior remains unchanged.

---

### Task 1: Lock truthful presentation contract

**Files:**
- Create: `tests/test_hybrid_song_orchestration_ui_source_regressions.py`
- Create: `.github/workflows/0-9-11-hybrid-song-orchestration-ux.yml`

**Interfaces:**
- Consumes: current `SongPage::drawGeneratorHint()` and `PhrasePage::drawProductView()` source.
- Produces: a CI gate that fails while misleading labels remain.

- [x] **Step 1: Write the failing source-regression test**

The test requires:

```text
Song hint contains no RND / SMART / EVOL / FILL labels
Song hint contains GEN ALT:%d/4
PHRASE product view contains no DEPTH label
PHRASE product view contains LEVEL
PHRASE product view contains LAST GEN: %s
```

- [x] **Step 2: Add a focused workflow**

Run:

```bash
python3 tests/test_hybrid_song_orchestration_ui_source_regressions.py
```

- [ ] **Step 3: Verify RED**

Expected failure before production edits:

```text
Song generator hint still exposes misleading musical mode "RND"
```

or the first equivalent unmet contract.

---

### Task 2: Neutralize Song generator mode presentation

**Files:**
- Modify: `src/ui/pages/song_page.cpp` in `SongPage::drawGeneratorHint()` only.
- Test: `tests/test_hybrid_song_orchestration_ui_source_regressions.py`

**Interfaces:**
- Consumes: existing `gen_mode_` integer selector.
- Produces: neutral UI text describing an alternative generation selection without promising evolution/fill semantics.

- [ ] **Step 1: Keep selector behavior unchanged**

Do not change `cycleGeneratorMode()`, `materializeSongTracks()`, Atlas variation selection, modeTag or keyboard mapping.

- [ ] **Step 2: Replace semantic mode-name array**

Replace the current names with a neutral ordinal presentation:

```cpp
const int modeIdx = static_cast<int>(gen_mode_);
char buf[20];
std::snprintf(buf, sizeof(buf), "GEN ALT:%d/4", modeIdx + 1);
```

Keep current hint bounds and rendering.

- [ ] **Step 3: Run the focused source regression**

Expected: Song portion passes; PHRASE portion still fails until Task 3.

---

### Task 3: Neutralize PHRASE realization presentation

**Files:**
- Modify: `src/ui/pages/phrase_page.cpp` in `PhrasePage::drawProductView()` only.
- Test: `tests/test_hybrid_song_orchestration_ui_source_regressions.py`

**Interfaces:**
- Consumes: existing `GroovePuterState::currentGenerationLevel()` and accepted-generation outcome state.
- Produces: truthful labels without changing P1/P2/P3 semantics.

- [ ] **Step 1: Rename presentation label**

Change:

```cpp
gfx.drawText(x + 88, LayoutManager::lineY(0), "DEPTH");
```

to:

```cpp
gfx.drawText(x + 88, LayoutManager::lineY(0), "LEVEL");
```

- [ ] **Step 2: Rename previous-attempt status**

Change:

```cpp
std::snprintf(line, sizeof(line), "LAST G: %s", outcome);
```

to:

```cpp
std::snprintf(line, sizeof(line), "LAST GEN: %s", outcome);
```

- [ ] **Step 3: Preserve all event handling**

Do not change `handleProductEvent()`, `generationLevelCode()`, P key behavior, `G` behavior or placement.

- [ ] **Step 4: Run focused source regression**

Expected:

```text
Hybrid Song orchestration UX source regressions: PASS
```

---

### Task 4: Regression verification

**Files:**
- No production changes expected.

**Interfaces:**
- Consumes: exact post-patch SHA.
- Produces: evidence that presentation cleanup did not alter musical/runtime behavior.

- [ ] **Step 1: Run focused 0.9.11 UX workflow**

Expected: PASS.

- [ ] **Step 2: Run existing core regressions on the PR SHA**

Required evidence includes the existing Song generation source regression and relevant Phrase/Pattern/P3 UI checks.

- [ ] **Step 3: Confirm build gates**

At minimum verify the existing PR workflows for host regressions and Cardputer ADV compile are not newly broken by this presentation-only patch.

- [ ] **Step 4: Inspect diff**

Expected production diff:

```text
src/ui/pages/song_page.cpp   presentation strings only
src/ui/pages/phrase_page.cpp presentation strings only
```

No changes to generation, persistence, runtime ownership or keyboard handling.

---

### Task 5: Record next UX dependencies

**Files:**
- Update: `docs/audits/0_9_11_HYBRID_SONG_ORCHESTRATION_UX_CENSUS_2026-09-09.md` only if research findings change.

**Interfaces:**
- Produces: explicit follow-up boundary for O1/O2/O3.

- [ ] **Step 1: Keep future semantics deferred**

The next checkpoint must separately design/prove:

```text
EMPTY vs REST persistence
material reuse vs make-unique semantics
NEW vs VAR vs DEV runtime contracts
occurrence relation storage/projection
keyboard ownership redesign
form assistance that preserves accepted user material
```

None of these are folded into the truthfulness patch.
