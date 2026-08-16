# 0.9.9 Parallel Work Contracts

## Purpose

This document freezes the boundary that allows 0.9.8 Undo / Safe Editing and
0.9.9 Generation Continuity / Live Arrangement to proceed in parallel without
creating competing mutation, rollback, transport, or persistence owners.

It is a contract document only. It does not define a mandatory C++ API and does
not authorize 0.9.9 production integration before 0.9.8 freezes its mutation
contract.

Research baseline for this contract:

```text
branch: research/0.9.9-generation-arrangement
base:   dev_0.9.6
SHA:    0b284d1fdb6596957ffa998b0d5effcc27d761ca
```

At the time this contract was written:

- 0.9.6 Output Ownership is merged;
- 0.9.7 Device Profiles has a clean release integration candidate but is not yet
  the merged release baseline;
- `research/0.9.8-undo-safe-editing` has no distinct 0.9.8 implementation;
- 0.9.9 remains research-only.

The production baseline must be re-resolved when 0.9.8 freezes.

---

## 1. Shared product invariant

The authoritative user-visible music is the realized persistent material:

```text
Pattern -> Phrase references -> Song structure
```

Generator recipes, seeds, provenance, pending launch requests, UI selection, and
transport cursors are not a replacement for realized musical state.

Therefore:

```text
Generate
-> manual edit
-> Save
-> firmware/generator update
-> Load
```

must restore the saved realized material, not silently regenerate it from the
current generator implementation.

No 0.9.8 or 0.9.9 work may introduce regeneration-on-load semantics.

---

## 2. Hard ownership split

### 2.1 0.9.8 owns persistent mutation and Undo

0.9.8 is the canonical owner of:

- what constitutes one persistent editing mutation;
- how pre-mutation state needed for Undo is captured;
- how a successful mutation enters Undo history;
- how Undo restores committed persistent state;
- how Scene revision/dirty state changes for a mutation;
- how failed mutations leave persistent state unchanged;
- how multi-object persistent edits are treated atomically when required;
- how existing local rollback/Undo mechanisms are consolidated or retained.

0.9.9 MUST NOT create a second persistent-mutation framework.

### 2.2 0.9.9 owns musical activation and compatibility/liveness

0.9.9 is the canonical owner of:

- when already prepared musical material becomes active in playback;
- musical boundary policy for Pattern/Phrase/Song activation;
- bounded pending activation state;
- replacement/cancellation semantics for pending musical activation;
- Pattern/Phrase/Song reference liveness rules relevant to safe reuse;
- compatibility of realized generated material across firmware changes;
- generation provenance/version metadata only if research proves it necessary;
- preservation of Output Ownership and Device Profile orthogonality.

0.9.9 MUST NOT make UI state the authoritative owner of a pending musical
transition.

### 2.3 Transport owns time, not persistence

Transport/scheduler code may determine that a musical boundary has arrived.
It must not become a general persistence or Undo owner.

A boundary callback may request application of a previously prepared change,
but it must not perform heavyweight generation, filesystem work, serialization,
or unbounded allocation.

### 2.4 Persistence owns committed state only

Project persistence stores committed persistent musical state.

By default, transient pending activation state is runtime-only and must not be
serialized unless a later explicit product requirement proves otherwise.

This means a Save before a pending activation boundary stores the last committed
persistent state, not an implicit future transport command.

---

## 3. Required 0.9.8 capability contract for 0.9.9

0.9.9 does not require specific class or function names. It requires the frozen
0.9.8 implementation to provide equivalent semantics.

### U8-01 — One committed user edit can be represented as one mutation

A logical edit that changes multiple persistent objects must be representable as
one Undo-visible mutation where partial visibility would be invalid.

Relevant 0.9.9 examples include:

```text
Pattern content + Song pattern reference
Pattern content + Phrase reference metadata
Song row structure + affected references
Phrase write/insert + Song structure
```

0.9.8 may use snapshots, deltas, callbacks, receipts, or another bounded design.
0.9.9 must not prescribe the implementation.

### U8-02 — Failure before commit is invisible

If validation/preparation fails before persistent commit:

- no persistent Scene content changes;
- no Undo entry is created;
- Scene revision does not advance;
- dirty state does not change.

### U8-03 — Successful mutation advances dirty/revision exactly once logically

A single user-visible persistent edit must not accidentally produce multiple
independent dirty/Undo mutations merely because several Scene structures were
written.

The implementation may have internal substeps, but the external mutation
contract is one logical edit.

