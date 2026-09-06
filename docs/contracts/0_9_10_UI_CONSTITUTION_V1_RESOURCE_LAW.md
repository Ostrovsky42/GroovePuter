# GroovePuter 0.9.10 — UI Constitution V1 — Resource Law

Status: normative addendum to `docs/contracts/0_9_10_UI_CONSTITUTION_V1.md`.

This addendum exists because UI architecture and memory architecture must remain compatible on Cardputer ADV. It does **not** authorize MEMORY-R1 implementation, framebuffer replacement, page-residency redesign, stack changes, DRAM-ceiling changes, or audio-architecture changes. It constrains UI design so those future memory changes do not require a product rewrite.

## Core law

**CAPABILITY != SIMULTANEOUS RESIDENCY.**

A product capability may remain available without requiring all of its UI/rendering/runtime resources to remain resident while its surface is inactive.

Memory pressure may change caching, prefetch and renderer/workspace residency. It may not change musical semantics, command meaning, Undo semantics, navigation meaning, source ownership or the existence of musical/domain state.

## Rendering backend is not a product contract

The current Cardputer renderer may use a full RGB565 framebuffer and dirty-region tracking. UI Constitution V1 must not make the existence of a full-screen framebuffer a semantic or surface API requirement.

A surface must express a deterministic visible result through a bounded rendering capability. The backend may later be implemented as:

- full framebuffer;
- strip renderer;
- tile renderer;
- direct clipped redraw;
- another bounded backend proven by measurement.

Changing backend must not require changing the musical/domain semantics of pages or surfaces.

Surface rendering must be designed toward these properties:

- deterministic from authoritative/read-only state plus bounded UI view state;
- idempotent for the same state and viewport;
- clip-safe;
- replayable without reading previous framebuffer contents;
- independent of XOR/restore-from-old-pixel tricks;
- allocation-free in steady-state rendering paths;
- not dependent on screen-wide retained pixel history.

The current DirtyTileTracker may remain as a useful implementation mechanism, but it is not part of the product-level UI Constitution. The Constitution promises visible behaviour and bounded rendering semantics, not a particular framebuffer strategy.

## Page lifecycle is not subsystem lifecycle

**PAGE EXISTENCE MUST NEVER BE REQUIRED FOR MUSICAL STATE EXISTENCE.**

Destroying or recreating a UI page/renderer must not, by itself, destroy or reinterpret musical/domain state.

Examples of the intended boundary:

- destroying the MIDI Player page does not imply stopping or destroying an authoritative SMF playback subsystem;
- destroying the Sampler page does not imply destroying an active sample voice/domain owner;
- destroying the Phrase page does not imply destroying Phrase material;
- destroying a Pattern/Phrase NOTES renderer does not imply changing the engine SequencedSource.

`onEnter` / `onExit` may own UI workspace resources such as browser rows, temporary waveform/display caches, component graphs and editor scratch. They may not silently become lifetime owners for sounding or accepted musical material unless that ownership is explicitly part of the domain contract.

## Page recreation is an invariant

The low-memory implementation already permits page eviction. UI Constitution therefore treats recreation as an ordinary supported event, not an emergency anomaly.

For every surface whose context is expected to survive navigation/eviction:

```text
destroy renderer
    -> recreate renderer
    -> reconcile against authoritative state
    -> same musical state
    -> same defined musician-visible context
    -> same semantic commands available
```

If a value must survive renderer destruction, it cannot be owned only by the renderer object.

Its owner must be either:

- musical/domain state; or
- bounded UI continuity/session state.

Ephemeral animation phase, transient press state and construction-only scratch may remain renderer-local.

Selection, viewport, selected bar/note and other spatial continuity must each be classified deliberately rather than inherited accidentally from page-object lifetime.

## Resource pressure is semantically neutral

The current implementation may use thresholds such as a free-DRAM heuristic to choose aggressive renderer eviction. Such numeric thresholds are implementation policy, not product semantics.

Crossing a memory threshold may change:

- how many inactive renderers are retained;
- cache/prefetch policy;
- construction timing;
- scratch residency.

It must not change:

- available musical commands;
- command meaning;
- selected source semantics;
- transport semantics;
- Undo semantics;
- workflow meaning;
- accepted material;
- user-visible truth.

