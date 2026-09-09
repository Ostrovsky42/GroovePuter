# G4 Reference Genre / Musical-Idea Contracts Design

Status: **0.9.11 architecture contract**

Authoritative starting point:

- feature base: `8d7713e67431a6bc8eba2f7dfcee4c94b3fe821b`
- source research: `docs/gf2/GF2_G4_C0_MATERIALIZED_IDEA_CORPUS.md`
- reference/calibration genres: Acid, House, Dub Techno, Drum & Bass

## 1. Goal

Make the existing generator choose **coherent musical statements inside a genre**, vary those statements without silently changing their identity, and develop them through Phrase time without rerolling them.

The target semantic chain is:

```text
GENRE IDENTITY
    invariants / prohibitions
        ↓
MUSICAL IDEA
    coherent structural statement
        ↓
IDEA VARIATION
    P1 / P2 / P3 + bounded realization changes
        ↓
PHRASE DEVELOPMENT
    bar-function development of the same statement
        ↓
EXISTING MATERIALIZATION / PLAYBACK
```

The four pilot genres are **verification/calibration fixtures**, not a runtime taxonomy.

## 2. Non-goals

This checkpoint does not add:

- `ReferenceGenre` runtime state;
- a new persisted `MusicalIdea` field;
- Scene/save/project schema changes;
- a new sequencer or Phrase owner;
- a new genre or recipe to escape compatibility defects;
- UI controls such as IDEA/MORPH/TRAJECTORY;
- a generic rule DSL;
- runtime maps, heap-owned catalogs, strings, or dynamic allocation;
- a single perceptual/diversity score;
- four hand-authored canonical patterns per genre.

It does not mass-retune the remaining genres. Acid/House/Dub Techno/DnB form the first acceptance suite; other genres are audited after the architecture is proven.

## 3. Evidence that constrains the design

G4-C0 materialized 1,536 one-bar cases through the production seam and proved:

1. Representation capacity already exists.
2. P1/P2/P3 preserve composition selection identity for 128/128 identities in every pilot.
3. Different semantic tuples can collapse to the same role/material structure.
4. Dub Techno recipe 5 is structurally misaligned with an intended techno-skeleton contract: only 28/128 P2 samples preserve the four-floor skeleton, exactly the Steppers population.
5. House has a high-weight non-four-floor `FunkHouseBridge` candidate; therefore a strict four-floor House law is **not yet authoritative enough to hard-code blindly**.
6. DnB drums are strong, but the selected bass comes from generic `kBassDrive`; explicit requested bass IDs bypass the family arbitration already present in `bass_rhythm.cpp`.
7. Acid has genuine role diversity, but its exact invariant and articulation/lifetime ownership need further evidence.
8. Acid/House/Dub Techno may select non-Loop phrase-law labels while the explicit trajectory is unreachable; DnB is the only pilot where Stage-12 phrase-law execution is causally admitted.

These findings require small, evidence-backed semantic boundaries rather than more randomness.

## 4. Existing ownership to preserve

### 4.1 Genre/profile editorial owner

`GenerationProfileView` remains the owner of genre/recipe editorial choices:

- weighted role identity candidates;
- corridor / activity intent;
- harmonic-change rate;
- secondary-role policy.

`rhythmCompatibilityFor(settings)` remains the owner of genre/recipe rhythm-archetype membership.

### 4.2 Archetype grammar owner

`ReferenceVocabulary` / `RhythmArchetype` already owns:

- lane grammar;
- anchors;
- protected space;
- lane relationships;
- density constraints;
- timing eligibility;
- mutation policy.

G4 must **not duplicate those facts** into a second genre-rule table.

### 4.3 Composition owner

`resolveGenerationComposition()` remains the command-time composition selection seam.

The first G4 increment will treat a successfully validated `GenerationCompositionResult` as the ephemeral Musical-Idea value. A new `MusicalIdeaSelection` struct is deliberately deferred until evidence shows that `GenerationCompositionResult` cannot express the semantic boundary cleanly.

This avoids creating an owner merely to rename an existing fixed-capacity value.

### 4.4 Phrase owner

`PreparedPhraseExecution` / `preparePhraseExecution()` remain the multi-bar owner. Phrase develops a frozen selection; it does not select a new idea every bar.

### 4.5 Runtime owner

Audio runtime only plays materialized Pattern/Phrase state. No G4 semantic state moves into audio lifetime ownership.

## 5. New semantic boundary: structural laws, not a framework

Use the existing `src/generation/composition/genre_structural_laws.h` as the narrow home for profile/archetype relationship predicates.

The design allows small, pure, fixed-capacity predicates such as:

```cpp
bool bassRhythmCompatibleWithFamily(
    BassRhythmId bass,
    RhythmFamily family);

bool compositionObeysStructuralLaws(
    const GenreSettings& settings,
    const GenerationCompositionResult& composition);
```

