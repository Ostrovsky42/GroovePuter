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

- [x] **Step 3: Verify RED**

Authoritative RED was observed before production edits:

```text
AssertionError: Song generator hint still exposes misleading musical mode "RND"
```

GitHub Actions run: `34325261060`.

---

### Task 2: Neutralize Song generator mode presentation

**Files:**
- Modify: `src/ui/pages/song_page.cpp` in `SongPage::drawGeneratorHint()` only.
- Test: `tests/test_hybrid_song_orchestration_ui_source_regressions.py`

**Interfaces:**
- Consumes: existing `gen_mode_` integer selector.
- Produces: neutral UI text describing an alternative generation selection without promising evolution/fill semantics.

- [x] **Step 1: Keep selector behavior unchanged**

`cycleGeneratorMode()`, `materializeSongTracks()`, Atlas variation selection, modeTag and keyboard mapping are unchanged.

- [x] **Step 2: Replace semantic mode-name array**

The current presentation is now:

```cpp
int modeIdx = static_cast<int>(gen_mode_);
char buf[20];
std::snprintf(buf, sizeof(buf), "GEN ALT:%d/4", modeIdx + 1);
```

- [x] **Step 3: Run the focused source regression**

The focused contract passed in the verified patch job.

---

### Task 3: Neutralize PHRASE realization presentation

**Files:**
- Modify: `src/ui/pages/phrase_page.cpp` in `PhrasePage::drawProductView()` only.
- Test: `tests/test_hybrid_song_orchestration_ui_source_regressions.py`

**Interfaces:**
- Consumes: existing `GroovePuterState::currentGenerationLevel()` and accepted-generation outcome state.
- Produces: truthful labels without changing P1/P2/P3 semantics.

- [x] **Step 1: Rename presentation label**

`DEPTH` is now presented as `LEVEL`.

- [x] **Step 2: Rename previous-attempt status**

`LAST G` is now presented as `LAST GEN`.

- [x] **Step 3: Preserve all event handling**

`handleProductEvent()`, `generationLevelCode()`, P key behavior, G behavior and placement are unchanged.

- [x] **Step 4: Run focused source regression**

Observed in the patch job:

```text
Hybrid Song orchestration UX source regressions: PASS
Song generation source regressions passed
Phrase UI source regressions: PASS
```

Production patch commit: `e9482e2340990c12614d816da41e40b36ccb9a8b`.

---

### Task 4: Regression verification

**Files:**
- No production changes expected.

**Interfaces:**
- Consumes: exact post-patch SHA.
- Produces: evidence that presentation cleanup did not alter musical/runtime behavior.

- [ ] **Step 1: Run focused 0.9.11 UX workflow on the current PR head**

Expected: PASS.

- [ ] **Step 2: Run existing core regressions on the PR SHA**

Required evidence includes the existing Song generation source regression and relevant Phrase/Pattern/P3 UI checks.

- [ ] **Step 3: Confirm build gates**

At minimum verify the existing PR workflows for host regressions and Cardputer ADV compile are not newly broken by this presentation-only patch.

- [x] **Step 4: Inspect diff**

Verified production diff is presentation-only:

```text
src/ui/pages/song_page.cpp   removes RND/SMART/EVOL/FILL presentation; shows GEN ALT ordinal
src/ui/pages/phrase_page.cpp DEPTH -> LEVEL; LAST G -> LAST GEN
```

No generation, persistence, runtime ownership or keyboard handling changed.

---

### Task 5: Record next UX dependencies

**Files:**
- Update: `docs/audits/0_9_11_HYBRID_SONG_ORCHESTRATION_UX_CENSUS_2026-09-09.md` only if research findings change.

**Interfaces:**
- Produces: explicit follow-up boundary for O1/O2/O3.

- [x] **Step 1: Keep future semantics deferred**

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
