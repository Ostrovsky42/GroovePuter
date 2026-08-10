# GroovePuter Generation Roadmap — Stages 7–13

Status: design / planning branch

Base: PR #195 head `704f6976f74a7d227e4d38f8a86a487f35a37506`

This document defines the next cross-genre generation stages after the Groove Vocabulary Stage 1–6.1 foundation and Stage 7 hardware curation work.

The roadmap is intentionally **not Lo-Fi-specific**. Lo-Fi / slow groove is used as an early stress case because it exposes weaknesses that also matter to House, Techno, Electro, Hip-Hop, Breaks, Dub, DnB, Trip-Hop, Funk/Soul and other directions.

No production code is introduced by this branch.

---

## 1. Architectural premise

The generator should not model a genre as one hardcoded pattern recipe.

Target composition model:

```text
Genre / Variant
      |
      +-- Rhythm archetype
      +-- Feel / timing law
      +-- Bass role + bass rhythm vocabulary
      +-- Chord / harmonic rhythm vocabulary
      +-- Melodic rhythm / motif vocabulary
      +-- Phrase evolution law
      +-- optional articulation / timbre policy
```

The layers must remain separable:

```text
GENRE != FEEL != RHYTHM != BASS != CHORD != MELODY != PHRASE EVOLUTION
```

A genre selects and weights compatible vocabulary. It does not own literal note masks for every layer.

The same vocabulary entry may be valid for several genres when the musical evidence supports that reuse.

---

## 2. Why slow Lo-Fi belongs in this roadmap

A low-BPM genre is not difficult because of tempo itself. It is difficult because musical identity increasingly depends on:

- silence and protected space;
- role-relative timing;
- long note durations;
- sparse bass movement;
- harmonic rhythm;
- melodic restraint;
- multi-bar repetition and delayed response.

A naive implementation such as `LoFi = existing pattern at 76 BPM` is not acceptable.

Lo-Fi therefore acts as a useful falsification target for the generic architecture. If the architecture can produce convincing slow pocket without special-case pattern code, the same primitives should improve many existing genres.

---

# Stage 7 — Production Rhythm Vocabulary Completion and Reachability

Stage 7 owns the transition from researched / auditioned rhythm identities to normal user-reachable production generation.

## Stage 7A / 7B — completed audition work

Already established:

- temporary hardware audition path;
- deterministic seed navigation;
- P1/P2/P3 listening;
- Atlas-derived candidate falsification;
- safe backup / restore;
- candidate distinction decisions.

## Stage 7C — production routing and UI reachability

Purpose:

Make admitted `ReferenceVocabulary` entries reachable through normal generation without retaining the temporary audition UI.

Required contracts:

1. Every production rhythm archetype has explicit compatibility metadata.
2. `AUTO` selects only compatible archetypes for the active Genre / Variant.
3. Manual rhythm selection fixes the archetype identity while seed and P-level change realization.
4. Genre changes may normalize an incompatible manual rhythm back to `AUTO` or another valid selection using one deterministic policy.
5. Existing Scene compatibility is preserved.
6. Audition-only state does not enter Scene persistence.

Minimal intended GENRE UI model:

```text
GENRE    ELECTRO
VARIANT  MIAMI BASS
RHYTHM   AUTO
MORPH    0
APPLY    MATERIALIZE
```

or:

```text
RHYTHM   electro_backskip
```

Stage 7C is complete only when new production rhythm entries are audible through the ordinary workflow.

### Stage 7 acceptance

- all admitted Stage 7 grammars are reachable in normal generation;
- manual archetype selection is deterministic;
- `AUTO` never selects an incompatible archetype;
- P1/P2/P3 preserve identity;
- no existing genre loses its valid generation path;
- hardware smoke confirms at least one manual and one AUTO path per affected genre family.

---

# Stage 8 — Feel and Slow-Groove Temporal Model

Purpose:

Make timing feel independent from BPM and from rhythm topology.

This is a cross-genre capability, not a Lo-Fi mode.

## Required primitives

### 8.1 Role-relative timing

Represent bounded timing tendencies by role, for example:

```text
Kick        centered / slight early
Backbeat    centered / slight late
ClosedHat   swing corridor
BassRhythm  centered / slight late
ChordRhythm centered / late
Melodic     wider expressive corridor
```

Do not encode timing as unbounded random jitter.

### 8.2 Timing laws

Initial generic laws should include at least:

```text
Straight
SwingCompatible
LaidBack
DrunkenControlled
PushPull
```

