# UI Constitution U1C Frame Status Snapshot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make global status chrome render from one bounded semantic snapshot captured once by `MiniAcidDisplay` before the active page draws, eliminating the second live `MiniAcid` read during chrome rendering.

**Architecture:** `MiniAcidDisplay` remains the shell/frame owner. It derives typed `UiStatusContext`, calls `UI::captureUiStatusSnapshot(MiniAcid&, UiStatusContext)` once, lets the current page draw, then passes the immutable captured `UiStatusSnapshot` to a pure chrome renderer. This is a narrow first step toward the Constitution's “one frame = one truth” law: it makes shell status coherent, but it does **not** yet claim that Pattern/Phrase body material is captured into the same snapshot.

**Tech Stack:** C++17 production UI, Python source-regression gate, GitHub Actions, existing U1A/U1B/P3-U1 preservation gates.

**Spec:** `docs/contracts/0_9_10_UI_CONSTITUTION_V1.md`

## Global Constraints

- Preserve visible product form; no intentional chrome redesign in U1C.
- Preserve U1A typed semantic location and U1B `UiSequencedSource` / `UiTransportOwner` authority.
- Do not change Pattern/Phrase playback, note lifetime, Undo ownership, audio architecture, page order, themes, shortcuts or product vocabulary.
- Do not change framebuffer strategy, DirtyTileTracker policy, page residency/eviction, DRAM ceiling or stack sizes.
- Do not copy Pattern/Phrase material, framebuffer pixels or renderer backend objects into the shell snapshot.
- `UiStatusSnapshot` remains bounded by the existing `sizeof(UiStatusSnapshot) <= 16` contract.
- No production code before the focused U1C RED is observed failing on the exact branch candidate.
- U1C closes only shell-status capture/render coherence. Full body-material coherent-view/lifetime evidence remains a later checkpoint.

---

### Task 1: Wire and observe the existing U1C RED

**Files:**
- Existing RED: `tests/test_ui_constitution_u1c_frame_snapshot.py`
- Create: `tests/run_ui_constitution_u1c_tests.sh`
- Modify: `.github/workflows/0_9_10_ui_constitution_v1.yml`

**Interfaces:**
- Consumes: existing `ui_common.h`, `ui_common.cpp`, `MiniAcidDisplay::update()`.
- Produces: a focused CI job named `u1c-frame-status-snapshot`.

- [ ] **Step 1: Add a focused runner**

Create `tests/run_ui_constitution_u1c_tests.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

python3 "${ROOT_DIR}/tests/test_ui_constitution_u1c_frame_snapshot.py"
printf '%s\n' 'UI Constitution U1C frame-status snapshot gate: PASS'
```

- [ ] **Step 2: Add U1C to the branch workflow**

Add these path triggers on push and pull request:

```yaml
- 'tests/test_ui_constitution_u1c_*'
- 'tests/run_ui_constitution_u1c_tests.sh'
- 'docs/audits/0_9_10_UI_CONSTITUTION_U1C_EVIDENCE.md'
- 'docs/superpowers/plans/2026-09-06-ui-constitution-u1c-frame-status-snapshot.md'
```

Add job:

```yaml
u1c-frame-status-snapshot:
  runs-on: ubuntu-latest
  timeout-minutes: 15
  steps:
    - uses: actions/checkout@v4
      with:
        fetch-depth: 0

    - name: Run U1C focused gate
      shell: bash
      run: |
        set -o pipefail
        bash tests/run_ui_constitution_u1c_tests.sh 2>&1 | tee ui-constitution-u1c.log

    - name: Upload U1C focused gate log
      if: always()
      uses: actions/upload-artifact@v4
      with:
        name: ui-constitution-u1c-log
        path: ui-constitution-u1c.log
        if-no-files-found: error
```

- [ ] **Step 3: Verify RED on the exact candidate**

Expected failure is specifically that production still lacks the explicit capture/pure-render split:

```text
captureUiStatusSnapshot
pure drawStatusChrome(IGfx&, const UiStatusSnapshot&)
MiniAcidDisplay-owned frameStatus captured before currentPage->draw(...)
```

Do not proceed if the job fails only because the runner path or workflow YAML is malformed.

---

### Task 2: Expose bounded status capture and make chrome a pure snapshot renderer

**Files:**
- Modify: `src/ui/ui_common.h`
- Modify: `src/ui/ui_common.cpp`

**Interfaces:**
- Produces:

```cpp
UI::UiStatusSnapshot UI::captureUiStatusSnapshot(
    MiniAcid& miniAcid,
    UiStatusContext context);

void UI::drawStatusChrome(
    IGfx& gfx,
    const UiStatusSnapshot& status);

void UI::drawLiveMixLockBadge(
    IGfx& gfx,
    const UiStatusSnapshot& status);
```

- [ ] **Step 1: Declare the capture/render boundary in `ui_common.h`**