### U8-04 — Undo operates on committed persistent state

Undo is not a synonym for cancelling an uncommitted transport request.

If a musical change is still only pending activation and has not committed a
persistent edit, cancelling that pending activation must not consume an Undo
history entry.

If the persistent edit already committed, Undo restores the committed pre-edit
state according to 0.9.8 policy.

### U8-05 — Runtime-only activation does not become an Undo mutation by itself

Examples:

- selecting which already-persisted Song slot is currently playing;
- applying a queued playback-direction toggle;
- moving a runtime playhead.

These are not persistent editing mutations unless they also modify persisted
project state.

### U8-06 — Existing Scene revision semantics remain coherent

Current code has an 8-byte `SceneRevisionState` with `currentRevision` and
`persistedRevision`, plus `markSceneMutated()`, save/load markers, snapshot, and
restore.

0.9.8 may change the implementation, but must preserve the user-visible
invariant:

```text
committed persistent edit => dirty
successful Save           => clean
successful Load           => loaded state is clean
failed/prepared-only edit  => no false dirty transition
```

### U8-07 — Realtime commit path must be bounded

If 0.9.9 later invokes a 0.9.8-controlled mutation at a musical boundary, the
boundary-critical part must be deterministic and bounded.

Forbidden at the boundary:

- filesystem access;
- JSON serialization;
- full-project copies;
- heap-heavy command construction;
- unbounded history traversal;
- generation of musical material;
- waiting/spinning on another task.

If the chosen 0.9.8 Undo representation cannot satisfy this requirement, live
arrangement must be split from 0.9.9 rather than creating a second fast-path
mutation framework.

### U8-08 — 0.9.8 must publish its freeze contract

Before 0.9.9 production integration begins, 0.9.8 must document:

- canonical mutation entry point or equivalent ownership rule;
- atomicity unit;
- revision/dirty rule;
- Undo capture timing;
- Undo capacity and RAM cost;
- failure behavior;
- whether mutation execution can safely occur in a bounded transport boundary;
- treatment of existing `SongPage` local Undo/rollback code.

0.9.9 consumes that frozen contract and does not infer it from implementation
spelling.

---

## 4. 0.9.9 work allowed before 0.9.8 freezes

The following work is explicitly parallel-safe.

### P9-01 — Compatibility characterization

Lock current persistence facts with docs/tests where they are already correct:

- saved Pattern data is realized material;
- manual edits remain material after Save/Load;
- PhraseBank persistence restores persisted Phrase references/metadata;
- Song structure restores saved references;
- loading does not invoke regeneration merely because generated provenance
  exists;
- compatibility-sensitive enum IDs remain stable where already persisted.

Tests must validate semantic behavior, not fragile source literals.

### P9-02 — Quantized-generation characterization

Characterize the existing generation path without changing its owner:

```text
STOP: immediate commit
PLAY: prepare outside realtime path -> pending -> BAR_START commit
changed target before commit: cancellation
```

Also characterize repeated Generate behavior and current publication capacity.

Do not replace the existing fixed-slot publication mechanism before 0.9.8.

### P9-03 — Pattern/Phrase/Song reference-liveness audit

Define all roots that keep a Pattern slot live.

Current Song-generated reclamation scans Song references. Phrase slots can also
persist `patternRefs` and may represent mutable backing.

The audit must answer, per storage/source mode:

- does a valid Phrase reference keep a Pattern live?
- may a Song-generated slot referenced only by Phrase be reclaimed?
- what happens to `REFERENCE VIEW` / `REF MUT` when backing material is changed?
- do derived phrases inherit the same liveness requirement?
- are manual/imported patterns permanently non-reclaimable by Song generation?

Do not add a regression test that intentionally locks unsafe reclamation as the
expected behavior. If current behavior is unsafe or ambiguous, document a
future failing-condition test and wait for the production fix stage.

### P9-04 — Realized-state compatibility matrix

Prepare fixtures/cases for:

```text
old project -> new firmware
new project -> same firmware reboot/load
generated Pattern -> manual edit -> Save/Load
saved Phrase -> generator implementation changed
saved Song -> PhraseGenerator changed
```

Expected result is based on realized saved material.

No `generationModelVersion` or `rngSeedVersion` field may be added merely to make
these tests pass.

### P9-05 — Mutation entry-point inventory

Map persistent mutation entry points in:

- Pattern editing;
- Song editing;
- Song generation;
- Phrase capture/derive/clear/write/insert;
- generated Phrase -> Song;
- quantized generation commit;
- Scene reset/load/save related mutation boundaries.

