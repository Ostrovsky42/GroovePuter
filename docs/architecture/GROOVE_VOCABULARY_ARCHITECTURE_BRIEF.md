# Groove Vocabulary Architecture — Audited Design Brief

**Status:** architecture review result; no production implementation is approved by this document  
**Audit base:** `dev_0.9_test` @ `18f1e49d2eda0528f758bbe0d374e5699f18857c`  
**Docs branch:** `agent/20260808-01-groove-vocabulary-architecture`  
**Purpose:** define the next-generation GroovePuter rhythm-generation architecture, ownership boundaries, migration stages, invariants, metrics, persistence impact and acceptance gates before implementation PRs are opened.

This document replaces the original audit checklist with the conclusions obtained from the actual `dev_0.9_test` implementation. It remains a design document only.

Do **not** treat this document as approval to:

- write production implementation;
- change Scene codecs;
- add Genre IDs;
- change UI or key bindings;
- remove legacy generator structures;
- add 50–70 archetypes immediately;
- merge a generator rewrite into the 0.9 release path.

---

# Executive conclusion

The original diagnosis is confirmed by the current code.

The primary procedural generation path is still structurally close to:

```text
Genre
  ↓
GenerativeParams
GenreBehavior.stepMask
DrumGenreTemplate masks
  ↓
independent synth/drum placement
  ↓
SynthPattern / DrumPatternSet
```

A second path bypasses procedural generation entirely:

```text
Genre recipe
  ↓
AtlasRuntime
  ↓
precompiled concrete events
  ↓
SynthPattern / DrumPatternSet
```

The missing middle layer is a compact curated rhythmic grammar:

```text
Genre
  ↓
Rhythm Vocabulary selection
  ↓
Multi-role RhythmArchetype
  ↓
RhythmPhraseRealizer
  +
BarEvolution
  +
P-level realization budget
  ↓
legal multi-role RhythmPhrasePlan
  ↓
role-specific phrase generation
  ↓
materialization into existing patterns
```

Primary architectural contract:

> **Genre MUST NOT directly place rhythm events.**
>
> Genre selects, weights and constrains RhythmArchetypes.
>
> RhythmArchetype defines why a groove works rhythmically.
>
> Genre defines why a legal realization is perceived as a specific style.

Four important corrections to the initial proposal follow from the real code:

1. The new component should be named **`RhythmPhraseRealizer`** or **`GrooveRealizer`**, not generic `PhraseRealizer`, because GroovePuter already has `PhraseCore` with its own A/B/C/D phrase ownership, roles and persistence.
2. **`BarEvolution` MUST NOT own pages, banks, pattern slots or Song rows.** It produces a transient musical plan. A separate materializer receives explicit destinations.
3. **ACID SHOULD NOT be a RhythmFamily in v1.** Acid identity is a combination of a general rhythmic archetype, an `AcidBass` role/phrase contract and articulation behavior.
4. Strong genres should be migrated in two steps: first rhythmic placement/drums, then VoiceRole/Bass v2. Mixing both changes in one implementation wave would make regressions impossible to localize.

---

# A. Current-state audit

## A1. Audited files and subsystems

The audit covered the requested generator and persistence areas plus related runtime call sites:

```text
src/dsp/genre_manager.h/.cpp
src/dsp/drum_genre_templates.h/.cpp
src/dsp/mode_manager.h/.cpp
src/dsp/groove_profile.h/.cpp
src/dsp/advanced_pattern_generator.h/.cpp
src/dsp/miniacid_engine.h/.cpp
src/dsp/atlas_runtime.h/.cpp
src/dsp/deterministic_rng.h
src/dsp/swappable_synth_voice.h/.cpp
src/dsp/mono_synth_voice.h
src/dsp/mini_tb303.h/.cpp

src/input/performance_keyboard.h/.cpp
src/input/internal_synth_output.h/.cpp
src/input/musical_event.h

src/phrase/phrase_types.h
scenes.h
scenes.cpp
src/ui/pages/genre_page.cpp
```

Repository-wide search was also used for generation entry points, `stepMask`, role assumptions, Scene ownership, live input, Atlas runtime and persistence.

---

## A2. CURRENT GENERATION FLOW

### Genre UI entry

```text
GenrePage::applyCurrent()
    |
    | writes Scene::genre
    | maps genre/recipe -> GrooveboxMode
    | optionally applies BPM
    v
MiniAcid::regeneratePatternsWithGenre()
```

`GenrePage::applyCurrent()` owns explicit user materialization. `PROFILE ONLY` changes metadata; `MATERIALIZE` calls regeneration; optional tempo application is a separate choice.

This is already a useful ownership boundary: genre selection does not have to regenerate unless the user requests it.

### Regeneration dispatcher

`MiniAcid::regeneratePatternsWithGenre()` currently does:

```text
syncGrooveModeToGenre()
        |
        v
AtlasRuntime::applyRecipe(recipe, variationIndex=0, ...)
        |
        +-- success -> copy concrete events -> return
        |
        +-- no Atlas recipe
                 |
                 v
     GenreSceneView::getCompiledGenerativeParams()
     GenreSceneView::getBehavior()
                 |
                 +--> ModeManager::generatePattern(..., voiceIndex=0)
                 |
                 +--> ModeManager::generatePattern(..., voiceIndex=1)
                 |
                 +--> ModeManager::generateDrumPattern(...)
```

This means the project currently has two generation models rather than one unified pipeline:

1. procedural generation from genre parameters/bitmasks;
2. direct materialization of precompiled Atlas events.

### Synth generation

`MiniAcid::regeneratePatternsWithGenre()` and `MiniAcid::randomize303Pattern()` obtain:

```text
GenerativeParams
GenreBehavior
```

and pass them to `GrooveboxModeManager::generatePattern()`.

The current synth path still treats physical voice as musical role:

```text
voiceIndex == 0 -> bass-oriented behavior
voiceIndex == 1 -> lead/arp-oriented behavior
```

The distinction affects register, density, root forcing, motif behavior and other generation decisions. This is a real architectural coupling, not only a UI convention.

Reggae contains an additional explicit split:

```text
voice 0 -> stepMask 0x1111, bass-like
voice 1 -> stepMask 0xAAAA, offbeat lead/skank-like
```

Therefore:

```text
Synth A != Bass
Synth B != Lead
```

requires a real migration layer.

### Drum generation

`DrumGenreTemplate` is currently an authoritative rhythmic source. Templates contain fixed masks and per-lane probabilities/velocity behavior. The drum generator then decorates those skeletons with probabilistic ghosts, hats and fills.

The current model therefore has useful strong skeletons, but does not express why lanes work together as a relational unit.

### Atlas generation

`AtlasRuntime::applyRecipe()` validates a precompiled event list, clears destinations and writes concrete events into:

```text
SynthPattern A
SynthPattern B
DrumPatternSet
```

Atlas events already contain concrete:

```text
target
step
note
velocity
timing
probability
accent/slide flags
```

The current production call uses `variationIndex = 0` in `regeneratePatternsWithGenre()`.

This proves that the target Atlas boundary must be changed from direct runtime materialization to offline extraction/curation feeding a smaller runtime vocabulary.

---

## A3. `GenreBehavior.stepMask` is a placement primitive

`GenreCatalog::behavior()` contains fixed masks such as:

```text
0xFFFF
0xAAAA
0xAA55
0xF0F0
0x8888
```

Recipe-specific branches also replace `stepMask`.

The mask therefore currently acts as all of the following:

```text
genre characteristic
placement restriction
implicit rhythmic skeleton
```

This responsibility moves to `LaneGrammar` / `RhythmArchetype` in the new architecture.

A 16-bit mask remains an excellent embedded representation for zones and anchors; the problem is not bitmasks themselves. The problem is treating one mask as the whole rhythmic grammar.

---

## A4. Genre owns too many concerns

`GenreBehavior` currently mixes:

```text
rhythm placement
motif length
scale behavior
chromatic behavior
octave behavior
cluster behavior
timbre
```

