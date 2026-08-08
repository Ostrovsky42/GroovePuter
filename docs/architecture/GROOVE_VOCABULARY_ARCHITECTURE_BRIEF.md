# Groove Vocabulary Architecture — Design Brief

**Status:** design/audit brief; no production implementation is approved by this document  
**Base branch:** `dev_0.9_test`  
**Purpose:** prepare an architecture decision and explicit contracts for the next generation of GroovePuter rhythm, phrase and bass generation before any implementation PRs are opened.

## Task

Analyze the current `dev_0.9_test` implementation and prepare an architecture decision for the next generation of the GroovePuter generator.

Do **not** write production code, create implementation PRs, change Scene codecs, add genre IDs, change UI/key bindings, or remove legacy structures during this architecture-review stage.

First:

1. inspect the real current implementation in `dev_0.9_test`;
2. validate the architecture below against the code rather than old documentation;
3. identify integration points and conflicts;
4. define strict ownership and data contracts;
5. propose a staged migration;
6. define testable invariants, metrics and acceptance gates;
7. propose the final documentation tree.

Return the architecture review for approval before implementation.

---

## Problem statement

The strongest current generated results are the styles that already have a strong rhythmic skeleton: Techno, Rave, Acid, Dub/Deep Chord and Drum & Bass. Freer styles often degrade into weak, ringtone-like random melodic material.

The goal is **not** to reduce the number of variations. The problem is that variation is currently insufficiently constrained by a coherent multi-lane groove and phrase grammar.

The current conceptual model is too close to:

```text
Genre
  ↓
kick density
snare probability
hat probability
swing
  ↓
independent generators
  ↓
events
```

The target model is:

```text
Genre
  ↓
Rhythm Vocabulary selection
  ↓
Multi-track Rhythm Archetype
  ↓
Phrase realization
  ↓
Controlled mutation
  ↓
P1 / P2 / P3
  ↓
events
```

Primary architectural contract:

> Genre must not directly generate rhythm events.
>
> Genre selects, weights and constrains Rhythm Archetypes.
>
> Rhythm Archetype defines why a groove works rhythmically.
>
> Genre defines why the result is perceived as a specific style.

---

## 1. Current-state audit

Inspect the actual `dev_0.9_test` code, especially:

```text
src/dsp/genre_manager.*
src/dsp/drum_genre_templates.*
src/dsp/mode_manager.*
src/dsp/groove_profile.*
src/dsp/advanced_pattern_generator.*
src/dsp/miniacid_engine.*
scenes.*
```

and related pattern/song/phrase code.

Also inspect the live-performance path:

```text
src/input/performance_keyboard.*
src/input/internal_synth_output.*
src/input/musical_event.*
```

Use repository-wide code search to locate all production call sites where:

- Genre directly affects event placement;
- drum bitmasks are authoritative rhythm sources;
- `GenreBehavior.stepMask` is used;
- density/probability effectively becomes rhythm identity;
- Synth A is assumed to be bass;
- Synth B is assumed to be lead/arp;
- `voiceIndex` acts as musical role;
- P1/P2/P3 mutate already-realized events;
- generation and live-keyboard paths intersect;
- Scene persistence stores relevant parameters.

Deliver an exact `CURRENT GENERATION FLOW` with files, classes, functions and responsibilities.

---

## 2. Ownership model

Define unambiguous ownership boundaries for at least:

```text
Genre
RhythmFamily
RhythmArchetype
PhraseRealizer
BarEvolution
MutationPolicy
VoiceRole
BassPhraseGenerator
ChordPhraseGenerator
LeadPhraseGenerator
BassPerformancePolicy
Articulation
Scene
```

For each component specify:

- what it owns;
- what it must not own;
- inputs;
- outputs;
- persisted vs transient state;
- immutable catalog data vs runtime state;
- whether it may depend on Genre;
- whether it may depend on physical Synth A/B.

The following distinctions are mandatory:

```text
Genre != Pattern
Genre != RhythmArchetype
Synth A != Bass
Synth B != Lead
VoiceRole != Synth engine TYPE
Generated performance != Live keyboard performance
```

---

## 3. Rhythm Vocabulary

Target hierarchy:

```text
RhythmFamily
    ↓
RhythmArchetype
    ↓
legal realizations
```

Candidate families:

