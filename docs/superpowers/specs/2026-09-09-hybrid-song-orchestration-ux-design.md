# Hybrid Song Orchestration UX — Design

**Date:** 2026-09-09

**Authoritative base:** `integration/20260909-0.9.10-pattern-phrase-runtime-ui-gf2` @ `fcd0d77da5ed6ef38419547477ab26e77ec6ff26`

## Goal

Reframe GroovePuter Song UX from generator-attempt orchestration to hybrid material orchestration without pretending that `VARIATE` / `DEVELOP` semantics already exist where runtime does not support them.

The target user model is:

```text
GENRE SPACE
  -> IDEA
  -> MATERIAL
  -> RELATION
  -> FORM
  -> SONG
```

Song remains compact and reference-oriented. The UX layer makes musical relation and intent visible instead of exposing seeds, retries, implementation depth, or misleading generator modes as the primary musical model.

## Evidence from current implementation

### Song is currently pattern-reference orchestration

`SongPage` edits per-row references for Synth A, Synth B and Drums. QWERTY assigns existing patterns. `G` materializes a new pattern into a free slot and assigns that reference to the current Song cell. Double-`G` rolls back the provisional single-cell materialization and replaces it with a whole-row generation.

This is a valid low-memory storage model, but not yet a musical orchestration model.

### Current generator-mode presentation is misleading

The Song UI exposes:

```text
RND / SMART / EVOL / FILL
```

via `SmartPatternGenerator::Mode`, but the current `SongPage::materializeSongTracks()` does not prove that these are four distinct user-facing musical operations. `gen_mode_` participates in the mode tag and Atlas variation selection; fallback materialization uses ordinary `GrooveboxModeManager` generation.

Therefore `EVOL` and `FILL` must not be promoted into the future orchestration vocabulary until their semantics are made explicit and testable.

### Public PHRASE is generator-request centric

The public `PHRASE` page presents:

```text
LENGTH
DEPTH
TO
OCC
LAST G
LAST ACCEPTED
```

and `G` immediately materializes generated Phrase content into Song.

This exposes the mechanics of the next generation attempt rather than the musical relation the user wants.

### Useful semantic vocabulary already exists internally

`PhraseCore` already contains evidence of useful concepts:

```text
Main
Variation
Break
Ending
Generated
Derived
ReferenceView
OwnedEvents
parentId
```

`PhraseGenerator` also contains:

```text
Base
MicroVariation
Return
Development
Breakdown
Build
Fill
EndingFill
```

These names are not automatically authoritative product semantics. Their implementations must be evaluated independently. They are evidence that material provenance and phrase-scale roles are already recognized problems in the project.

## Product vocabulary

The orchestration vocabulary is deliberately smaller than the current internal vocabulary.

### Material

A musical object that can be played, edited, reused, persisted, or derived.

### Placement relation

The meaning of one Song occurrence relative to musical context:

```text
NEW      introduce a new musical idea
REPEAT   reuse the same material intentionally
VAR      a recognizable variation of existing material
DEV      development/continuation of an existing idea
REST     intentional silence
RETURN   reuse an earlier established idea in a return function
```

These relations are occurrence semantics. They are not all properties of the underlying material.

For example, the same material `Q` may appear as:

```text
row 01  Q  NEW
row 02  Q  REPEAT
row 08  Q  RETURN
```

No duplicate material is required for `REPEAT` or `RETURN`.

## Required distinction: EMPTY vs REST

`EMPTY` means no compositional decision has been assigned yet.

`REST` means the user or form engine explicitly decided that the occurrence should be silent.

Automatic fill must never treat REST as an unassigned hole.

## Three UX levels

### Level 1 — Material

Primary questions:

```text
what material is this?
what plays?
what is being edited?
```

Operations include play, edit, select existing material, and make unique when reference semantics require it.

### Level 2 — Relation

Primary question:

```text
what is this occurrence doing relative to the current musical idea?
```

Operations are NEW / REPEAT / VAR / DEV / REST / RETURN when supported by runtime.