`GenerativeParams` mixes:

```text
note density
swing
microtiming
velocity
ghost probability
fill probability
drum syncopation
sparse kick/hats
pitch-related constraints
```

The new architecture must not simply move these fields into a bigger `RhythmArchetype`. Only rhythmic topology and rhythmic realization policy belong there.

Pitch grammar, timbre and physical engine choice stay outside Rhythm Vocabulary.

---

## A5. `GrooveProfile` is not Rhythm Vocabulary

`GrooveProfile` exposes `PatternCorridors` containing:

```text
notesMin
notesMax
accentProbability
slideProbability
swingAmount
```

and applies mode-specific budget adjustments.

This is a density/articulation corridor layer. It can remain temporarily as compatibility input, but it does not define multi-lane groove grammar and must not be renamed or promoted into the new vocabulary layer.

---

## A6. P1/P2/P3 current behavior

The initial concern that P1/P2/P3 were necessarily implemented as cumulative mutation of concrete events is **not confirmed as the central runtime model** in this branch.

Atlas runtime can expose multiple compiled variants, but `MiniAcid::regeneratePatternsWithGenre()` currently requests variant `0`.

Separately, `PhraseCore` has:

```text
MAIN
VARIATION
BREAK
ENDING
```

but these are phrase storage/arrangement semantics, not generator P-level semantics.

Therefore the new P1/P2/P3 contract can be defined cleanly as independent realizations from grammar.

---

## A7. Deterministic RNG foundation

`DeterministicRng` already provides a small deterministic xorshift32 state and unbiased bounded sampling.

The new architecture should reuse this style of explicit local RNG state and domain separation.

Legacy/global random usage still exists in older/secondary generator code, including `rand()`-based paths. These must not be copied into Groove Vocabulary.

Hard contract:

```text
No global random stream in the new realization path.
```

---

## A8. Scene and materialized patterns are already the correct playback source of truth

`Scene` persists concrete materialized pattern state:

```text
DrumPatternSet banks
Synth A banks
Synth B banks
Song slots
PhraseBank
GenreSettings
FeelSettings
Synth patches
```

Each drum/synth step stores concrete event attributes such as:

```text
hit/note
accent
slide/ghost
velocity
timing
FX
probability
```

This allows Groove Vocabulary v1 to be introduced **without changing the Scene codec**.

The new generator can remain a producer of existing materialized pattern types.

---

## A9. Scene Load already protects user-owned synth state from Genre projection

Current Scene load restores persisted synth engines/patch parameters and restores genre metadata separately. The load path explicitly avoids re-projecting genre sound over a restored synth patch.

That existing rule becomes normative for generator v2:

```text
Scene synth patch wins over Genre metadata on load.
```

`applyGenreTimbre()` still exists as a legacy helper and can force TB303-oriented timbre/engine choices for some recipes, but the audited `GenrePage::applyCurrent()` path does not need it as a continuous owner.

New code MUST NOT depend on `applyGenreTimbre()`.

---

## A10. Live keyboard path is already architecturally separate from generated pattern events

Current live path:

```text
PerformanceKeyboard
    ↓
MusicalEventRouter
    ↓
InternalSynthOutput
    ↓
MiniAcid::liveNoteOn/liveNoteOff
    ↓
SwappableSynthVoice
```

`MusicalEventSource` distinguishes:

```text
PerformanceKeyboard
PatternPlayer
Arpeggiator
MidiInput
```

`InternalSynthOutput` intentionally ignores `PatternPlayer` for internal voices because the sequencer already renders them in the audio engine; routing them again would double-trigger.

This is a good foundation for Bass Performance Policy.

Current limitations:

```text
MiniAcid::liveNoteOn() returns while transport is playing
live NoteOn passes accent=false, slide=false
live input has no generic articulation contract
```

---

## A11. TB303 already contains useful legato-slide behavior

`TB303Voice::startNote()` detects a slide when:

```text
slide flag is true
AND gate is already open
AND the voice is active
```

and then changes target frequency without starting a fresh amplitude envelope.

The DSP capability exists; the live-input path simply does not currently carry semantic articulation to it.

This supports introducing a small capability/articulation adapter rather than redesigning every synth engine.

---

## A12. Existing PhraseCore constrains naming and ownership

`PhraseCore` already owns:

```text
A/B/C/D phrase slots
1/2/4/8 bar lengths
MAIN / VARIATION / BREAK / ENDING roles
source/storage metadata
pattern references per bar and track
```

It is fixed-capacity and tightly budgeted.

Therefore:

```text
PhraseCore != RhythmPhraseRealizer
```

The new realizer produces a transient multi-bar plan. A caller/materializer may later write results into existing patterns and optionally create/update PhraseCore references.

The realizer itself never owns PhraseCore slots.

---

## A13. Main technical-debt map

| Current component | Mixed responsibility to remove or contain |
|---|---|
| `GenreBehavior` | rhythm placement + melodic rules + timbre |
| `GenerativeParams` | density + articulation + drum behavior + pitch concerns |
| `DrumGenreTemplate` | genre identity + concrete one-bar skeleton |
| `GrooveboxModeManager` | macro mode + generator + physical voice→musical role |
| `GrooveProfile` | mode + density/articulation budget |
| `AtlasRuntime` | research-derived data + direct production materialization |
| `MiniAcid` generation methods | orchestration + role assumptions + legacy backend selection |
| `voiceIndex` | physical destination + musical role |
| `GenreBehavior.stepMask` | genre descriptor + placement contract |

---

# B. Architecture decision

## Context

GroovePuter already has:

- strong deterministic-generation foundations;
- compact 16-step pattern structures;
- mature Scene/pattern persistence;
- working Song and PhraseCore composition layers;
- several strong genre outputs whose rhythmic skeletons serve as a control group.

The missing component is a reusable multi-role rhythmic grammar that preserves structural identity while still generating many legal realizations.

## Decision

Introduce a new immutable/runtime-bounded **Groove Vocabulary** layer between Genre and concrete pattern events.

```text
Genre
    ↓
GenreGenerationProfile
    ↓
weighted RhythmArchetype selection
    ↓
RhythmPhraseRealizer
    + BarEvolution
    + RealizationLevel
    + explicit deterministic GenerationContext
    ↓
RhythmPhrasePlan
    ↓
role-specific generators / binders
    ↓
PatternMaterializer
    ↓
existing SynthPattern / DrumPatternSet
```

## Rationale

This layer:

- preserves variation instead of shrinking it;
- makes cross-lane relationships first-class;
- makes protected silence first-class;
- separates genre identity from rhythmic identity;
- removes physical Synth A/B from musical role semantics;
- is compatible with current Scene persistence;
- can be introduced incrementally;
- can be implemented with fixed arrays, masks and bounded passes suitable for ESP32-S3.

## Alternatives rejected

### Add more `DrumGenreTemplate` presets

Rejected as final architecture. It improves preset coverage but does not create relational grammar.

### Add more probabilities/density parameters

Rejected. Independent stochastic lanes are a root cause of weak combined groove.

### Use Atlas concrete patterns as the runtime generator

Rejected. Atlas remains research input; runtime must use a curated generalized vocabulary.

### Generic dynamic graph/CSP engine

Rejected. The problem size is small and fixed; a specialized bitmask resolver is simpler, cheaper and more deterministic.

### Rewrite the whole generator framework in one PR

Rejected. Strong existing genres are a regression control group and must remain rollback-safe through staged migration.

### Persist full grammar realization state

Rejected for v1. Existing materialized events already provide exact playback persistence.

## Consequences

- new generator code is introduced alongside legacy backends first;
- old patterns/scenes remain playable even when vocabulary definitions evolve;
- Genre becomes a selector/constraint source rather than event owner;
- role-aware synth generation becomes a later migration stage, not a prerequisite for validating the groove layer;
- Atlas direct materialization must eventually become a compatibility/backend path rather than the conceptual center of generation.