```text
FOUR_FLOOR
JACK_SHUFFLE
MACHINE_SYNCOPATION
BREAKBEAT
DNB_ROLL
UK_2STEP
HIPHOP_BACKBEAT
DUB_PULSE
FUNK_16TH
DISCO_DRIVE
INDUSTRIAL_MACHINE
MOTORIK_DRIVE
SPARSE_PULSE
FREE_SPACE
```

This list is not final. Audit whether any families are redundant, should instead be archetypes inside a wider family, or are actually genre/bass behavior rather than rhythm family. In particular, determine whether `ACID` should be a RhythmFamily or a genre/bass behavior over more general rhythm families.

Avoid taxonomy expansion for its own sake.

---

## 4. RhythmArchetype is a grammar, not a bitmap preset

Do not design `RhythmArchetype` as only a set of fixed 16-bit masks.

Conceptually it should express:

```text
RhythmArchetype
    id
    family
    meter
    phraseBars

    lane grammars
    lane relationships

    accent contract
    silence/protected zones
    swing contract
    density contract

    allowed mutation policy
    allowed bar evolution / trajectories
```

The embedded runtime representation must remain compact and suitable for ESP32-S3. Prefer static/constexpr catalog data, compact enums, bitfields/value structures and bounded algorithms. Do not introduce a heavyweight dynamic graph framework.

Propose concrete C++ data contracts, but no production implementation in this stage.

---

## 5. Lane Grammar

Evaluate a minimum lane grammar containing:

```text
required
preferred
optional
forbidden
protected silence
```

Candidate roles:

```text
KICK
SNARE
CLAP
CLOSED_HAT
OPEN_HAT
PERC
BASS_RHYTHM
CHORD_STAB
MELODIC_RHYTHM
```

Reduce or merge roles where the distinction is not useful at runtime.

`protected silence` is a first-class concept: variation/fills must not automatically occupy musically important gaps.

---

## 6. Lane Relationships

This is a central reason for the new layer. Independent good lane generation is insufficient if the combined groove is weak.

Evaluate a minimal relationship vocabulary such as:

```text
AVOID
FOLLOW
ANSWER
FILL_GAPS
COINCIDE
ANTICIPATE
LOCK_TO
DO_NOT_DUPLICATE
REQUIRE_OFFSET
FOLLOW_OR_ANSWER
```

Do not adopt the list mechanically. Define:

- minimal orthogonal relationship types;
- directionality;
- scope/zone masks;
- hard vs soft constraints;
- priority/conflict resolution;
- deterministic realization;
- behavior when all constraints cannot be satisfied simultaneously.

Define a deterministic resolution order. A candidate is:

```text
hard anchors
protected silence
hard relationships
density
soft relationships
optional variation
```

Change it if a better order follows from the code/model.

---

## 7. P1 / P2 / P3 contracts

P1/P2/P3 must not mean:

```text
P1 events
   ↓ mutate events
P2
   ↓ mutate harder
P3
```

Target semantics:

```text
P1 = canonical realization budget
P2 = variation realization budget
P3 = transformation realization budget
```

Every level should be realized from grammar:

```text
RhythmArchetype
+
P-level
+
seed
+
phrase context
    ↓
legal realization
```

Define exactly what is preserved and what is legal at P1/P2/P3.

At all levels, prohibit at least:

- moving protected hard anchors without explicit archetype permission;
- filling protected silence;
- converting a broken groove into unintended four-floor;
- independently randomizing all lanes;
- violating mandatory lane relationships.

---

## 8. BarEvolution as a first-class vocabulary

Do not stop at a single 16-step bar. Support 2–4 bar trajectories.

Evaluate reusable trajectories/functions such as:

```text
AAAA
AABA
ABAB
AABB
AABC
ABAC
A--B
BUILD_FILL
CALL_RESPONSE
BREAK_RETURN
```

Distinguish pattern identity from bar function. `AABA` must not automatically mean byte-identical event copies.

Candidate bar functions:

```text
STATEMENT
REPEAT
REPEAT_WITH_GHOSTS
RESPONSE
REDUCTION
BUILD
TURNAROUND
BREAK
RETURN
```

Determine:

- whether trajectories should be standalone catalog entities;
- whether archetypes contain allowed trajectory IDs + weights;
- how P1/P2/P3 interacts with BarEvolution;
- how this fits the existing 16 pages × A/B × 8 pattern slots matrix;
- how it avoids conflicting with Song mode and existing pattern ownership.