A user must not experience different instrument semantics merely because free heap is just above or below an implementation threshold.

## Memory evidence for a UI surface has two costs

`sizeof(Page)` or steady-state retained bytes are insufficient evidence.

For each heavy page/surface/workspace, memory characterization must distinguish at least:

1. **RETAINED COST** — memory remaining while the page/surface is active after construction settles.
2. **CONSTRUCTION PEAK** — worst bounded free-heap/largest-block state during constructor, `onEnter`, first draw and other required activation work.

A future residency gate should be able to observe, as applicable:

```text
PAGE ENTER
preFree
preLargest

CONSTRUCTION
localMinFree
localMinLargest

PAGE ACTIVE
postFree
postLargest

PAGE EXIT
recoveredFree
recoveredLargest

A -> B -> A repeated
no drift
heap integrity PASS
```

Numerical pass/fail thresholds are not invented by this document. MEMORY-R1 or current authoritative memory contracts establish them from hardware evidence.

## UI state must not duplicate musical state

The resource architecture must not solve rendering convenience by creating a second authoritative musical model.

Preferred ownership remains:

```text
ENGINE / DOMAIN STATE
        |
        v
bounded coherent read projection
        |
        v
UI surface + bounded continuity state
        |
        v
semantic command
        |
        v
authoritative mutation owner
```

UI-local state may include selection, viewport, focus, drag preview and other bounded presentation state. Authoritative note onset/duration/source/material must remain owned by the existing musical/runtime model.

## Phrase timing axes remain independent

Phrase editing must keep these concepts independent:

- **MUSICAL EXTENT** — 1 / 2 / 4 / 8 bars;
- **VIEWPORT** — which bounded time interval is visible;
- **SNAP / GRID** — editing resolution such as 1/8 / 1/16 / 1/32;
- **NOTE DURATION** — real bounded duration in Phrase time;
- playback LOOP/ONESHOT behaviour, where applicable, remains a separate playback concept.

Screen width does not define musical duration. Phrase length does not require proportionally larger persistent UI state. Increasing Phrase length must primarily change the domain extent and viewport navigation, not multiply renderer memory by the same factor.

The current Phrase prototype's onset + continuation-span direction is compatible with this law. Its fixed two-bar visibility and event-ordinal row layout remain prototype limitations, not contracts.

## Product controls remain musical

Memory implementation details must remain invisible as musical controls.

Do not expose product controls such as:

- BUFFER;
- CACHE;
- RESIDENCY;
- WINDOW SIZE as a memory concept;
- EVENT CAPACITY;
- PAGE ARENA;
- allocator thresholds.

Controls such as SOURCE, LENGTH, GRID and LOOP are valid only where they correspond to real musician decisions and their semantics remain independent of the memory strategy.

## Backend-change acceptance

A future replacement of the current full framebuffer with strip/tile/direct rendering is a MEMORY/renderer implementation checkpoint, not part of U1A.

When that work happens, acceptance must show:

- preserved visible semantics for the same coherent frame state;
- clipping/ownership correctness;
- no new steady-state dynamic allocation;
- bounded construction/scratch memory;
- no regression in page recreation/continuity;
- measured transfer/composition cost;
- hardware audio/SD/MIDI safety.

UI Constitution V1 must make that backend replacement possible without rewriting each product surface's semantic model.

## Normative summary

```text
CAPABILITY != SIMULTANEOUS RESIDENCY
PAGE LIFETIME != MUSICAL SUBSYSTEM LIFETIME
RENDERER MAY DIE; DOMAIN STATE MUST NOT DIE WITH IT
RESOURCE PRESSURE MAY CHANGE CACHE POLICY; NEVER MUSICAL SEMANTICS
RENDERING MUST NOT REQUIRE A FULL-SCREEN FRAMEBUFFER
SURFACE MEMORY EVIDENCE = RETAINED COST + CONSTRUCTION PEAK
UI MAY PROJECT MUSICAL STATE; UI MAY NOT DUPLICATE ITS AUTHORITY
PHRASE EXTENT, VIEWPORT, SNAP AND NOTE DURATION ARE INDEPENDENT
```

These rules are normative for subsequent UI Constitution checkpoints.