---

# C. Ownership contracts

| Component | Owns | Must not own | Inputs | Outputs | Persistence |
|---|---|---|---|---|---|
| **Genre** | archetype weights/exclusions, genre corridors, role suggestions, harmonic/timbre suggestions | concrete rhythm events, synth physical ownership | Genre settings, Feel, generation request | `GenreGenerationProfile` | existing `GenreSettings` |
| **RhythmFamily** | coarse taxonomy | events, pitch, timbre, Genre identity | catalog | classification | immutable catalog only |
| **RhythmArchetype** | lane grammar, relationships, protected silence, swing/density corridor, legal realization policy, allowed trajectories | concrete pitches, synth TYPE, Scene slots, Song rows | immutable catalog | grammar description | immutable catalog only |
| **RhythmPhraseRealizer** | deterministic grammar realization algorithm | catalog ownership, Scene, pattern allocation, timbre | archetype, P-level, seed/context, trajectory | `RhythmPhrasePlan` | transient runtime only |
| **BarEvolution** | reusable sequence of bar functions | pattern IDs, Song positions, exact events | trajectory + realization context | bar-function plan | immutable catalog only |
| **MutationPolicy** | legal realization budgets per P-level | mutation of old materialized patterns as source of truth | P-level | allowed realization operations/budgets | immutable catalog only |
| **VoiceRole** | musical function semantics | physical Synth A/B identity, engine TYPE | user state / Genre suggestion | role constraint | eventually persisted |
| **BassPhraseGenerator** | generated bass pitches, gates, motif development and articulation intents | drum placement, live keyboard phrase interpretation | bass rhythm plan, harmony, role, genre pitch corridor, seed | generated bass phrase | transient |
| **ChordPhraseGenerator** | future chord pitch realization | rhythm grammar ownership | chord rhythm plan + harmony | generated chord phrase | transient |
| **LeadPhraseGenerator** | future melodic pitch realization | global groove topology | melodic rhythm plan + motif/harmony | generated melodic phrase | transient |
| **BassPerformancePolicy** | interpretation of live held notes for bass role | generated phrase creation | held notes, role, engine capabilities, performance settings | articulation commands | runtime settings; optional persistence later |
| **Articulation** | semantic note-transition intent | engine selection | performance/generator phrase | legato/glide/accent/retrigger intent | value object |
| **Scene** | user-visible materialized result and persistent choices | vocabulary catalog and resolver scratch state | materialized patterns/settings | exact playback/project state | persisted |

Mandatory distinctions:

```text
Genre != Pattern
Genre != RhythmArchetype
Synth A != Bass
Synth B != Lead
VoiceRole != SynthEngineType
Generated phrase != Live keyboard performance
PhraseCore != RhythmPhraseRealizer
```

---

# D. Proposed data contracts

The following declarations are architectural pseudocode, not approved production implementation.

## D1. RhythmFamily

The initial candidate taxonomy should be reduced rather than expanded.

```cpp
enum class RhythmFamily : uint8_t {
    FourFloor,
    MachineSyncopation,
    Breakbeat,
    UkTwoStep,
    HipHopBackbeat,
    DubPulse,
    Funk16,
    SparsePulse,
};
```

Do not initially make separate families for:

```text
JACK_SHUFFLE       -> FourFloor archetype/property
DNB_ROLL           -> Breakbeat archetype group
DISCO_DRIVE        -> FourFloor archetype
INDUSTRIAL_MACHINE -> MachineSyncopation archetype
MOTORIK_DRIVE      -> straight/four-floor archetype
FREE_SPACE         -> SparsePulse/no-rhythm policy
ACID               -> Genre + BassPhrase/Articulation behavior
```

A family should exist only if several archetypes share a useful common grammar identity.

## D2. RhythmRole

Vocabulary roles are musical rhythmic functions, not physical drum voices.

```cpp
enum class RhythmRole : uint8_t {
    Kick,
    Backbeat,
    ClosedHat,
    OpenHat,
    Percussion,
    BassRhythm,
    ChordRhythm,
    MelodicRhythm,
    Count
};
```

Important v1 correction:

```text
Snare / Clap / Rim
```

should not be three independent vocabulary roles by default. They often represent one structural function:

```text
Backbeat
```

A later binding/materialization layer chooses the physical voice or layer combination.

Therefore:

```text
RhythmRole != VoiceId
```

## D3. Step masks

```cpp
using StepMask = uint16_t;
```

16-bit masks remain the preferred compact representation for a 16-step bar. They represent zones/constraints, not complete pattern presets.

## D4. LaneGrammar

```cpp
struct LaneGrammar {
    RhythmRole role;

    StepMask required;
    StepMask preferred;
    StepMask optional;
    StepMask forbidden;
    StepMask protectedSilence;

    uint8_t minHits;
    uint8_t maxHits;

    uint8_t accentProfileId;
    uint8_t flags;
};
```

Semantics:

- `required`: immutable/canonical structural anchors;
- `preferred`: normal candidate space;
- `optional`: legal lower-priority additions;
- `forbidden`: structural prohibition;
- `protectedSilence`: intentional musical emptiness that survives variation/fills.

Catalog invariants:

```text
required & forbidden == 0
required & protectedSilence == 0
minHits <= maxHits
required popcount <= maxHits
```

`forbidden` and `protectedSilence` are intentionally separate concepts.

## D5. Global protected silence

Per-lane silence is not sufficient for dub, sparse and broken idioms.

```cpp
StepMask globalProtectedSilence;
```

This prevents unrelated optional roles from automatically occupying intentionally empty subdivisions.

## D6. Relationship vocabulary

The original candidate list is larger than necessary. A minimal orthogonal v1 vocabulary is:

```cpp
enum class RelationshipOp : uint8_t {
    Exclude,
    Coincide,
    Offset,
    Respond,
    FillGaps,
};
```

Mapping from initial concepts:

```text
AVOID / DO_NOT_DUPLICATE -> Exclude
COINCIDE / LOCK_TO       -> Coincide or required anchors
FOLLOW / ANTICIPATE /
REQUIRE_OFFSET           -> Offset
ANSWER / FOLLOW_OR_ANSWER-> Respond
FILL_GAPS                -> FillGaps
```

## D7. LaneRelationship

```cpp
enum class ConstraintStrength : uint8_t {
    Soft,
    Hard,
};

struct LaneRelationship {
    RhythmRole source;
    RhythmRole target;

    RelationshipOp op;
    ConstraintStrength strength;

    StepMask zoneMask;

    int8_t minOffset;
    int8_t maxOffset;

    uint8_t weight;
};
```

Direction contract:

```text
source = anchor/reference role
target = constrained role
```

Example:

```text
source = Backbeat
target = Kick
op = Exclude
strength = Hard
```

means kick may not collide with the referenced backbeat anchors in the active zone.

## D8. SwingContract

```cpp
struct SwingContract {
    uint8_t minPercent;
    uint8_t preferredPercent;
    uint8_t maxPercent;

    StepMask affectedSteps;
    uint8_t affectedRoleMask;
};
```

The archetype owns the legal corridor; Genre/Feel selects a concrete value inside the corridor.

## D9. DensityContract

```cpp
struct DensityContract {
    uint8_t minTotalHits;
    uint8_t preferredTotalHits;
    uint8_t maxTotalHits;
};
```

Per-role density remains in `LaneGrammar`.

Density is subordinate to hard constraints. The generator MUST NOT violate protected silence or hard relationships merely to hit preferred density.

## D10. P-level

```cpp
enum class RealizationLevel : uint8_t {
    P1Canonical,
    P2Variation,
    P3Transformation,
};
```

## D11. MutationPolicy

`MutationPolicy` is retained as a name, but its meaning changes.

It controls **how a fresh legal realization may differ**, not how already-materialized events are destructively randomized.

