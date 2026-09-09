# 0.9.11 Hybrid Song Orchestration — UX Census

Date: 2026-09-09

Authoritative base: `fcd0d77da5ed6ef38419547477ab26e77ec6ff26`

Research branch: `research/20260909-01-hybrid-song-orchestration-ux`

## Scope

Research and characterize current Song / Phrase product orchestration before implementing the 0.9.11 hybrid material model.

No G0–G4 musical semantics are implemented by this census.

## 1. Current Song mental model

The current Song page is a compact pattern-reference arranger.

Each row holds references for:

- Synth A;
- Synth B;
- Drums.

QWERTY assignment places an existing pattern reference into the selected cell.

`G` materializes fresh physical material into a free Pattern slot and assigns the resulting reference to the selected Song cell.

Double-`G` is a timing gesture rather than a distinct musical operation:

1. first tap commits one generated cell;
2. second tap inside the double-tap window rolls the provisional result back;
3. the whole row is then generated.

This is responsive and bounded, but the visible action is oriented around generator invocation rather than musical intent.

## 2. Current Song generator mode presentation

The page exposes four labels:

```text
RND
SMART
EVOL
FILL
```

The current materialization path does not establish that these are four user-facing musical operations.

`gen_mode_` contributes to a mode tag and Atlas variation selection. Fallback generation still uses `GrooveboxModeManager` with the active genre/recipe/seed.

Therefore:

```text
EVOL != proven musical development
FILL != proven compositional fill
```

The labels are stronger than the implementation evidence.

Classification: **misleading control semantics**.

## 3. Current public PHRASE mental model

The public PHRASE page presents the next generator request:

```text
LENGTH
DEPTH
TO
OCC
LAST G
LAST ACCEPTED
```

The primary gesture is again:

```text
configure request
-> G
-> generated Phrase is placed into Song
```

This is not yet a material relationship workflow.

## 4. P1 / P2 / P3

`generation_request_state.h` defines the values as realization levels:

```text
P1 Canonical
P2 Variation
P3 Transformation
```

They are valid generator policy.

They are not a sufficient user-facing replacement for:

```text
NEW IDEA
VARIATION OF THIS IDEA
DEVELOP THIS IDEA
```

`DEPTH` is therefore a poor product label. A neutral realization-level label is more truthful until G4 provides musical operation semantics.

## 5. Existing semantic archaeology

### PhraseCore

Existing vocabulary includes:

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

This proves that the project has already needed provenance and material relation.

It does not mean PhraseCore must become the future orchestration owner.

### PhraseGenerator

Existing phrase roles include:

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

However the current transformations are mechanically narrow. Examples include accent toggling, one-note octave movement, thinning, or a final-quarter fill.

The vocabulary is useful research evidence; the musical semantics require G4 validation.

## 6. Required product separation

The future product model must distinguish:

```text
MATERIAL
RELATION
FORM
```

Material answers:

> What musical object is this?

Relation answers:

> What does this occurrence do relative to established material?

Form answers:

> How are several occurrences organized over time?

## 7. Occurrence relation

Candidate product vocabulary:

```text
NEW
REPEAT
VAR
DEV
REST
RETURN
```

These are not all Material properties.

Example:

```text
01 Q  NEW
02 Q  REPEAT
08 Q  RETURN
```

The same physical material can carry multiple Song-level functions without duplication.

## 8. EMPTY versus REST

The current negative pattern reference effectively represents an empty/unassigned cell.

Future orchestration needs a distinct intentional-rest state.

```text
EMPTY = no decision yet
REST  = deliberate silence
```

This distinction is required before any automatic form fill can be trusted.

## 9. Keyboard ownership census

Current Song input already owns a dense set of gestures.

### Navigation and selection

- arrows — move cursor;
- Shift/Ctrl + arrows — extend selection;
- Ctrl+W / Ctrl+S — larger vertical jumps;
- Alt + comma/dot — top/end;
- copy/cut/paste application events;
- selection locking and movement.

### Pattern/material assignment

- QWERTY pattern keys — assign existing pattern references;
- `B` — switch assignment bank;
- Alt+B — destructive bank flip on stored reference/selection.

### Generation

- `G` — generate selected cell;
- double-`G` — rollback provisional cell and generate row;
- Ctrl+G — cycle generator mode;
- Alt+G + selection — batch generate selected area.

### Song/live behavior

- Ctrl+B — request playback slot switch;
- Alt+X — toggle LiveMix;
- Alt+M — Song mode;
- Ctrl+M / Ctrl+N — delete/insert row;
- `L` and Ctrl+L — loop operations;
- Ctrl+R — reverse / long-press start behavior;
- `P` — cursor to playhead;
- `V` — lane focus;
- `X` — split comparison;
- Alt+J — Song to Phrase handoff;
- marker shortcuts.

Conclusion:

> Do not assign NEW / VAR / DEV / REST / RETURN physical keys until a keyboard ownership redesign is evidence-backed.

## 10. Immediate safe production scope

The first production change should not add new orchestration capability.

It should only improve truthfulness:

1. Replace Song `RND/SMART/EVOL/FILL` presentation with a neutral generation-alternative indicator.
2. Rename public PHRASE `DEPTH` to neutral `LEVEL`.
3. Rename `LAST G` to `LAST GEN`.
4. Preserve all current generation, placement, rollback, runtime lifetime and keyboard behavior.

No semantic action is added.

## 11. Explicitly deferred

Do not implement in the first patch:

- Idea ownership;
- material ancestry storage;
- NEW/VAR/DEV runtime commands;
- REST persistence;
- form engine;
- Fill Song rewrite;
- new keyboard mappings;
- G0–G4 generator changes;
- genre changes.

## 12. Regression contract

The first UI patch must prove:

```text
Song no longer displays RND/SMART/EVOL/FILL as musical meanings
PHRASE no longer calls P-level DEPTH
PHRASE identifies prior generation outcome as generation
```

while existing Song generation source regressions, Phrase regressions, P3 lifetime regressions and Cardputer build remain unchanged.

## 13. Research conclusion

The current implementation already has useful low-memory reference mechanics and several internal role/provenance concepts.

The central product problem is not absence of every primitive. It is semantic fragmentation:

```text
correct-ish words live in internal layers
while public UX is organized around generator attempts
```

0.9.11 should preserve the bounded runtime/storage work and replace the user mental model incrementally rather than performing a Song rewrite first.
