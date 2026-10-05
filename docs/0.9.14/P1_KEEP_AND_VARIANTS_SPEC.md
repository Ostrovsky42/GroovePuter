# 0.9.14 P1 — Keep, variants, continue (specification only, no product code)

Status: **draft for the owner's decision.** Base: `ccb17652`. Tags: **[OBSERVED]** read from code or measured; **[INFERENCE]** reasoned;
**[OPEN]** needs an owner decision or a measurement.

## 1. The scenario

> Found A → tried a variant → picked the one I like → continued from exactly that → could go back.

P0 gives: TAKE (`G`), one grow (`D`: DEVELOP + BREAK, or BREAK alone), one Undo. What is missing is choosing among variants and continuing from a chosen one.
The owner's one-minute listening (A A DEV A BRK A DEV, no new generation) held up by ear, so form by repetition is not the blocker; choice and continuation are.

## 2. Facts that constrain the design

* **[OBSERVED]** A page holds 16 pattern slots. A (4) + cycle (8) = 12; a second full cycle **does not fit** on the page. Any "grow again" must **replace**, not append.
* **[OBSERVED]** The Undo owner holds **one** receipt. After undoing a cycle the receipt for A is gone (measured in the P0 tests). More than one step back needs a design, not a flag.
* **[OBSERVED]** DEVELOP and BREAK are pure functions of `(recipe, law)`: rebuilt from the stored recipe and verified equal to the published bars. Laws are fixed programmes
  (DevelopReturn P3: Statement Build RepeatWithGhosts Turnaround; SparseDrift P3: Statement RepeatWithGhosts Break Return). There is **no "develop further"**: a second application of the same law is the same bars.
* **[OBSERVED]** The kept phrase carries a natural law (RepeatReply 39/64, DevelopReturn 25/64, never Loop). A section whose programme equals A's own is skipped.
* **[OBSERVED]** Variation seeds come from one `GenerationContext {projectSeed, phraseOrdinal}` that also drives the base realization. Changing the ordinal changes the **base idea**, not only the development.
* **[OBSERVED]** R0 compares the canonical hash of every lane of the kept phrase with the Song rows; any edit refuses the cycle (`EditedSinceGeneration`).

## 3. What "variant" and "keep" can mean without new theory

1. **Variant of the development (same A).** `D` again publishes a different DEVELOP/BREAK of the same A and replaces the previous cycle in one Undo.
   Needs a **variation salt** that changes ghost/drop choices but not the base bars. **[OPEN, measure]**: whether the current derivation allows a salt that leaves A unchanged
   (`deriveVariationSeed(identity, level, salt)` exists; the evolution request reads `realizationGeneration`). If not, a salt needs its own domain in the seed derivation.
2. **Keep a section as the new anchor.** Choose DEVELOP or BREAK and make it the material to continue from. Because a section is `(recipe, law)`, the anchor is
   `(recipe, law, salt)`, not a new recipe. The next grow builds **other** laws from the same recipe against the anchor's law (a law equal to the anchor's is skipped, as today).
   **[OPEN]**: with only two development laws this exhausts after one step (A, then DEVELOP as anchor gives BREAK). Real A → A′ → A″ needs more than two laws or a salt (item 1).
3. **Edited phrases.** Out of scope. Blocking on any edit is a boundary, not a goal; carrying an edit through rebuild means transforming the physical bars (lossy), which is a separate design.

## 4. Recommended slices

* **Slice V1: variants by salt.** `D` on a grown phrase replaces the cycle with another variant (one Undo). No new UI concept: same key, result toast says which variant number.
  Acceptance: A untouched; two consecutive variants differ on at least one lane in at least one section; one Undo returns to the state before the replacement; both are reproducible from `(recipe, salt)`.
  Precondition: the salt measurement above.
* **Slice V2: keep a section.** Only after V1 is heard. Needs an owner decision on the key and on what happens to the other section.
* **No slice for edited phrases** until V1/V2 are used.

## 5. Decisions needed from the owner

1. Replace, not append, on repeated `D` (the page capacity forces it): acceptable?
2. How many variants may the player step back through: one (as the Undo owner allows) or a small ring?
3. Should `D` on an already grown phrase mean "another variant" (recommended) or stay refused (`ALREADY GROWN`, current)?

## 6. Non-goals

UI redesign, semantic/lineage additions, new genres, changing DEVELOP strength, persistence across sessions.
