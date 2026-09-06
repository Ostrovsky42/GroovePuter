# 0.9.10 UI Constitution V1 — U1E Pixel-Zone Geometry Evidence

Status: **CLOSED**

Checkpoint: `U1E — PIXEL-ZONE GEOMETRY OWNERSHIP`

## Scope

U1E corrected ownership of the 240x135 display geometry only. It did not redesign preserved pages, change musical state, change transport/source semantics, or introduce a new renderer.

The defect was that `Layout::CONTENT` extended through the same bottom 10 pixels later painted by the shell performance HUD. Page-owned content and shell-owned HUD therefore had overlapping geometry even when the final composited frame looked acceptable.

## RED

RED head:

`33b8edaae508c99605a60c988cf46d3e871b72c7`

Workflow run:

`34050380030`

The focused U1E gate failed at compile time on the old geometry:

- `Layout::CONTENT.h == 103`, expected `93`;
- `rectEnd(Layout::CONTENT) == 119`, while `Layout::PERFORMANCE_HUD.y == 109`.

This localized the failure to the previously overlapping ownership contract rather than a page-specific rendering symptom.

## GREEN

Production commit:

`6942c0ebf8cffffb58ca439e9c06042e41582e17`

The canonical fixed partition is now:

| Region | Y inclusive | Height | Owner |
|---|---:|---:|---|
| Global chrome | 0..15 | 16 | shell |
| Surface body (`CONTENT`) | 16..108 | 93 | active surface |
| Performance HUD | 109..118 | 10 | shell |
| Command footer | 119..134 | 16 | shell |

`CONTENT` and `PERFORMANCE_HUD` are now adjacent rather than overlapping.

The production change was intentionally limited to `src/ui/screen_geometry.h`. Existing consumers such as `LayoutManager::clearContent`, SMF browser/redraw and Project body-relative layout automatically consume the corrected canonical rectangle.

## Preservation decisions

U1E did **not** mechanically reduce `MAX_LINES` from 8 to 7 and did not move the existing `lineY(7)` anchor. The body has seven complete 12-pixel rows plus the already-used compact bottom status/hint baseline. This avoids converting an ownership correction into an incidental visual redesign.

The focused gate also protects the existing PERFORM composition anchors so the geometry correction cannot silently rearrange that preservation-target surface.

## Verification

Exact production-head workflow run:

`34050427270`

On `6942c0ebf8cffffb58ca439e9c06042e41582e17`:

- U1A semantic location — PASS;
- U1B source/transport truth — PASS;
- U1C coherent frame status — PASS;
- U1D renderer-residency continuity — PASS;
- U1E pixel-zone geometry — PASS;
- inherited P3-U1 Pattern/Phrase preservation — PASS.

## Resource impact

The production delta changes compile-time geometry constants only.

- no retained heap was added;
- no new runtime buffer was added;
- no new task or lock was added;
- no audio/SD/MIDI path was changed;
- no new display operation was introduced by U1E itself.

This checkpoint does not claim new Cardputer runtime measurements; it removes an ownership ambiguity without adding runtime state.

## Remaining boundary

U1E establishes non-overlapping canonical zones. It does not by itself prove that every arbitrary surface draw call is physically clipped to its body capability. That stronger bounded-rendering capability remains a separate later contract.