For each entry point record:

```text
persistent objects touched
current guard
current revision call
current local rollback/Undo
PLAY allowed?
realtime involvement
candidate 0.9.8 migration need
```

This is documentation/research until 0.9.8 freezes.

### P9-06 — Activation-state model research

0.9.9 may define activation semantics independently of Undo implementation.

Candidate default contract to validate:

```text
Pattern replacement while PLAY -> next BAR_START
Song playback-slot activation   -> next Song-row boundary
Phrase live activation          -> phrase-safe boundary derived from existing
                                   Song/Phrase playback semantics
STOP                             -> no delayed realtime wait
```

This remains a behavioral contract, not permission to add production queues.

### P9-07 — Bounded pending semantics research

The preferred product model is bounded state, not an event queue.

Research must determine exact behavior for:

```text
Generate A
Generate B before A boundary
```

and equivalent Phrase/Song activation requests.

Preferred candidate when compatible with current behavior:

```text
one pending activation per logical target
newest unpublished compatible intent replaces the older intent
```

If current quantized-generation semantics differ, characterize them before
changing anything.

### P9-08 — Cross-feature invariants

Add/prepare semantic tests proving generation/arrangement does not mutate:

- 0.9.6 Output Ownership (`INTERNAL/MIDI/LAYER`);
- selected/active 0.9.7 Device Profile;
- mute state unless the user action explicitly changes it;
- unrelated Synth TYPE/parameter state;
- unrelated Song/Phrase slots.

---

## 5. 0.9.9 work forbidden before 0.9.8 freezes

The following changes are blocked:

### B9-01 — No new general transaction API

Do not add types such as:

```text
MusicalTransaction
SceneTransaction
MutationCommand
UndoAdapter
PendingPersistentMutation
```

unless they are part of the accepted 0.9.8 implementation.

### B9-02 — No second Undo/rollback subsystem

Do not extend `SongPage::UndoHistory` for new 0.9.9 behavior.

Do not create another local snapshot stack for Phrase or generation.

Do not generalize `PendingCellGeneration` into a project-wide transaction model.

### B9-03 — No new persistent pending queue

Do not persist launch/replacement requests.

Do not add an unlimited or generic arrangement queue.

### B9-04 — No transport scheduler rewrite

Use existing bar/Song boundaries where possible.

Do not introduce a new generic scheduler framework for 0.9.9.

### B9-05 — No live Phrase/Song production switching yet

Do not remove `STOP PLAYBACK FOR PHRASE`, defer Song slot switching, or change
playback boundary behavior until the 0.9.8 mutation contract is known and the
activation owner can be integrated without bypassing it.

### B9-06 — No persistence-schema expansion for provenance yet

Do not add:

```text
projectFormatVersion
generationModelVersion
rngSeedVersion
```

without a demonstrated compatibility failure that cannot be solved by current
realized-state persistence.

Existing compatibility-sensitive persisted IDs remain append-only/stable where
required.

### B9-07 — No Output Ownership or Device Profile redesign

0.9.9 must consume those owners, not replace them.

Device Profile never determines musical launch timing.

Output Ownership never determines which Pattern/Phrase/Song becomes active.

### B9-08 — No sampler dependency

The postponed sampler reliability/kit/streaming work remains outside this track.

---

## 6. Shared state taxonomy

Every 0.9.8/0.9.9 change must classify state into exactly one primary category.

### Persistent musical state

Examples:

- Pattern events/content;
- Song rows/references/length;
- PhraseBank metadata/references;
- persisted generation settings where they already exist.

Owner for mutation/Undo: **0.9.8**.

### Prepared candidate state

Material fully prepared and validated but not yet committed to persistent Scene
state.

Owner: producer subsystem, bounded lifetime.

It must not make the project dirty by itself.

### Pending activation state

A bounded runtime intent saying when an already prepared/committed musical state
should become active for playback.

Owner: **0.9.9 activation/transport integration**.

It is not Undo history and is not project persistence by default.

### Runtime playback state

Examples:

- playhead;
- current playback Song slot;
- current row/step;
- queued runtime direction toggle.

Owner: transport/runtime.

It must not accidentally become persistent musical state.

### Provenance/diagnostic metadata

Optional information describing where realized material came from.

It is never authoritative playback state.

---

## 7. Prepare / commit / activate contract

The combined lifecycle must preserve three distinct concepts.

### PREPARE

Heavy or failure-prone work occurs here when possible.

PREPARE may:

- resolve safe Pattern destinations;
- generate Pattern content into bounded temporary storage;
- validate Song/Phrase capacity;
- validate target identity;
- calculate deterministic seeds;
- reject an operation before mutation.

PREPARE must not require an Undo entry.

### COMMIT

COMMIT is a persistent editing event.

After 0.9.8 freezes, persistent 0.9.9 edits must use the canonical 0.9.8
mutation semantics.

A successful COMMIT:

- makes the new realized persistent state authoritative;
- records one logical Undo mutation where applicable;
- updates revision/dirty semantics exactly once logically.

A failed COMMIT must not leave partial persistent state.

### ACTIVATE

ACTIVATE changes what playback consumes at an explicit musical boundary.

ACTIVATE may be immediate while stopped or deferred while playing.

ACTIVATE must not regenerate material.

ACTIVATE must not create a second persistent mutation merely because playback
starts consuming already committed material.

---

## 8. Stop / Save / Load / reboot contract

These rules are required unless later research finds a concrete current product
behavior that must be preserved instead.

### STOP

No pending operation may remain in an ambiguous half-committed state.

For generation, preserve the current explicit stopped/immediate behavior unless
there is a tested reason to change it.

For future live arrangement activation, STOP semantics must be explicitly
chosen before implementation:

- activate immediately;
- cancel pending activation; or
- retain it for next Start.

The choice must be deterministic and tested. Do not inherit accidental behavior
from UI state.

### SAVE

SAVE serializes committed persistent state.

Prepared-only or activation-pending runtime state is not silently promoted into
persistent state by Save.

### LOAD

LOAD replaces persistent musical state with the loaded committed project state
and clears stale runtime pending activations.

There must be no zombie activation from the previous project after Load.

### Reboot/power loss

After reboot, only persisted committed state is authoritative.

A pending runtime activation that was never persisted must not reappear.

---

## 9. Pattern liveness contract

0.9.9 production must define one correct liveness rule before changing slot
reclamation.

At minimum, a Pattern slot is not reclaimable while any authoritative persisted
reference that requires that backing material remains live.

Potential reference roots include:

- Song slot A rows;
- Song slot B rows;
- valid PhraseBank Pattern references;
- any other persisted reference owner proven by code archaeology.

Manual/imported material with no Song reference must continue to be protected
from Song-generated reclamation unless an explicit ownership contract says the
material is disposable.

The existing Song-generated ownership bit is provenance/ownership evidence, not
by itself proof that a Pattern is dead.

Reclamation rule conceptually requires both:

```text
known reclaimable ownership
AND
no live authoritative reference
```

---

## 10. 0.9.8 handoff checklist required before 0.9.9 production

0.9.9 production is unblocked only when all of the following are known from the
accepted 0.9.8 head:

- [ ] exact 0.9.8 baseline/head SHA;
- [ ] canonical persistent mutation owner;
- [ ] atomic multi-object mutation semantics;
- [ ] Undo capture timing;
- [ ] Undo capacity and fixed/dynamic RAM cost;
- [ ] dirty/revision semantics;
- [ ] failure/no-partial-write semantics;
- [ ] treatment of `SongPage::UndoHistory`;
- [ ] treatment of `PendingCellGeneration` rollback receipt;
- [ ] PhraseWorkspace mutation integration policy;
- [ ] whether bounded commit can be invoked at a musical boundary safely;
- [ ] Save/Load interaction with Undo state;
- [ ] hardware/host acceptance status.

If any item affecting mutation ownership is UNKNOWN, 0.9.9 may continue
research/tests but must not create production integration around a guessed API.

---

## 11. Parallel branch strategy

While 0.9.8 is under development:

### Research branch

Keep:

```text
research/0.9.9-generation-arrangement
```

for docs, archaeology, compatibility matrices, and non-invasive semantic test
design.

### Optional contract-lock branch after 0.9.7 freezes

A separate 0.9.9 pre-production branch may be created from the exact accepted
0.9.7 release baseline for tests-only work:

```text
research/0.9.9-a0-compatibility-lock
```

Allowed there:

- semantic host regressions;
- fixture files;
- test helpers with no production behavior change;
- compatibility/liveness characterization that describes correct current
  behavior.

Not allowed there:

- mutation APIs;
- Undo integration;
- live arrangement implementation;
- persistence schema changes.

When 0.9.8 freezes, rebase/port only the relevant tests onto the accepted 0.9.8
baseline rather than merging an old production tree blindly.

---

## 12. Required parallel test matrix

