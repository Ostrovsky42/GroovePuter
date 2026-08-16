# GroovePuter 0.9.8 — Undo / Safe Editing Research

## Purpose

Define the smallest safe 0.9.8 ownership model for user-facing Undo without creating a second Scene framework, a desktop-style history stack, or another page-local rollback mechanism.

The release goal is deliberately narrow:

```text
persistent edit
    ↓
record one bounded before-state receipt
    ↓
commit mutation
    ↓
UNDO
    ↓
restore that logical mutation atomically
```

0.9.8 is not a general persistence rewrite. It is a safety layer for destructive musical editing on Cardputer ADV.

## Baseline

Research starts from the clean 0.9.7 release candidate:

```text
release/0.9.7-final
3341df09900098b649a4300696c0883fc0c14d61
```

This is a candidate baseline, not yet an immutable 0.9.7 FINAL merge SHA. Production 0.9.8 must be rebased/delta-audited against the exact accepted 0.9.7 merge commit before integration.

0.9.5 Sampler Reliability remains postponed and is not a dependency for this research.

## Executive finding

Undo is **not absent** in the current repository. It is fragmented.

At least two independent rollback models already exist:

1. Song page one-shot Undo for Cut/Paste/Delete;
2. Song generation provisional rollback for double-tap generation.

Phrase and Song generation additionally already expose transaction-like commit boundaries.

Therefore the correct 0.9.8 task is not “add Ctrl+Z”. It is:

> establish one bounded retained user-Undo owner and migrate selected destructive mutations to it without changing musical behavior.

A third unrelated rollback mechanism is explicitly forbidden.

---

## Archaeology

### A. Existing Song Undo — recovery candidate, not final architecture

`src/ui/pages/song_page.cpp` currently owns namespace-local state:

```text
UndoActionType
UndoCell
UndoHistory
std::vector<UndoCell>
g_undo_history
```

It records Song references for:

- Cut;
- Paste;
- Delete / clear.

`GROOVEPUTER_APP_EVENT_UNDO` restores the saved Song cells and clears history.

Useful behavior already exists, but this is not a suitable global owner because:

- it is private to Song UI;
- it uses `std::vector`;
- it captures only Song pattern references;
- Pattern/Phrase/generation cannot reuse it cleanly;
- its lifetime is tied to page implementation rather than project mutation ownership.

Verdict:

```text
RECOVERY / MIGRATION CANDIDATE
```

Do not simply extend this structure to every page.

### B. Pending Song generation rollback — transaction abort, not user history

`SongPage::PendingCellGeneration` stores a bounded rollback receipt containing:

- Song slot / row / track;
- old and generated references;
- old Song length;
- `SceneRevisionState revisionBefore`.

This is used when a first single-cell `G` is provisionally committed and a fast second `G` converts the gesture into whole-row generation. The first mutation is rolled back before the row transaction is prepared.

This is an important precedent, but semantically it is an **internal transaction abort** rather than retained user Undo.

0.9.8 should share bounded receipt primitives where useful, while keeping these meanings distinct:

```text
transaction abort  = operation did not become final user history
user Undo          = revert the last successfully committed user mutation
```

### C. SongPatternMaterializer already has prepare → commit

`src/dsp/song_pattern_materializer.h` already prepares all requested musical material before mutating Scene state.

Its useful properties are:

- preparation is read-only;
- preparation is allocation-free;
- destination slots are resolved before commit;
- generator failure needs no rollback write;
- all requested tracks commit together;
- Scene revision advances once after successful commit;
- Song-generated pattern ownership uses an existing bit rather than adding Scene state.

This is exactly the kind of mutation boundary 0.9.8 should preserve.

Do not wrap materialization in a second transaction framework.

### D. PhraseWorkspace already has command boundaries

`src/phrase/phrase_workspace.h` owns bounded commands for:

```text
capture
derive
clear
writeToSong / insertIntoSong
```

Each command validates first, executes its synchronous commit under the existing `AudioGuard`, and advances Scene revision once only after successful mutation.

This is a strong future integration seam for Undo receipts.

