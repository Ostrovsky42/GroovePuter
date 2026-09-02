# Groove Vocabulary — Musical Contracts and Stage 2 Gate

**Status:** normative companion to `GROOVE_VOCABULARY_ARCHITECTURE_BRIEF.md`; architecture is not yet frozen for `RhythmPhraseRealizer` implementation  
**Base:** `dev_0.9_test` @ `18f1e49d2eda0528f758bbe0d374e5699f18857c`  
**Purpose:** close the musical-contract gaps found during review of the audited Groove Vocabulary design before Stage 2 implementation begins.

This document does **not** replace the audited architecture brief. It narrows and strengthens it.

Where this document is more specific than the brief, this document is normative for Groove Vocabulary Core v1.

In particular, this document refines the old wording that P1/P2/P3 are “independent/fresh realizations”. They are fresh realizations in the sense that P2/P3 are **not destructive mutations of an already-materialized P1 event array**. They are **not independent redraws of phrase identity**.

Stage policy:

```text
Stage 1 — data model/catalog validation
    MAY proceed once the types below are represented in the model.

Stage 2 — RelationshipResolver / RhythmPhraseRealizer
    MUST NOT begin until the normative contracts in this document are accepted.
```

---

# 1. Musical objective

A valid generator is not one that merely satisfies local constraints.

The target is:

```text
Archetype
    ↓
PhraseRhythmIdentity
    │
    ├── P1 / Statement  -> A
    ├── P2 / Variation  -> A'
    └── P3 + Intent     -> A'' / Fill / Break / Build / Turnaround
```

Across bars:

```text
A       A       A'      Fill
core    core    core     transformed subset
```

not:

```text
A1      A2      A3      A4
legal   legal   legal    legal
```

where every bar is independently correct but the phrase has no memory.

The vocabulary core therefore has two simultaneous obligations:

1. generate many legal variants;
2. preserve a deterministic phrase identity long enough for repetition and development to be perceptible.

---

# 2. PhraseRhythmIdentity — shared structural memory

## 2.1 Contract

Every generated phrase MUST establish one explicit `PhraseRhythmIdentity` before P-level-specific variation is applied.

The identity represents the stable rhythmic decisions that make P1, P2 and P3 recognizably related.

Typical identity decisions include:

```text
selected RhythmArchetype
structural Kick core
structural Backbeat core
structural BassRhythm core
structural ChordRhythm core
protected-space structure
primary call/response skeleton
selected trajectory identity when already fixed by phrase context
```

The identity MUST NOT contain concrete pitches or SynthEngineType.

The identity MAY be represented compactly. It does not need to be persisted in Scene v1 and it does not need a heap-owning graph.

Conceptual bounded representation:

```cpp
struct PhraseRhythmIdentity {
    RhythmArchetypeId archetypeId;
    uint8_t phraseBars;

    StepMask structuralCore[kMaxPhraseBars][kRhythmRoleCount];
    StepMask canonicalCore[kMaxPhraseBars][kRhythmRoleCount];

    ProtectedSpace protectedSpaces[kMaxProtectedSpaces];
    uint8_t protectedSpaceCount;

    TrajectoryId trajectoryId;
};
```

The exact layout may be smaller; the contract is more important than the shape.

## 2.2 RNG separation

Identity selection and variation MUST use separate deterministic domains.

Normative derivation model:

```text
identitySeed = derive(
    projectGenerationSeed,
    archetypeId,
    phraseOrdinal,
    RhythmIdentityDomain)

p1VariationSeed = derive(identitySeed, P1, P1VariationDomain)
p2VariationSeed = derive(identitySeed, P2, P2VariationDomain)
p3VariationSeed = derive(identitySeed, P3, P3TransformationDomain)
```

`RealizationLevel` MUST NOT be mixed into the seed before shared identity decisions are made.

Changing P1 -> P2 -> P3 MAY change:

```text
ghosts
optional percussion
accent realization
optional displacement
response detail
reduction/fill subset
turnaround detail
```

but MUST NOT independently redraw:

```text
primary structural kick identity
primary backbeat identity
base bass-rhythm identity
protected-space identity
core call/response relation
```

unless the selected `TransformationIntent` explicitly authorizes transformation of a canonical, transformable subset.

## 2.3 Selection continuity

Archetype selection is part of phrase identity.

Normative commands/intent semantics:

```text
VARIATE current Phrase
    MUST reuse current RhythmArchetype and PhraseRhythmIdentity.

NEW Phrase
    MAY select a new RhythmArchetype according to Genre weights.

REINTERPRET / explicit Genre rematerialization
    MAY select another family/archetype.
```

A “give me another variation” action MUST NOT silently roll a new archetype.

---

# 3. Repeat semantics — repetition is memory

`BarFunction::Repeat` MUST NOT mean “generate another arbitrary legal member of the archetype”.

Normative bar-function semantics:

## Statement

Establishes the structural identity for the relevant bar position.

MAY realize legal velocity, timing and articulation detail.

## Repeat

MUST preserve the structural onset core established by the corresponding Statement.

MAY vary only non-structural expression such as:

```text
velocity inside allowed corridor
microtiming from FEEL
non-structural accent expression
```

MUST NOT add/drop/displace structural onsets.

A byte-identical event array is allowed but not required.

## RepeatWithGhosts

MUST preserve the complete Repeat structural onset core.

MAY add only explicitly legal ghost/ornament events inside the ornament budget.

MUST NOT convert ghosts into new structural anchors.

## Response

MUST preserve immutable anchors and the phrase identity.

MAY change only the relationship-defined response subset.

## Reduction

MAY suppress transformable canonical events within the reduction budget.

MUST preserve immutable anchors and protected space.

## Build

MAY increase ornament/secondary density within legal zones.

MUST NOT fill protected space or change immutable anchors.

## Turnaround

MAY transform the explicitly marked turnaround subset, normally near a phrase/bar boundary.

MUST preserve identity outside that subset.

## Break

MAY suppress canonical anchors only where their anchor mutability permits suspension for Break.

MUST preserve immutable anchors.

MAY intentionally create very sparse or silent roles if the archetype's Break policy allows it.

## Return

MUST restore the canonical structural identity after a Break/Reduction/Turnaround unless a new phrase identity is explicitly requested.

---

# 4. Anchor mutability — immutable is not canonical

The old `required` concept is too coarse because a canonical four-floor kick cannot simultaneously be mandatory forever and legally disappear during a real Break.

Core v1 MUST distinguish at least three event classes:

```text
ImmutableAnchor
CanonicalAnchor
OptionalEvent
```

Recommended compact lane representation:

```cpp
enum class AnchorMutability : uint8_t {
    Immutable,
    Canonical,
};

struct LaneGrammar {
    RhythmRole role;

    StepMask immutableAnchors;
    StepMask canonicalAnchors;
    StepMask preferred;
    StepMask optional;
    StepMask forbidden;

    // Coordinate-level rhythmic duration policy over declared onset space.
    // Normal is implicit when a realized onset is in none of these masks.
    StepMask shortGate;
    StepMask heldGate;
    StepMask tieGate;

    StepMask protectedSilence;

    uint8_t structuralMin;
    uint8_t structuralMax;
    uint8_t ornamentMax;

    uint8_t accentProfileId;
    uint8_t flags;
};
```

Semantics:

```text
ImmutableAnchor
    MUST survive P1/P2/P3 and every BarFunction.

CanonicalAnchor
    MUST be present in Statement/P1 unless the archetype explicitly declares otherwise.
    MAY be suppressed/transformed only by a BarFunction + TransformationIntent that permits it.

OptionalEvent
    MAY be selected according to realization budget.

Gate overlays
    shortGate / heldGate / tieGate classify legal onset coordinates.
    MUST be mutually exclusive.
    MUST be subsets of declared legal onset space.
    MUST NOT overlap forbidden/protected silence.
    GateClass::Normal is implicit for every realized onset not covered by an
    explicit gate overlay.
```

An archetype MAY define zero immutable anchors.

Some groove identities are relational rather than tied to one permanent step.

Catalog invariants:

```text
immutableAnchors & canonicalAnchors == 0
immutableAnchors & forbidden == 0
canonicalAnchors & forbidden == 0
immutableAnchors & protectedSilence == 0
canonicalAnchors & protectedSilence == 0
shortGate & heldGate == 0
shortGate & tieGate == 0
heldGate & tieGate == 0
(shortGate | heldGate | tieGate) & ~declaredOnsetSpace == 0
(shortGate | heldGate | tieGate) & (forbidden | protectedSilence) == 0
structuralMin <= structuralMax
popcount(immutableAnchors) <= structuralMax
```

---