```cpp
enum MutationFlags : uint16_t {
    AllowOptionalAdds       = 1 << 0,
    AllowPreferredDrops     = 1 << 1,
    AllowGhostConversion    = 1 << 2,
    AllowOptionalDisplace   = 1 << 3,
    AllowAccentVariation    = 1 << 4,
    AllowReduction          = 1 << 5,
    AllowTurnaround         = 1 << 6,
    AllowBreak              = 1 << 7,
};

struct MutationBudget {
    uint8_t maxAdds;
    uint8_t maxDrops;
    uint8_t maxDisplacements;
    uint8_t maxAccentChanges;
    uint16_t flags;
};

struct MutationPolicy {
    MutationBudget level[3];
};
```

## D12. BarEvolution

```cpp
enum class BarFunction : uint8_t {
    Statement,
    Repeat,
    RepeatWithGhosts,
    Response,
    Reduction,
    Build,
    Turnaround,
    Break,
    Return,
};

using TrajectoryId = uint8_t;

struct BarTrajectory {
    TrajectoryId id;
    uint8_t barCount;
    BarFunction bars[4];
};
```

Names such as `AAAA`, `AABA`, `ABAB`, `BUILD_FILL`, `CALL_RESPONSE`, `BREAK_RETURN` are human-readable trajectory identities, not exact byte-copy instructions.

Example conceptual `AABA`:

```text
Statement
Repeat
Response
Statement
```

The repeated bar may be a fresh legal realization with preserved identity rather than a byte-identical pattern.

## D13. Archetype trajectory references

```cpp
struct TrajectoryRef {
    TrajectoryId id;
    uint8_t weight;
    uint8_t allowedLevelsMask;
};
```

## D14. RhythmArchetype

```cpp
using RhythmArchetypeId = uint16_t;

struct RhythmArchetype {
    RhythmArchetypeId id;
    RhythmFamily family;

    uint8_t meterNumerator;
    uint8_t meterDenominator;
    uint8_t allowedPhraseBarsMask;
    uint8_t activeRoleMask;

    StepMask globalProtectedSilence;

    const LaneGrammar* lanes;
    uint8_t laneCount;

    const LaneRelationship* relationships;
    uint8_t relationshipCount;

    const TrajectoryRef* trajectories;
    uint8_t trajectoryCount;

    SwingContract swing;
    DensityContract density;
    MutationPolicy mutation;
};
```

Catalog data must be static/constexpr or equivalent flash-resident immutable data. No heap-owning runtime catalog graph.

## D15. VoiceRole

```cpp
enum class VoiceRole : uint8_t {
    Auto = 0,

    Bass,
    AcidBass,
    Chord,
    Stab,
    Lead,
    Arp,
    Drone,
    MelodicSequence,
};
```

`Auto` is the compatibility/default role for old Scenes.

Examples:

```text
Synth A + TB303     + AcidBass
Synth A + SH101     + Bass
Synth B + WAVEMORPH + Chord
Synth B + SID       + Lead
```

---

# E. Generation pipeline

## E1. Full generated flow

```text
Scene Genre selection
        |
        v
GenreGenerationProfile
        |
        | archetype weights / excludes / corridors
        v
RhythmArchetypeSelector
        |
        v
RhythmArchetype
        +
BarTrajectory
        +
RealizationLevel
        +
GenerationContext
        |
        v
RhythmPhraseRealizer
        |
        v
RhythmPhrasePlan
   |         |           |
   |         |           |
Drums   BassRhythm   Chord/Melodic rhythm
             |
             v
    BassPhraseGenerator
             +
    HarmonicContext
             +
    VoiceRole
             +
    Genre pitch constraints
             |
             v
    GeneratedPitchPhrase
             |
             v
PatternMaterializer
             |
             v
existing SynthPattern / DrumPatternSet
```

`RhythmPhrasePlan` contains musical roles, not physical Synth A/B or drum engine voices.

## E2. Generated bass

```text
RhythmArchetype
    ↓
BassRhythmPlan
    |
    | onset positions
    | protected rests
    | density
    | kick/backbeat relationships
    | bar function
    v
BassPhraseGenerator
    + HarmonicContext
    + VoiceRole
    + Genre pitch corridor
    + deterministic BassPitch RNG
    ↓
GeneratedBassPhrase
    ↓
SynthPattern materializer
```

Hard contract:

> `BassPhraseGenerator` MUST NOT create an onset outside the supplied `BassRhythmPlan`.

Rhythm is resolved first; pitch comes later.

## E3. Live bass

```text
PerformanceKeyboard
    ↓
HeldNoteState
    ↓
VoiceRole
    ↓
BassPerformancePolicy
    ↓
ArticulationResolver
    + SynthCapabilities
    ↓
SwappableSynthVoice
```

`BassPhraseGenerator` is never part of the live-performance path.

---

# F. P1 / P2 / P3 contract

## P1 — canonical realization

P1 MUST preserve:

- all required anchors;
- all protected silence;
- all hard relationships;
- family/archetype topology;
- required backbeat identity;
- legal swing corridor;
- legal density corridor;
- required role presence.

P1 MAY vary:

- deterministic selection among preferred candidates;
- small legal optional additions;
- velocity/accent realization;
- limited ghost choices.

P1 should be the easiest realization to recognize as a canonical member of the archetype.

## P2 — variation realization

P2 MUST preserve all hard P1 identity.

P2 MAY additionally use:

- more optional events;
- ghost additions/removals;
- limited displacement of optional/preferred events;
- percussion answers;
- alternate legal kick anticipation;
- moderate reduction;
- moderate response behavior.

P2 is another realization of the same grammar, not a mutated P1 event array.

## P3 — transformation realization

P3 MAY additionally use:

- `Break`;
- `Turnaround`;
- `Build`;
- stronger `Reduction`;
- stronger call/response;
- wider legal density change;
- wider legal optional displacement.

P3 remains inside the same archetype identity.

## Forbidden at every level

```text
remove/move immutable required anchors
fill protected silence
violate hard relationships
silently switch RhythmFamily
collapse a broken archetype into unintended four-floor
create events outside lane legal domains
independently randomize every lane
change physical Synth engine
change explicit VoiceRole
choose Scene/Song destinations
```

---

# G. Relationship semantics

## G1. Deterministic resolution order

The initial candidate order should be refined to:

```text
1. Validate catalog + request
2. Instantiate hard anchors
3. Apply forbidden masks and protected silence
4. Derive legal candidate masks from hard relationships
5. Apply BarFunction + P-level realization budget
6. Satisfy minimum/target density inside legal space
7. Optimize soft relationships/preferences
8. Add optional ornamentation
9. Final hard-invariant validation
10. Materialize only on success
```

Density is deliberately after the hard legal-space reduction.

## G2. Hard relationships

A hard relationship MUST hold in every valid output.

A hard relationship failure is not an acceptable degraded result; it is generation failure.

## G3. Soft relationships

Soft relationships affect candidate weights/preferences but may lose to:

- protected silence;
- hard relationships;
- minimum required density;
- deterministic conflict resolution.

## G4. Conflict resolution

Obvious catalog contradictions should be detected by host/catalog validation before firmware use.

Runtime soft conflicts are resolved by:

```text
1. higher weight
2. stable relationship/role order
3. stable step order
4. RNG only among equivalent legal candidates
```

## G5. Impossible hard realization

The realizer MUST NOT silently violate hard rules.

Conceptual result:

```cpp
enum class RealizationStatus : uint8_t {
    Ok,
    ValidButSparse,
    InvalidConstraintSet,
};
```

Materialization is transactional: destination patterns remain unchanged if realization fails.

## G6. Density underflow

If hard constraints permit fewer events than preferred density:

```text
hard constraints win
```

A legal sparse output is preferable to a denser invalid output.

---

# H. Persistence impact

## H1. Groove Vocabulary v1 does not require a Scene codec change

Persist exactly what the project already needs for exact playback:

```text
GenreSettings
materialized SynthPattern / DrumPatternSet
Song
PhraseCore
Synth patches / engines
Feel and other current project state
```