---

## 9. Voice roles

Treat the current `voiceIndex == 0 -> bass` / `voiceIndex == 1 -> lead` behavior as a legacy limitation.

Separate:

```text
physical track:
Synth A / Synth B

musical role:
Bass / Acid / Lead / Chord / Stab / Arp / Drone / Sequence / ...
```

VoiceRole must not be the synth engine TYPE.

Valid examples include:

```text
Synth A + TB303 + Role::Bass
Synth A + VA + Role::Bass
Synth B + VA + Role::Chord
Synth B + SID + Role::Lead
```

Decide:

- where role is stored;
- whether it is persisted in Scene;
- neutral/default role values;
- old Scene migration behavior;
- whether Genre selects a role as a generation-time suggestion or owns it persistently;
- whether the user can change role independently of Genre.

---

## 10. Bass Generator v2

RhythmArchetype may define bass:

```text
onset grammar
relationship to kick/snare
silence zones
density
bar function
```

It must not own concrete bass pitches.

Pitch generation should happen later:

```text
Bass rhythmic plan
    ↓
BassPhraseGenerator
    +
harmonic context
    +
VoiceRole
    +
Genre constraints
    +
BarEvolution
    +
VARY
    ↓
pitch phrase
```

Evaluate a minimal orthogonal set of bass phrase strategies from candidates such as:

```text
PEDAL
ROOT_FIFTH
OCTAVE
WALK
SYNCOPATED
ROLLING
ACID
CALL_RESPONSE
DESCENDING
UPWARD_PUSH
CHORD_TONES
APPROACH_NOTE
```

Reduce this list where strategies overlap.

The contract must address:

- motif memory;
- anchor notes;
- interval limits;
- register discipline;
- repetition;
- chord-tone preference;
- approach notes;
- octave displacement;
- slides;
- rests;
- gate lengths;
- drum-groove relationships;
- anti-ringtone behavior.

---

## 11. Live Bass Performance is a separate path

Keyboard bass must not pass through `BassPhraseGenerator`. The player is already the phrase generator.

Target path:

```text
PerformanceKeyboard
    ↓
VoiceRole
    ↓
BassPerformancePolicy
    ↓
Articulation
    ↓
Synth engine
```

Audit the current live input path and define the smallest useful Bass Performance contract, including:

```text
monophonic note priority: LAST / LOW / HIGH
legato
glide / slide
velocity response
accent response
retrigger policy
range handling
engine-specific articulation capability
```

Analyze TB303 separately: overlapping live notes/legato may naturally map to slide for TB303, but must not automatically map the same way to every synth engine.

If capability flags are needed, consider a bounded model such as:

```text
supportsLegato
supportsGlide
supportsAccent
supportsVelocityFilterResponse
supportsRetriggerPolicy
```

Avoid framework over-design.

---

## 12. Genre must not continuously own timbre

Genre may, at generation time:

```text
choose/suggest role
choose rhythm archetype
choose phrase constraints
optionally propose initial timbre
```

But after Scene/synth state exists or the user edits it:

```text
Genre must not continuously project itself over user sound state.
```

Audit `applyGenreTimbre()` and all related call sites. Define a migration boundary that prevents Genre from replacing synth TYPE or overwriting user-owned sound state.

---

## 13. Phrase/Motif Vocabulary boundary

Phrase/Motif Vocabulary is not the first implementation wave, but the architecture must leave a clean interface for it:

```text
Rhythm Vocabulary
    ↓
Phrase / Motif Vocabulary
    ↓
Harmonic Role
    ↓
Controlled Development
```

Do not fully design this subsystem now. Define only the minimum contract needed so Rhythm Vocabulary does not absorb melodic pitch grammar.

---

## 14. Atlas separation

Atlas / Mood Lab remains a research corpus:

```text
Atlas
    ↓ analysis / extraction / curation
Runtime Rhythm Vocabulary
    ↓
Generator
```

Do not make runtime patterns direct copies of Atlas patterns.

Atlas may contain hundreds or thousands of examples while runtime contains a curated set of families/archetypes/trajectories.

Define:

- what may be automatically extracted from Atlas;
- what needs manual curation;
- a runtime groove fingerprint suitable for duplicate detection;
- what the runtime vocabulary must not know about Atlas.