Replace the live-engine chrome signatures with the three signatures above. Keep `MiniAcid` only on the capture side.

- [ ] **Step 2: Preserve the existing authoritative status derivation**

Keep the current U1B builder logic exactly as the source of truth:

```text
Synth A source -> currentSequencedSource(0)
Synth B source -> currentSequencedSource(1)
Song -> UiTransportOwner::Song
SMF -> UiTransportOwner::Smf
Pattern address -> only UiSequencedSource::Pattern
```

Expose it through `captureUiStatusSnapshot(...)`. Do not create a second builder or copy source/transport logic into `MiniAcidDisplay`.

- [ ] **Step 3: Make `drawStatusChrome` render only the captured snapshot**

Change it from:

```cpp
void drawStatusChrome(IGfx& gfx,
                      MiniAcid& miniAcid,
                      UiStatusContext context);
```

to:

```cpp
void drawStatusChrome(IGfx& gfx,
                      const UiStatusSnapshot& status);
```

The existing `gStatusSnapshot` / formatted-line cache may remain. It compares/format-caches the supplied snapshot only; it must not re-read `MiniAcid`.

- [ ] **Step 4: Make the compatibility badge hook snapshot-only**

Implement:

```cpp
void drawLiveMixLockBadge(IGfx& gfx,
                          const UiStatusSnapshot& status) {
    drawStatusChrome(gfx, status);
}
```

Do not add a compatibility overload that accepts `MiniAcid&`; the RED explicitly guards against restoring the second live read.

---

### Task 3: Let `MiniAcidDisplay` own one status snapshot for the frame

**Files:**
- Modify: `src/ui/miniacid_display.cpp`

**Interfaces:**
- Consumes: `captureUiStatusSnapshot`, snapshot-only `drawLiveMixLockBadge`.

- [ ] **Step 1: Capture after typed context is resolved and before body draw**

In `MiniAcidDisplay::update()`, after computing `statusContext`, add:

```cpp
const UI::UiStatusSnapshot frameStatus =
    UI::captureUiStatusSnapshot(mini_acid_, statusContext);
```

This line must execute before:

```cpp
currentPage->draw(gfx_);
```

- [ ] **Step 2: Reuse the same snapshot after body draw**

Replace:

```cpp
UI::drawLiveMixLockBadge(gfx_, mini_acid_, statusContext);
```

with:

```cpp
UI::drawLiveMixLockBadge(gfx_, frameStatus);
```

Do not capture again after page draw.

- [ ] **Step 3: Keep the snapshot semantic-only**

Do not add `currentPhraseBuffer`, Pattern event-buffer copies, framebuffer references, `CardputerDisplay`, or backend-specific renderer state to `MiniAcidDisplay` for this checkpoint.

---

### Task 4: Verify GREEN and preservation on one exact HEAD

**Files:** none beyond Tasks 1–3.

- [ ] **Step 1: Run U1C focused gate**

Expected: `u1c-frame-status-snapshot` SUCCESS.

- [ ] **Step 2: Run inherited Constitution gates on the same SHA**

Expected on the same exact head:

```text
u1a-semantic-location       SUCCESS
u1b-source-transport-truth  SUCCESS
u1c-frame-status-snapshot   SUCCESS
preserve-p3-u1-semantics    SUCCESS
```

- [ ] **Step 3: Review the production diff**

The U1C production diff should be limited to:

```text
src/ui/ui_common.h
src/ui/ui_common.cpp
src/ui/miniacid_display.cpp
```

plus U1C test/CI/docs files. Any changes to DSP, Phrase runtime, scene storage, page layout, framebuffer, residency or themes are scope violations.

---

### Task 5: Record U1C evidence without creating a stale verification pin

**Files:**
- Create: `docs/audits/0_9_10_UI_CONSTITUTION_U1C_EVIDENCE.md`

**Interfaces:** none.

- [ ] **Step 1: Record RED provenance**

Record the exact observed RED SHA/run and the actual failure reason.

- [ ] **Step 2: Record GREEN architecture**

Document:

```text
semantic capture owner: MiniAcidDisplay frame shell
status truth builder: UI::captureUiStatusSnapshot
chrome renderer input: const UiStatusSnapshot&
second live MiniAcid read during chrome render: forbidden
snapshot size: existing <=16-byte contract
```

- [ ] **Step 3: State what U1C does not prove**

Explicitly leave pending:

```text
body Pattern/Phrase coherent read-view lifetime
renderer/pixel ownership
four-zone body geometry
physical 1:1 readability
Cardputer hardware acceptance
static/heap/construction-peak memory evidence
render-transfer/audio timing evidence
```

- [ ] **Step 4: Require a fresh exact-head rerun after the evidence commit**

Because the evidence path is a workflow trigger, the evidence commit itself must receive SUCCESS for all four jobs before U1C is called host/CI-closed.