Exact names may be refined during TDD, but the boundary must retain these properties:

- pure;
- allocation-free;
- no mutable/global runtime state;
- based on existing IDs/families/grammar;
- no production `ReferenceGenre` concept;
- no opaque score;
- no duplication of lane masks that already live in `ReferenceVocabulary`.

A structural law answers a musician-level question such as:

- “is this bass organization compatible with this rhythmic family?”
- “does this recipe retain its defining temporal skeleton?”

It does not encode cosmetic production choices.

## 6. Selection model: filter candidates before choosing

The current composition path selects an archetype and then independently chooses downstream role IDs from profile-wide bags. Salts create deterministic coupling, but the *candidate space* remains too broad.

G4 changes this at the smallest useful seam:

```text
profile candidate view
        +
selected archetype / family
        +
structural law
        ↓
compatible bounded candidate view
        ↓
existing deterministic weighted selector
```

Do **not** select an invalid value and retry in a random/rejection loop. Rejection loops make determinism and probability mass harder to reason about.

Preferred implementation is a bounded stack/local fixed array with the same small maximum candidate count already used by composition selection, or a selector that skips incompatible candidates while preserving canonical ID ordering and weights.

The existing deterministic seed/domain contract remains unchanged.

## 7. First protected relationship: DnB bass-family coherence

### 7.1 Current defect

DnB selects archetypes 413–416, all breakbeat material, but the DnB profile draws bass from `kBassDrive`:

```text
KickLock
OffbeatPush
RollingDrive
SyncopatedHook
```

The role realizer already defines Breakbeat auto candidates as:

```text
KickAnswer
GapFill
HalfTimePocket
SyncopatedHook
```

However `realizeBassRhythm()` returns an explicit requested ID directly, so the composition-selected value bypasses this family vocabulary.

### 7.2 Contract

For G4, an explicitly selected bass identity must be legal for the selected rhythm family under the same musician-level compatibility semantics used by the bass role.

This is a general relationship, not a DnB special case.

The first RED should prove that current DnB composition can select a bass identity that is illegal for Breakbeat. GREEN should make all DnB selected bass IDs family-compatible while preserving deterministic weighted selection.

### 7.3 Scope

Do not modify the physical bass pattern formulas in this task. The change is candidate coherence before materialization.

## 8. Dub Techno structural identity

### 8.1 Current defect

Recipe 5 (`GenerativeMode::Reggae`) currently selects rhythm candidates:

```text
409 OneDropSpace
410 Steppers
411 SparseSkank
412 ChordResponse
```

Only Steppers provides the currently measured four-floor skeleton. G4-C0 therefore proves that the route is dominated by dub/reggae temporal identities before FX/timbre are considered.

### 8.2 Contract

Dub Techno must not depend on reverb/delay/timbre to become “Techno”. Its selected drum temporal skeleton must come from a techno-compatible rhythm space; dub character remains available through:

- bass sparsity/lifetime;
- chord timing and omission;
- protected space;
- phrase development;
- FEEL / production layers after structure is valid.

### 8.3 Minimal implementation principle

Correct **recipe compatibility data** rather than introducing a new persisted genre.

Do not automatically reuse the entire `kTechnoBase`; choose the smallest evidence-backed techno-compatible candidate set that preserves multiple ideas. The characterization test must materialize candidates and verify the intended structural predicate rather than merely assert a list of IDs.

If a strict four-floor law proves too narrow for the desired Dub Techno vocabulary, the test/design must be revised before broadening production code. The acceptance criterion is a structural techno skeleton, not an arbitrary ID whitelist.

## 9. House handling

Do not delete `FunkHouseBridge` merely because it is non-four-floor in the first corpus.

House already sounds strong to the project owner, and G4-C0 only proved that strict four-floor is not currently universal. Therefore the first G4 production increment records House as a **reference regression**, but does not impose a new prohibition until a materialized listening/structural witness establishes what must replace or constrain 713.

This protects a good-sounding genre from being “fixed” into a narrower canonical template without evidence.

## 10. Acid handling

Acid is also a reference regression, not an immediate rewrite target.

The current four archetypes provide genuine role diversity. Before changing Acid production data, a later characterization must identify which bass onset/lifetime/articulation relations distinguish valid sparse/syncopated Acid from generic dance patterns after timbre removal.

P3 click compression alone is insufficient justification for a production rewrite.

## 11. Phrase-law truthfulness

Current behavior can advertise/select a non-Loop law without an admitted explicit trajectory for Acid/House/Dub Techno.

The 0.9.11 rule is:

> If a non-Loop phrase law is selected as semantic composition data, Phrase must have a causal production programme for it; otherwise composition must not claim that law.