Do not persist:

```text
candidate masks
relationship resolver scratch
RhythmPhrasePlan scratch
BarEvolution working state
MutationBudget working state
RNG objects
```

## H2. Archetype ID / seed / P-level

These are not required for playback because the materialized result is already persisted.

They may later be stored as optional provenance if UX requires exact regenerate/re-derive semantics:

```cpp
struct GenerationProvenance {
    uint16_t archetypeId;
    uint32_t seed;
    uint8_t level;
    uint8_t vocabularyVersion;
};
```

Do not add this to the first implementation wave.

Old Scenes must continue to play even if an old vocabulary archetype is later removed or changed.

## H3. VoiceRole persistence

VoiceRole is different because it becomes a user-editable semantic choice.

Recommended final behavior:

```text
missing role in old Scene -> VoiceRole::Auto
explicit role -> persisted and user-owned
```

Genre contract:

```text
if role == Auto:
    Genre MAY suggest/resolve an initial role

if role != Auto:
    Genre MUST preserve the explicit user role
```

Role persistence should be a dedicated later PR, not combined with Groove Vocabulary Core.

## H4. New/Clear behavior

When VoiceRole/provenance eventually exist:

```text
New/Clear -> VoiceRole::Auto
New/Clear -> empty generation provenance
```

The existing clear path already resets patterns, songs, PhraseCore, genre, feel and other project state, so the new fields should follow the same ownership rule.

---

# I. Migration plan

The migration is intentionally split so each stage has a rollback boundary and a clear behavioral hypothesis.

## Stage 0 — 0.9 freeze and characterization

### Purpose

Create a stable control baseline before generator v2 work affects production behavior.

### Files/areas

```text
tests/
docs/tests/
existing generator diagnostics only
```

### Behavior change

None.

### Compatibility

100% current behavior.

### Tests

Characterize:

```text
Techno
Rave
Acid
Dub / Deep Chord
Drum & Bass
```

Capture normalized fingerprints, density/relationship-like statistics and listening notes across a fixed seed/sample corpus.

### Acceptance gate

The control group can be reproduced and compared after later stages.

### Out of scope

Any new generator behavior.

---

## Stage 1 — Groove Vocabulary data model

### Purpose

Introduce compact types/catalog contracts only.

### New area

Recommended:

```text
src/generation/rhythm/
```

Possible files:

```text
rhythm_types.h
rhythm_catalog.h/.cpp
rhythm_validation.h/.cpp
```

### Legacy replaced

None.

### Behavior change

None.

### Persistence impact

None.

### Tests

Catalog/static validation.

### Acceptance gate

- no runtime behavior change;
- no heap-owning catalog objects;
- contradictions can be detected on host;
- Cardputer/SDL build remains green.

### Out of scope

Realization, Scene changes, Genre mapping, VoiceRole.

---

## Stage 2 — Relationship resolver + RhythmPhraseRealizer

### Purpose

Implement deterministic legal realization without connecting it to production Generate.

### Areas

```text
src/generation/rhythm/rhythm_realizer.*
src/generation/rhythm/relationship_resolver.*
src/generation/generation_context.*
```

### Legacy replaced

None yet.

### Behavior change

None in production.

### Tests

Property tests over many seeds/P-levels.

### Acceptance gate

```text
protected anchor violations == 0
protected silence violations == 0
hard relationship violations == 0
determinism violations == 0
```

### Out of scope

Scene, UI, role persistence, synth pitch generation.

---

## Stage 3 — Curated reference vocabulary

### Purpose

Add approximately 19–20 archetypes derived from strong current material.

### Areas

```text
rhythm_catalog_reference.*
genre_archetype_map.*
```

### Legacy replaced

None yet.

### Behavior change

None by default.

### Tests

- catalog validation;
- duplicate fingerprint metrics;
- archetype usage entropy;
- legal P1/P2/P3 realization diversity.

### Acceptance gate

Vocabulary produces multiple distinct legal realizations without structural collapse.

### Out of scope

Weak-genre expansion and 50–70 archetype catalog.

---

## Stage 4 — Pattern materializer and shadow backend

### Purpose

Convert `RhythmPhrasePlan` into existing pattern structures and compare against legacy generation without switching normal user behavior.

### Areas

```text
new PatternMaterializer
MiniAcid generation orchestration boundary
ModeManager compatibility boundary
drum generation adapter
```

### Compatibility

Introduce an explicit internal backend concept such as:

```text
LegacyAtlas
LegacyProcedural
Vocabulary
```

This is a migration mechanism, not persisted Scene state.

### Why this is required

`AtlasRuntime::applyRecipe()` currently returns early with concrete events. Without an explicit backend/mapping boundary, some genres/recipes will never reach the new vocabulary path.

### Tests

Side-by-side host metrics and shadow outputs.

### Acceptance gate

Materializer produces valid existing `SynthPattern` / `DrumPatternSet` data transactionally.

### Out of scope

Default user-facing backend switch.

---

## Stage 5 — Migrate strong rhythmic/drum paths

### Purpose

Move rhythmic placement for the control styles to Vocabulary while leaving synth pitch generation legacy-compatible where practical.

### Target control material

```text
Techno
Rave
Acid drums
Dub / Deep Chord drums/stab rhythm
Drum & Bass drums
```

### Legacy being replaced

`DrumGenreTemplate` selection/placement for migrated mappings and direct Genre placement masks for the migrated rhythmic roles.

### Compatibility adapter

`RhythmPhrasePlan -> DrumPatternSet` and temporary synth-rhythm adapter where needed.

### Persistence impact

None.

### Tests

- automated invariant gates;
- normalized fingerprint comparisons;
- listening regression against 0.9 baseline.

### Acceptance gate

No strong-genre regression and sufficient valid realization diversity.

### Rollback boundary

Per-genre/profile backend mapping can return to legacy without reverting unrelated stages.

### Out of scope

VoiceRole and Bass Generator v2.

---

## Stage 6 — BarEvolution transient core

### Purpose

Add 2–4 bar grammar without granting the generator storage ownership.

### Output

```text
RhythmPhrasePlan[1..4 bars]
```

### Critical ownership rule

The realizer NEVER chooses:

```text
page
bank
pattern index
Song row
PhraseCore slot
```

Caller provides explicit materialization targets.

### Tests

- deterministic trajectory selection;
- correct `BarFunction` semantics;
- no storage mutation in the realizer.

### Out of scope

Automatic Song/PhraseCore composition.

---

## Stage 7 — VoiceRole runtime model

### Purpose

Remove musical-role dependence on `voiceIndex`.

### Areas

```text
generation role resolver
MiniAcid generation orchestration
ModeManager compatibility adapter
```

### Compatibility

Initial compatibility behavior:

```text
Synth A + Auto -> legacy Bass suggestion
Synth B + Auto -> legacy Lead suggestion
```

This preserves old expectations while making the mapping explicit and replaceable.

### Persistence impact

None in this stage.

### Tests

- role can differ from physical A/B;
- role does not imply engine TYPE;
- swapping roles does not require changing synth engines.

### Out of scope

Scene codec update.

---

## Stage 8 — VoiceRole persistence

### Purpose

Persist explicit user-owned semantic roles.

### Scene impact

Small versioned optional fields.

### Legacy decode

```text
missing role -> Auto
```

### Tests

- old Scene load;
- Save/reboot/Load role round trip;
- synth engine/parameters remain unchanged by role restore;
- Genre does not overwrite explicit role.

### Acceptance gate

Legacy Scenes load with equivalent materialized patterns and synth state.

---

## Stage 9 — Bass Generator v2

### Purpose

Replace `voiceIndex == 0 -> low random/motif bass` with a role-aware phrase generator.

### Areas

```text
BassPhraseGenerator
ModeManager synth-generation compatibility path
MiniAcid::randomize303Pattern
MiniAcid::regeneratePatternsWithGenre
```

### Compatibility

Non-bass roles may remain on legacy pitch generation temporarily.