Tests prepared before 0.9.8 should cover the following contracts where current
behavior is already correct.

| Case | Required invariant |
|---|---|
| Generate while stopped | realized material commits immediately |
| Generate while playing | current quantized-generation boundary preserved |
| Target changes before pending generation commit | stale target is not mutated |
| Generator fails during prepare | no partial persistent mutation |
| Song multi-track generation fails before commit | Pattern/Song state unchanged |
| Repeated Song generation | manual/imported Pattern data is not reclaimed |
| Generated Pattern manually edited then saved | manual edit survives Load |
| PhraseBank save/load | Phrase metadata/refs survive exactly |
| Song save/load | realized Song structure survives exactly |
| Firmware generator code changes | saved realized material does not regenerate on Load |
| Save with runtime pending activation | only committed persistent state is saved |
| Load/reboot | stale pending runtime activation is absent |
| Output Ownership | generation/arrangement does not change INTERNAL/MIDI/LAYER |
| Device Profile | generation/arrangement does not select/change profile |

Cases involving an ambiguous/possibly unsafe current behavior must first be
recorded as research findings, not encoded as a passing regression expectation.

---

## 13. Acceptance gates for starting 0.9.9 production

### Gate A — release baseline

0.9.7 must be integrated/frozen, or 0.9.8 must already be based on the accepted
post-0.9.7 release line.

### Gate B — 0.9.8 ownership freeze

The 0.9.8 handoff checklist above is complete.

### Gate C — no competing owner

There is exactly one persistent mutation/Undo owner for the 0.9.9 integration
path.

### Gate D — liveness decision

Phrase/Song/Pattern reference liveness has an explicit tested contract before
Song-generated reclamation is extended.

### Gate E — realtime feasibility

The accepted 0.9.8 mutation semantics can support the intended boundary commit,
or live arrangement is split into a later release.

### Gate F — persistence continuity

No production stage requires regeneration-on-load or an unproven project schema
migration.

---

## 14. First production stage after 0.9.8 freeze

The first 0.9.9 production stage should remain narrow:

```text
0.9.9-A — Compatibility + Pattern/Phrase liveness
```

Scope:

- port/rebase accepted compatibility regressions to the 0.9.8 baseline;
- resolve Pattern liveness across Song + Phrase persisted references;
- preserve manual/imported material safety;
- preserve Song-generated ownership-bit semantics where valid;
- verify Save/reboot/Load realized-state continuity;
- no new live-arrangement UI;
- no scheduler rewrite;
- no generator framework rewrite;
- no provenance version fields unless a failing compatibility case proves need.

Only after this stage is green should 0.9.9 integrate canonical persistent
prepare/commit semantics and then live activation.

---

## 15. Split triggers

Keep 0.9.9 as one release theme but split Live Arrangement back out if any of
these becomes true after 0.9.8:

1. 0.9.8 cannot atomically represent required Pattern+Song/Phrase edits without
   a special second mutation framework;
2. boundary-time integration requires heavyweight Undo work in the realtime
   path;
3. Song/Phrase live activation requires an independent transport-engine rewrite;
4. safe liveness requires an incompatible persistence redesign;
5. the first production slice must simultaneously modify generation migration,
   Song runtime, Phrase persistence, and Undo internals to be useful.

In that case retain the compatibility/liveness portion as 0.9.9 and move Live
Arrangement to the next release rather than weakening the ownership contracts.

---

## 16. Explicit non-goals

- no generation framework rewrite;
- no Groove Vocabulary rewrite;
- no Atlas rewrite;
- no TEXTURE resurrection;
- no generic scheduler framework;
- no unlimited launch queue;
- no new Song engine;
- no new Phrase model;
- no second Undo subsystem;
- no sampler redesign;
- no Device Profile redesign;
- no Output Ownership redesign;
- no DAW timeline/editor;
- no regeneration-on-load architecture;
- no generator-version metadata without a demonstrated compatibility need.

---

## 17. Decision

Parallel work is **GO WITH BOUNDARIES**.

0.9.8 may implement and freeze persistent mutation/Undo independently.

0.9.9 may simultaneously perform compatibility/liveness research, semantic
regression locking, lifecycle mapping, and activation-contract design.

0.9.9 production code that depends on persistent mutation ownership remains
**WAIT FOR 0.9.8 FREEZE**.

The critical handoff rule is:

```text
0.9.8 defines HOW committed persistent edits are made/undone.
0.9.9 defines WHEN prepared musical state becomes active and how references stay safe.
```

Neither release may duplicate the other's owner.