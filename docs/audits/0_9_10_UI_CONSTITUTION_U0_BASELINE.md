# 0.9.10 — UI Constitution V1 — U0 Baseline

Checkpoint: `U0 — Baseline / preservation census / ownership map`

UI workstream:

`feature/20260906-04-0.9.10-ui-constitution-v1`

Historical implementation base:

`465a1b1189ecb94fdc82a66ca0ec02de248609e2`

This document records the state inspected before the first production UI change. It is not a permanent final verification pin.

## Parallel-line provenance

At U0 inspection:

- UI/P3-U1 source line: `465a1b1189ecb94fdc82a66ca0ec02de248609e2`
- P3 lifetime line: `aded0e183a934f78623030226b67b5d0b598648b`
- merge base between them: `020582e19933e0c83bc62ad7a05bced2ceab2b23`
- P3 lifetime is eight commits ahead of that merge base;
- P3-U1 is on a separate development line.

The eight P3-lifetime commits affect recovery/SD diagnostics, `GroovePuter.ino`, generation reference vocabulary, platform SD code, build/upload tooling and tests. They do **not** modify `src/ui/**` in that delta.

Therefore U0/U1 UI semantic work may proceed in parallel on the dedicated UI branch without pretending those eight commits are merged. Integration with the current P3-lifetime candidate remains an explicit later provenance boundary.

No UI checkpoint may silently claim those P3-lifetime commits as part of its verified candidate until ancestry actually contains them.

## Current ownership map

### Navigation/domain location

Current runtime navigation is centered around:

- `WorkflowMode`
- `Workspace`
- integer `WorkflowPages::*` page IDs
- `normalizeLegacyPage()`
- `workspaceForPage()` / `pageForWorkspace()` / `modeForPage()`

Observed debt:

- legacy persisted identities (`Generation`, `Texture`, Synth parameter aliases, Sampler) remain members of runtime enums and are normalized repeatedly;
- page ID is doing more work than a canonical `Workflow / Target / Surface` location should eventually do.

Preservation requirement:

- current workflow organization and established physical navigation are not to be changed merely because the runtime representation is cleaned up.

### Geometry

Current `screen_geometry.h` declares:

- HEADER: y 0..15, 16 px;
- CONTENT: y 16..118, 103 px;
- FOOTER: y 119..134, 16 px;
- PERFORMANCE_HUD: y 109..118, 10 px, inside CONTENT;
- `MAX_LINES = 8` with 12 px line height.

This creates a hidden overlap contract: a normal page is told it has the entire 103 px content area while the global performance strip later consumes its bottom 10 px.

Target Standard geometry under the Constitution:

- chrome 0..15;
- body 16..108;
- performance strip 109..118;
- footer 119..134.

This is an ownership correction, not permission to redesign preserved page composition.

### Page layout side effect

`IPage::setBoundaries()` currently:

1. stores the rectangle;
2. calls `UI::publishActivePageTitle(getTitle().c_str())`.

This proves layout propagation currently publishes global presentation state.

`MultiPage::draw()` and `MultiPage::handleEvent()` also propagate bounds to the active child as part of ordinary operation.

Constitution classification:

`KNOWN DEFECT`

Required property:

**LAYOUT MUST BE SEMANTICALLY PURE.**

The first RED should make it impossible for `setBoundaries()` to publish semantic/page identity.

### Global page title storage

`ui_active_page_title.h` owns a separate global title buffer. It is currently fed by page layout and later read by global status chrome.

Current role:

geometry propagation -> title storage -> title substring parsing -> semantic status context.

Constitution classification:

`KNOWN DEFECT`

Presentation title may survive as presentation data if still useful, but it must stop being an authority for semantic location.

### Status context

`ui_common.cpp` currently derives `UiStatusContext` using `statusContextForTitle()` and substring checks such as:

- MIDI PLAYER;
- MIDI KEYBOARD / PERFORM;
- SYNTH A / SYNTH B;
- FEEL / TEXTURE;
- GENRE;
- DRUM;
- SONG / ARRANGE;
- PROJECT / SETUP;
- OVERVIEW / PATTERN.

Both `drawStandardHeader()` and `drawStatusChrome()` participate in this title-driven context path.

Constitution classification:

`KNOWN DEFECT`

Required property:

renaming display text does not change semantic context.

### Status source

`UiStatusSource` currently contains:

- Pattern;
- Song;
- Smf.

It does not contain Phrase.

After SMF handling, `buildUiStatusSnapshot()` currently chooses:

`miniAcid.songModeEnabled() ? Song : Pattern`

rather than reading the actual per-Synth engine `SequencedSource` for Synth A/B.

Constitution classification:

`KNOWN DEFECT / P0 semantic truth`

Required property:

when source is applicable to Synth A/B, the displayed source is derived from the authoritative engine source. Song/SMF transport ownership must not be collapsed into the same axis without an explicit scope rule.

### Frame coherence

`MiniAcidDisplay::update()` currently performs broadly:

1. persistence/style/paging service;
2. page `tick()`;
3. page `draw()`;
4. global status chrome;
5. performance HUD;
6. overlays/toast;
7. flush.

The page renderer, chrome and HUD can obtain live state independently. No explicit whole-frame semantic-revision contract currently prevents a body/header hybrid state.

Constitution classification:

`KNOWN ARCHITECTURAL GAP`

U1 must establish the narrowest coherent-frame semantic snapshot/view boundary without copying full Pattern/Phrase buffers or holding the audio mutation guard during drawing.

### Header ownership

Pages may draw a standard header through existing page draw paths. After the page is drawn, `MiniAcidDisplay::update()` calls `drawLiveMixLockBadge()`, which delegates to `drawStatusChrome()` and repaints the full 240x16 header.

Current effective structure:

page header -> global erase/repaint -> final header.

Constitution classification:

`KNOWN DEFECT`

The first ownership migration should preserve the recognizable resulting header as closely as practical while reducing ownership to one shell renderer.

### Renderer residency

`MiniAcidDisplay::getPage_()` lazily creates pages.

On Cardputer it checks free internal DRAM and uses an aggressive eviction mode below 16384 bytes. In normal mode it attempts to keep the current and previous page; in aggressive mode it may keep only the requested page.

Page objects currently own various local continuity details.

Constitution classification:

`KNOWN ARCHITECTURAL GAP`

Required property:

renderer eviction may change memory residency, but must not change defined musician continuity.

No residency policy change is authorized in U0/U1. First characterize which state is actually lost.

## Preservation census

### PERFORM — PRESERVE

Strong preservation target.

Keep unless a specific regression is proven:

- KEY / CHORD / ARP / RHYTHM local contexts;
- fixed positions for the local contexts;
- remembered selected row per local context;
- contextual disabled/N/A behaviour;
- important side-effect hints;
- direct hardware-performance mental model.

U1 architecture work must not redesign PERFORM.

### Synth A/B shell — PRESERVE BEHAVIOR + PRODUCT FORM

Preserve:

- distinct Synth A/B target identity;
- NOTES / KNOBS / MORE local structure;
- existing shortcut/muscle-memory behaviour unless a concrete conflict is proven;
- existing entity colour identity.

Pattern/Phrase source truth may change what the source indicator says, but that semantic fix is not permission to rearrange Synth navigation.

### FEEL — PRESERVE BEHAVIOR / GENERAL FORM

Preserve:

- field-list editing mental model;
- current useful field grouping/order;
- distinction between live and next-generation semantics;
- recognizable page structure.

Known future cleanup such as Tab duplicating ordinary field navigation requires global command-map evidence before changing the key.

### GENRE — PRESERVE BEHAVIOR / GENERAL FORM

Preserve field-list/product form while later replacing implementation-facing vocabulary only where the new label precisely describes behaviour.

Do not reorder fields as a side effect of introducing a common field-list grammar.

### Themes / entity colours — PRESERVE

Preserve CARBON/CYBER/AMBER identity and Synth A/Synth B/Drums entity distinction.

Known defect: a theme must not be allowed to remove mandatory information.

### Current Phrase NOTES renderer — PROTOTYPE

Current P3-U1 Phrase rendering is explicitly not treated as a preserved final product surface.

Known limitations from the inspected implementation/audit:

- at most two bars shown in the detail span;
- rows derived from event-buffer index modulo visible rows;
- final edit behaviour remains incomplete in the current slice;
- the implementation bypasses the ordinary Pattern/MultiPage NOTES draw path.

The musical semantics behind it are preserved; this visual prototype is not.

### Global status/header ownership — KNOWN DEFECT

Do not preserve:

- title substring semantics;
- PAT/PHR lie;
- double header ownership;
- default assumption that page header is authoritative when shell later replaces it.

Preserve only useful final visual/product identity while rebuilding ownership underneath.

## What U0 does not authorize

U0 does not authorize:

- a new visual design;
- a new theme;
- new animations;
- a piano roll;
- removal of existing features;
- stack/DRAM policy changes;
- audio architecture changes;
- persistence-format expansion;
- TAKE history as a new domain model;
- global keyboard remapping;
- changing page residency policy.

## First implementation boundary

The first production boundary is `U1A — semantic location/layout purity`, not a shell redesign.

U1A is expected to prove at minimum:

1. `setBoundaries()` no longer publishes active page/title semantic state;
2. semantic status context no longer depends on title substring parsing;
3. presentation titles remain free to change without changing context;
4. existing page/workflow navigation and visible product form remain otherwise unchanged;
5. tests demonstrate the old failure and the new invariant.

PAT/PHR source truth belongs immediately after or together with the typed-location boundary only if it can be done without conflating sequenced source with Song/SMF transport ownership.

## U0 decision

Proceed with UI Constitution V1 in the dedicated branch.

Parallel work is acceptable because the inspected eight-commit P3-lifetime delta does not touch `src/ui/**`. This is not permission to ignore later integration: ancestry and full relevant gates must be re-established before closure.

The migration starts by making the existing GroovePuter harder to lie or lose context — not by making it look different.
