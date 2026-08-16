# GroovePuter 0.9.8-R2 — Undo Receipt Measurements

## Exact stack

```text
dev_0.9.7
9f80cb179f530089bd46f27e03bdde0f7684ba72

0.9.8-R1
c5c7764c3407030c696697e2aba14c52398f0c4e
```

Exact measurement head:

```text
agent/20260816-0.9.8-r2-undo-owner
cf4c37e4f65560f95decefe1dee7e27fa2d7ff99
```

Workflow:

```text
Undo Safe Editing 0.9.8 R2 measurements
run 31920214165
SUCCESS
```

## Exact host measurements

```text
SceneRevisionState                       8 B
SynthStep                                7 B
SynthPattern                           112 B
DrumStep                                 6 B
DrumPattern                             96 B
DrumPatternSet                        1192 B
SongPosition                             8 B
Song                                  1032 B
PhraseSlot                              60 B
PhraseBank                             244 B
Scene                                26048 B
```

All measured musical value types above are trivially copyable on the repository
host test ABI.

The `Scene` result is decisive: a second retained full-Scene snapshot would cost
about 26 KiB before owner metadata and is rejected as the 0.9.8 history model.
The existing `sceneTransactionScratch()` remains transaction scratch only.

## Candidate receipt measurements

```text
SynthPatternUndoPayload                116 B
SongRectUndoPayload                   1034 B
PhraseSlotUndoPayload                   62 B
MaterializedRowUndoPayload            1428 B
```

The materialized-row candidate deliberately represents the currently known
multi-object shape:

```text
Synth A Pattern before-image
+
Synth B Pattern before-image
+
Drum PatternSet before-image
+
Song row references / length metadata
```

It is not yet the final R5 generation receipt design. Larger multi-bar generation
must use domain-specific deltas/ownership receipts rather than forcing a full
Scene or arbitrary unlimited buffer.

## Candidate owner footprints

```text
BoundedUndoSlot<256>                   268 B
BoundedUndoSlot<512>                   524 B
BoundedUndoSlot<1024>                 1036 B
BoundedUndoSlot<1536>                 1548 B
BoundedUndoSlot<2048>                 2060 B
```

## R2 retained-capacity decision

The smallest currently justified capacity is:

```text
1536 bytes payload
1548 bytes total BoundedUndoSlot footprint
```

Why not 1024:

- `SongRectUndoPayload` is 1034 B;
- `DrumPatternSet` is 1192 B;
- `MaterializedRowUndoPayload` is 1428 B.

Why not 2048:

- no currently characterized Tier-1 receipt requires it;
- recovered/free heap is not a feature budget;
- Cardputer ADV fixed DRAM should reserve only measured need.

The 1536-B choice is therefore a bounded R2 capacity, not permission to encode
every future mutation as a large before-image. Future receipt shapes that do not
fit must be redesigned as bounded domain deltas or explicitly re-characterized.

## 0.9.9 / U8 consequences

This measurement supports the parallel-work contract:

- PREPARE can build/validate material without touching retained Undo;
- a COMMIT receipt for the already-known one-row multi-object materialization can
  fit in bounded fixed storage;
- ACTIVATE remains runtime/0.9.9 ownership;
- a future musical-boundary commit can avoid filesystem, JSON, generation,
  waiting and heap-backed command construction.

It does **not** yet prove boundary timing cost on Cardputer ADV. R7 hardware
acceptance must measure the final chosen commit path.

## Next R2 step

Instantiate one authoritative retained owner using `BoundedUndoSlot<1536>` and
migrate one simple Synth Pattern destructive operation.

Required vertical-slice semantics:

```text
PREPARE
  capture bounded before-state
  determine no-op/failure before commit

COMMIT
  guarded persistent write
  publish one retained receipt
  advance Scene revision exactly once

UNDO
  restore exact affected Pattern state
  restore pre-mutation SceneRevisionState
  consume receipt
```

No Song/Phrase migration, global Ctrl+Z binding, Redo, transport scheduling or
0.9.9 activation state belongs in R2.