### Level 3 — Form

Primary question:

```text
how should several material occurrences be organized over time?
```

The form layer can propose relations, but the user may replace any individual decision.

## Hybrid orchestration model

The preferred model is not fully automatic Song generation and not material-only manual arranging.

The user can begin from existing material. Song may then suggest the next structural intent. If new material is required, the user may generate it or select existing material. Existing user material remains authoritative and is never silently regenerated during Fill Song.

Conceptually:

```text
existing Q
  -> REPEAT Q
  -> VAR of Q
  -> REST
  -> NEW W
  -> RETURN Q
```

## UI presentation principle

The overview should primarily answer **what** is placed. Focused context should answer **why / how it relates**.

A compact Song grid remains valid for 240×135. Relationship metadata should not be repeated inside every track cell if it makes the grid unreadable.

Candidate focused presentation:

```text
ROW 03   W
REL VAR <- Q
```

or:

```text
ROW 08   Q
REL RETURN
```

The final label set must be verified on-device for readability.

## Input semantics

No physical key mapping is frozen in this design.

Current Song keyboard ownership is already dense: QWERTY assignment, `G`/double-`G`/Ctrl-`G`/Alt-`G`, `B` variants, `P`, `V`, `L`, Alt-J, copy/paste, selection and markers.

A keyboard ownership census is required before assigning NEW / VAR / DEV / REST / RETURN gestures.

## `G` contract

`G` must no longer be treated as a universal musical semantic across Song and Phrase.

Current behavior may remain during compatibility phases, but the product design must distinguish:

```text
GENERATE NEW
PLACE EXISTING
VARIATE EXISTING
DEVELOP EXISTING
```

UI must not present `VARIATE` or `DEVELOP` if both still map to the same reroll/generation path.

## P1/P2/P3 presentation

P1/P2/P3 remain valid generator realization policy:

```text
P1 Canonical
P2 Variation
P3 Transformation
```

but are not the primary orchestration vocabulary. They may remain as an advanced or secondary control while musical actions expose user intent.

## PhraseGenerator roles

Existing `PhraseBarRole` values are research evidence, not accepted semantics.

Current transformations such as one-note octave displacement for `Development` or event thinning for `Breakdown` must not be assumed to satisfy the musical meaning of those words. G4 research decides which roles survive and how they are implemented.

## Immediate safe UI improvements

The following are allowed before G4 implementation if backed by regression tests:

1. Make current material target / edit target explicit.
2. Make `PLAY` vs `EDIT` source and target unambiguous.
3. Remove or de-emphasize labels that claim stronger semantics than the runtime proves.
4. Distinguish technical generator policy from musical actions in presentation.
5. Preserve existing Pattern/Phrase/Song runtime behavior and lifetime invariants.

## Explicit non-goals for the first UX checkpoint

Do not implement:

- a new Idea owner;
- a universal Material framework;
- automatic Variation semantics;
- automatic Development semantics;
- ancestry graph UI;
- desktop-style arranger timeline;
- new Song runtime ownership;
- new persistence format;
- genre coefficient changes;
- G0–G4 musical generation fixes.

## Dependency on GF2

UX and GF2 have separate responsibilities.

```text
G0-G3: preserve and realize chosen musical intent correctly
G4: produce new idea / variation / development semantics
O1-O3: expose material relation and form to the user
G5: split genre tree only where independent identity is proven
```

UX must not compensate for missing G0–G4 behavior by relabeling existing rerolls.

## Acceptance criteria for this design line

A user should eventually be able to answer without reading implementation details:

1. What is playing?
2. What material is selected?
3. What object will the next edit modify?
4. Is this occurrence a reuse, a deliberate rest, or an unassigned hole?
5. Is the requested operation new material, a variation, or development?
6. Will editing this material change other Song occurrences that reference it?
7. Can the previous established idea be reused without generating a duplicate?
8. Can automatic assistance fill structure without overwriting accepted user material?

The immediate implementation checkpoint is successful if it improves truthfulness and causality of the current UI without exposing fake future capabilities.