Names are provisional until implementation review.

### 8.3 Tempo independence

The same rhythm archetype may run at different BPM values without silently changing identity. Timing offsets must be represented in a form that remains musically bounded across the supported tempo range.

### 8.4 Long-duration and silence support contract

Stage 8 defines the timing/duration requirements needed by later chord and melody stages:

- notes may intentionally span steps / bars;
- rests are first-class decisions;
- protected space may apply across roles;
- slow BPM must not force denser note generation.

### Stage 8 falsification genres

Use at least:

- Lo-Fi / Chill-Hop;
- Trip-Hop;
- Dub;
- House;
- fast Breaks / DnB control.

The stage fails if slow feel only works by special-casing Lo-Fi or if fast genres regress.

---

# Stage 9 — Bass Rhythm Vocabulary v2

Purpose:

Separate bass rhythmic identity from bass pitch contour and synth articulation.

Target model:

```text
Bass = BassRhythm role
     + PitchContour role
     + Articulation policy
```

Do not define Acid as one bass pattern. Acid may combine several rhythm frames with compatible contour and articulation behavior.

## Initial BassRhythm families

The exact catalog is evidence-driven, but the model must support identities such as:

```text
RootPulse
KickLock
KickAnswer
GapFill
OffbeatPush
SparseAnchor
RollingDrive
HalfTimePocket
SyncopatedHook
SustainAndDrop
```

These are capability examples, not automatically approved production names.

## Relationship vocabulary

Reuse the constrained relationship model where possible:

```text
Coincide
Offset
Respond
FillGaps
Exclude
```

Bass must be able to relate to Kick without being generated as a copy of Kick.

## Bass acceptance

- same drum rhythm can produce multiple valid bass roles;
- same bass role can operate across multiple compatible rhythm archetypes;
- pitch generation can change without changing BassRhythm identity;
- Acid articulation does not own rhythm topology;
- sparse slow bass can leave entire bars or large regions empty when the phrase law permits it.

---

# Stage 10 — Chord and Harmonic Rhythm Vocabulary

Purpose:

Make chord placement and chord duration independent from chord voicing / harmony choice.

Target separation:

```text
Harmony progression
       !=
ChordRhythm
       !=
Voicing / timbre
```

## Required chord-rhythm capabilities

At minimum the architecture must express:

```text
HeldPad
WholeBarHold
HalfBarChange
OffbeatStab
BackbeatStab
AnticipatedChange
SparseChordReply
DubChordSpace
SyncopatedComp
```

Again, these are design examples until evidence and audition promote them.

## Duration is structural

For slow genres, duration cannot be inferred only from NoteOn density.

The chord layer must support:

- held notes across multiple steps;
- held notes across a bar boundary when legal;
- explicit release points;
- intentional empty bars;
- anticipations before harmonic boundaries;
- protected space around bass / melody events.

## Harmonic rhythm acceptance

- changing progression does not destroy ChordRhythm identity;
- changing ChordRhythm does not require rewriting the harmony engine;
- long held material survives Save / Load and Song / Phrase workflows;
- Dub, House and Lo-Fi can share harmonic primitives without sounding identical because their routing / feel / role relationships differ.

---

# Stage 11 — Melodic Rhythm and Motif Vocabulary

Purpose:

Separate **when a melody speaks** from **which pitches it chooses**.

Target model:

```text
MelodicRhythm
    + MotifShape
    + PitchPolicy
    + RegisterPolicy
    + Articulation
```

## Required melodic-rhythm capabilities

The architecture must support at least the behavioral space represented by:

```text
SparseCall
DelayedAnswer
TwoNoteHook
PickupPhrase
LongTone
RestHeavy
BarEndResponse
SyncopatedMotif
DriftPhrase
RepeatedCell
```

These are capability labels, not guaranteed final vocabulary names.

## Silence invariant

A valid melodic phrase may contain:

```text
0 notes in a bar
1 note in a bar
2–3 notes in a bar
```

when allowed by the selected genre / phrase contract.

Generation must not treat emptiness as failure and refill it automatically.

## Motif identity

P1/P2/P3 transformations must preserve recognizable motif identity when a motif is active. Variation may alter timing, octave, ornament or response placement only within the active contract.

---

# Stage 12 — Multi-Bar Phrase Evolution

Purpose:

Move beyond independent one-bar regeneration and make repetition, memory and delayed change explicit.

This stage should integrate with the existing Phrase Core rather than create a second phrase owner.