The implementation should preserve `PreparedPhraseExecution` ownership. Two legal solutions exist per archetype/profile:

1. admit an existing compatible phrase-evolution trajectory; or
2. filter unavailable phrase-law choices at composition time.

Do not globally enable every archetype in Stage 12.

The first implementation should prefer truthful filtering when no validated trajectory vocabulary exists. Enabling a trajectory is allowed only where the existing phrase-evolution catalog has a tested structural programme.

## 12. Idea vs variation

P1/P2/P3 remain realization levels.

For one frozen composition:

```text
P1/P2/P3 may change:
  ornaments
  bounded optional events
  expression/surface
  permitted local topology

P1/P2/P3 must preserve:
  selected archetype
  selected role identities
  defining structural-law relationships
```

A P-level must not silently turn one selected role identity into another and still claim semantic stability.

No new idea-reroll behavior is added to `generationAttemptOrdinal`.

## 13. Activity coherence

Activity is currently projected primarily into drum structural density. G4 eventually needs ensemble coherence, but it must not introduce a generic continuous “idea density” knob.

This checkpoint only establishes the architecture needed for later activity filtering:

- profile corridor remains activity-intent owner;
- structural-law predicates may classify role identities as compatible with sparse/dense intent using existing discrete musical identities;
- no activity changes are implemented until a dedicated RED demonstrates an incoherent sparse/dense cross-role result.

## 14. Observation and acceptance

Reference-genre tests retain separate projections:

```text
SEMANTIC
SURFACE
ROLE + LIFETIME
CLICK
PHYSICAL TIME
```

No aggregate `diversityScore` is introduced.

Every production task must provide a concrete before/after witness.

### 14.1 DnB acceptance

- selected bass identity is compatible with selected rhythm family for every sampled identity;
- multiple bass structures remain reachable;
- P1/P2/P3 preserve selection identity;
- DnB role diversity does not collapse to one pattern.

### 14.2 Dub Techno acceptance

- every allowed recipe-5 rhythm candidate passes the chosen techno-skeleton materialized predicate;
- at least several distinct structural candidates remain reachable;
- dub bass/chord vocabulary remains available;
- no new genre/schema/runtime owner is introduced.

### 14.3 House/Acid acceptance

- existing reference corpus remains runnable;
- no unrequested narrowing of their current idea space;
- any metric movement is reported rather than silently accepted.

### 14.4 Phrase acceptance

For every sampled composition whose semantic phrase law is non-Loop:

```text
phraseTrajectory != kNoTrajectoryId
```

or that non-Loop law must have been filtered out before selection.

The same frozen composition remains active across all bars.

## 15. Memory and embedded constraints

All new command-time semantic data must be fixed-capacity and trivially copyable where practical.

Prefer:

- `constexpr` compatibility data;
- pure predicates;
- stack/local bounded arrays;
- existing compact IDs.

Avoid:

- heap allocations;
- `std::vector`, maps, strings;
- duplicated pattern/phrase buffers;
- new persistent fields.

Final verification must report Cardputer ADV fixed DRAM and binary-size deltas against the authoritative base.

## 16. Implementation order

The architecture is intentionally incremental:

1. **G4-R1** — characterization tests for reference contracts, no production behavior change.
2. **G4-I1** — general bass-family compatibility boundary; fix DnB selection coherence.
3. **G4-I2** — correct Dub Techno recipe temporal compatibility using materialized techno-skeleton evidence.
4. **G4-I3** — make phrase-law semantics causally truthful by filtering unavailable laws or admitting tested trajectories.
5. **G4-V1** — rerun reference corpus, compare before/after, run embedded/memory gates.

House and Acid remain protected calibration genres during these first implementation steps.

## 17. Stop conditions

Stop and revisit the design if any step requires:

- a new persisted MusicalIdea schema;
- a new sequencer/Phrase owner;
- a generic genre-rule DSL;
- scattered `if (genre == Acid/House/Dub/DnB)` logic across realizers;
- a random retry/rejection loop to obtain legal combinations;
- a major reduction in House/Acid structural variety without a listening/structural witness;
- heap allocation in generation/audio hot paths;
- a test that can only be expressed as an opaque quality threshold.

## 18. Architectural decision

For the first 0.9.11 G4 implementation, **do not introduce a new `MusicalIdeaSelection` type**.

Use the existing `GenerationCompositionResult` as the frozen ephemeral idea value, but make its selected components coherent through structural-law candidate filtering. This is the smallest architecture that separates:

```text
genre legality
from
idea selection
from
P-level realization
from
Phrase development
```

If later G4 work discovers idea properties that are not representable without overloading `GenerationCompositionResult`, introducing a dedicated fixed-capacity type becomes a separate, evidence-backed architectural checkpoint.