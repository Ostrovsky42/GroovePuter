# UI Constitution U1G Tab Grammar Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make plain Tab mean peer/local representation only, removing the redundant `next field` meaning from FEEL and GENRE while preserving Synth/PERFORM and Fn+Tab workflow behavior.

**Architecture:** No command framework is introduced. FEEL and GENRE already have complete Up/Down focus navigation, so U1G removes only their plain-Tab capture and updates their footer copy. Synth and PERFORM remain explicit peer-representation consumers; top-level Meta/Fn+Tab remains the workflow owner.

**Tech Stack:** C++17 embedded UI, Python structural regression tests, GitHub Actions.

**Spec:** `docs/contracts/0_9_10_UI_CONSTITUTION_V1.md`

## Global Constraints

- No musical state, transport, source, Pattern/Phrase lifetime, Undo, Scene persistence, page geometry, or renderer ownership changes.
- Preserve Synth `NOTES / KNOBS / MORE` Tab cycling.
- Preserve PERFORM `KEY / CHORD / ARP / RHYTHM` Tab behavior.
- Preserve Meta/Fn+Tab workflow switching before page dispatch.
- FEEL/GENRE Up/Down remain the field-navigation authority.
- No visual redesign beyond removing the now-invalid `TAB/` footer token.

---

### Task 1: Characterize Tab semantic split

**Files:**
- Create: `tests/test_ui_constitution_u1g_tab_grammar.py`
- Create: `tests/run_ui_constitution_u1g_tests.sh`
- Create: `.github/workflows/0_9_10_ui_constitution_u1g.yml`

**Interfaces:**
- Consumes: existing page handlers and footer literals.
- Produces: a focused gate proving one semantic meaning per Tab context.

- [ ] **Step 1: Write RED assertions** requiring FEEL/GENRE not to consume plain Tab, requiring Up/Down focus anchors to remain, and requiring Synth/PERFORM/Fn+Tab anchors to remain.
- [ ] **Step 2: Run `bash tests/run_ui_constitution_u1g_tests.sh`.** Expected RED: FEEL and GENRE still contain `UIInput::isTab(event)` and advertise `TAB/U/D:FIELD`.
- [ ] **Step 3: Commit the RED gate** without production changes.

### Task 2: Remove field-list Tab capture

**Files:**
- Modify: `src/ui/pages/feel_page.cpp`
- Modify: `src/ui/pages/genre_page.cpp`
- Test: `tests/test_ui_constitution_u1g_tab_grammar.py`

**Interfaces:**
- Consumes: `UIInput::navCode`, existing `moveFocus` functions.
- Produces: FEEL/GENRE where Up/Down alone navigate fields and plain Tab is unclaimed.

- [ ] **Step 1: Delete only the `UIInput::isTab(event) -> moveFocus(1)` block from FEEL.**
- [ ] **Step 2: Change FEEL footer from `TAB/U/D:FIELD L/R:CHANGE` to `U/D:FIELD L/R:CHANGE`.**
- [ ] **Step 3: Delete only the equivalent Tab block from GENRE.**
- [ ] **Step 4: Change GENRE footer to `U/D:FIELD L/R:CHANGE`.**
- [ ] **Step 5: Run the U1G focused gate.** Expected PASS.
- [ ] **Step 6: Commit the minimal two-file GREEN.**

### Task 3: Preserve inherited contracts

**Files:**
- Create after verification: `docs/audits/0_9_10_UI_CONSTITUTION_U1G_EVIDENCE.md`

**Interfaces:**
- Consumes: U1A-U1F gates and inherited P3-U1 gate.
- Produces: evidence that the input-grammar correction did not alter engine or Pattern/Phrase semantics.

- [ ] **Step 1: Verify U1G focused workflow on the exact GREEN head.**
- [ ] **Step 2: Verify the existing UI Constitution matrix U1A-U1F and inherited P3-U1 on the same production head.**
- [ ] **Step 3: Record RED, GREEN, exact verification SHA/run IDs, and the intentionally unchanged Synth/PERFORM/Fn+Tab behaviors.**