# 5. Rhythm authority includes gate/duration and importance

Rhythm Vocabulary MUST remain pitch-free, but pitched rhythmic roles cannot be represented by NoteOn positions alone.

For pitched roles and duration-sensitive percussion, `RhythmPhrasePlan` MUST carry rhythmic duration intent and structural importance.

Minimum semantic vocabulary:

```cpp
enum class GateClass : uint8_t {
    Short,
    Normal,
    Held,
    Tie,
};

enum class EventImportance : uint8_t {
    Structural,
    Secondary,
    Ghost,
};

struct RhythmEventIntent {
    uint8_t step;
    GateClass gate;
    EventImportance importance;
    uint8_t accentClass;
};
```

This is architectural pseudocode. The implementation may encode these fields more compactly.

`GateClass::Normal` is the default/implicit class. A lane grammar therefore does
not need a `normalGate` mask; explicit Short/Held/Tie overlays are sufficient and
avoid duplicating the realized onset mask.

Normative authority:

```text
RhythmPhraseRealizer owns:
    onset
    rhythmic gate/duration intent
    structural importance
    protected rest/tie intent

Pitch generators own:
    pitch selection
    motif/contour development
    harmonic interpretation

Physical binders own:
    engine/voice translation
```

`BassPhraseGenerator` MUST NOT:

```text
create a new onset
remove a structural onset
turn a protected rest into an onset
shorten/extend a rhythmic event in a way that violates its GateClass/Tie contract
```

It MAY choose pitch and pitch articulation that are compatible with the supplied rhythmic intent.

Examples with identical onsets but different legal groove meaning:

```text
Held:     C---    C---    C---    C---
Short:    C.      C.      C.      C.
Tie:      C-------C-----------C-------
```

These MUST NOT collapse to the same `BassRhythmPlan` semantics.

---

# 6. Relationship semantics — normative predicates

Core v1 keeps the five-operation vocabulary:

```cpp
enum class RelationshipOp : uint8_t {
    Exclude,
    Coincide,
    Offset,
    Respond,
    FillGaps,
};
```

The operation names alone are insufficient. Their truth semantics are normative below.

## 6.1 Common coordinate model

A relationship operates over structural phrase coordinates.

For v1:

```text
one bar = 16 structural steps
phrase coordinate = barIndex * 16 + step
```

A `zoneMask` filters step positions inside each participating bar.

`RelationshipScope` controls whether relative windows may cross a bar boundary:

```cpp
enum class RelationshipScope : uint8_t {
    BarLocal,
    Phrase,
};
```

Semantics:

```text
BarLocal:
    offsets outside [0, 15] are clipped/rejected;
    no wrap to the same bar and no implicit modulo arithmetic.

Phrase:
    offsets operate on phrase coordinates and MAY cross into adjacent bars;
    they MUST remain inside the current phrase;
    no wrap from final phrase step to phrase start.
```

There is no implicit circular wrap in v1.

## 6.2 Exclude

Direction:

```text
source = reference occupancy
target = constrained occupancy
```

Hard predicate:

```text
for every qualifying target event in zone:
    target coordinate MUST NOT coincide with source occupancy selected by the relationship.
```

`Exclude` is universal over qualifying target events.

A hard Exclude violation makes the realization invalid.

Soft Exclude reduces candidate weight but does not create a hard prohibition.

## 6.3 Coincide

`Coincide` requires a bounded number of qualifying target events to share coordinates with source occupancy.

Conceptual parameters:

```text
minMatches
maxMatches (0 = no explicit upper bound other than lane budget)
```

Hard predicate:

```text
matchingTargetCount >= minMatches
AND matchingTargetCount <= effectiveMax
```

It does **not** mean every source must be doubled by target unless `minMatches`/lane constraints make that explicit.

## 6.4 Offset

Default Offset semantics are target-oriented:

```text
each qualifying target event requires at least one qualifying source event
inside [target - maxOffset, target - minOffset]
```

For a positive `+1..+2` musical offset represented as source -> target:

```text
target occurs 1..2 structural steps after a source.
```

The relationship MUST define the signed interval unambiguously in code/docs.

Scope controls cross-bar behavior.

A hard Offset does **not** require every source event to create a target event.

If source-oriented cardinality is needed, use `Respond` instead of changing Offset semantics.

## 6.5 Respond

`Respond` is source-oriented.

Each qualifying source occurrence opens one response window.

