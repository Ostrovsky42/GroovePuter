# 0.9.8 R5 — Mutation ownership closure

## Purpose

R5 is a closure audit, not a feature slice. It verifies that the 0.9.8 safe-editing work has one retained mutation owner for Pattern, Song and Phrase user edits, while generation and activation keep their already accepted boundaries.

## Canonical contract

Persistent owned edits follow:

`capture before -> detached PREPARE -> reject no-op / validate -> UndoOwner COMMIT -> bounded resident assignment -> exactly one Scene revision`

A failed or no-op edit must leave the previous receipt and revision state intact.

## Song legacy owner removal

R4 made the canonical Song wrapper the active owner, but the old page-local `UndoHistory` implementation still remained compiled inside `song_page.cpp` behind legacy routing. R5 removes that retained heap-backed history completely.

The following remain intentionally dynamic because they are clipboard storage, not Undo state:

- one-cell Song clipboard;
- rectangular Song area clipboard (`std::vector<int>`);
- whole-Song clipboard.

`GROOVEPUTER_APP_EVENT_UNDO` has one Song implementation after R5: the canonical `UndoOwner<1536>` route.

## Ownership inventory

- Pattern manual persistent edits -> canonical UndoOwner.
- Song manual persistent edits -> canonical UndoOwner.
- Phrase capture / derive / clear -> canonical Phrase receipt.
- Phrase -> Song write/insert -> canonical Song receipt.
- Generation commits -> accepted specialized generation owner; unchanged by R5.
- Navigation / transport / selection / preview -> runtime-only, no fake receipt or revision.
- Save/load -> persistence baseline operations, not user mutation receipts.
- Queued reverse activation -> remains outside 0.9.8 ownership consolidation and is reserved for the 0.9.9 activation boundary.

## Acceptance

R5 reruns the complete R4 suite and adds a permanent source-regression guard that rejects:

- a second Song `UndoHistory` / `UndoCell` / `UndoActionType` owner;
- `g_undo_history` returning to Song;
- a legacy Song app-event Undo implementation;
- ownership leaking back into PhraseWorkspace PREPARE helpers;
- Pattern/Song canonical commit helpers publishing revision directly instead of through UndoOwner.

R5 adds no feature, scheduler, Redo, multi-level history, persistence of Undo across reboot, MIDI recording, or activation semantics.
