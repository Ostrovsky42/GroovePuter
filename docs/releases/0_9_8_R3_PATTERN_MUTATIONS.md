# GroovePuter 0.9.8-R3 — Pattern Persistent Mutation Migration

## Exact base

R3 starts from the accepted R2 merge on `dev_0.9.8`:

```text
9151b5bd14fadfcbbfef7cea0ea5175154b2a371
```

Branch:

```text
agent/20260816-0.9.8-r3-pattern-mutations
```

R2 already established:

- one authoritative `UndoOwner`;
- one bounded `BoundedUndoSlot<1536>` retained receipt;
- `SynthPatternUndoPayload` with stable page/synth/bank/pattern identity;
- exact Scene revision restoration;
- Alt+Backspace Reset Pattern as the first production vertical slice.

R3 expands that same ownership boundary across manual persistent Synth Pattern
editing. It does not introduce a second history system.

## Scope

R3 migrates the existing manual Pattern edit surface to:

```text
PREPARE
  capture stable Pattern address + complete before image
  build complete after image off to the side
  perform all clamping / selection expansion / no-op checks

COMMIT
  publish exactly one Pattern before-state receipt
  perform exactly one resident Pattern assignment under AudioGuard
  advance Scene revision exactly once through UndoOwner
```

Covered manual operations include:

- direct NOTE ENTRY writes and held-note continuation;
- NOTE ENTRY Backspace;
- single-step Backspace / REST;
- rectangular selection clear;
- note +/- edits;
- octave +/- edits;
- accent toggle / rectangular accent set;
- slide toggle / rectangular slide set;
- step FX cycle;
- step FX parameter adjustment;
- Pattern rotation;
- full Pattern paste;
- rectangular/row paste.

A multi-step selection is one logical mutation and therefore publishes one
receipt/revision, not one receipt per changed step.

## Pure prepared Pattern layer

R3 adds:

```text
src/state/synth_pattern_edit.h
```

The helpers operate only on a caller-owned `SynthPattern` value. They do not
know about:

- `MiniAcid`;
- `SceneManager`;
- `UndoOwner`;
- AudioGuard;
- filesystem / JSON;
- transport / scheduler;
- generation.

This makes PREPARE semantics executable in host tests and prevents a UI handler
from needing to mutate the live Pattern merely to discover the result.

The helper intentionally preserves existing `MiniAcid` edit semantics. In
particular, `clear303Step` historically clears note/articulation/probability/FX
but preserves step `velocity` and `timing`; R3 keeps that behavior rather than
silently redefining REST.

## Full no-op admission

R3 compares the complete prepared Pattern against the captured before image
before `UndoOwner::commitPrepared()`.

Therefore operations such as:

```text
note +1 while already clamped at max note
FX param +1 while already 255
FX param -1 while already 0
clear an already-clear step
paste identical data
rotate a structurally symmetric Pattern
```

must not:

- replace the previous Undo receipt;
- advance Scene revision;
- enter the audio critical section for a persistent write.

## Legacy handler strategy

`pattern_edit_page_legacy.h` remains the retained implementation for the large
existing UI surface. R3 compiles it under:

```text
handleEventLegacyUnowned
```

and places a narrow `handleEventLegacy` ownership wrapper in front of it.

The wrapper intercepts manual persistent Pattern writes and delegates:

- navigation;
- runtime selectors;
- Copy;
- the accepted R2 Reset Pattern path;
- the accepted R2 application Undo path;
- unrelated UI behavior.

This avoids a broad rewrite of the established Pattern Editor while making the
persistent mutation boundary explicit and testable.

## Runtime selector correction

R3 separates audio exclusion from persistence ownership.

Before R3 the Pattern page helper named `withAudioGuard()` also called
`markSceneMutated()` unconditionally. The same helper is used by Pattern/bank
selection even though `SceneManager::setCurrentSynthPatternIndex()` and
`setCurrentBankIndex()` update runtime selector fields rather than persisted
`Scene` data.

That coupling could expire a valid Undo receipt merely by navigating to another
Pattern/bank.

In R3:

```text
withAudioGuard() = audio exclusion only
```

Persistent Pattern edits use `commitPatternMutation()` / `UndoOwner`.
Runtime selectors do not dirty the Scene.

Song chaining remains a separate persistent domain. Q-I selection advances the
Scene revision only when chaining actually writes a Song reference. R3 does not
publish a Pattern receipt for that Song mutation.

## Generation boundary / 0.9.9 compatibility

Generation is intentionally excluded from R3 Pattern receipt ownership.

Plain `G` uses the existing quantized generation contract and can return:

```text
CommittedNow
PendingNextBar
AttemptUnavailable
Failed
```

A `PendingNextBar` result is not a completed persistent mutation. Folding that
path into the manual Pattern helper would make 0.9.8 own scheduling/activation
semantics that belong to the 0.9.9 PREPARE / COMMIT / ACTIVATE track.

R3 therefore preserves:

```text
manual Pattern edit
    -> R3 PREPARE + UndoOwner COMMIT

quantized generation
    -> existing generation boundary
    -> Scene revision only when an immediate persistent commit occurs
    -> no R3 Pattern receipt claim for pending activation
```

The retained legacy generation shortcut also stays outside Pattern receipt
ownership. Because it mutates persistent Pattern data synchronously, it still
advances Scene revision so an older manual Pattern Undo expires instead of
restoring across an unrelated generation mutation.

## Explicit non-goals

R3 does **not** migrate or add:

- Song Undo;
- Phrase Undo;
- generation/materialization Undo;
- pending musical-boundary commands;
- scheduler ownership;
- 0.9.9 ACTIVATE ownership;
- Redo;
- multi-level history;
- global Ctrl+Z routing;
- filesystem/page loading during Undo;
- sampler changes.

## Focused acceptance

Run:

```bash
bash tests/run_undo_0_9_8_r3_tests.sh
```

The runner first reruns the complete R2 authoritative owner/measurement contract,
then executes:

```text
test_pattern_mutations_0_9_8_r3_source_regressions.py
test_pattern_mutations_0_9_8_r3.cpp
```

The C++ contract verifies:

- full Pattern equality/no-op detection;
- exact `-2 / -1 / playable note` transition semantics;
- min/max note clamping;
- octave adjustment;
- accent/slide mutation;
- FX cycle;
- FX parameter saturation no-ops;
- rotation direction;
- legacy clear semantics including retained velocity/timing.

The source contract verifies:

- capture -> detached after -> no-op -> owner COMMIT ordering;
- one bounded resident assignment under AudioGuard;
- no page-owned `markSceneMutated()` in manual Pattern COMMIT;
- manual edit interception ahead of the retained legacy handler;
- Paste/Ctrl+V cannot bypass the owner;
- R2 Reset/Undo remains intact;
- NOTE ENTRY cannot bypass the owner;
- quantized generation remains outside R3 ownership;
- runtime selectors no longer own Scene revision;
- Song chaining revision remains separate.

## Repository acceptance

Focused R3 success is necessary but not sufficient for merge.

The exact R3 head must also pass the normal repository matrix, including:

- Core host regressions;
- SDL build;
- Cardputer ADV build;
- fixed DRAM budget;
- SEQTRAK MIDI-only build;
- all triggered release-specific workflows.

R3 adds no new retained history buffer beyond the R2 `UndoOwner`; the prepared
`SynthPattern after` is stack/local transaction state rather than fixed global
DRAM. The normal fixed-DRAM gate remains authoritative for the production build.