The relationship defines:

```text
minResponsesPerWindow
maxResponsesPerWindow
minOffset
maxOffset
scope
```

Hard predicate:

```text
for every qualifying source occurrence:
    responseCount(window) is within the declared bounds
```

If `minResponsesPerWindow == 0`, a response is optional and the relation normally acts as a weighted structural preference.

Overlapping response windows MUST use deterministic ownership/scoring so one target event is not accidentally counted twice where the contract forbids it.

Default v1 rule:

```text
one target event may satisfy at most one hard Respond window;
choose nearest eligible source, then stable source order on ties.
```

## 6.6 FillGaps

`FillGaps` is a candidate-weighting relationship, not a command to occupy every empty subdivision.

Semantics:

```text
target candidate weight increases in legal space left unoccupied
by the selected source occupancy.
```

It MUST still obey:

```text
forbidden zones
protected space
structural/ornament density budgets
hard relationships
lane max counts
```

Hard `FillGaps` SHOULD NOT be used in Core v1. If the catalog needs a mandatory event count, express it through lane/density minimums plus legal candidate space.

## 6.7 Relationship priority

Normative resolver priority:

```text
1. validate request/catalog
2. instantiate immutable anchors
3. establish PhraseRhythmIdentity and canonical anchors
4. apply forbidden masks
5. apply role-scoped protected spaces
6. derive legal space from hard relationships
7. apply BarFunction + TransformationIntent permissions
8. satisfy musical structural minimums
9. satisfy preferred structural density where possible
10. optimize soft relationships
11. add ornaments/ghosts inside ornament budget
12. apply non-structural accent/gate expression
13. final hard validation
14. transactional materialization
```

Density never outranks hard legality.

---

# 7. Protected space is role-scoped

A single `globalProtectedSilence` mask is too coarse for musical use.

Core v1 MUST use role-scoped protected space:

```cpp
using RhythmRoleMask = uint16_t;

struct ProtectedSpace {
    StepMask steps;
    RhythmRoleMask affectedRoles;
};
```

Examples:

```text
true global silence:
    affectedRoles = ALL

Dub space:
    affectedRoles = Kick | BassRhythm | ChordRhythm
    hats/percussion may continue

UKG gap:
    affectedRoles = Kick | BassRhythm
    shuffled hats may remain active
```

Protected space outranks:

```text
preferred density
ornament density
fills
soft relationships
P2 variation
P3 transformation
```

A BarFunction may suppress events to create more space, but it MUST NOT fill an existing protected space unless a different phrase identity/archetype is explicitly selected.

---

# 8. P-level magnitude is separate from TransformationIntent

`RealizationLevel` answers:

```text
How far may this realization deviate from the shared identity?
```

It MUST NOT also encode:

```text
What musical purpose should the deviation serve?
```

Core contract:

```cpp
enum class RealizationLevel : uint8_t {
    P1Canonical,
    P2Variation,
    P3Transformation,
};

enum class TransformationIntent : uint8_t {
    Auto,
    Fill,
    Reduce,
    Break,
    Build,
    Turnaround,
    Response,
};
```

Normative meaning:

```text
P1
    canonical realization of PhraseRhythmIdentity.

P2
    bounded variation of the same identity.

P3
    wider transformation budget of the same identity.

TransformationIntent
    selects the musical direction of allowed transformation.
```

Examples:

```text
P3 + Break
P3 + Turnaround
P2 + Response
P2 + Reduce
```

`Auto` may choose an intent from the archetype/trajectory legal set using a dedicated deterministic domain.

A UI does not need to expose `TransformationIntent` in Core v1. The contract exists now so P3 does not become an uncontrolled surprise bucket.

---

# 9. FEEL owns timing interpretation

Groove Vocabulary MUST NOT become a second FEEL system.

The old `SwingContract` wording is refined as follows:

```text
RhythmArchetype owns timing eligibility/compatibility.
FEEL owns actual timing interpretation and amount.
Genre does not own final swing timing.
```

Recommended minimal contract:

```cpp
enum class TimingCompatibility : uint8_t {
    StraightOnly,
    SwingCompatible,
    ShufflePreferred,
};

struct TimingEligibility {
    TimingCompatibility compatibility;
    StepMask sensitiveSteps;
    RhythmRoleMask affectedRoles;
};
```

The archetype MAY say:

```text
these roles/steps are swing-sensitive
this structure is straight-only
this structure is shuffle-compatible/preferred
```

The archetype MUST NOT persist or continuously project a separate swing percentage that competes with `FeelSettings`.

If implementation needs a safety corridor, it is a compatibility clamp around FEEL, not a second owner of feel amount.

---

# 10. Core v1 grid is explicitly 4/4 × 16 structural steps

Groove Vocabulary Core v1 supports:

```text
meter: 4/4 only
structural grid: 16 steps per bar
phrase length: 1..4 bars
```

Do not claim arbitrary meter support in v1.

`meterNumerator` / `meterDenominator` SHOULD be removed from the first runtime `RhythmArchetype` structure unless they are required for validation metadata.

If retained, v1 catalog validation MUST require:

```text
meterNumerator == 4
meterDenominator == 4
```

Future 6/8, 3/4, triplet structural grids or variable-resolution vocabularies require an explicit architecture extension.

Swing/microtiming does not change the 16-step structural coordinate grid.

---

# 11. Cross-bar relationships

BarEvolution cannot be musically first-class while all relationships are implicitly one-bar.

Core v1 MUST define boundary behavior now.

Use `RelationshipScope::Phrase` when relationships are allowed to cross bar boundaries.

Examples:

```text
Bass pickup at bar N step 15
    -> Kick at bar N+1 step 0

Fill at final two steps of bar N
    -> protected role-scoped space at bar N+1 step 0
```

No 64-bit “one mask for the whole phrase” structure is required.

Phrase coordinates are computed transiently:

```text
absoluteStep = barIndex * 16 + step
```

Hard rules:

```text
no modulo wrap
no crossing phrase start/end
scope must be explicit
same seed/context -> same boundary resolution
```

---

# 12. Density distinguishes structure from ornament

A single `minTotalHits/preferredTotalHits/maxTotalHits` encourages false intensity by adding hats/ghosts.

Core v1 MUST at least distinguish structural and ornament density.

Recommended compact contract:

```cpp
struct DensityContract {
    uint8_t structuralMin;
    uint8_t structuralPreferred;
    uint8_t structuralMax;
    uint8_t ornamentMax;
};
```

`LaneGrammar` may additionally define per-role structural min/max.

Definitions:

```text
Structural event
    contributes to groove identity and musical viability.

Secondary event
    may contribute to structure depending on lane/archetype.

Ghost/ornament event
    cannot satisfy a structural minimum unless explicitly promoted by the archetype.
```

P2/P3 MUST NOT satisfy a structural-density target by adding only ornament events.

---

# 13. ValidButSparse vs invalid musical minimum

`ValidButSparse` means:

```text
all hard musical minimums are satisfied,
but preferred density could not be reached.
```

It MUST NOT mean:

```text
a required musical role/minimum was lost but the output is still technically legal.
```

Normative statuses:

```cpp
enum class RealizationStatus : uint8_t {
    Ok,
    ValidButSparse,
    InvalidConstraintSet,
};
```

Rules:

```text
below preferred target but >= all structural minimums
    -> ValidButSparse is allowed.

below any immutable/canonical requirement that is active for the BarFunction
below any role structuralMin
below any hard relationship cardinality
    -> InvalidConstraintSet.
```

Example:

```text
Backbeat structuralMin = 2
hard constraints leave only 1 legal backbeat event

=> InvalidConstraintSet
```

not `ValidButSparse`.

---

# 14. Physical binding MUST preserve the realized groove

`RhythmRole::Backbeat` is a useful abstraction only if the physical binder does not reintroduce independent randomness.

Every realized event carries `EventImportance`.

Conceptual binding:

```text
Structural Backbeat
    -> main Snare
       + optional simultaneous Clap layer allowed by binder profile

Secondary Backbeat
    -> Snare or Rim according to deterministic profile

Ghost Backbeat
    -> low-velocity Snare/Rim according to deterministic profile
```

Hard binding contract:

```text
Physical binding MUST NOT invent a new rhythmic onset.
Physical binding MUST NOT delete a Structural onset.
Physical binding MUST NOT move an onset.
Physical binding MUST preserve protected space.
Physical binding MAY layer physical voices on the same realized onset.
Physical binding MAY choose sound/voice according to deterministic role-binding policy.
```

This applies to Snare/Clap/Rim and later to other abstract role bindings.

The binder is downstream of groove realization. It cannot “improve” topology.