### Tests

- bass onset set equals/subsets the supplied rhythm plan as defined;
- motif/register invariants;
- deterministic BassPitch domain;
- protected rests never filled;
- user explicit role respected.

### Acceptance gate

Generated bass becomes structurally coherent without reducing meaningful variation.

---

## Stage 10 — Bass Performance Policy

### Purpose

Add role-aware live bass behavior without passing live notes through generated phrase logic.

### Areas

```text
PerformanceKeyboard
InternalSynthOutput
articulation boundary
SwappableSynthVoice
TB303 adapter
```

### Tests

- note priority;
- legato/glide behavior;
- retrigger behavior;
- velocity/accent mapping;
- stuck-note prevention;
- PatternPlayer/live ownership;
- engine capability fallback.

### Out of scope

Generated BassPhraseGenerator.

---

## Stage 11 — Phrase/Motif vocabulary boundary

### Purpose

Introduce only the minimum pitch/motif interface after rhythm and bass contracts are proven.

Initial boundary:

```text
RhythmPlan
    + HarmonicContext
    + MotifContext
    ↓
PitchPhrase
```

Do not perform a full melodic framework rewrite in this stage.

---

## Stage 12 — weak-genre rehabilitation

Only after the strong control group remains stable, migrate styles whose current output is weak/ringtone-like.

The hypothesis to validate is that a strong multi-role rhythmic plan gives freer melodic generators enough structure to stay musical without reducing diversity.

---

## Stage 13 — vocabulary expansion

Expand toward 50–70 archetypes only after measuring:

```text
usage entropy
duplicate fingerprints
cross-family collisions
listening quality
strong-genre stability
```

Do not expand to meet a numeric target.

---

# J. Runtime budget

## J1. Catalog flash estimate

With compact fixed structures, a realistic starting estimate is:

```text
one archetype structural data: ~180–240 bytes
```

Approximate totals:

```text
20 archetypes:   ~4–6 KB structural, ~6–10 KB including metadata/names
50–70 archetypes:~14–18 KB structural, ~20–30 KB realistic total flash
```

Exact layout must be validated after real C++ declarations and linker map inspection.

## J2. Runtime RAM target

Catalog should remain flash-resident/static.

Transient realization state for up to 4 bars and 8 roles should target:

```text
< 1 KB RAM
```

Preferred engineering target:

```text
~512–768 bytes transient scratch
```

## J3. Heap contract

```text
0 heap allocation during rhythm realization
```

Avoid `std::vector` and other dynamically growing structures in the new embedded generation path.

## J4. Complexity

Bounds:

```text
bars <= 4
roles <= 8
steps = 16
relationships small/fixed
passes small/fixed
```

Most operations should be 16-bit mask operations plus bounded iteration.

No backtracking search with unbounded retries.

## J5. Audio-thread isolation

Hard invariant:

```text
Rhythm realization MUST NOT execute inside the audio rendering callback.
```

Generation happens in control/UI context, builds a complete temporary result, validates it, then materializes transactionally into existing pattern state.

---

# K. Test strategy

## K1. Catalog validation

Every archetype must satisfy:

```text
valid masks
required/forbidden/protected-silence consistency
gate-mask disjointness and declared-onset containment
valid density ranges
valid relationship role references
no obviously impossible hard constraints
valid trajectory references
valid P-level budgets
```

## K2. Relationship unit tests

Test each v1 operation independently:

```text
Exclude
Coincide
Offset
Respond
FillGaps
```

for hard and soft behavior.

## K3. Property tests

For each archetype:

```text
for seed in representative large corpus
for level in P1/P2/P3
```

assert:

```text
required anchors present
protected silence untouched
hard relationships satisfied
forbidden zones empty
explicit Short/Held/Tie gate overlays preserved; Normal remains implicit
density legal or explicitly ValidButSparse
same input -> same output
```

## K4. Determinism contract

```text
same relevant scene generation state
same archetype
same root seed
same P-level
same phrase position
same generation domain

=> same realization
```

Recommended generation domains:

```text
ArchetypeSelection
RhythmSkeleton
DrumsOrnament
BassRhythm
BassPitch
ChordRhythm
ChordPitch
LeadRhythm
LeadPitch
BarEvolution
```

Domain derivation should conceptually use:

```text
derive(projectGenerationSeed,
       archetypeId,
       level,
       phraseOrdinal,
       generationDomain)
```

## K5. Domain-isolation tests

Examples:

```text
change BassPitch seed
EXPECT drum realization unchanged

change LeadPitch seed
EXPECT bass realization unchanged

change BarEvolution seed
EXPECT trajectory may change
AND archetype hard identity remains valid
```

## K6. Strong-family automated gates

### FourFloor / Techno / Rave

```text
hard kick anchor violations == 0
protected silence violations == 0
hard backbeat relationship violations == 0
unique valid realizations >= threshold
```

### DubPulse

```text
protected silence violations == 0
max density respected
required chord/skank relationship respected
P3 does not fill protected space
```

### Breakbeat / D&B

```text
backbeat anchor violations == 0
hard kick/backbeat relationship violations == 0
P3 four-floor collapse == 0
ghost/response events stay legal
```

### UK 2-Step

```text
protected backbeat violations == 0
protected silence violations == 0
hard offset relationship violations == 0
forbidden four-floor collapse == 0
unique valid realization count >= threshold
```

### Acid

Before Bass v2, rhythm gate is based on the selected general archetype.

After Bass v2 add:

```text
motif reuse >= minimum
register violations == 0
protected bass-rest violations == 0
illegal slide/articulation violations == 0
```

## K7. Listening regression

Automated metrics do not replace listening.

Blind A/B should compare legacy vs Vocabulary with matched:

```text
Genre
BPM
synth timbre
FX
output level
```

For each control genre use multiple seeds and compare:

```text
P1
P2
P3
drums only
bass + drums
full mix
```

Rate:

```text
groove coherence
genre identity
repetition fatigue
variation usefulness
bad-generation rate
```

The new core is not accepted if the strong control group becomes less reliable.

---

# L. Quality metrics

## rhythm_family_count

Number of production-enabled families that contain at least one active runtime archetype.

Do not count unused enum values.

## rhythm_archetype_count

Number of enabled curated runtime archetypes.

## multi_lane_relationship_count

Total number of active cross-role relationship clauses in the enabled vocabulary.

Also record:

```text
mean_relationships_per_archetype
```

## phrase_trajectory_count

Number of distinct `BarTrajectory` definitions actually referenced by enabled archetypes.

## archetype_usage_entropy

For a fixed corpus of genres × seeds × P-levels, let `p_i` be the observed frequency of selected archetype `i`.

```text
H = -Σ p_i log2(p_i)
Hnorm = H / log2(active_archetype_count)
```

Low entropy may indicate one archetype dominates. High entropy is not automatically a goal; genre correctness has priority.

## genre_archetype_overlap

Use weighted Jaccard over normalized genre archetype weights:

```text
Σ min(weightA[i], weightB[i])
--------------------------------
Σ max(weightA[i], weightB[i])
```

This quantifies intentional vs accidental vocabulary sharing between genres.

## archetype_variant_count

For one archetype + P-level:

```text
generate N seeds
normalize groove fingerprints
count unique valid fingerprints
```

This measures empirical legal diversity rather than theoretical combinations.

## duplicate_groove_fingerprint_rate

Generate a fixed corpus across archetypes and compare normalized structural fingerprints.

```text
samples colliding with another archetype's normalized fingerprint
-----------------------------------------------------------------
total valid generated samples
```

Track separately:

```text
intra_family_collision_rate
cross_family_collision_rate
```

Cross-family collisions are more suspicious.

## normalized groove fingerprint

The structural fingerprint should intentionally ignore:

```text
BPM
SynthEngineType
timbre
small velocity noise
minor microtiming jitter
low-energy ghost variation
```

It should preserve:

```text
phrase bar count
structural RhythmRole onset masks
required anchors
protected silence
bar functions
major accents
cross-role relationship topology
```

