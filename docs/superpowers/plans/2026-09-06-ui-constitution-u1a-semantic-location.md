# UI Constitution U1A Semantic Location Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove title/geometry-driven semantic status context and replace it with a typed page-location projection while preserving the existing product appearance and navigation.

**Architecture:** `MiniAcidDisplay` remains the owner of top-level page navigation. It projects the canonical page index into a small typed `UiLocation` (`Workflow + Target + Surface`) and passes the derived status context explicitly to the shell status renderer. `IPage::setBoundaries()` becomes geometry-only. This slice does not redesign chrome, change page order, change local Synth/PERFORM tabs, or solve frame-material coherence yet.

**Tech Stack:** C++17 host tests, Python source regressions, GitHub Actions, ESP32/Cardputer production sources.

**Spec:** `docs/contracts/0_9_10_UI_CONSTITUTION_V1.md`

## Global Constraints

- Preserve the current visible product form except for semantic truth corrections.
- Do not change audio architecture, sequenced-note lifetime, Undo ownership, DRAM ceiling, stack sizes, page residency policy, themes, page order or hardware shortcuts.
- No production code before focused RED coverage exists and is observed failing on the exact branch candidate.
- `setBoundaries()` must change geometry only.
- Presentation title must not be a semantic input.
- Typed location must not infer Synth PAT/PHR source; source remains engine-owned and is handled in the following U1 source-truth slice.
- Do not introduce a generic UI framework in this slice.

---

### Task 1: Characterize geometry purity and typed top-level location

**Files:**
- Create: `tests/test_ui_constitution_u1a.cpp`
- Modify: `tests/test_ui_status_chrome_source_regressions.py`
- Create: `tests/run_ui_constitution_u1a_tests.sh`
- Create: `.github/workflows/0_9_10_ui_constitution_v1.yml`

**Interfaces:**
- Consumes: existing `IPage`, `WorkflowMode`, `WorkflowPages::*`, `UiStatusContext`.
- Produces test expectations for `UI::tryUiLocationForPage(int, UiLocation&)` and `UI::uiStatusContextForLocation(const UiLocation&)`.

- [ ] **Step 1: Write the failing behavioural test**

Create a minimal `IPage` subclass with title `LAYOUT SENTINEL PAGE`. Publish a different global title sentinel first, call `setBoundaries(Rect{4,16,232,93})`, then assert the rectangle changed while `UI::activePageTitle()` did not. The production change that makes this test pass is removal of title publication from `IPage::setBoundaries()`.

Also assert the desired typed mapping:

```cpp
UI::UiLocation location{};
assert(UI::tryUiLocationForPage(WorkflowPages::kGenre, location));
assert(location.workflow == WorkflowMode::Generate);
assert(location.target == UI::UiTarget::Generation);
assert(location.surface == UI::UiSurface::Genre);
assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::Genre);

assert(UI::tryUiLocationForPage(WorkflowPages::kSynthA, location));
assert(location.workflow == WorkflowMode::Hub);
assert(location.target == UI::UiTarget::SynthA);
assert(location.surface == UI::UiSurface::Local);
assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::SynthA);
```

Cover every canonical top-level page and assert an invalid integer fails closed rather than fabricating a location.

- [ ] **Step 2: Replace old source-regression expectations**

The Python regression must reject these production dependencies:

```text
statusContextForTitle
statusContextForTitle(UI::activePageTitle())
publishActivePageTitle(getTitle().c_str())
```

and require explicit typed context to enter `drawStatusChrome`/the compatibility overlay call.

Retain unrelated pattern-address, Song shortcut and local-slot assertions.

- [ ] **Step 3: Add a focused runner**

Compile the C++ test with C++17, warnings-as-errors, repository include path, and `src/ui/ui_core.cpp`, then execute it and the updated Python source regression.

- [ ] **Step 4: Add a branch workflow**

Run the focused runner on pushes to `feature/20260906-04-0.9.10-ui-constitution-v1`, relevant pull requests and manual dispatch. Upload the focused log on every run.

- [ ] **Step 5: Verify RED**

Expected failures are specifically:

- `IPage::setBoundaries()` changes the active title sentinel;
- typed `UiLocation` interfaces do not yet exist;
- source regression sees the existing title-driven semantic path.

Do not proceed to production changes if the failure is due only to compile-path mistakes or workflow setup.

---

### Task 2: Introduce the smallest typed location projection

**Files:**
- Create: `src/ui/ui_location.h`
- Modify: `src/ui/ui_status_chrome.h`

**Interfaces:**

Produce:

```cpp
namespace UI {

enum class UiTarget : uint8_t {
    Performance,
    MidiPlayer,
    Generation,
    Overview,
    SynthA,
    SynthB,
    Drums,
    Song,
    Phrase,
    PhraseCore,
    Project,
};

enum class UiSurface : uint8_t {
    Performance,
    Player,
    Genre,
    Feel,
    Overview,
    Local,
    Drums,
    Arrange,
    Phrase,
    PhraseCore,
    Project,
};

struct UiLocation {
    WorkflowMode workflow;
    UiTarget target;
    UiSurface surface;
};

bool tryUiLocationForPage(int canonicalPage, UiLocation& out);
UiStatusContext uiStatusContextForLocation(const UiLocation& location);

}
```

`UiSurface::Local` is intentional for top-level Synth pages in U1A: the shell does not pretend to know whether NOTES/KNOBS/MORE is active yet. That refinement belongs to the later surface-command migration.

- [ ] **Step 1: Implement `tryUiLocationForPage` as a bounded canonical switch**

Map only current canonical `WorkflowPages` IDs. Do not silently normalize legacy aliases here; legacy migration remains at the existing session/navigation boundary during this slice.

Unknown page IDs return `false` and do not fabricate semantic state.

- [ ] **Step 2: Implement `uiStatusContextForLocation` as a pure typed projection**

Map target to the existing status context tokens. `Phrase`/`PhraseCore` may map to the most accurate existing context available without adding product copy in this slice; if an existing context cannot represent them truthfully, add a typed status context value rather than falling back through title text.

- [ ] **Step 3: Run focused tests**

The location assertions should become GREEN while the layout-purity/source-path assertions remain RED until Task 3.

---

### Task 3: Remove geometry/title semantic authority

**Files:**
- Modify: `src/ui/ui_core.h`
- Modify: `src/ui/ui_common.h`
- Modify: `src/ui/ui_common.cpp`
- Modify: `src/ui/miniacid_display.cpp`

**Interfaces:**
- Consumes: `tryUiLocationForPage`, `uiStatusContextForLocation`.
- Produces: explicit `UiStatusContext` argument entering global status rendering.

- [ ] **Step 1: Make `IPage::setBoundaries()` geometry-only**

Reduce it to the normal `Frame::setBoundaries(rect)` behaviour. Remove the active-title publication dependency from `ui_core.h`.

- [ ] **Step 2: Remove title parsing from status construction**

Delete `titleContains`, `statusContextForTitle` and `gStatusContext` ownership from `ui_common.cpp`.

`drawStandardHeader()` remains a presentation renderer and no longer publishes semantic context.

Change status rendering so the context arrives explicitly, for example:

```cpp
void drawStatusChrome(IGfx& gfx,
                      MiniAcid& miniAcid,
                      UiStatusContext context);
```

`buildUiStatusSnapshot` receives that context rather than reading a global inferred one.

- [ ] **Step 3: Project current top-level location in `MiniAcidDisplay::update()`**

For the already-canonical `page_index_`, call `tryUiLocationForPage`. Convert to `UiStatusContext` and pass it into the existing global chrome hook. Invalid page IDs use `UiStatusContext::Unknown`, matching the existing invalid-page fallback rather than inventing context.

Do not change page rendering order or visual content in this task.

- [ ] **Step 4: Run focused tests and existing P3-U1 gate**

Focused U1A tests must be GREEN. The existing P3-U1 focused runner must remain GREEN because this migration is not allowed to change Pattern/Phrase musical semantics.

- [ ] **Step 5: Commit the GREEN slice**

Commit only the typed location/layout-purity change and its tests. Do not include source-truth PAT/PHR work, shell geometry changes, continuity, input grammar or vocabulary cleanup.

---

### Task 4: Review gate for preservation and provenance

**Files:**
- Modify: `docs/audits/0_9_10_UI_CONSTITUTION_U0_BASELINE.md` only if evidence needs an appended U1A result section.

**Interfaces:** none.

- [ ] **Step 1: Compare visible behaviour by code path**

Confirm no intentional changes to page order, PERFORM contexts, Synth NOTES/KNOBS/MORE, FEEL/GENRE field order, themes, footer copy or shortcuts.

- [ ] **Step 2: Record exact final HEAD and test evidence**

Record the exact GREEN candidate and whether hardware/SDL visual capture is still pending.

- [ ] **Step 3: Architecture defense**

Answer:

- semantic top-level context owner: `MiniAcidDisplay` navigation projection;
- geometry owner: `Frame/IPage` bounds only;
- presentation title owner: presentation only, no semantic authority;
- failure for invalid page: typed projection fails closed to Unknown;
- regression proof: focused U1A behavioural + source tests;
- preserved product behaviour: all existing visual/navigation structure in this slice.