This separation must allow generalized break grammar without storing exact named-break transcriptions.

---

## 15. Initial reference vocabulary scope

Do not begin with 50–70 archetypes. First validate the model with approximately 20 archetypes derived from styles that already sound strong.

Candidate reference set:

```text
FOUR_FLOOR
  straight_drive
  offbeat_drive
  hypnotic_sparse
  broken_techno

ACID / acid-capable
  straight_acid
  rolling_acid
  syncopated_acid
  sparse_acid

DUB_PULSE
  sparse_skank
  chord_response
  steppers

BREAKBEAT / DNB
  two_step_roll
  ghosted_roll
  sparse_fast
  halftime_switch
  turnaround_break

UK_2STEP
  classic_2step
  skippy_2step
  sparse_2step
  shuffled_4x4
```

Audit and revise this list. Do not add archetypes just to hit a number.

---

## 16. Migration strategy

The desired broad order is:

```text
0.9 freeze
    ↓
Groove Vocabulary Core
    ↓
Relationship Engine
    ↓
20-archetype reference vocabulary
    ↓
migrate existing strong genres
    ↓
Techno / Acid / Dub / D&B regression gate
    ↓
Role model
    ↓
Bass Generator v2
    ↓
Bass Performance Policy
    ↓
Phrase/Motif Vocabulary
    ↓
weak-genre rehabilitation
    ↓
50–70 archetype expansion
```

Validate this order against the real code and change it only with concrete justification.

For each proposed stage specify:

- purpose;
- production files/areas touched;
- legacy subsystem replaced or adapted;
- compatibility adapter if needed;
- persisted-state impact;
- required tests;
- rollback boundary;
- explicit out-of-scope items.

No giant framework PR.

---

## 17. Compatibility and persistence

Audit:

- `GenreSettings`;
- pattern persistence;
- Song persistence;
- future VoiceRole persistence;
- defaults;
- version migration;
- decode-only legacy fields;
- Clear/New project behavior.

Prefer not to persist transient grammar-realization scratch state.

Evaluate whether the minimum persisted generation identity should contain some subset of:

```text
selected archetype id
P-level
seed
role
user-realized events
```

Do not adopt this list without checking the existing Scene model and expected Save/Load semantics.

---

## 18. Determinism

Preserve deterministic generation and RNG isolation.

The architecture should be able to state a boundary similar to:

```text
same scene-relevant state
same archetype
same seed
same P-level
same phrase position
same generation domain

=> same realization
```

Define generation domains so that changing/regenerating Bass does not necessarily rebuild Drums, and vice versa, unless the UX explicitly asks for a coupled regeneration.

Do not return to a shared global random stream.

---

## 19. Embedded performance constraints

Target: ESP32-S3 / Cardputer ADV.

Do not build a heavyweight dynamic constraint graph.

Review:

- RAM/DRAM cost;
- flash cost;
- stack use;
- allocations;
- runtime complexity;
- generation time;
- audio-thread isolation.

Prefer:

- static/constexpr vocabulary;
- no heap allocation during generation, or strictly bounded allocation if unavoidable;
- bounded relationship resolution;
- deterministic complexity;
- no generator work that risks real-time audio starvation.

Provide realistic approximate memory budgets for both:

```text
20 archetypes
50–70 archetypes
```

Exact cycle measurements are not required at architecture-review stage.

---

## 20. Quality metrics

Define host-side metrics including at least:

```text
rhythm_family_count
rhythm_archetype_count
multi_lane_relationship_count
phrase_trajectory_count

archetype_usage_entropy
genre_archetype_overlap
archetype_variant_count

duplicate_groove_fingerprint_rate

P1_P2_structural_distance
P1_P3_structural_distance

protected_anchor_violation_count
protected_silence_violation_count
relationship_violation_count
```

Provide formal definitions.

### Duplicate groove fingerprint

Propose a normalized fingerprint that preferably ignores:

- BPM;
- sound engine;
- timbre;
- small velocity noise;
- minor ghost variation;

while still detecting structurally equivalent grooves.

### Structural distance

Do not use plain all-event Hamming distance as the only metric. Account for:

- protected anchors;
- lane identity;
- relationships;
- protected silence;
- bar function.

---