### E. Pattern editor has mutations but no common Undo owner

Pattern editing currently has many persistent writes routed through `withAudioGuard()`, which also calls `markSceneMutated()`.

Examples include:

- step note edits;
- clear/rest;
- accent/slide/FX edits;
- paste;
- pattern generation;
- selection edits.

The Pattern clipboard itself currently uses dynamic `std::vector` storage. 0.9.8 must not copy that design into the retained Undo owner.

### F. Global Undo event already exists

`GROOVEPUTER_APP_EVENT_UNDO` already exists in `src/ui/ui_core.h`.

Therefore 0.9.8 does not need a new event framework.

However a global `Ctrl+Z` binding cannot be assumed: Synth Sound pages already use `Ctrl+Z/X/C/V` for reset-parameter shortcuts. UI binding is therefore a later compatibility decision, not an R1 architecture premise.

### G. Scene revision already has the required dirty-state primitive

`src/state/scene_revision.h` owns an 8-byte runtime state:

```text
currentRevision
persistedRevision
```

It already supports:

```text
sceneRevisionSnapshot()
restoreSceneRevision()
markSceneMutated()
```

User Undo must preserve dirty semantics correctly.

Two cases must be distinguished:

```text
saved state → edit → undo
```

should return to clean if the data exactly returns to the saved state.

But:

```text
already dirty state → edit → undo
```

must return to the previous dirty state, not falsely become clean.

The retained Undo receipt therefore needs enough revision metadata to restore the pre-mutation revision state or an equivalent proven semantic.

### H. Full Scene snapshot is not the default design

`Scene` contains pattern banks, sampler state, Tape state, PhraseBank, two Songs, genre/feel state, custom phrases and more.

The repository also already owns a global static full-Scene transaction scratch object used by Scene loading/validation:

```text
sceneTransactionScratch()
```

That object must remain transaction scratch. It is not safe retained Undo history because another load/validation operation may legitimately overwrite it.

0.9.8 must not:

- allocate another permanent full `Scene` by default;
- retain Undo in `sceneTransactionScratch()`;
- add an unmeasured large DRAM reservation.

Exact `sizeof(Scene)` and candidate receipt sizes must be measured before approving any snapshot strategy.

---

## Proposed ownership contract

### One authoritative retained user Undo owner

0.9.8 should introduce one small owner outside individual pages.

Conceptually:

```text
UndoOwner

hasUndo()
clear()
record(...bounded before-state...)
undo(Scene&, AudioGuard/commit boundary)
```

This is a conceptual contract only. R1 must not freeze C++ names before the mutation inventory is complete.

### One level first

Initial product contract:

```text
one committed user mutation
→ one Undo
→ history becomes empty
```

A new successful undoable mutation replaces the previous retained receipt.

No redo in 0.9.8 unless implementation evidence shows it is essentially free and does not expand ownership complexity.

### Bounded receipts, not generic heap snapshots

Preferred receipt categories are small domain-specific before-state deltas, for example:

```text
Pattern step / selected steps
Pattern whole-pattern before image
Song cell / rectangular Song reference region
Phrase slot before image
Phrase-to-Song insertion affected rows
Generation affected Pattern slots + Song references
```

Every receipt type must have a compile-time upper bound.

No `std::vector`, `std::deque`, heap allocation, filesystem I/O or JSON work in the retained owner.

### Successful mutation replaces history; failure/no-op does not

Required rule:

```text
prepare receipt
attempt operation

failure/no-op
    → existing Undo remains untouched

successful mutation
    → publish new receipt atomically
```

This avoids losing a valid Undo merely because a later command failed admission, generation or validation.

### Undo itself is a persistent mutation operation

User Undo runs on the control/UI side, under the existing safe mutation boundary. It is not an audio callback operation.

It must restore all pieces of one logical mutation together and then consume the receipt.

### Runtime state is not automatically Undo state

0.9.8 does not mean every changing value is undoable.

Do not automatically include:

- transport play/stop;
- live notes;
- current cursor/focus;
- active playback position;
- transient audition state;
- device connection state;
- pending next-boot Device Profile selection;
- MIDI active-note tables;
- scheduler queues.