Physical Snare/Clap/Rim should normalize to `Backbeat` when evaluating rhythmic identity.

Recommended two-tier fingerprint:

### Tier 1 — structural masks

```text
Kick per bar
Backbeat per bar
Hat structural mask
BassRhythm structural mask
ChordRhythm structural mask
global protected silence
BarFunction sequence
```

### Tier 2 — relational signature

For important role pairs compute a coarse lag histogram:

```text
-2, -1, 0, +1, +2, other
```

Suggested pairs:

```text
Kick <-> Backbeat
Kick <-> BassRhythm
Backbeat <-> Hats
Kick <-> ChordRhythm
```

This helps detect structurally equivalent grooves whose exact bitmasks differ slightly.

## structural distance

Plain Hamming distance is insufficient.

For valid outputs only, use a weighted distance conceptually composed of:

```text
D =
  w_event    * structural onset delta
+ w_role     * role-identity delta
+ w_relation * relational-signature delta
+ w_silence  * protected-space delta
+ w_bar      * bar-function delta
```

Hard violations are not large distances; they are invalid outputs and are counted separately.

Expected statistical relationship:

```text
median D(P1,P2) > novelty minimum
median D(P1,P2) < archetype identity ceiling
median D(P1,P3) > median D(P1,P2)
```

## protected_anchor_violation_count

Count missing or illegally displaced required anchors.

Target:

```text
0
```

## protected_silence_violation_count

Count events created inside protected-silence zones.

Target:

```text
0
```

## relationship_violation_count

Count hard relationship violations.

Target:

```text
0
```

Soft relationships should use a satisfaction-rate metric instead of a hard failure count:

```text
soft_relationship_satisfaction_rate
```

---

# M. Initial curated runtime vocabulary

The first vocabulary should be approximately 19–20 archetypes, not an arbitrary exact count.

ACID is intentionally not a separate family in v1.

## FourFloor

### 1. `straight_drive`

Canonical straight floor with stable kick anchors and controlled upper-lane variation.

Control use: Techno.

### 2. `offbeat_hat_drive`

Four-floor foundation with a strong offbeat hat/open-hat relationship.

### 3. `hypnotic_sparse`

Four-floor skeleton with reduced upper density and protected gaps.

Control use: minimal/deep techno-like material.

### 4. `rave_push`

Straight floor with denser hat/percussion response and stronger turnaround energy.

Control use: Rave.

## MachineSyncopation

### 5. `broken_techno`

Machine kick skeleton with intentional holes and a stable secondary/backbeat relationship.

### 6. `detroit_sync`

Syncopated machine relationship with percussion response and less rigid four-floor behavior.

### 7. `electro_machine`

Broken kick/backbeat relationship suitable for electro-style spacing.

## DubPulse

### 8. `sparse_skank`

Large protected-silence zones, sparse drum anchors and offbeat chord rhythm.

### 9. `chord_response`

Drum anchors followed or answered by chord rhythm.

### 10. `steppers`

Strong quarter-note propulsion with dub-specific backbeat/chord relationships.

## Breakbeat

### 11. `two_step_roll`

Fast stable backbeat with broken kick relationships.

Control use: D&B.

### 12. `ghosted_roll`

Two-step base with legal ghost continuity.

### 13. `sparse_fast_break`

Reduced kick density while preserving breakbeat backbeat identity.

### 14. `halftime_switch`

Same breakbeat family with an explicitly legal halftime transformation.

### 15. `turnaround_break`

Canonical break identity with strong bar-end transformation behavior.

## UkTwoStep

### 16. `classic_2step`

Stable backbeat, broken kick, shuffle-compatible hats and protected gaps.

### 17. `skippy_2step`

More kick anticipation and percussion response while preserving 2-step identity.

### 18. `sparse_2step`

Lower kick density with more protected silence.

### 19. `shuffled_4x4`

UK shuffle with stronger floor presence but explicit guards against collapsing into generic straight techno.

## Optional twentieth archetype

Do not predeclare one merely to hit the number 20.

Add a twentieth only if extraction/fingerprint analysis from the existing strong corpus reveals a genuinely separate cluster, likely in one of:

```text
FourFloor / broken_push
Breakbeat / rolling_sparse
```

Formal v1 target:

```text
19–20 justified archetypes
```

---

# M1. Acid mapping

Acid should select from general rhythmic archetypes and add bass/articulation identity later.

Conceptual example only:

```text
Acid Genre
    ↓
weighted archetypes

straight_drive
hypnotic_sparse
broken_techno
rave_push
detroit_sync
```

Then:

```text
VoiceRole = AcidBass
BassPitchStrategy = AcidContour
Articulation = slide/accent capable
```

This is more reusable than `RhythmFamily::Acid`.

---

# M2. Bass Generator v2 strategy vocabulary

The original list contains several concepts that belong to rhythm or contour rather than top-level pitch strategy.

Do not make these independent pitch strategies by default:

```text
SYNCOPATED     -> RhythmPlan property
ROLLING        -> RhythmPlan property
CALL_RESPONSE  -> relationship / BarEvolution
DESCENDING     -> contour modifier
UPWARD_PUSH    -> contour modifier
APPROACH_NOTE  -> note-selection modifier
WALK           -> composition of chord-tone + stepwise behavior
```

Recommended minimal top-level set:

```cpp
enum class BassPitchStrategy : uint8_t {
    Pedal,
    ChordTone,
    OctaveMotion,
    StepwiseMotion,
    AcidContour,
};
```

`ChordTone` may use root/fifth/third preference profiles instead of multiplying strategy enums.

## Anti-ringtone bass contract

Bass generation should keep a small bounded motif state such as:

```text
2–4 interval motif
anchor/root note
last note
phrase direction
change budget
```

Invariants:

```text
bounded register
bounded large-leap rate
limited pitch-class count per bar
minimum motif reuse
anchor-note preference
repetition before novelty
approach notes must resolve toward a target
octave displacement preserves harmonic function
rests come from RhythmPlan
```

Randomness selects among legal phrase decisions; it does not independently choose every next pitch.

---

# N. Open architectural questions

Only issues that remain genuinely unresolved after the code audit are listed here.

## N1. Backbeat physical binding

### Question

Should runtime vocabulary distinguish Snare, Clap and Rim structurally?

### Recommended default

No. Use `RhythmRole::Backbeat` and a separate physical binding/materialization policy.

### Alternative

Expose Snare/Clap/Rim as independent RhythmRoles.

### Consequence

The alternative couples vocabulary to the current eight-voice drum implementation and makes reuse harder.

---

## N2. Live bass vs PatternPlayer ownership during playback

### Question

How should direct internal live bass behave while transport is playing?

Current `MiniAcid::liveNoteOn()` rejects internal live input while playback is active.

### Recommended default

Target-local live override:

```text
while a live key is held:
    LivePerformance owns that synth voice
    PatternPlayer triggers for that target are suppressed

after final live release:
    PatternPlayer resumes on the next sequencer event
```

### Alternative

Keep current mutual exclusion: internal live synth only while stopped.

### Consequence

The alternative is simpler but prevents the intended Bass Performance workflow from acting as a live instrument over a running groove.

This decision should be implemented only in the later Bass Performance stage.

---

## N3. VoiceRole persistence timing

### Recommended default

Implement runtime `Auto` resolution first, then add Scene persistence in a separate PR.

### Alternative

Change Scene schema in the same PR that introduces VoiceRole.

### Consequence

The alternative enlarges migration/rollback surface before the role model itself is proven.

---

## N4. Multi-bar destination allocation

### Question

Who chooses pattern slots for a 2–4 bar realization?

### Recommended default

The caller provides explicit materialization targets.

```text
RhythmPhraseRealizer
    -> RhythmPhrasePlan

caller/storage layer
    -> MaterializationTarget[]
    -> PatternMaterializer
```

### Alternative

Let the generator search for free pages/banks/pattern slots.

