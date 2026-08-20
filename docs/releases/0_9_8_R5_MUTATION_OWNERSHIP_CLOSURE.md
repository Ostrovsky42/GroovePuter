# 0.9.8 R5 — Mutation ownership closure

## Purpose

R5 is a closure audit, not a new editing feature. It verifies that the destructive
persistent editing domains completed in R2–R4 have one retained Undo owner and
that legacy page-local history cannot silently re-enter the active architecture.

## Closed ownership map

| Domain | Persistent edit owner | R5 result |
| --- | --- | --- |
| Pattern manual edits | `UndoOwner<1536>` / `UndoKind::Pattern` | closed |
| Song arrangement edits | `UndoOwner<1536>` / `UndoKind::Song` | closed |
| Phrase bank edits | `UndoOwner<1536>` / `UndoKind::Phrase` | closed |
| Phrase -> Song manual write | `UndoOwner<1536>` / `UndoKind::Song` | closed |
| Generation/materialization | specialized generation owner / 0.9.9 boundary | intentionally not migrated |
| Transport/navigation | runtime state | no Undo receipt, no fake revision |
| Save/load | persistence baseline | no fake user mutation |

## Legacy Song history removal

R4 made the page-local `UndoHistory` unreachable from the public Song mutation
path, but the old object still compiled into the page and retained a
`std::vector<UndoCell>`. R5 removes that second owner vocabulary entirely:

- no `UndoActionType`;
- no `UndoCell`;
- no `UndoHistory`;
- no `g_undo_history`;
- no legacy `GROOVEPUTER_APP_EVENT_UNDO` restore branch;
- no transient `old_patterns` before-images whose only consumer was that history.

Dynamic Song area clipboard storage remains. Clipboard data is transfer state,
not retained history, and is deliberately separate from `UndoOwner`.

## Direct revision marks outside the R5 destructive-edit domains

The repository still has domain-local persistent parameter controls (for example
FEEL/GENRE/Sampler/Drum/Tape) that advance the Scene revision directly. R5 does
not silently widen the 0.9.8 destructive Undo product contract to every scalar
parameter in the application. The closure criterion here is narrower and
explicit: Pattern, Song and Phrase manual/destructive edit paths have canonical
before-state receipts; runtime navigation does not dirty; generation activation
stays in 0.9.9.

Any future decision to make scalar parameter edits undoable must add a bounded
receipt/domain contract explicitly rather than introducing another page-local
history mechanism.

## Invariants

- exactly one retained Undo history owner: `UndoOwner<1536>`;
- successful canonical mutation replaces the previous receipt;
- failed/no-op preparation leaves the previous receipt intact;
- Save does not expire Undo;
- navigation does not expire Undo;
- Pattern/Song/Phrase page helpers do not call `markSceneMutated()` themselves;
- generation/pending activation is not pulled into R5.

## Acceptance

Focused R5 reruns all R4 acceptance (which itself reruns R2/R3) and adds source
closure checks for the removed Song owner and current ownership boundaries.
Normal exact-head host, SDL, ADV normal/fixed-DRAM and SEQTRAK MIDI-only gates
remain mandatory before merge.