## 21. Regression gates

Treat existing strong styles as the control group:

```text
Techno
Rave
Acid
Dub / Deep Chord
Drum & Bass
```

The architecture is not successful if these regress.

Define host + listening regression strategy.

Example invariant test:

```text
Generate N realizations of UK 2-Step P3

EXPECT:
  protected snare anchor violations == 0
  protected silence violations == 0
  forbidden four-floor collapse == 0
  hard relationship violations == 0
  unique valid realization count >= threshold
```

Define equivalent property/regression tests for multiple families.

---

## 22. Explicitly out of scope for this architecture-review task

Do not:

- write production implementation;
- create implementation PRs;
- change Scene codec;
- add new Genre IDs;
- add 50–70 archetypes immediately;
- fully design melodic Phrase Vocabulary;
- rewrite the generator framework;
- change UI;
- change key bindings;
- remove legacy structures before the migration plan is accepted.

---

# Required architecture-review output

Return one review with the following sections.

## A. Current-state audit

Actual generation flow with exact files/classes/functions, mixed responsibilities and technical debt.

## B. Architecture decision

```text
Context
Decision
Rationale
Alternatives rejected
Consequences
```

## C. Ownership contracts

Table:

```text
Component | Owns | Must not own | Inputs | Outputs | Persistence
```

for at least:

- Genre
- RhythmFamily
- RhythmArchetype
- PhraseRealizer
- BarEvolution
- VoiceRole
- BassPhraseGenerator
- BassPerformancePolicy
- Scene

## D. Proposed data contracts

Concrete C++ declarations/pseudocode for:

```text
RhythmFamily
RhythmRole
LaneGrammar
LaneRelationship
SwingContract
DensityContract
MutationPolicy
BarEvolution
RhythmArchetype
VoiceRole
```

No production implementation.

## E. Generation pipeline

Show the full flow from Genre to events, with separate flows for:

```text
generated bass
live bass
```

## F. P1/P2/P3 contract

State preserved, allowed mutations and forbidden transformations.

## G. Relationship semantics

Hard/soft constraints, priorities and deterministic conflict resolution.

## H. Persistence impact

Required and explicitly unnecessary Scene/persistence changes.

## I. Migration plan

For each stage:

```text
Purpose
Files/areas
Behavior change
Compatibility
Tests
Acceptance gate
Out of scope
```

## J. Runtime budget

Approximate CPU/RAM/flash impact.

## K. Test strategy

Unit, host, property, regression and listening tests.

## L. Metrics

Formal definitions of the quality metrics.

## M. Initial ~20-archetype proposal

A curated runtime set derived from strong existing material. Do not pad the list.

## N. Open architectural questions

Only genuinely unresolved questions. For each:

```text
question
recommended default
alternative
consequence
```

Do not push obvious implementation decisions back to the reviewer.

## O. Recommended docs tree

Inspect the existing docs tree first and propose the minimum non-duplicative final structure, potentially including:

```text
docs/architecture/GENERATION_ARCHITECTURE.md
docs/architecture/RHYTHM_VOCABULARY.md
docs/architecture/RHYTHM_CONTRACTS.md
docs/architecture/VOICE_ROLES.md
docs/architecture/GENERATOR_MIGRATION_PLAN.md
docs/testing/GENERATOR_QUALITY_METRICS.md
```

---

# Acceptance criteria for the architecture

The proposed architecture is acceptable only if all of the following can hold simultaneously:

1. Genre no longer directly generates individual rhythm events.
2. Groove is produced from a coherent multi-lane grammar.
3. Variation cannot destroy protected groove identity.
4. P1/P2/P3 are legal grammar realizations, not random event mutation levels.
5. 2–4 bar development is a first-class concept.
6. Synth A/B no longer define musical role.
7. Bass rhythm is constrained by the groove.
8. Bass pitch phrase is a separate layer.
9. Live bass remains a direct performance instrument.
10. Genre does not reclaim ownership of user-edited timbre.
11. Atlas is separated from runtime vocabulary.
12. Runtime remains bounded, deterministic and inexpensive enough for ESP32-S3.
13. Strong existing genres do not regress.
14. The architecture increases meaningful variation rather than reducing it.
15. Migration can be delivered through several small, controlled PRs without a framework rewrite.

The architecture review must be completed and approved before implementation changes begin.