---

# 15. P1 / P2 / P3 refined normative contract

## P1 — canonical

P1 MUST:

```text
establish/reuse PhraseRhythmIdentity
preserve immutable anchors
instantiate active canonical anchors
preserve all protected spaces
satisfy hard relationships
satisfy musical structural minimums
use canonical BarFunction behavior
```

P1 MAY vary only within low-budget legal optional/ornament/expression decisions.

## P2 — variation

P2 MUST reuse the same `PhraseRhythmIdentity`.

P2 MAY:

```text
add/remove bounded ghosts
select alternate optional events
apply limited optional displacement
vary accents/gates inside contract
realize bounded response/reduction behavior
```

P2 MUST NOT redraw the structural core independently.

## P3 — transformation

P3 MUST reuse the same `PhraseRhythmIdentity` and receive a `TransformationIntent`.

P3 MAY transform only subsets explicitly marked transformable by:

```text
anchor mutability
BarFunction
MutationPolicy
TransformationIntent
```

P3 MAY temporarily suppress canonical anchors during legal Break/Reduction behavior.

P3 MUST preserve immutable anchors and all hard constraints that remain active for the selected intent.

## Fresh realization clarified

The phrase:

```text
fresh legal realization
```

means:

```text
do not mutate an already-materialized previous event array as the generator source of truth.
```

It does **not** mean:

```text
redraw PhraseRhythmIdentity from unrelated random decisions.
```

---

# 16. Revised determinism model

Recommended domains:

```text
ArchetypeSelection
RhythmIdentity
P1Variation
P2Variation
P3Transformation
TransformationIntent
DrumsOrnament
BassPitch
ChordPitch
LeadPitch
FeelExpression
PhysicalBinding
BarEvolution
```

Normative derivation hierarchy:

```text
project seed
    ↓
phrase/archetype identity seed
    ↓
PhraseRhythmIdentity
    ├── P1 variation domain
    ├── P2 variation domain
    ├── P3 transformation domain
    ├── drums ornament domain
    ├── bass pitch domain
    └── physical binding domain
```

Tests MUST prove:

```text
P1/P2/P3 share identity fingerprint
changing BassPitch does not alter drums/rhythm identity
changing PhysicalBinding does not alter role-level onset topology
changing P-level does not reroll archetype
Repeat does not reroll structural core
```

---

# 17. Revised normalized fingerprint

The normalized groove fingerprint SHOULD now include rhythmic duration/importance and distinguish identity from ornament.

Tier 1 identity fingerprint:

```text
archetype id/family for diagnostics only
phrase bar count
BarFunction sequence
per-role Structural onset masks
per-role Canonical/Immutable anchor masks
role-scoped ProtectedSpace masks
major structural accents
coarse GateClass signature for pitched/duration-sensitive roles
```

Tier 2 relational fingerprint:

```text
Kick <-> Backbeat lag signature
Kick <-> BassRhythm lag signature
Backbeat <-> Hat lag signature
Kick <-> ChordRhythm lag signature
hard relationship satisfaction signature
```

Tier 3 ornament fingerprint, excluded from phrase-identity equality by default:

```text
ghosts
minor percussion
small velocity differences
minor microtiming
physical Backbeat binding choice
```

For the same phrase:

```text
identityFingerprint(P1) == identityFingerprint(P2)
```

is the normal expectation unless P2 explicitly transforms a canonical subset that the fingerprint marks as variant while preserving a separate immutable/core fingerprint.

For P3:

```text
immutableCoreFingerprint(P3) == immutableCoreFingerprint(P1)
```

MUST hold.

A transformation-specific distance is measured on the transformable subset.

---

# 18. Stage gates revised

## Stage 1 — data model/catalog validation

Stage 1 MAY begin after the model includes or explicitly reserves the following contracts:

```text
PhraseRhythmIdentity
immutable vs canonical anchors
GateClass
EventImportance
ProtectedSpace + RhythmRoleMask
RelationshipScope
TransformationIntent
structural vs ornament density
4/4 × 16-step v1 limitation
```

Stage 1 still has no production generation behavior.

## Stage 2 — resolver/realizer

Stage 2 MUST NOT begin until all of the following are accepted as normative:

```text
shared phrase identity seed hierarchy
Repeat semantics
anchor mutability
rhythmic duration authority
exact relationship predicates
cross-bar policy
role-scoped protected space
TransformationIntent separation
ValidButSparse musical-minimum rule
binding preservation rule
```