## Phrase lengths

Required first-class lengths:

```text
1 bar
2 bars
4 bars
8 bars
```

## Phrase-level state

The generator must be able to preserve:

- rhythm identity;
- bass role;
- motif identity;
- harmonic rhythm;
- protected space;
- variation history.

## Evolution operations

Examples of generic phrase operations:

```text
Repeat
MicroVary
Answer
Thin
Fill
Break
Restore
Cadence
DelayEntry
DropRole
```

No operation may silently replace the active identity unless the contract explicitly requests a new phrase.

## P-level interpretation

Recommended direction:

```text
P1 = stable phrase identity
P2 = bounded local / bar variation
P3 = phrase-aware fill, break, empty or ending behavior
```

P3 should not simply mean "more random notes".

## Slow-groove stress case

A valid 4-bar slow phrase may deliberately look like:

```text
Bar 1  motif speaks
Bar 2  motif rests
Bar 3  motif answers
Bar 4  sparse cadence
```

The same architecture must also support high-energy four-bar evolution for House, Techno, Breaks and DnB.

---

# Stage 13 — Genre / Variant Composition Matrix and Production Curation

Purpose:

Turn the independent vocabularies into a coherent, user-visible generator across the full genre catalog.

Stage 13 does **not** create one bespoke generator per genre.

It defines compatibility and weighting between already-tested primitives.

## Composition matrix

Each Genre / Variant should declare compatible and weighted choices for:

```text
Rhythm archetypes
Feel laws
BassRhythm roles
ChordRhythm roles
MelodicRhythm roles
Phrase evolution laws
BPM range / suggested BPM
Grid constraints
Density corridors
Articulation policies where relevant
```

## Genre expansion

The current nine top-level modes are not treated as a permanent limit.

Candidates for explicit top-level directions should be evaluated by musical usefulness and UI clarity, including at least:

```text
Acid
House
Techno
Electro
Rave
Breaks
UK Garage
Drum & Bass
Hip-Hop
Funk / Soul
Dub / Reggae
Trip-Hop
Lo-Fi / Chill-Hop
Synthwave / Outrun
Darksynth
Chip
```

This list is a design target for curation, **not a frozen enum assignment**.

Variants should carry narrower stylistic lenses where possible, for example:

```text
Chicago Jack
Rolling Acid
Detroit
Minimal Techno
Miami Bass
Go-Go
Classic 2-Step
Dark Skippy
Footwork
Dub Techno
Deep Chord
Minimal Space
Classic Chill
Dusty Jazz
Drunken Groove
```

Do not promote every variant into a top-level Genre.

## UI target

Keep the existing compact GENRE workflow. Avoid a new large configuration screen.

Preferred model:

```text
GENRE    LO-FI
VARIANT  DUSTY JAZZ
RHYTHM   AUTO
FEEL     LAID BACK
APPLY    MATERIALIZE
```

Detailed manual vocabulary selection may reuse existing navigation patterns rather than creating a new workspace unless hardware testing proves that necessary.

## AUTO contract

AUTO must be reproducible from the same project seed and phrase context.

AUTO selects from compatible weighted vocabulary; it must not bypass the vocabulary system with legacy hardcoded generation.

## Manual contract

A manually selected vocabulary identity remains fixed until the user changes it or selects AUTO.

---

# Cross-stage invariants

The following apply to Stages 7–13.

## Ownership

1. Scene remains the persisted owner of user-visible generation choices.
2. Phrase Core remains the phrase-level owner where phrase state is required.
3. Vocabulary catalogs are immutable definitions, not runtime owners.
4. Genre maps / weights vocabulary; Genre does not duplicate vocabulary definitions.
5. Atlas remains research evidence, not production source-pattern storage.

## Determinism

6. Same seed + same explicit generation context must reproduce the same result.
7. Different RNG domains must not accidentally perturb unrelated layers.
8. Adding a new vocabulary entry must not change existing deterministic corpora unless a routing weight intentionally changes and the affected test explicitly records that contract change.

## Musical identity

9. P2/P3 may vary a phrase without silently replacing its P1 identity.
10. Empty space is valid musical material.
11. Role relationships are explicit and bounded.
12. Genre labels alone are insufficient evidence for a new vocabulary identity.
13. Similar candidates must be consolidated rather than inflating vocabulary count.

## Runtime

