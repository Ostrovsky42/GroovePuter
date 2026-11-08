# 0.9.10 UI Constitution V1 — U1F Shell Pixel Ownership Evidence

Status: **CLOSED**

Checkpoint: `U1F — SINGLE SHELL OWNER FOR HEADER / FOOTER PIXELS`

## Scope

U1F removes the remaining duplicate global header/footer pixel ownership without redesigning page bodies or changing musical semantics.

Before U1F, pages called `drawStandardHeader()` / `drawStandardFooter()` as pixel renderers while `MiniAcidDisplay` later repainted global status chrome. The visible frame therefore depended on composition order between multiple owners of the same shell regions.

U1F keeps established page call-sites temporarily, but changes their authority:

- page header helper no longer paints global header pixels or re-reads live `MiniAcid` state;
- page footer helper publishes a bounded footer model;
- `MiniAcidDisplay` owns the stack-local shell frame for one composition;
- shell renders status chrome once from the already captured `frameStatus`;
- shell renders footer once from the copied footer model;
- performance HUD is composited after chrome/footer according to the existing frame order.

## RED

RED head:

`fe184d8f624c2eb4be59c01739a70f224564a6c8`

Workflow run:

`34050831399`

The U1F focused gate failed exactly because the required shell-frame owner did not yet exist:

`fatal error: ../src/ui/ui_shell_frame.h: No such file or directory`

U1A-U1E remained green at RED, so the new failure was isolated to the missing shell ownership layer.

## GREEN production delta

Production commit:

`6c3631e009a468f06d168c208d8ecd7964a004db`

The GREEN production delta contains exactly four files relative to the RED head:

- `src/ui/miniacid_display.cpp` — +6 / -4;
- `src/ui/ui_common.cpp` — +28 / -6;
- `src/ui/ui_common.h` — +17 / -4;
- `src/ui/ui_shell_frame.h` — new, 52 lines.

No page renderer, Pattern/Phrase mutation code, transport code, engine code, persistence schema, geometry constant or eviction policy changed in U1F.

## Shell-frame lifetime

`UiFooterModel` owns bounded copies:

- `char left[64]`;
- `char right[64]`;
- validity flag.

`UiShellFrameModel` is stack-local in `MiniAcidDisplay::update()` and is compile-time constrained with:

`static_assert(sizeof(UiShellFrameModel) <= 136)`.

The focused C++ test proves that page-local footer buffers are copied, survive mutation of their original storage, are NUL-terminated when truncated, and clear deterministically.

The implementation does not allocate heap memory for the frame model. A pointer-sized shell-frame binding is retained in `ui_common.cpp` so unchanged page helper call-sites can publish into the active stack model. U1F does not claim an exact target-ELF byte delta for that pointer without a Cardputer build measurement.

## Frame ownership order

The focused structural gate requires the canonical order:

1. `beginShellFrameModel(shellFrame)`;
2. active page body composition;
3. `endShellFrameModel()`;
4. `drawStatusChrome(gfx_, frameStatus)`;
5. `drawShellFooter(gfx_, shellFrame.footer)`;
6. performance HUD composition.

Pages therefore supply presentation data but no longer own the global header/footer pixels.

The invalid-page path follows the same shell footer owner rather than bypassing the architecture with direct `LayoutManager::drawHeader/Footer` calls.

## U1C / U1A contract convergence

The first exact-head verification exposed two older structural tests that required the compatibility call:

`drawLiveMixLockBadge(gfx_, frameStatus)`.

That symbol dependency was not the semantic invariant. U1F deliberately makes `drawStatusChrome(gfx_, frameStatus)` the canonical shell sink while the compatibility wrapper remains available outside the canonical frame path.

Two tests-only commits aligned the old anchors without weakening their semantic checks:

- `ed63ca0ee353709b27c15611a6d0cef9e7626318` — U1C frame-snapshot sink;
- `b36b20b1f682fc415a4ad6b3829c921e5cb7a43c` — U1A/status regression sink.

Compare `6c3631e0...` to `b36b20b1...` shows only these two test files changed; production code is identical.

The retained invariants still require:

- one captured `frameStatus` before page draw;
- chrome render from that exact snapshot after page draw;
- no second `MiniAcid` read inside status rendering;
- typed semantic location rather than title parsing;
- direct authoritative BPM capture;
- PAT/PHR and pattern-address source regressions remain protected.

## Final verification

Verification head:

`b36b20b1f682fc415a4ad6b3829c921e5cb7a43c`

Workflow run:

`34051440941`

Result:

- U1A semantic location — PASS;
- U1B source/transport truth — PASS;
- U1C coherent frame status — PASS;
- U1D renderer-residency continuity — PASS;
- U1E pixel-zone geometry — PASS;
- U1F shell pixel ownership — PASS;
- inherited P3-U1 Pattern/Phrase semantics — PASS.

## Visual/product preservation

U1F intentionally does not introduce a visual redesign. Before U1F the final global status chrome already repainted the header after page composition, so removing the earlier page-owned header paint removes redundant ownership rather than intentionally changing the final visible header.

Footer strings and page call-sites remain intact; they are copied into a frame model and painted once by the shell.

## Remaining debt

The compatibility helper names `drawStandardHeader` / `drawStandardFooter` now overstate their authority and can be migrated later, but renaming all page APIs is not required to prove U1F.

U1F also does not yet make footer/help text derive from one typed semantic command binding. It only establishes the shell as the sole pixel owner. Input-command/footer coherence remains a separate interaction-contract checkpoint.