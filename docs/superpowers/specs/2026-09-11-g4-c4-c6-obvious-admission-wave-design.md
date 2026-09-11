# G4-C4–C6 Obvious Admission Wave Design

## Goal

Close the remaining four high-confidence single-family REVIEW_REQUIRED owners without turning RhythmFamily labels into genre identity.

The wave contains four owner-specific admission contracts:

1. **C4 — Broken / Classic 2-Step** (`mode=7, recipe=8`)
   - admitted set: `{417,419}`;
   - structural reading: two distinct UKG-compatible kick organizations are intentionally admitted — broken classic two-step and shuffled four-on-floor — while the backbeat stays anchored on steps 4/12 and shuffle timing remains eligible/preferred;
   - explicit prohibition: `418 SkippyTwoStep` and `420 MachineSyncopation` are not admitted by this recipe.

2. **C5 — Rave / Psytrance** (`mode=4, recipe=4`)
   - admitted set: `{401,402,406}`;
   - structural reading: all admitted ideas preserve the quarter-kick skeleton on steps 0/4/8/12 while hats/bass/articulation can differ;
   - explicit prohibition: broken/sparse non-quarter kick grammars are outside this recipe's admission contract;
   - this does not make Psytrance equivalent to House or Acid.

3. **C6a — Reggae / BASE** (`mode=5, recipe=0`)
   - admitted set: `{409,410,411,412}`;
   - structural reading: the base owner keeps the full current DubPulse vocabulary: one-drop space, steppers, sparse skank, chord response;
   - this is an admission contract only, not a claim that a single drum pattern defines Reggae.

4. **C6b — Reggae / Minimal Space** (`mode=5, recipe=11`)
   - admitted set: `{409,411,412}`;
   - structural reading: it is a strict sparse subset of Reggae BASE;
   - explicit prohibition: `410 Steppers` is excluded. This negative rule is the key distinction from BASE.

## Why one wave

C2 already isolated these four owners as the complete remaining fully-observed plural single-family set after C3. They are independent contract claims but share the same evidence/provenance machinery. Running one bounded wave avoids three copies of the same publisher/freshness cycle while keeping separate contract IDs and separate negative controls.

## Contract IDs

- `G4-C4-CLASSIC-2STEP-ADMISSION`
- `G4-C5-PSYTRANCE-FOURFLOOR-ADMISSION`
- `G4-C6-REGGAE-BASE-DUBPULSE-ADMISSION`
- `G4-C6-REGGAE-MINIMAL-SPACE-DUBPULSE-ADMISSION`

## Evidence boundary

Evidence must come from structural fields and exact owner admission sets. Owner/recipe display labels and weights are not proof inputs.

For C4 and C5, the focused structural test must inspect the underlying reference grammar rather than merely checking the family enum:

- C4: both admitted archetypes preserve backbeat anchors 4/12; 417 uses a broken kick grammar while 419 uses quarter kicks; both expose shuffle-oriented timing.
- C5: 401, 402, 406 each preserve quarter-kick anchors 0/4/8/12.

For C6, owner-set relations are themselves the intended musical rule:

- BASE admits all four current DubPulse structural ideas;
- Minimal Space is exactly BASE minus Steppers.

## Non-claims

- No full genre equivalence follows from sharing a RhythmFamily.
- No bass contract is added for C4, C5, or C6.
- No timbre, FX, recipe name, genre label, or weight is part of proof.
- No production generator behavior is changed.
- Phrase/temporal ownership remains out of scope.
- Deep Chord singleton remains REVIEW_REQUIRED and is not auto-promoted.

## Expected census effect

Starting from C3:

- proven contract layers: `7`;
- proven owners: `4`;
- REVIEW_REQUIRED owners: `29`;
- UNKNOWN rows: `11136`.

After C4–C6:

- proven contract layers: `11`;
- proven owners: `8`;
- REVIEW_REQUIRED owners: `25`;
- UNKNOWN rows: `9600`.

Exactly `4 × 384 = 1536` rows move from REVIEW_REQUIRED/UNKNOWN to owner-specific PROVEN admission contracts.

## C2 effect

After the wave there must be no remaining `B_SINGLE_FAMILY_PLURAL` candidates and no automatic promotion. C2 should begin directly with the mixed-family wave. Deep Chord remains the singleton tail.

## Verification

1. RED: all retained C0/R1/I3/I4/I5/I6+C1+C3 gates pass, then the new contract test fails because C4–C6 contracts are absent.
2. Structural test proves the C4 and C5 grammar witnesses and the C6 exact subset/prohibition relation.
3. Counterfactual tests reject admission drift and Minimal Space reintroduction of Steppers.
4. Label/name/weight mutations do not alter contract judgement.
5. I6 run A/B remains deterministic after promotions.
6. Committed I6 and C2 evidence is regenerated from code and freshness-checked byte-for-byte.
7. Final workflow has `contents: read` only and temporary publisher is absent.
8. Final exact SHA has no `src/` diff from C3.

## Closure trigger

The deterministic I6/C2 evidence has been published; this documentation-only commit exists to trigger the final read-only exact-SHA closure gate after removal of the temporary publisher.
