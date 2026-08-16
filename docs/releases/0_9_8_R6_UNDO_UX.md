# 0.9.8 R6 — Undo UX / shortcut consistency

## Scope

R6 exposes the accepted one-level Undo architecture through one user gesture and
one vocabulary. It adds no receipt type, history depth, restore dispatcher or
mutation owner.

## Shortcut decision

`Ctrl+Z` is intentionally **not** global Undo. SYNTH KNOBS uses `Ctrl+Z/X/C/V`
for TB303 parameter resets, including Cutoff reset on `Ctrl+Z`. Stealing that
chord would regress an established instrument control.

R6 therefore uses **`Ctrl+U`** (`U` = Undo). No current literal Ctrl+U binding was
found in the production pages. The Phrase help abbreviation `Ctrl+U/D` means
Ctrl+Up/Down arrows, not the U/D letter keys.

The shortcut is translated to the existing `GROOVEPUTER_APP_EVENT_UNDO` before
the active page receives it, so Cardputer and SDL share the same application
routing.

## Page ownership remains authoritative

The active page remains the restore owner:

- Pattern page -> `UndoKind::Pattern`;
- Song page -> `UndoKind::Song`;
- Phrase page -> `UndoKind::Phrase`, and `UndoKind::Song` for its manual
  Phrase-to-Song write path.

`MiniAcidDisplay` never decodes a receipt and never restores Scene data. If the
active page declines the event, R6 only reports:

- `UNDO: EMPTY` when there is no retained receipt;
- `UNDO: RETURN PAGE` when a receipt still exists for another owner/page.

That retained receipt is left untouched. R6 does not load a paged Scene from SD
or create cross-page restore ownership.

## User messages

Successful domain restores use:

- `UNDO: PATTERN`
- `UNDO: SONG`
- `UNDO: PHRASE`

Unavailable targets report `UNDO: RETURN PAGE`. Pattern expired receipts report
`UNDO: EXPIRED`.

`UndoKind::Generation` remains reserved in the bounded slot, but the merged
0.9.8 line has no current `UndoKind::Generation` publisher. R6 therefore does not
invent a fake generation restore path; generation/activation ownership stays on
its existing 0.9.9 track.

## Help

Both on-device `Alt+H` global help and `src/ui/docs/keys.md` document `Ctrl+U`.

## Acceptance

Focused R6 reruns R2-R5 and checks shortcut compatibility, central no-restore
routing, domain messages, and the preserved Synth Sound Ctrl+Z reset. Normal
exact-head host/SDL/ADV/fixed-DRAM/SEQTRAK gates remain mandatory before merge.