14. No heap allocation in realtime audio / MIDI critical paths.
15. No per-sample generation logic.
16. Regeneration remains outside critical MIDI realtime handling.
17. Cardputer ADV DRAM gate remains mandatory.
18. SEQTRAK MIDI-only build remains mandatory.

## UI / persistence

19. Existing Scene documents remain decodable.
20. New persisted fields require explicit neutral defaults and legacy decode behavior.
21. Manual selection and AUTO selection must be distinguishable in saved state if manual selection becomes persistent.
22. No temporary audition state may be persisted.

---

# Validation strategy

Each implementation stage must have three layers of validation.

## A. Structural host tests

Examples:

- definition validity;
- fingerprint uniqueness;
- compatibility matrix validity;
- deterministic generation;
- density / role bounds;
- relationship invariants;
- P1/P2/P3 identity preservation;
- source checks preventing ownership leakage.

## B. Cross-genre corpus tests

Every generic primitive must be tested against at least one genre that motivated it and at least one contrasting control genre.

Examples:

```text
LaidBack feel:
  Lo-Fi       target
  Trip-Hop    compatible
  House       control
  DnB         negative / restricted control

Sparse bass:
  Lo-Fi       target
  Dub         compatible
  Acid        constrained control

Long chord hold:
  Lo-Fi       target
  Dub         compatible
  House       alternate use
```

## C. Hardware listening

Anything promoted as a new musical identity must pass Cardputer hardware audition before production curation.

Listening must distinguish:

```text
GOOD
GREAT
RANDOM
DEAD
BUSY
COLLAPSE
SAME-AS-EXISTING
```

`SAME-AS-EXISTING` is a successful falsification result, not a failed test process.

---

# Three-review completion rule

A stage is not complete immediately after tests pass.

After the implementation appears complete, perform three consecutive control reviews on one unchanged SHA with increasing breadth:

1. focused code / contract review;
2. cross-layer architecture / regression review;
3. full relevant CI / embedded / musical-gate review.

If any review finds a real issue:

```text
review counter -> 0/3
fix issue
freeze new SHA
restart three reviews
```

Only `3/3 CLEAN` on one unchanged SHA qualifies the stage for completion reporting.

---

# Proposed implementation order

Do not attempt Stages 8–13 in one PR.

Recommended dependency order:

```text
7C  Production rhythm routing + UI reachability
 |
 v
8   Feel / slow-groove temporal primitives
 |
 v
9   BassRhythm vocabulary v2
 |
 v
10  Chord / harmonic rhythm vocabulary
 |
 v
11  Melodic rhythm / motif vocabulary
 |
 v
12  Multi-bar phrase evolution
 |
 v
13  Full Genre / Variant composition matrix + production curation
```

Parallel research / audition branches are allowed, but production ownership changes should respect this dependency chain.

---

# First practical target: Lo-Fi without Lo-Fi special casing

After Stage 8–12 primitives exist, a Lo-Fi family should be expressible only through composition and weighting, for example:

```text
Genre: Lo-Fi / Chill-Hop
BPM: 70–88

Feel:
  LaidBack high weight
  DrunkenControlled medium weight
  Straight low weight

Rhythm:
  sparse / boom-bap / half-time compatible archetypes

BassRhythm:
  SparseAnchor
  KickAnswer
  GapFill

ChordRhythm:
  HeldPad
  WholeBarHold
  SparseChordReply

MelodicRhythm:
  RestHeavy
  DelayedAnswer
  TwoNoteHook

Phrase evolution:
  4-bar preferred
  repetition > replacement
  intentional empty bars allowed
```

If Lo-Fi requires a separate hardcoded generator to sound convincing, the generic roadmap has failed its architectural goal.

---

# Definition of Done for Stage 13

The Stage 7–13 program is complete only when:

- all production vocabularies are independently testable;
- the normal UI exposes genre / variant and rhythm AUTO/manual behavior;
- genre generation composes vocabulary rather than bypassing it;
- low-BPM Lo-Fi / Trip-Hop / Dub remain musical without special-case pattern code;
- House / Techno / Electro / Acid remain stable and retain their identities;
- Breaks / UKG / DnB retain non-four-floor behavior where required;
- bass, chords and melody can leave intentional space;
- 2/4/8-bar phrases preserve identity across bounded evolution;
- Scene Save / Load reproduces all persisted generation choices;
- Cardputer ADV, DRAM and SEQTRAK MIDI-only gates pass;
- representative hardware audition passes for slow, straight, broken and high-energy families;
- three consecutive clean reviews pass on one unchanged final SHA.