Stage 2 acceptance MUST additionally prove:

```text
identity continuity violations == 0
repeat structural drift violations == 0
immutable anchor violations == 0
canonical-anchor illegal-suspension violations == 0
protected-space violations == 0
hard relationship violations == 0
binding-onset invention violations == 0
binding structural-drop violations == 0
```

---

# 19. Normative invariants G-21…G-25

These extend G-01…G-20 from the audited architecture brief.

## G-21 — Phrase identity continuity

```text
P1/P2/P3 of the same Phrase MUST share one deterministic PhraseRhythmIdentity.

Higher P-levels MAY transform explicitly transformable subsets,
but MUST NOT independently redraw the structural core.
```

## G-22 — Repeat stability

```text
BarFunction::Repeat MUST preserve structural onset identity.

Repeat MAY vary non-structural expression,
but MUST NOT regenerate a different structural groove merely because it is legal.
```

## G-23 — Transformable anchors

```text
Anchor mutability MUST be explicit.

Immutable anchors survive every legal transformation.
Canonical anchors define Statement identity but MAY be suspended only by explicitly legal
Break/Reduction/Turnaround behavior.
```

## G-24 — Rhythm duration authority

```text
RhythmPhrasePlan MUST own rhythmic onset plus gate/duration intent
and event structural importance for duration-sensitive roles.

Pitch generators MUST NOT rewrite that rhythmic authority.
```

## G-25 — Binding preservation

```text
Physical role binding MUST preserve realized rhythmic topology.

It MUST NOT invent, remove or move structural onsets.
It MAY only choose/layer compatible physical voices at already-realized coordinates.
```

---

# 20. Additional normative clarifications

These are required even though they do not receive separate G-numbers in Core v1.

## Relationship truth semantics

Every relationship operator MUST have documented:

```text
direction
quantifier/cardinality
scope
cross-bar behavior
window semantics
hard/soft interpretation
stable tie-breaking
```

No implementation-defined musical semantics are allowed.

## Scoped silence

Protected space MUST carry an affected-role mask.

A single absolute global silence mask is insufficient as the only model.

## P3 intent

P3 transformation magnitude MUST be separate from `TransformationIntent`.

## FEEL ownership

Archetype controls timing eligibility/compatibility only; FEEL owns actual timing interpretation.

## Grid truth

Core v1 is exactly 4/4 on a 16-step structural grid, 1–4 bars.

## Selection continuity

Variation of an existing phrase reuses its archetype/identity. New/reinterpret actions may select a new archetype.

## Musical minimum

`ValidButSparse` is legal only above all mandatory structural minimums.

---

# 21. Updated Groove Vocabulary Core v1 Definition of Done

The Core v1 milestone is not complete until all of the following are true:

```text
[ ] <= 8 justified initial RhythmFamily categories
[ ] ~19–20 curated strong-material archetypes
[ ] Groove Vocabulary Core v1 explicitly limited to 4/4 + 16-step structural grid

[ ] LaneGrammar separates immutable anchors, canonical anchors, preferred/optional/forbidden space
[ ] role-scoped ProtectedSpace exists
[ ] structural and ornament density are separate

[ ] five-operation relationship vocabulary remains minimal
[ ] every RelationshipOp has normative predicate semantics
[ ] relationship direction/quantifier/cardinality are explicit
[ ] relationship BarLocal/Phrase scope is explicit
[ ] no implicit modulo/wrap behavior exists

[ ] PhraseRhythmIdentity exists explicitly
[ ] identity RNG is separated from P1/P2/P3 variation RNG
[ ] P1/P2/P3 share one phrase identity
[ ] VARIATE reuses current archetype/identity

[ ] Repeat preserves structural identity
[ ] RepeatWithGhosts preserves core and changes only bounded ornament
[ ] Break/Reduction/Turnaround use explicit anchor mutability
[ ] Return restores canonical identity

[ ] GateClass or equivalent rhythmic duration intent exists
[ ] EventImportance or equivalent structural/secondary/ghost semantic exists
[ ] BassPhraseGenerator cannot create onsets outside BassRhythmPlan
[ ] Pitch generators cannot rewrite rhythmic gate/tie authority

[ ] RealizationLevel and TransformationIntent are separate concepts
[ ] FEEL owns actual timing interpretation
[ ] archetypes expose timing compatibility/eligibility only

[ ] ValidButSparse cannot violate structural musical minimums
[ ] physical binding cannot invent new onsets
[ ] physical binding cannot delete/move structural onsets
[ ] Backbeat binding preserves Structural/Secondary/Ghost semantics

[ ] explicit deterministic domains
[ ] zero heap during realization
[ ] < 1 KB transient realization state target remains plausible
[ ] Rhythm realization does not run in the audio path

[ ] Pattern materialization remains transactional
[ ] no Scene codec change in Groove Vocabulary Core
[ ] no Song/page/bank/pattern/PhraseCore allocation ownership in the realizer
[ ] no VoiceRole persistence migration in the core milestone

[ ] host catalog validation
[ ] relationship unit tests
[ ] phrase identity property tests
[ ] repeat stability property tests
[ ] cross-bar boundary tests
[ ] normalized identity + relational fingerprint metrics
[ ] strong-genre shadow comparison
[ ] listening regression for Techno/Rave/Acid/Dub/D&B
```