0.9.6 Output Ownership and 0.9.7 Device Profiles must remain orthogonal to the Undo owner.

---

## Initial 0.9.8 scope

The release should prioritize destructive musical edits where accidental loss is costly.

### Tier 1 — required

- Pattern clear / destructive step edit group;
- Pattern paste;
- Pattern Generate when the mutation has actually committed;
- Song Cut/Paste/Delete migrated from local Undo;
- Phrase clear;
- Phrase INSERT/REPLACE into Song;
- Song generation/materialization where bounded restoration can be proven.

### Tier 2 — include only if the same owner naturally supports it

- Phrase capture/derive replacing an existing slot;
- Song row insertion/deletion;
- selected-area pattern operations.

### Explicitly out of scope

- 0.9.5 LOAD KIT / sampler transactional kit load;
- full project Load undo;
- redo stack;
- unlimited history;
- persisted Undo across reboot;
- undo of transport/playback state;
- undo of MIDI connection/profile runtime state;
- Tape/Recorder/Voice recovery;
- generation-model redesign;
- Song/Phrase live-arrangement redesign;
- new top-level UI page.

---

## Quantized generation boundary

Pattern generation can return states such as immediate commit or pending next-bar publication.

Undo history must describe **committed musical state**, not an attempt that has not yet published.

Therefore:

```text
Generate requested while playing
    ↓
PendingNextBar
    ↓
NO retained Undo receipt yet
    ↓
actual commit at boundary
    ↓
publish Undo receipt for the state that was replaced
```

If current generation plumbing cannot expose a bounded pre-commit receipt at the actual publication boundary without complicating realtime ownership, defer that specific generation Undo to a later 0.9.8 checkpoint rather than snapshotting the whole Scene.

---

## Memory / realtime constraints

Target platform: M5Stack Cardputer ADV / ESP32-S3, normal no-PSRAM product profile.

0.9.8 must measure, on exact accepted baseline and candidate heads:

```text
sizeof(Scene)
sizeof(each Undo receipt)
sizeof(Undo owner)
fixed DRAM delta
free internal heap
largest internal block
```

Required architecture properties:

- fixed-size retained storage;
- no per-Undo heap allocation;
- no filesystem/JSON in Undo execution;
- no new mutex in audio callback;
- no per-sample Undo checks;
- no full-screen redraw requirement;
- existing `AudioGuard` remains the mutation synchronization primitive unless evidence proves it insufficient.

Recovered/free memory is not a feature budget.

---

## Proposed implementation sequence

### 0.9.8-R1 — Mutation ownership + bounded receipt contract

Research plus executable host contracts only.

Deliverables:

- inventory of current destructive mutations;
- classify each as runtime-only / persistent non-undo / undo candidate;
- prove existing Song local Undo and generation rollback ownership;
- define fixed upper bounds;
- define dirty/revision semantics;
- prohibit heap/full-Scene retained history;
- no user-visible behavior change.

### 0.9.8-R2 — Central owner + one simple Pattern vertical slice

Introduce the smallest fixed-size owner and migrate one easy destructive Pattern operation.

Acceptance must prove:

```text
before
→ edit
→ undo
→ byte-equivalent affected musical state restored
```

No Song/Phrase migration yet.

### 0.9.8-R3 — Pattern safe editing

Add bounded coverage for the chosen Pattern operations:

- clear;
- paste;
- selected edits where practical;
- immediate generation commit.

Quantized pending generation remains gated by the commit-boundary requirement above.

### 0.9.8-R4 — Song / Phrase mutation integration

Migrate the existing Song one-shot Undo behavior into the central owner and cover Phrase operations that can mutate Song rows atomically.

Delete the old page-local retained owner only after equivalent tests are green.

### 0.9.8-R5 — Generation/materialization consolidation

Unify bounded restoration for Song materialization and the existing provisional generation rollback where doing so reduces duplicate ownership without changing transaction semantics.

Do not force transaction-abort and user-Undo into one lifecycle if that makes either less correct.

