# 0.9.11 O1 — Hybrid Song Orchestration Design

Date: 2026-09-09

Branch: `research/20260909-01-hybrid-song-orchestration-ux`

Status: DESIGN FROZEN FOR REVIEW

## 1. Goal

Introduce the minimum Song-level semantic foundation required for hybrid material orchestration without increasing the resident `Song` footprint, inventing unimplemented musical actions, or weakening the accepted bounded Undo/lifetime architecture.

The first implementation target is deliberately narrower than the future orchestration model.

O1 establishes:

- `EMPTY != REST`;
- a semantic Song-cell boundary instead of raw integer interpretation;
- explicit material-reference identity;
- safe `REUSE` semantics through shared references;
- a future-proof storage envelope for occurrence relations;
- a bounded design for explicit `MAKE UNIQUE` copy-on-write material forking.

O1 does **not** implement `NEW IDEA`, `VARIATION`, `DEVELOPMENT`, `RETURN`, form generation, or G4 musical semantics.

## 2. Product model

The orchestration stack is:

```text
GENRE SPACE
    ↓
IDEA
    ↓
MATERIAL
    ↓
OCCURRENCE RELATION
    ↓
FORM
    ↓
SONG
```

Material answers:

> What musical object is referenced here?

Occurrence relation answers:

> What does this placement mean in the Song?

Form answers:

> How do placements relate over time?

A material identity and an occurrence relation are distinct concepts. The same material may appear as an introduction, repeat, or later return without duplication.

## 3. Existing constraints

### 3.1 Song footprint

Current `SongPosition` is four `int16_t` values and therefore 8 bytes per row.

Current `Song` contains 128 rows plus bounded scalar state.

The accepted Undo architecture stores a complete `Song` in `SongUndoPayload` and constrains that receipt to approximately 1 KiB. The canonical Undo owner has a fixed 1536-byte payload capacity.

Therefore O1 must not grow `SongPosition` or `Song`.

### 3.2 Existing pattern reference range

Pattern references are currently bounded to `0..255`; `-1` means no referenced Pattern.

This leaves unused representational space inside the existing 16-bit cell while preserving the 2-byte footprint.

### 3.3 Current raw-cell assumptions

Existing consumers frequently interpret raw values directly:

```text
ref >= 0  => occupied/material
ref < 0   => empty
```

This assumption exists in Song editing, Phrase insertion, persistence, merge/trim logic, liveness/reference counting, UI and live arrangement comparison.

A semantic cell representation is not safe until these consumers migrate to canonical accessors.

### 3.4 Persistence

Current scene JSON stores raw Song track references as:

```json
{
  "a": 12,
  "b": 4,
  "drums": 0,
  "voice": -1
}
```

Existing decode clamps unknown negative values to `-1`.

Therefore occurrence relation must not depend on persisting tagged negative pattern references directly.

## 4. O1 logical SongCell contract

Each Song track occurrence has exactly one logical state:

```text
EMPTY
MATERIAL(patternRef, relation)
REST
```

### EMPTY

No compositional decision has been made for this track at this row.

Consequences:

- auto-fill may populate it;
- merge may fall back to another Song source;
- trailing EMPTY does not preserve Song length;
- UI should present it as unresolved/unassigned.

### MATERIAL

A valid material reference exists.

Initial O1 relation is `UNSPECIFIED` only. Future relation values may include musically validated meanings, but O1 must not expose labels for semantics that G4 has not implemented.

### REST

A deliberate decision that this track is silent at this Song occurrence.

Consequences:

- auto-fill must not populate it;
- merge must not fall back to another Song source;
- REST counts as a used Song occurrence for length/trim decisions;
- playback is silent for that track;
- UI must distinguish REST from EMPTY;
- copy/paste, insert/delete, alternate and Undo must preserve it exactly.

## 5. Physical representation

### 5.1 Requirement

The physical Song cell must remain exactly 2 bytes.

### 5.2 Encoding envelope

Use the existing `int16_t` storage envelope, but remove raw interpretation from consumers.

Reserved semantic ranges:

```text
-1       EMPTY
-2       REST
0..255   MATERIAL with legacy-compatible reference and UNSPECIFIED relation
256+     reserved tagged MATERIAL range for future occurrence relations
```

O1A only needs `EMPTY`, `REST`, and legacy-compatible `MATERIAL`.

The tagged positive range is reserved by contract but should not be populated until a later approved relation checkpoint.

### 5.3 Canonical API

Production code must migrate toward canonical helpers conceptually equivalent to:

```text
SongCell::empty()
SongCell::rest()
SongCell::material(patternRef)

cell.isEmpty()
cell.isRest()
cell.hasMaterial()
cell.patternRef()
cell.relation()
```

The exact C++ API may follow existing repository naming conventions, but the semantics above are fixed.

No consumer should infer Song meaning from `raw < 0`, `raw >= 0`, or direct access to a raw storage member after O1 migration is complete.

## 6. Persistence contract

Scene persistence remains backward compatible.

### 6.1 Existing refs stay unchanged

Material refs continue to serialize through existing fields:

```json
{
  "a": 12,
  "b": 4,
  "drums": 0,
  "voice": -1
}
```

### 6.2 Optional occurrence metadata

Add an optional packed relation field on each Song row.

Conceptual shape:

```json
{
  "a": -1,
  "b": 4,
  "drums": 0,
  "voice": -1,
  "rel": 1
}
```

`rel` is a compact row-level packed value. The recommended envelope is 3 bits per track, 12 bits total for four tracks.

Initial meanings need only represent:

```text
UNSPECIFIED
REST
```

Other values remain reserved until separately approved.

### 6.3 Compatibility behavior

Old scene without `rel`:

```text
all occurrences decode as UNSPECIFIED;
-1 continues to mean EMPTY.
```

New scene opened by old firmware:

- material refs remain valid;
- REST stores `ref=-1`, so old firmware degrades it to silence;
- old firmware may not preserve relation metadata on re-save, which is acceptable as an explicit backward-degradation limitation.

New firmware must never require old firmware to understand tagged reference values.

## 7. Canonical semantic operations

O1A must define semantic operations rather than raw integer mutation.

Minimum operations:

```text
setEmpty(row, track)
setRest(row, track)
setMaterial(row, track, patternRef)
cellAt(row, track)
```

All must preserve current bounds behavior and Scene mutation ownership.

## 8. Song transformation semantics

### 8.1 Insert/delete

Row insertion/deletion moves the complete semantic `SongPosition` occurrence state.

Inserted row initializes to EMPTY, never REST.

### 8.2 Copy/paste

Copy/paste must copy occurrence semantics atomically with material refs.

REST pasted as REST.

EMPTY pasted as EMPTY.

MATERIAL pasted with its material identity and occurrence relation.

### 8.3 Alternate

Interleave copies complete Song positions, including REST.

Padding for missing source rows uses EMPTY.

### 8.4 Merge

Merge fallback is allowed only when the destination occurrence is EMPTY.

```text
A = EMPTY  => B may supply material or REST
A = REST   => B must not override it
A = MATERIAL => B must not override it
```

This is a core musical invariant: deliberate silence has priority over fallback material.

### 8.5 Trim/length

A row is considered used if any track contains:

```text
MATERIAL or REST
```

Trailing EMPTY may be trimmed.

Trailing REST must preserve Song length.

## 9. Playback and liveness

`REST` has no material reference and therefore contributes no audible Pattern for that track.

Material reference counting must count only MATERIAL cells.

`REST` must never make a Pattern appear live/referenced.

Live Song arrangement comparison must compare semantic occurrence state, not only decoded pattern reference, so:

```text
EMPTY -> REST
REST -> EMPTY
```

is recognized as a persistent Song mutation even though both are silent in Pattern-reference terms.

Audible track mask remains based on MATERIAL presence only.

## 10. Phrase boundary

Current Phrase reference-view logic copies Pattern refs into Song and often treats all negative refs as empty.

O1 must separate these questions:

```text
Does this Song occurrence contain material?
Is this occurrence unresolved EMPTY?
Is this occurrence deliberate REST?
```

Phrase insertion must not silently overwrite REST when operating in a mode that promises non-overwrite behavior.