---

# 22. Core v1 acceptance examples

## UK 2-Step P1/P2/P3

Generate one phrase identity and all three realization levels.

EXPECT:

```text
same selected archetype
same immutable/core identity fingerprint
backbeat identity preserved
protected-role spaces preserved
P2 differs through bounded variation only
P3 differs only according to selected legal TransformationIntent
forbidden four-floor collapse == 0
hard relationship violations == 0
```

## FourFloor Break

Statement:

```text
Kick: X---X---X---X---
```

If quarter-note kicks are canonical but not immutable, a legal Break MAY produce a reduced/silent canonical-kick subset according to Break policy.

EXPECT:

```text
Break can suppress canonical anchors explicitly marked transformable
immutable anchors, if any, remain
Return restores canonical floor identity
```

A Break is invalid if it silently suppresses an immutable anchor.

## Dub protected space

Protected space affects:

```text
Kick | BassRhythm | ChordRhythm
```

EXPECT:

```text
those roles remain silent in the zone
ClosedHat/Percussion may continue if otherwise legal
P3 Fill cannot occupy the protected roles
```

## D&B pickup across bar boundary

Relationship scope is `Phrase`.

EXPECT:

```text
bar N step 15 may satisfy/respond to bar N+1 step 0 relationship
no wrap from final phrase step to phrase start
same context -> same resolution
```

## Backbeat binding

Realizer emits:

```text
Backbeat @ step 4, Structural
```

Binder MAY produce:

```text
Snare @ step 4
Snare + Clap @ step 4
```

Binder MUST NOT produce:

```text
Snare @ step 5
Clap @ step 7
no physical event at step 4
```

---

# 23. Final architecture state after this review

The intended pipeline is now:

```text
Genre
    ↓ selects/weights/constrains
RhythmArchetype
    ↓
PhraseRhythmIdentity
    + BarEvolution
    + RealizationLevel
    + TransformationIntent
    ↓
RhythmPhraseRealizer
    ↓
RhythmPhrasePlan
    │
    ├── structural/secondary/ghost event intent
    ├── role-scoped protected space
    ├── gate/tie intent for duration-sensitive roles
    └── cross-role relationships already resolved
    ↓
role-specific pitch generators
    ↓
physical role binding
    ↓
transactional PatternMaterializer
    ↓
existing Scene patterns
```

For a phrase:

```text
RhythmArchetype
    ↓
PhraseRhythmIdentity
    │
    ├── P1 Statement     -> A
    ├── P2 Variation     -> A'
    └── P3 + Intent      -> A'' / Fill / Break / Build / Turnaround
```

For live bass:

```text
PerformanceKeyboard
    ↓
BassPerformancePolicy
    ↓
ArticulationResolver
    ↓
Synth engine
```

Live performance remains outside `RhythmPhraseRealizer` and `BassPhraseGenerator`.

---

# 24. Freeze decision

After this companion contract is accepted:

```text
Stage 1 data model/catalog work:
    READY

Stage 2 RhythmPhraseRealizer implementation:
    READY only when Stage 1 types encode these contracts
    and the Stage 2 gate tests are specified.

Architecture/contracts:
    MAY then be marked Groove Vocabulary Core v1 frozen.
```

Until then, `GROOVE_VOCABULARY_ARCHITECTURE_BRIEF.md` remains an audited design brief rather than a frozen implementation contract.