### 0.9.8-R6 — UI / help / compatibility

Expose Undo through the existing application event and choose a context-safe Cardputer binding after auditing current shortcuts.

`Ctrl+Z` is not assumed because Synth Sound already owns it.

UI should provide short feedback such as:

```text
UNDO: PATTERN
UNDO: SONG PASTE
UNDO: PHRASE INSERT
NOTHING TO UNDO
```

No new top-level page.

### 0.9.8-R7 — ADV acceptance

Freeze one exact SHA and run full software + hardware acceptance.

---

## Host acceptance contract

Before 0.9.8 can be considered complete, executable tests should prove at least:

- one authoritative retained Undo owner;
- no `std::vector` / `std::deque` / heap-backed retained history;
- receipt storage has compile-time upper bounds;
- successful mutation publishes one receipt;
- failed/no-op mutation preserves the previous valid receipt;
- second successful mutation replaces the first receipt;
- Undo is one-shot and clears itself after success;
- clean → edit → Undo restores clean state;
- dirty → edit → Undo restores prior dirty state;
- Pattern restoration is exact for the covered mutation;
- Song rectangular mutation restores all affected cells atomically;
- Phrase INSERT/REPLACE restores Song length and affected references atomically;
- generation Undo restores both allocated/overwritten musical material and all references it changed;
- no unrelated Pattern/Song/Phrase data changes;
- Output Ownership remains unchanged;
- Device Profile / MIDI settings remain unchanged;
- runtime transport/live-note state is not captured as persistent Undo history;
- existing Scene load/save contracts remain unchanged.

---

## Hardware assumptions

Hardware:

- M5Stack Cardputer ADV / ESP32-S3;
- normal no-PSRAM firmware profile;
- built-in display/audio path unchanged;
- Yamaha SEQTRAK optional for regression validation of MIDI/output independence.

No new wiring is introduced.

Cardputer ADV PORT.A remains unchanged for unrelated peripherals:

```text
SDA GPIO2
SCL GPIO1
```

---

## Hardware acceptance checklist

On one exact accepted 0.9.8 candidate SHA:

- [ ] cold boot reaches normal UI with no WDT/reset;
- [ ] edit Pattern → Undo restores audible and visible result;
- [ ] clear Pattern/selection → Undo restores it;
- [ ] paste Pattern material → Undo restores replaced material;
- [ ] Song Cut/Paste/Delete → Undo restores exact references;
- [ ] Phrase INSERT/REPLACE → Undo restores Song rows and length;
- [ ] Generate → Undo restores the previous musical material for supported commit modes;
- [ ] second destructive edit replaces first Undo history exactly once;
- [ ] repeated Undo after consumption reports no available Undo and changes nothing;
- [ ] Save → edit → Undo returns dirty indicator to clean when data matches saved state;
- [ ] dirty project → edit → Undo remains dirty;
- [ ] playback continues correctly after supported edits/Undo;
- [ ] no stuck notes after edit/Undo while using MIDI/LAYER output;
- [ ] Output Ownership values survive all Undo tests unchanged unless the user directly edits ownership outside this scope;
- [ ] selected Device Profile survives all Undo tests unchanged;
- [ ] fixed DRAM gate passes;
- [ ] free internal heap and largest internal block show no progressive loss over repeated edit/Undo cycles;
- [ ] 30-minute edit/generate/Undo soak has no WDT, reset, corruption or progressive fragmentation.

---

## Release decision

Current verdict:

```text
GO — 0.9.8 Undo / Safe Editing is a valid next production track,
provided implementation begins with ownership consolidation rather than UI.
```

The highest-risk mistake would be to bolt a global shortcut onto the existing Song `UndoHistory`. The repository already contains the right transaction boundaries to do better with a small bounded owner.

## First production PR after research

The preferred first production slice is:

```text
0.9.8-R1
bounded mutation/Undo contract + executable tests
```

It should introduce no user-visible feature and no broad refactor. Its purpose is to freeze the owner, memory bounds, dirty semantics and mutation lifecycle before Pattern/Song/Phrase code starts migrating.
