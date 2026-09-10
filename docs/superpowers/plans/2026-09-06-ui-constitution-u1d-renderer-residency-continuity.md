# UI Constitution V1 — U1D Renderer Residency Continuity

Checkpoint: `U1D — renderer residency must not own musician continuity`

## Goal

Make high-level UI continuity independent of whether a page object happens to remain resident in DRAM.

This is an ownership correction, not a visual redesign and not a persistence-format change.

## Observed failure mode

`MiniAcidDisplay::getPage_()` may destroy page objects while navigating. On Cardputer the aggressive path below the existing free-DRAM threshold may keep only the requested page.

Several useful positions currently live only inside renderer objects:

- Synth A/B: `synth_tab_` defaults to NOTES on construction;
- PERFORM: tools visibility, selected tool context, and per-context selected rows default on construction;
- FEEL: focused row and local preset cursor default on construction;
- GENRE: focused row defaults on construction.

Therefore two identical navigation sequences can return to different UI positions depending only on renderer residency / memory pressure.

Musical values remain authoritative in `MiniAcid`, `Scene`, or `PerformanceKeyboard`; U1D must not copy those values into UI continuity state.

## Required invariant

**RENDERER RESIDENCY MAY CHANGE MEMORY USE, BUT MUST NOT CHANGE DEFINED MUSICIAN CONTINUITY.**

The shell owns a tiny fixed-size runtime continuity value. Pages project that state while resident and restore it when recreated.

## Scope

U1D preserves only high-level view/navigation continuity already preserved implicitly when a page object remains alive:

- Synth A and Synth B selected local representation: NOTES / KNOBS / MORE;
- PERFORM tools-layer visibility;
- PERFORM selected KEY / CHORD / ARP / RHYTHM context;
- PERFORM selected row per tool context;
- FEEL focused row;
- FEEL local preset cursor;
- GENRE focused row.

The continuity payload is runtime-only and fixed-size. It is owned by `MiniAcidDisplay`, not by global storage and not by `UiSessionState` persistence.

## Explicit non-goals

U1D does not:

- change the `<16384` Cardputer eviction threshold;
- change which pages are retained;
- change Cardputer persistence schema;
- persist local focus across reboot;
- copy musical values into the shell;
- change shortcuts, labels, layout, themes, or draw order;
- redesign Pattern/Phrase NOTES;
- hold the audio guard while drawing or restoring UI view state.

## Pattern editor boundary

`PatternEditPage` also owns cursor/focus/selection/note-entry state that can be lost on recreation. U1D records this as real residency-sensitive state but does **not** restore it yet.

Reason: those coordinates are material-sensitive. Restoring a stale selection after page/bank/pattern/material changes requires a separate validity/sanitization contract. Blind preservation would trade one bug for another.

A later narrow checkpoint may preserve Pattern editor continuity only after proving the restore target is still valid.

## Proposed runtime owner

Introduce one fixed-size `UI::UiViewContinuityState` value owned by `MiniAcidDisplay`.

Constraints:

- no heap allocation;
- target size `<= 16 bytes`;
- bounded integer fields only;
- no Pattern/Phrase event buffers;
- no scene, engine, MIDI, transport, or audio state;
- page constructors receive the owner explicitly from the shell;
- every page clamps/sanitizes restored values before use.

`UiSessionState` remains unchanged because renderer eviction and device reboot are different lifetime boundaries.

## TDD sequence

1. RED: prove the shell has no residency-independent view-continuity owner and current page defaults are renderer-local.
2. GREEN: add the tiny runtime owner and wire only Synth/PERFORM/FEEL/GENRE.
3. Verify U1A, U1B, U1C and inherited P3-U1 gates unchanged.
4. Record evidence on the exact verified SHA.

## Acceptance

U1D is complete when:

1. destroying and recreating one of the scoped pages no longer changes its high-level UI position;
2. musical state is still read from its existing authoritative owners;
3. `UiSessionState` persistence layout is unchanged;
4. the continuity owner is fixed-size and `<= 16 bytes`;
5. page residency/eviction policy is byte-for-byte unchanged unless a separate checkpoint authorizes it;
6. U1A/U1B/U1C and inherited P3-U1 preservation remain GREEN.
