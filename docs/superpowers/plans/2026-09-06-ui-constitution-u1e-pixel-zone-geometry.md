# UI Constitution V1 — U1E Pixel-Zone Geometry

Checkpoint: `U1E — shell/body pixel-zone geometry`

## Goal

Make the declared standard screen geometry match the Constitution and the pixels that the shell already owns.

This checkpoint removes the hidden overlap between page body and the global performance strip. It does not introduce a new renderer, clipping API, visual redesign, header migration, footer migration, or page-density redesign.

## Current defect

Current geometry declares:

- HEADER: y 0..15;
- CONTENT: y 16..118;
- PERFORMANCE_HUD: y 109..118;
- FOOTER: y 119..134.

Therefore the final 10 pixels of `CONTENT` have two owners: active page and shell HUD.

`LayoutManager::clearContent()` fills the full current CONTENT rectangle, so ordinary page composition explicitly writes into pixels later owned/repainted by the shell.

SMF browser/redraw and Project body overlays also use `Layout::CONTENT.h`, so they currently receive the same overlapping extent.

## Required invariant

**STANDARD PIXEL ZONES ARE ADJACENT AND NON-OVERLAPPING.**

The standard geometry is:

- global chrome: y 0..15, 16 px;
- surface body: y 16..108, 93 px;
- performance strip: y 109..118, 10 px;
- command footer: y 119..134, 16 px.

The four zones cover the 240x135 screen vertically with no gap and no overlap.

## Scope

U1E changes only the declared geometry and the helpers/consumers that already derive their extent from `Layout::CONTENT`.

Expected production change:

- `Layout::CONTENT.h`: 103 -> 93;
- comments/docs around content height and line anchors updated;
- `LayoutManager::clearContent()` automatically stops at y108 because it already uses the CONTENT rectangle;
- SMF and Project consumers already using CONTENT inherit the correct body extent.

## Line anchors

U1E does not mechanically reduce `MAX_LINES` from 8 to 7.

The current product uses `lineY(0)..lineY(7)` as baseline anchors. With unchanged `CONTENT.y=16`, `CONTENT_PAD_Y=2`, and `LINE_HEIGHT=12`, `lineY(7)` remains y=102 and still begins inside the true body.

The Constitution statement that a standard field list has no more than seven *full 12-pixel rows* is not equivalent to saying only seven baseline anchors may exist. Existing pages use the eighth anchor as compact status/hint content.

U1E therefore preserves those spatial habits and updates comments to distinguish full rows from anchors. Any later density cleanup requires separate UX evidence.

## Explicit non-goals

U1E does not:

- change page field order;
- remove or move line-7 status/hint content;
- change fonts, colors, tabs, meters, or themes;
- change PERFORMANCE_HUD position or appearance;
- remove the current double header owner;
- migrate footer ownership;
- add a clipped graphics facade;
- change page object bounds yet;
- change renderer residency or memory policy;
- change Pattern/Phrase musical semantics.

## TDD sequence

1. RED: geometry test proves current CONTENT overlaps PERFORMANCE_HUD.
2. GREEN: reduce CONTENT to the true 93-pixel body and update geometry comments only.
3. Verify that `clearContent()` remains derived from CONTENT rather than a magic y/height.
4. Verify PERFORMANCE_WAVEFORM remains fully inside PERFORMANCE_HUD.
5. Re-run U1A/U1B/U1C/U1D and inherited P3-U1 preservation.
6. Record exact-head evidence.

## Acceptance

U1E is complete when:

1. `HEADER.end == CONTENT.begin`;
2. `CONTENT.end == PERFORMANCE_HUD.begin`;
3. `PERFORMANCE_HUD.end == FOOTER.begin`;
4. `FOOTER.end == SCREEN_H`;
5. `CONTENT == {0,16,240,93}`;
6. `PERFORMANCE_HUD == {0,109,240,10}`;
7. `LayoutManager::clearContent()` clears only CONTENT;
8. lineY(7) remains inside the body and existing line anchors are preserved;
9. no visual/product reorganization is introduced;
10. U1A-D and inherited P3-U1 remain GREEN.
