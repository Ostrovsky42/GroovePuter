# GroovePuter 0.9.10 — UI Constitution V1 — U1A Evidence

Status: **HOST/CI CLOSED; HARDWARE VISUAL + MEMORY/REALTIME DELTA PENDING**

Branch: `feature/20260906-04-0.9.10-ui-constitution-v1`

Historical UI/P3-U1 baseline: `465a1b1189ecb94fdc82a66ca0ec02de248609e2`

U1A verified candidate before this evidence-only commit: `4d9c87e24961583bc9e42b34cf4283dd6a1db516`

Authoritative CI run for that candidate: `34045950095`

## Scope

U1A is deliberately narrow. It changes semantic ownership of top-level UI location/context and removes geometry/title text as semantic inputs.

It does **not** implement:

- PAT/PHR source truth separation (U1B);
- one-frame coherent material snapshots beyond the existing status slice;
- shell/pixel-owner migration (U2);
- continuity store or page-residency changes;
- framebuffer replacement or MEMORY-R1 renderer work;
- visual redesign;
- product-language rewrite;
- Phrase timeline redesign.

## Contracts carried into U1A

U1A is governed by:

- `docs/contracts/0_9_10_UI_CONSTITUTION_V1.md`
- `docs/contracts/0_9_10_UI_CONSTITUTION_V1_RESOURCE_LAW.md`
- `docs/audits/0_9_10_UI_CONSTITUTION_U0_BASELINE.md`

Preservation rule for this slice: architecture may become stricter underneath, but existing product form and P3-U1 musical semantics are not intentionally redesigned.

Resource rule for this slice: typed semantic APIs remain independent of the current Cardputer full-framebuffer implementation and page-residency policy.

## RED evidence

The focused U1A RED was published at `32d5884a3e75e8d79befc2067eb55132b6e31d71`.

GitHub Actions run `34045522383` failed for two independently identified reasons:

1. **real semantic RED** — `ui_common.cpp` still contained `statusContextForTitle` / title-driven status-context derivation;
2. **harness inheritance noise** — C++17 `-Werror` promoted already-existing C++20 bit-field-initializer extension warnings from inherited headers to errors.

The harness warning was not treated as the semantic defect. The repository already uses explicit suppression for this inherited extension in other host gates; U1A follows the same principle.

## GREEN implementation boundary

The resulting U1A architecture now has:

### Geometry purity

`IPage::setBoundaries()` only updates geometry. It no longer publishes active-page title or other global semantic state.

Invariant:

```text
setBounds(rect)
    -> geometry only
```

### Typed top-level location

`src/ui/ui_location.h` introduces bounded typed coordinates:

```text
UiLocation
    WorkflowMode workflow
    UiTarget target
    UiSurface surface
```

Canonical runtime page identity is explicitly projected through `tryUiLocationForPage()`.

Legacy persisted aliases are not normalized inside this new semantic type. Migration remains at the existing persistence/navigation boundary.

### Presentation text is not semantic API

Status context is no longer reconstructed from page-title strings or substring matching.

`MiniAcidDisplay`, as the top-level navigation/display owner, projects the canonical page into a typed `UiLocation`, derives `UiStatusContext`, and passes that context explicitly to global status rendering.

### Source policy intentionally unchanged

U1A does **not** claim PAT/PHR correctness.

The current source-selection logic remains the pre-U1A Song/Pattern/SMF policy. The code explicitly leaves per-Synth PAT/PHR source-vs-transport separation to U1B.

This is intentional scope control, not a completed source-truth claim.

## Resource Law preservation

U1A does not add a semantic dependency on:

- `CardputerDisplay`;
- the full RGB565 framebuffer;
- `frame_`;
- the `<16384` page-eviction heuristic;
- page-object residency as musical state ownership.

The Resource Law guard is part of the focused runner on the verified candidate.

No strip/tile/direct renderer was implemented here.

## Verification on candidate `4d9c87e24961583bc9e42b34cf4283dd6a1db516`

GitHub Actions run: `34045950095`.

### `u1a-semantic-location`

Result: **SUCCESS**

This gate includes:

- layout-purity behavior test;
- typed-location mapping test;
- status-chrome source regression guard rejecting title-driven semantic context;
- U1A Resource-Law source regression guard.

### `preserve-p3-u1-semantics`

Result: **SUCCESS**

Command:

```text
bash tests/run_pattern_phrase_p3_u1_tests.sh
```

This protects the inherited P3-U1 Pattern/Phrase semantics while the UI semantic ownership changes underneath them.

## Preservation statement

No intentional visual redesign is part of U1A.

In particular, U1A does not intentionally change:

- PERFORM layout/KEY-CHORD-ARP-RHYTHM model;
- Synth `NOTES / KNOBS / MORE` navigation;
- FEEL/GENRE field organization;
- CARBON/CYBER/AMBER product identity;
- Pattern/Phrase mutation semantics;
- cross-bar note lifetime;
- current page-residency behavior.

The principal visible difference expected from this slice is **none**. Its purpose is to make the same UI semantic context originate from typed navigation identity rather than display text/geometry side effects.

## Memory / realtime evidence

U1A did not intentionally change:

- framebuffer strategy;
- DirtyTileTracker strategy;
- page eviction threshold;
- audio architecture;
- task stacks;
- DRAM ceiling;
- Phrase/runtime event capacity.

However, no fresh Cardputer memory/realtime characterization is attached to this U1A host checkpoint. Therefore no numeric DRAM, construction-peak, stack-HWM, display-transfer or audio-underrun delta is claimed here.

The separate R0/R1 memory workstreams remain authoritative for those measurements.

## Hardware status

**PENDING.**

Host/CI evidence proves the semantic ownership boundary and inherited P3-U1 test preservation. It does not prove physical 240x135 visual equivalence, readability or realtime interaction on Cardputer ADV.

Accordingly this document does not claim `UI ready` or hardware closure.

## U1A technical defense

**Who owns top-level semantic location?**

The canonical navigation/display layer, projected into `UiLocation`.

**Who owns presentation title?**

Presentation only; title text no longer defines semantic context.

**What is the lifetime?**

Typed location is derived from current canonical navigation state and is not tied to page-renderer object lifetime.

**What happens on layout change?**

Only geometry changes.

**What happens on unknown/noncanonical page identity?**

`tryUiLocationForPage()` fails closed instead of guessing semantics from text.

**What test prevents regression?**

The U1A layout-purity, typed-location and source-regression guards in `tests/run_ui_constitution_u1a_tests.sh`.

**What existing product behavior was intentionally preserved?**

The P3-U1 focused gate remains GREEN on the verified candidate; no product visual redesign belongs to this slice.

## Next admissible semantic slice

U1B may now address the distinct unresolved problem:

```text
transport/playback ownership
        !=
per-target sequenced source
```

For Synth A/B, PAT/PHR must ultimately come from authoritative engine `SequencedSource` rather than Song mode or page inference.

U1B must begin with its own RED and must not silently fold shell redesign, framebuffer work or product visual redesign into the source-truth correction.