### Consequence

The alternative incorrectly makes the generator an owner of Song, pattern paging and storage/navigation state and risks conflict with the existing matrix/PhraseCore architecture.

---

# O. Recommended documentation tree after architecture approval

The minimum non-duplicative documentation set should be:

```text
docs/architecture/
    GENERATION_ARCHITECTURE.md
    RHYTHM_VOCABULARY.md
    RHYTHM_CONTRACTS.md
    RHYTHM_RELATIONSHIPS.md
    BAR_EVOLUTION.md
    VOICE_ROLES.md
    BASS_GENERATION.md
    LIVE_PERFORMANCE_ARCHITECTURE.md
    GENERATION_DETERMINISM.md
    GENERATION_PERSISTENCE.md
    ATLAS_RUNTIME_BOUNDARY.md
    GENERATOR_MIGRATION_PLAN.md

docs/testing/
    GENERATOR_QUALITY_METRICS.md
    GENERATOR_V2_ACCEPTANCE.md
    STRONG_GENRE_REGRESSION.md
```

Optional ADRs are useful only for decisions likely to be revisited:

```text
docs/adr/
    ADR_RHYTHM_VOCABULARY_LAYER.md
    ADR_VOICE_ROLE_SEPARATION.md
    ADR_ATLAS_RUNTIME_SEPARATION.md
```

Do not split every subsection of this brief into a separate file until implementation starts; otherwise documentation will duplicate itself before contracts stabilize.

---

# Normative architecture invariants

These rules should become the normative front matter of the future architecture documents.

## G-01 — Genre placement ownership

```text
Genre MUST NOT directly place rhythm events.
```

## G-02 — Rhythm/pitch separation

```text
RhythmArchetype MUST NOT own concrete pitch or SynthEngineType.
```

## G-03 — Grammar, not preset

```text
RhythmArchetype MUST describe a multi-role grammar,
not merely one fixed 16-step pattern bitmap.
```

Bitmasks are allowed as compact representations of anchors/zones.

## G-04 — Protected silence priority

```text
Protected silence MUST outrank density, fills,
variation and soft relationships.
```

## G-05 — Hard relationship validity

```text
A hard relationship violation is generation failure.
```

## G-06 — P-level realization

```text
P1/P2/P3 MUST independently realize grammar.
They MUST NOT be defined as cumulative random mutation
of previously materialized events.
```

## G-07 — Physical track vs musical role

```text
VoiceRole MUST be independent from SynthA/SynthB.
```

## G-08 — Role vs engine

```text
VoiceRole MUST be independent from SynthEngineType.
```

## G-09 — Bass rhythm authority

```text
BassPhraseGenerator MUST NOT create rhythmic onsets
outside BassRhythmPlan.
```

## G-10 — Live/generative separation

```text
Live keyboard performance MUST NOT pass through BassPhraseGenerator.
```

## G-11 — Genre/timbre ownership

```text
Genre MAY propose initial timbre during explicit generation,
but MUST NOT continuously project timbre over persisted
or user-edited synth state.
```

## G-12 — Persistence source of truth

```text
Materialized Scene pattern events remain the playback source of truth.
```

## G-13 — Deterministic domains

```text
Rhythm realization MUST use explicit deterministic RNG domains.
```

## G-14 — RNG isolation

```text
Changing one generation domain MUST NOT consume RNG state
from unrelated domains.
```

## G-15 — Heap budget

```text
Rhythm realization MUST NOT allocate from the heap.
```

## G-16 — Audio isolation

```text
Rhythm realization MUST NOT run in the audio rendering path.
```

## G-17 — Storage ownership

```text
RhythmPhraseRealizer MUST NOT choose Song position,
page, bank, pattern slot or PhraseCore slot.
```

## G-18 — Atlas boundary

```text
Runtime Rhythm Vocabulary MUST NOT depend on exact Atlas pattern IDs
or named-break transcriptions.
```

## G-19 — Diversity with identity

```text
An archetype is valid only if it yields multiple distinct legal
realizations while preserving its hard structural identity.
```

## G-20 — Strong-genre regression gate

```text
Generator v2 is not accepted if Techno, Rave, Acid,
Dub/Deep Chord or Drum & Bass regress against the 0.9 control baseline.
```

---

# Atlas separation contract

Target data flow:

```text
Atlas / Mood Lab corpus
        ↓
offline analysis / extraction
        ↓
candidate structural clusters
        ↓
manual musical curation
        ↓
Runtime RhythmFamily / RhythmArchetype catalog
        ↓
Generator
```

## Safe automatic extraction candidates

Atlas tooling may automatically derive candidate statistics such as:

```text
normalized lane onset masks
anchor frequency by role/step
co-occurrence and exclusion rates
relative offset histograms
per-role density ranges
bar-to-bar structural distance
swing/microtiming distribution summaries
groove fingerprints / duplicate clusters
```

## Requires manual curation

Do not automatically promote the following directly into runtime rules:

```text
which correlation is musically structural vs incidental
which silence is protected
which relationships are hard vs soft
which examples belong to the same archetype
family/archetype naming
named-break legal/copyright-sensitive transcription decisions
P1/P2/P3 transformation budgets
```

## Runtime MUST NOT know

```text
Atlas row IDs
editorial corpus filenames
named-break source identity
exact source transcription identity
research-only annotations
```

The runtime catalog contains generalized musical grammar only.

---

# Recommended first post-0.9 milestone

Name the first implementation milestone:

```text
Groove Vocabulary Core v1
```

Definition of Done:

```text
[ ] <= 8 justified initial RhythmFamily categories
[ ] ~19–20 curated strong-material archetypes
[ ] LaneGrammar with required/preferred/optional/forbidden/protectedSilence
[ ] global protected silence
[ ] 5-operation relationship vocabulary
[ ] hard/soft deterministic resolver
[ ] P1/P2/P3 fresh-realization contracts
[ ] reusable BarEvolution model
[ ] explicit deterministic domains
[ ] zero heap during realization
[ ] < 1 KB transient realization state target
[ ] Pattern materialization adapter
[ ] no Scene codec change
[ ] no Song/pattern allocation ownership
[ ] no VoiceRole migration in the core milestone
[ ] host property tests
[ ] normalized fingerprint metrics
[ ] strong-genre shadow comparison
```

The milestone tests one central hypothesis only:

> A coherent multi-role rhythmic grammar can preserve or improve GroovePuter's strong genres while giving future freer generators a stable musical skeleton without reducing meaningful variation.

Only after this hypothesis passes the regression/listening gate should development continue into:

```text
VoiceRole
    ↓
Bass Generator v2
    ↓
Bass Performance Policy
    ↓
Phrase/Motif Vocabulary
    ↓
weak-genre rehabilitation
```

---

# Architecture acceptance criteria

The architecture is acceptable only if all of the following remain simultaneously true:

1. Genre no longer directly owns individual rhythm placement.
2. Groove is realized from a coherent multi-role grammar.
3. Variation cannot destroy protected groove identity.
4. P1/P2/P3 are legal fresh grammar realizations rather than cumulative random event mutations.
5. 2–4 bar development is first-class but does not own Song/pattern storage.
6. Synth A/B no longer define musical role.
7. Bass rhythm is constrained by the groove plan.
8. Bass pitch phrase generation is a separate layer.
9. Live bass remains a direct performance instrument.
10. Genre does not reclaim ownership of user-edited timbre.
11. Atlas is separated from runtime vocabulary.
12. Materialized Scene patterns remain playback source of truth.
13. Runtime remains bounded, deterministic and inexpensive enough for ESP32-S3.
14. Strong existing genres do not regress.
15. Meaningful variation increases or remains high; the solution does not rely on reducing variety.
16. Migration can be delivered through multiple small PRs with per-stage rollback boundaries.
17. Groove Vocabulary Core can be validated before VoiceRole/Bass v2 changes are introduced.

No production implementation should begin until this audited architecture brief is reviewed and accepted.