For occupancy/conflict checks:

```text
EMPTY = available
REST = occupied compositional decision
MATERIAL = occupied
```

Phrase generation or insertion may intentionally replace REST only through an explicit overwrite operation.

## 11. Reference identity and REUSE

REUSE means multiple Song/Phrase occurrences reference the same physical material.

It creates no material copy.

Existing global reference counting across Song and Phrase remains the source of truth for shared material liveness.

A shared reference is not itself a Variation, Development, or Return. Those are occurrence/idea semantics and remain out of O1.

## 12. MAKE UNIQUE design

O1B introduces an explicit material fork operation.

### 12.1 Semantics

Given a selected MATERIAL occurrence:

- if global reference count is `<= 1`: no mutation, return `ALREADY UNIQUE`;
- if shared: copy the referenced material into a safe unreferenced destination and update only the selected occurrence to the new reference.

### 12.2 Preconditions

`MAKE UNIQUE` is invalid for:

```text
EMPTY
REST
invalid/off-page material that cannot be resolved safely
```

### 12.3 Atomicity

Preparation must complete before the first persistent mutation:

1. resolve source material;
2. confirm source is shared;
3. find a safe destination;
4. snapshot destination state;
5. build copied destination material;
6. build updated Song occurrence;
7. validate live/publication target.

COMMIT performs the bounded writes atomically through the existing mutation/Undo discipline.

Any preparation failure leaves Scene bytes unchanged.

### 12.4 MaterialKind

For Synth material, `MaterialKind` belongs to the material slot and must be copied with the material bytes.

A Pattern and Melody must never become separated from their slot kind.

### 12.5 Generated ownership bit

Current Song-generated material uses a reserved bit in Pattern data to mark reclaimable generated ownership.

Explicit `MAKE UNIQUE` changes the meaning of the destination: it is now user-preserved material.

Therefore the destination clone must clear the Song-generated reclaimable ownership bit.

Otherwise the allocator could later reclaim a material the user explicitly forked.

## 13. MAKE UNIQUE Undo

A plain `SongUndoPayload` is insufficient because MAKE UNIQUE mutates both:

- one Song occurrence reference;
- one destination material slot.

Undo must restore both.

Use a bounded fixed-value receipt containing only touched state:

```text
MaterialForkUndoPayload
  page / Song slot / row / track
  old Song occurrence
  destination material address
  previous destination material bytes
  previous MaterialKind where applicable
```

Do not snapshot the entire Scene.

The receipt must fit the existing 1536-byte canonical Undo capacity.

If a Drum destination plus metadata cannot fit the accepted bound, O1B must stop and redesign the receipt; increasing the global Undo payload is explicitly out of scope.

## 14. UI contract for O1

O1 does not assign new physical shortcuts for future orchestration semantics.

After backend semantics exist, UI may truthfully distinguish:

```text
EMPTY
REST
SHARED material
UNIQUE material
```

No `VAR`, `DEV`, `RETURN`, or `NEW IDEA` button may be added until the corresponding musical backend operation is independently implemented and validated.

Keyboard ownership remains a separate UX checkpoint.

## 15. No-go approaches

O1 rejects:

### Separate relation arrays in Song

Reason:

- resident DRAM growth;
- Song Undo growth;
- risk of relation/reference divergence;
- second owner for occurrence state.

### Raw negative sentinels as public API

Reason:

- current clamp destroys them;
- current consumers collapse all negative values to empty;
- cannot scale cleanly to future relation + material identity;
- persistence becomes fragile.

### Reusing MaterialKind for REST/relation

Reason:

MaterialKind describes the material slot representation, while REST/relation describes a Song occurrence. Different ownership level.

### Implementing future musical relation labels now

Reason:

G4 has not yet established honest `NEW IDEA / VARIATION / DEVELOPMENT / RETURN` behavior.

## 16. Implementation decomposition

### O1A — Song Cell Semantics

Scope:

- 2-byte SongCell authority;
- EMPTY / MATERIAL / REST;
- canonical accessors;
- no-growth compile-time size assertions;
- persistence relation codec;
- Song edit/copy/merge/alternate/trim semantics;
- live Song semantic comparison;
- Pattern liveness/reference-count protection;
- Phrase occupancy/insertion semantics;
- UI presentation for EMPTY vs REST only when backend truth exists;
- regressions forbidding raw semantic interpretation in production consumers.

No new musical generator behavior.

### O1B — Explicit Material Fork

Scope:

- shared/unique reference query;
- explicit MAKE UNIQUE core operation;
- bounded prepare/commit;
- destination material clone;
- MaterialKind preservation;
- generated-owner-bit clearing;
- fixed Undo receipt;
- live-safe publication;
- no keyboard mapping until UX ownership is separately approved.

## 17. Required test matrix

### SongCell representation

- `sizeof(SongPosition) == 8`;
- `sizeof(Song)` does not increase;
- EMPTY round-trip;
- REST round-trip;
- MATERIAL refs `0`, `255` round-trip;
- invalid persisted relation sanitizes to safe UNSPECIFIED/EMPTY behavior.

### Persistence

- old scene without `rel` loads unchanged;
- REST save/load remains REST;
- MATERIAL save/load preserves exact ref;
- old-format decode remains valid;
- streaming and document serializers agree.

### Song transformations

- insert shifts REST exactly;
- delete shifts REST exactly;
- copy/paste preserves REST;
- alternate preserves REST and pads with EMPTY;
- merge fills EMPTY but does not override REST;
- trim preserves trailing REST and removes trailing EMPTY.

### Playback/liveness

- REST produces no audible material reference;
- REST does not increment Pattern liveness;
- EMPTY->REST changes Song semantic equality/revision;
- REST->EMPTY changes Song semantic equality/revision;
- live activation captures semantic Song change correctly.

### Phrase boundary

- non-overwrite Phrase insert accepts EMPTY;
- non-overwrite Phrase insert rejects REST;
- non-overwrite Phrase insert rejects MATERIAL;
- explicit overwrite may replace REST;
- Phrase reference counting ignores REST.

### MAKE UNIQUE

- EMPTY => invalid/no mutation;
- REST => invalid/no mutation;
- unique material => ALREADY UNIQUE/no mutation;
- shared Song material => new ref, old occurrence unchanged elsewhere;
- Song+Phrase shared material => new ref, Phrase stays on old material;
- Synth MaterialKind copied;
- generated ownership bit cleared on explicit fork;
- no safe destination => zero mutation;
- Undo restores occurrence and destination bytes;
- Redo restores forked state;
- all receipts remain inside canonical Undo capacity.

### Platform gates

- core host regressions;
- Pattern/Phrase P3 lifetime regressions;
- P3-U1 UI regressions;
- live Song arrangement regressions;
- Cardputer ADV compile;
- fixed DRAM gate;
- SDL build;
- SEQTRAK MIDI-only build;
- scene persistence regressions.

## 18. Acceptance criteria

O1 is complete only when all of the following are true on one exact SHA:

1. Song can represent EMPTY, MATERIAL and REST without growing resident Song memory.
2. REST survives save/load, copy/paste, row movement and Undo.
3. REST blocks merge/auto-fill fallback and preserves Song length.
4. REST is silent and never counts as material liveness.
5. production consumers no longer infer Song semantics from raw integer sign.
6. shared material identity remains reference-based.
7. explicit MAKE UNIQUE uses copy-on-write, preserves source users, clears reclaimable generated ownership, and is Undo-safe.
8. no unimplemented `VAR/DEV/RETURN/NEW IDEA` semantics are exposed in UI.
9. all required host, embedded, memory, lifetime and UI gates are GREEN on the same exact SHA.

## 19. Deferred after O1

The following remain separate checkpoints:

- G0 intent preservation;
- G1 role integrity;
- G2 articulation expressivity;
- G3 activity control;
- G4 idea space;
- validated occurrence relations `NEW / REPEAT / VAR / DEV / RETURN`;
- form generation;
- hybrid Fill Song;
- keyboard orchestration redesign;
- genre-tree expansion G5.

O1 is infrastructure for these musical features, not their implementation.
