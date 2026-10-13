# GroovePuter Unified Harmonic System — Design

**Date:** 2026-10-13  
**Status:** design specification for review  
**Decision:** adopt Option 2 — one universal harmonic graph with genre-specific navigation policies and per-song variation genomes.  
**Repository:** `Ostrovsky42/GroovePuter`  
**Scope:** harmonic identity and harmonic-route generation. Rhythm, motif, feel, articulation, physical binding and Scene ownership keep their existing architectural authority.

---

## 1. Problem

GroovePuter already has a broad genre corpus, many patterns, recipes, archetypes and harmonic examples, but generated songs can still feel structurally repetitive inside a genre.

The current `ChordProgression` backend is intentionally compact: it selects from a small set of named `ProgressionId` grammars, each with one or two fixed variants and an intrinsic period of at most four harmonic events. AUTO selection is primarily keyed by `RhythmFamily`. This is deterministic and embedded-safe, but it creates a small number of recognizable harmonic trajectories even when the rhythmic and tonal material around them changes.

The target is not "more chord recipes". The target is a larger space of legal musical routes while retaining genre identity, deterministic replay and bounded ESP32-S3 cost.

The new mental model is:

```text
Genre
  -> harmonic corridor
  -> SongVariantGenome
  -> section intent
  -> bounded navigation through one harmonic graph
  -> ChordProgression WHAT events
  -> existing ChordRhythm WHEN
  -> tonal materialization / role projection
```

This follows the existing composition rule:

> Independent responsibility != independent randomization.

The harmonic system expands legal choices; it does not bypass upstream or downstream ownership.

---

## 2. Goals

1. Increase **intra-genre variation** without erasing genre identity.
2. Replace a growing collection of hard-coded progressions with a **small generative rule system**.
3. Preserve the existing architectural split:
   - `ChordProgression` owns **WHAT** harmony advances to;
   - `ChordRhythm` / harmonic timeline owns **WHEN** it advances.
4. Keep generation deterministic from stable seeds.
5. Keep runtime state fixed-capacity, heap-free and suitable for Cardputer ADV / ESP32-S3 without PSRAM.
6. Make song sections musically different without making them unrelated:
   - TAKE establishes the world;
   - KEEP stays close;
   - DEVELOP explores;
   - BREAK may leave the local center;
   - RETURN deliberately finds home.
7. Detect repetition structurally, including transposed copies of the same progression.
8. Preserve current legacy progressions as compatibility paths and regression witnesses.

---

## 3. Non-goals

This design does not:

- replace Groove Vocabulary rhythm ownership;
- make Genre emit concrete notes;
- introduce a global mutable "music brain" object;
- add heap allocation during generation or playback;
- require an LLM or network connection;
- make every genre harmonically complex;
- force polychords, augmented harmony or functional cadences into genres where they are stylistically inappropriate;
- change Scene authority after materialization;
- couple harmonic generation to a particular Synth A/B/SEQTRAK destination;
- delete the existing `ProgressionId` grammar set in the first migration stage.

---

# 4. Canonical harmonic space

The graph is transposition-invariant. Nodes describe harmonic states; edges describe legal transformations.

## 4.1 Harmonic node

Conceptually:

```text
HarmonicNode
  rootPitchClass : 0..11
  qualityFamily  : Major | Minor | Dominant | Diminished | Augmented | Sus | Extended
  tonalRelation  : optional relation to current home center
```

The graph vocabulary must not be forced into the current `ChordQuality` enum if that would lose harmonic meaning. A graph node is semantic input. Projection into the existing `HarmonicEvent { degree, quality, rootOffsetSemitones }` carrier happens at the boundary that already owns chord progression materialization.

A graph state is therefore independent from voicing, octave, physical synth and concrete MIDI notes.

---

# 5. Edge families

The universal graph is not stored as a giant adjacency table. Neighbors are derived from compact rules.

Each candidate edge has:

```text
family
operator
source
candidate target
base cost
voice-leading cost
home-distance delta
tension delta
```

## 5.1 DIATONIC

Movement among legal diatonic harmonic functions in the active tonal center.

Purpose:

- ordinary tonal continuity;
- stable TAKE/KEEP material;
- strong genre identity where conventional harmony matters.

The exact allowed functions are supplied by the active tonal/harmonic policy, not by a universal major-only table.

---

## 5.2 PRL — Neo-Riemannian neighbor graph

For root pitch class `r`, modulo 12:

```text
Major(r):
  P -> Minor(r)
  R -> Minor(r + 9)
  L -> Minor(r + 4)

Minor(r):
  P -> Major(r)
  R -> Major(r + 3)
  L -> Major(r + 8)
```

Examples from C major:

```text
P: C -> Cm
R: C -> Am
L: C -> Em
```

All three preserve two chord tones. Approximate local voice-leading cost:

```text
P = 1 semitone
L = 1 semitone
R = 2 semitones
```

Use:

- smooth harmonic variation;
- common-tone movement;
- Deep House, UKG, Lo-Fi, Ambient, Chillout, Detroit, Synthwave;
- controlled excursions that do not sound random.

PRL is a relation system, not a genre by itself.

---

## 5.3 FUNCTIONAL — dominant bridge

Any legal target chord may optionally be approached through its dominant:

```text
target root = r
V7(target) = Dominant7(r + 7)
```

Examples:

```text
C  -> E7 -> Am
C  -> B7 -> Em
Cm -> Eb7 -> Ab
```

This transforms a smooth graph edge into a directed tension-resolution gesture.

A route therefore distinguishes:

```text
C -> Am
```

from:

```text
C -> E7 -> Am
```

although both resolve to the same target.

---

## 5.4 FUNCTIONAL — ii-V-I and dominant preparation

For tonic root `T`, the major-mode canonical form is:

```text
ii  = Minor(T + 2)
V7  = Dominant7(T + 7)
I   = Major(T)

ii -> V7 -> I
```

Minor/modal equivalents are supplied by tonal policy rather than assumed globally.

This family is strongly weighted in Salsa, Jazz-influenced Lo-Fi/Hip-Hop/UKG and other genres that benefit from functional motion; it is low or forbidden in Acid, EBM and Footwork unless explicitly requested.

---

## 5.5 MODAL / BORROWED

Modal mixture and borrowed-function edges preserve a home center while changing mode or importing a nearby function.

Typical relations include, depending on active mode and genre policy:

```text
bIII
iv
bVI
bVII
I <-> i
major/minor parallel shifts
```

This family generalizes material currently represented by fixed progressions such as `MinorFall` and `BorrowedLift` without requiring a fixed four-chord order.

---

## 5.6 CHROMATIC

Chromatic movement is a bounded relation, not unrestricted random pitch selection.

Candidate operations may include:

```text
root +/- 1 semitone
root +/- 2 semitones when admitted by genre
chromatic mediant relation
tritone relation when admitted by genre
```

The genre and section policy control whether these candidates exist at all.

Examples of intended usage:

- EBM / Electro: relatively high;
- Trap / Hip-Hop: medium;
- Deep House: low;
- Dub Techno: low;
- Salsa: very low.

---

## 5.7 AUGMENTED PORTAL

The collected harmonic graph includes the directed cadence:

```text
ii -> V7 -> V+ -> I
```

Example:

```text
Dm -> G7 -> G+ -> C
```

The useful local resolution is:

```text
G7 = G B D F
G+ = G B D#
C  = C E G

G7 -> G+: D -> D#
G+  -> C : B -> C, D# -> E, G stays common
```

Augmented triads also form four pitch-class equivalence classes because the structure repeats every major third. This makes an augmented state a controlled modulation portal rather than a free random jump.

Portal use must be rare and section-sensitive. It is primarily a DEVELOP/BREAK tool and, in some genres, a strong RETURN preparation.

---

## 5.8 DIMINISHED / PASSING PORTAL

Diminished harmony may later serve the same architectural role as an augmented portal: a bounded connector between otherwise distant functions.

It is part of the universal vocabulary but starts disabled except where existing tonal materialization can represent it safely. No implementation stage may silently expand representational capability just because the graph can describe it.

---

# 6. POLYCHORD is a vertical layer, not a route edge

The collected "polychord star" is not fundamentally a next-chord graph. It describes possible upper structures over a base harmony.

For the seven diatonic triads of C major:

```text
C, Dm, Em, F, G, Am, Bdim
```

there are 21 unordered pairs and 42 directional root interpretations.

Example:

```text
base C  = C E G
upper Dm = D F A
union    = C D E F G A
```

Interpreted from C this produces an extended C color; interpreted from D it produces a different D-minor extension.

Architecture rule:

```text
base harmonic route
        +
optional upper-structure color
        -> role-specific pitch-class policy
```

The polychord layer MUST NOT change the base route identity.

For monophonic engines, "polychord" means a pitch-class weighting policy rather than simultaneous six-note playback. For example:

```text
base harmony: Cm9
Bass role:    C, G
303 role:     C, D, Eb, G
Pad role:     Eb, G, Bb, D
Lead role:    G, Bb, D
```

The same harmonic state can therefore coordinate independent parts without forcing polyphony into the 303/SID/AY engines.

---

# 7. One graph, different genre policies

Genre owns a **corridor**, not a progression.

A genre policy supplies weights and hard masks over edge families plus mobility limits. It never emits notes.

Use integer weights:

```text
0 = forbidden by default
1 = rare
2 = low
3 = medium
4 = strong
```

These are starting priors for listening/corpus tuning, not universal music-theory truths.

| Genre | DIA | PRL | FUNC | MODAL | CHR | AUG | POLY | Mobility | Home gravity |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Acid | 2 | 1 | 0 | 2 | 2 | 0 | 0 | 1 | 4 |
| Detroit Techno | 2 | 3 | 1 | 2 | 2 | 1 | 1 | 3 | 2 |
| Electro | 2 | 2 | 1 | 2 | 3 | 1 | 1 | 3 | 2 |
| EBM | 1 | 1 | 0 | 3 | 4 | 1 | 0 | 2 | 3 |
| UK Garage | 3 | 3 | 3 | 2 | 1 | 1 | 3 | 4 | 2 |
| Dub Techno | 2 | 2 | 1 | 2 | 1 | 0 | 2 | 1 | 4 |
| Ambient | 2 | 4 | 1 | 3 | 2 | 3 | 4 | 4 | 1 |
| Lo-Fi | 3 | 3 | 3 | 3 | 1 | 1 | 3 | 3 | 2 |
| Chillout | 3 | 3 | 2 | 2 | 1 | 1 | 3 | 3 | 2 |
| Deep House | 3 | 3 | 2 | 2 | 1 | 1 | 3 | 3 | 3 |
| Jungle | 2 | 1 | 1 | 2 | 2 | 0 | 0 | 1 | 4 |
| Trap | 2 | 2 | 1 | 3 | 3 | 1 | 1 | 2 | 3 |
| Hip-Hop | 3 | 3 | 2 | 3 | 2 | 1 | 3 | 3 | 2 |
| Footwork | 1 | 1 | 0 | 2 | 3 | 0 | 0 | 1 | 4 |
| DnB | 2 | 1 | 1 | 2 | 2 | 0 | 1 | 1 | 4 |
| Salsa | 4 | 0 | 4 | 1 | 1 | 0 | 1 | 4 | 3 |
| Synthwave | 4 | 3 | 2 | 2 | 1 | 2 | 2 | 3 | 2 |

### Critical rule

Variation must **not** be increased equally across genres.

Examples:

- Acid obtains most variation from 303 note grammar, accent, slide, octave, rests and pattern mutation; harmony stays centered.
- Jungle/DnB obtain most variation from break/bass transformation; harmony changes slowly and sectionally.
- UKG/Deep House/Lo-Fi can use much richer harmonic navigation and voicing variation.
- Ambient may use long PRL walks, portals and upper structures.
- Salsa should favor functional routing rather than PRL wandering.

A genre remains recognizable because harmony is only one constrained layer among tempo, groove, bass grammar, chord rhythm, voicing, articulation and arrangement.

---

# 8. SongVariantGenome

A fixed genre policy alone would eventually become recognizable. Therefore each generated song receives one deterministic, bounded harmonic personality.

Conceptual fields, each quantized to a small integer range:

```text
SongVariantGenome
  homeGravity        0..3
  harmonicMobility   0..3
  functionalBias     0..3
  modalBias          0..3
  chromaticRisk      0..3
  portalRisk         0..3
  verticalColor      0..3
  repeatTolerance    0..3
  cadenceStrength    0..3
```

The genome is derived once from:

```text
project/song seed
+ Genre identity
+ stable harmonic domain salt
```

and remains stable across all sections of the same song unless the user explicitly regenerates the song identity.

The genome **biases inside the genre corridor**. It may never enable an edge family that Genre forbids.

Example Deep House personalities:

```text
A: conservative / tonic-heavy / mostly R-L / few dominants
B: soulful / secondary dominants / richer upper structures
C: hypnotic / pedal bass / few base-chord changes / upper colors move
D: adventurous / weaker home gravity / borrowed harmony / one BREAK portal
```

All are Deep House; their route behavior differs.

---

# 9. Section policy: TAKE -> KEEP -> DEVELOP -> BREAK -> RETURN

The existing musical-development form becomes a tension/navigation policy rather than a fixed progression recipe.

## TAKE

Intent: establish the song's harmonic world.

```text
home gravity: maximum
novelty: low
graph distance from home: 0..1
portal: disabled
functional bridge: only if genre strongly prefers it
```

## KEEP

Intent: preserve identity while proving that the material can move.

```text
prefer DIATONIC / PRL near-neighbors
max local route distance: 1
repetition allowed but exact recent route gets a penalty
```

## DEVELOP

Intent: create recognizable development without losing ancestry.

```text
route distance: 1..2
functional bridges may appear
borrowed/modal edges may appear
polychord color may increase
one controlled chromatic move if genre/genome permits
```

## BREAK

Intent: create the largest contrast permitted by the selected genre and song personality.

```text
route distance: 2..4 for mobile genres
chromatic and portal families may unlock
home gravity decreases
vertical color may peak
static genres change another dimension rather than being forced to modulate
```

For Acid/Jungle/DnB/Footwork, BREAK does not imply "more chords". Their harmonic policy may remain nearly static while rhythm, bass, density or register carries the contrast.

## RETURN

Intent: deliberately re-establish home.

RETURN is not ordinary weighted random navigation.

Allowed return mechanisms, filtered by Genre:

```text
direct near-neighbor -> home
V7 -> I/i
ii -> V7 -> I/i
V7 -> V+ -> I/i
bounded shortest route through legal graph edges
```

Novelty weight falls; cadence and home-target scores dominate.

---

# 10. Bounded route selection

Do not implement a general unbounded graph search for every event.

At each harmonic decision:

1. Derive the small set of legal neighbor candidates from the current node.
2. Remove candidates forbidden by Genre.
3. Remove candidates forbidden by section policy.
4. Apply SongVariantGenome biases.
5. Score candidates using integer arithmetic.
6. Apply structural repetition penalties.
7. Select deterministically from the remaining weighted candidates.

Conceptual score:

```text
score =
    genreWeight
  + sectionWeight
  + genomeBias
  + voiceLeadingPreference
  + desiredTensionFit
  + cadenceFit
  + returnHomeBonus
  - homeDistancePenalty
  - recentEdgePenalty
  - recentRoutePenalty
```

No floating point is required.

A candidate set becoming empty is an explicit bounded failure, not permission to widen genre constraints silently.

Fallback order:

```text
1. legal DIATONIC/PRL hold or near-neighbor
2. hold current harmony
3. legacy safe progression backend when the caller requested compatibility mode
```

---

# 11. Structural anti-repetition memory

Exact note comparison is insufficient.

These progressions are structurally the same idea:

```text
C  -> Am  -> F  -> G
D  -> Bm  -> G  -> A
E  -> C#m -> A  -> B
```

The anti-repeat system therefore fingerprints relative musical structure, not absolute pitch.

A normalized route fingerprint includes:

```text
relative root deltas
quality-family transitions
edge-family sequence
operator sequence (P/R/L/etc.)
functional-bridge positions
section tension contour
return/cadence class
```

Pitch-class transposition is removed before hashing.

Recommended first carrier:

```text
uint64_t HarmonicFingerprint
```

Session memory can be a small fixed ring, for example 8-16 recent song/section fingerprints. This costs only tens to low hundreds of bytes and requires no heap.

Penalties occur at two levels:

```text
local: avoid repeating the same edge/operator pattern inside one song
session: avoid generating the same normalized song/section route again soon
```

The system should penalize similarity rather than reject every collision. Constrained genres such as Acid must be allowed to remain harmonically static without entering failure loops.

---

# 12. Relationship to existing `ChordProgression`

The current public architectural contract remains valuable:

```text
ChordProgression = WHAT
ChordRhythm      = WHEN
```

The unified graph becomes a new **WHAT selection strategy**, not a competing timing system.

Recommended boundary:

```text
Harmonic strategy selection
    |- legacy fixed grammar backend
    `- graph navigator backend
             |
             v
     bounded harmonic source/events
             |
             v
      ChordProgression public carrier
             |
             +---- ChordRhythm supplies event timing
             v
      tonal materialization
```

Legacy `ProgressionId` values are append-only and must not be renumbered because their numeric values participate in deterministic seed behavior.

Migration must therefore preserve all existing IDs and replay tests. The graph strategy is added without silently changing the semantics of existing IDs.

The first implementation does not need to enlarge `kMaxChordProgressionSourceEvents == 4` or `kMaxHarmonicEvents == 8`.

Longer song-scale variation is achieved by generating section-local bounded routes from one stable SongVariantGenome and carrying the terminal harmonic state into the next section. The whole 20-bar song does not need to live in one large RAM-resident graph plan.

---

# 13. Song-scale continuity without large storage

Each section receives:

```text
section start node
section intent
SongVariantGenome
GenerationContext
recent harmonic fingerprint state
```

and generates a bounded local route.

At the boundary:

```text
terminal node of TAKE
  -> start node of KEEP
  -> start node of DEVELOP
  -> start node of BREAK
  -> RETURN targets original home identity
```

This prevents the sections from becoming five unrelated mini-songs while avoiding a large full-song chord array.

RETURN stores or reconstructs the original home target from stable song identity rather than guessing it from the current node.

---

# 14. Role-aware realization

A harmonic state constrains pitch choice but does not directly emit every role's notes.

Example semantic state:

```text
Cm9 = {C, Eb, G, Bb, D}
```

Possible role projections:

```text
Bass:   C, G
Acid:   C, D, Eb, G with genre-specific note grammar
Chord:  Eb, G, Bb, D
Lead:   G, Bb, D
```

Therefore the graph increases coherence between independently generated roles without forcing identical realization.

Normative ownership:

```text
Harmony says which pitch classes/functions are legal or preferred.
Role strategy decides how its own legal onsets use that harmonic state.
Harmony must not create new rhythm onsets.
Physical binder must not alter harmonic identity.
```

---

# 15. Determinism and RNG domains

The graph system must follow the existing stable-seed architecture.

A dedicated append-only harmonic navigation RNG domain should be added during implementation rather than consuming an unrelated existing random stream.

Conceptually:

```text
song genome seed = root generation context + genre + HarmonicGenome domain
route seed       = song genome identity + section + HarmonicRoute domain
edge choice      = route seed + global harmonic ordinal
polychord seed   = harmonic state identity + role/section + HarmonicColor domain
```

Rules:

- same input seed and same upstream semantic identities => same output;
- adding a downstream polychord decision must not perturb rhythm, bass rhythm or unrelated harmonic-route choices;
- enum/domain values are append-only once released;
- no shared mutable global RNG stream.

---

# 16. Embedded constraints

The production target remains Cardputer ADV / ESP32-S3 with no PSRAM assumption.

Required implementation properties:

- fixed-capacity structures;
- no heap in generation/playback paths;
- no stored universal adjacency matrix;
- derive neighbors algorithmically;
- small integer weights and scores;
- bounded candidate count;
- bounded RETURN search depth;
- no recursive unbounded traversal;
- explicit `sizeof` guards for new carriers;
- host tests plus target build/DRAM gates.

A graph with 24 PRL major/minor states does not require 24 stored nodes: roots and transformations can be calculated from pitch classes. The same principle applies to dominant bridges and augmented portals.

---

# 17. Why the graph addresses the current repetition problem

The old compact backend effectively chooses among a small number of complete routes.

The new backend chooses among **relationships at each bounded decision point**, while the song genome keeps those choices correlated.

Bad model:

```text
Genre -> choose one of N progressions -> repeat it
```

Target model:

```text
Genre corridor
  + song personality
  + section intent
  + current harmonic state
  + recent structural memory
  -> choose next legal relationship
```

This increases combinatorial variety without increasing uncontrolled entropy.

Crucially, novelty is measured structurally. Transposing an old route does not count as a new idea.

---

# 18. Genre examples

## 18.1 Deep House

Possible genome A:

```text
high home gravity
medium PRL
medium functional
low chromatic
medium/high upper color
```

Route:

```text
TAKE:    Cm
KEEP:    Cm -> Ab
DEVELOP: Ab -> Eb -> G7 -> Cm
BREAK:   Cm -> Eb -> upper Fm color -> Ab
RETURN:  Dm7b5-like preparation only if supported -> G7 -> Cm
```

A different seed may stay mostly on pedal harmony and vary upper structures instead. Both remain legal Deep House profiles.

## 18.2 Acid

```text
TAKE:    Cm pedal
KEEP:    Cm
DEVELOP: Cm -> Ab -> Cm
BREAK:   optional C major/minor parallel flash or chromatic root shift
RETURN:  Cm
```

The harmonic system deliberately does less. Variation should primarily come from 303 grammar, accent, slide, octave, rests and pattern evolution.

## 18.3 Ambient

```text
TAKE:    C
KEEP:    C -> Em
DEVELOP: Em -> G -> Gm -> Eb
BREAK:   Eb -> augmented portal -> remote but legal center
RETURN:  bounded common-tone / portal path -> C
```

Long holds, register and upper structures can create variation without rapid harmonic rhythm.

## 18.4 UK Garage

```text
TAKE:    Cm9
KEEP:    Cm9 -> Abmaj7
DEVELOP: Abmaj7 -> Bb7 -> Ebmaj7 -> G7 -> Cm9
BREAK:   borrowed/modal color + richer upper structure
RETURN:  ii-like preparation -> G7 -> Cm9
```

Here harmony and voicing are high-value variation axes alongside syncopation.

## 18.5 Jungle / DnB

Base harmony may stay on one or two centers for many bars. Section contrast comes mainly from break/bass transformation. The graph is used at section boundaries, not forced into every bar.

## 18.6 Salsa

Prefer directed functional movement:

```text
ii -> V -> I
secondary dominants
circle-related motion
```

PRL and augmented wandering remain disabled or rare. The same universal graph is filtered into a functional corridor.

---

# 19. Testing strategy

## 19.1 Pure graph tests

For every operator:

- transposition invariance;
- pitch-class modulo correctness;
- PRL involution/relationship correctness where applicable;
- dominant target correctness;
- augmented equivalence-class correctness;
- forbidden-family filtering;
- bounded candidate count.

Examples:

```text
P(C) == Cm
R(C) == Am
L(C) == Em
V7(Am) == E7
V7(Cm) == G7
```

## 19.2 Determinism tests

- same seed, same genre, same section => byte-identical semantic route;
- changing only polychord-color seed does not change base route;
- changing only rhythm decisions does not consume harmonic RNG state;
- existing legacy progression seeds remain unchanged.

## 19.3 Genre corridor tests

Generate large deterministic seed sweeps and assert hard invariants:

- Acid never emits forbidden polychord/augmented edges by default;
- Salsa strongly routes through admitted functional edges and never requires PRL;
- Ambient can reach greater graph distance during BREAK than TAKE;
- Jungle/DnB harmonic change rate remains bounded by policy;
- RETURN ends at the stored home target or reports explicit failure.

## 19.4 Anti-repeat tests

- transposed `I-vi-IV-V` routes normalize to the same structural class;
- identical edge-family/operator sequences collide intentionally;
- different meaningful routes do not collapse trivially;
- constrained static genres do not enter rejection loops.

## 19.5 Memory/build gates

- `sizeof` assertions for every persistent/command-time carrier;
- no dynamic allocation in graph generation;
- host GCC/Clang/ASan/UBSan gates where already used;
- Cardputer ADV build;
- static DRAM comparison against the pre-change baseline;
- target listening run after structural tests are green.

---

# 20. Listening acceptance

Code correctness is necessary but insufficient.

For each priority genre, generate a fixed seed suite and compare old vs new behavior.

Priority listening set:

```text
Deep House
UK Garage
Ambient
Lo-Fi
Acid
Jungle/DnB
EBM
Salsa
Synthwave
```

Questions:

1. Can 10 consecutive songs in one genre be distinguished by harmonic trajectory?
2. Does the genre remain identifiable before looking at the label?
3. Does DEVELOP actually develop rather than merely replace material?
4. Is BREAK different for a musically relevant reason?
5. Does RETURN sound like return rather than another random regeneration?
6. Are constrained genres harmed by unnecessary chord motion?
7. Do repeated transposed routes still appear too often?

Any genre that gains novelty but loses identity is a failed tuning, not a success.

---

# 21. Migration stages

## Stage HGS-0 — Research/spec freeze

- freeze this graph vocabulary and ownership model;
- keep production unchanged;
- add no UI or Scene fields.

## Stage HGS-1 — Pure graph core

- semantic node representation;
- DIATONIC/PRL/FUNCTIONAL/MODAL/CHROMATIC/AUG neighbor derivation;
- fixed candidate carrier;
- pure host tests;
- no production routing.

## Stage HGS-2 — Genre corridor + SongVariantGenome

- genre policy table;
- deterministic bounded genome derivation;
- no materialization yet;
- seed sweep / fingerprint tooling.

## Stage HGS-3 — Route planner + structural fingerprint

- bounded section route planner;
- TAKE/KEEP/DEVELOP/BREAK/RETURN policies;
- recent fingerprint penalty;
- explicit RETURN behavior;
- offline/host evaluation first.

## Stage HGS-4 — ChordProgression backend integration

- integrate graph output behind the existing WHAT owner;
- preserve legacy backend and deterministic IDs;
- keep ChordRhythm untouched;
- project only representable semantic states;
- fail explicitly on unsupported projection.

## Stage HGS-5 — Role-aware harmonic consumption

- expose legal/preferred pitch-class sets to bass/chord/melody roles;
- monophonic engines consume harmony as policy, not as forced chord playback;
- no new onset ownership.

## Stage HGS-6 — Polychord / vertical-color layer

- add upper-structure selection after base-route stability is proven;
- keep base fingerprint separate from vertical color fingerprint;
- validate monophonic and polyphonic projections independently.

## Stage HGS-7 — Genre tuning and listening gate

- corpus-guided weight tuning;
- structured A/B listening;
- adjust genre matrices without changing graph semantics;
- only then consider deprecating any redundant fixed progression recipes.

---

# 22. Architecture invariants

The implementation is rejected if any of the following becomes true:

```text
Genre directly emits notes                         FAIL
Harmonic graph creates rhythm onset                FAIL
ChordRhythm chooses harmonic identity              FAIL
Graph navigation depends on physical Synth A/B     FAIL
Polychord layer changes the base route identity    FAIL
Downstream role widens forbidden Genre harmony     FAIL
RETURN silently changes the stored home identity   FAIL
Graph traversal becomes unbounded                  FAIL
New code requires runtime heap allocation          FAIL
Legacy ProgressionId values are renumbered         FAIL
Same seed ceases to replay deterministically       FAIL
Transposition alone defeats anti-repeat memory     FAIL
```

---

# 23. Final design decision

GroovePuter should not become a larger library of chord progressions.

It should become a system that understands a small set of harmonic relationships and navigates them differently according to Genre, song personality and section intent.

The target architecture is:

```text
                 UNIVERSAL HARMONIC GRAPH
       DIATONIC / PRL / FUNCTIONAL / MODAL
          CHROMATIC / AUGMENTED PORTALS
                         |
                         v
                  GENRE CORRIDOR
                         |
                         v
                SongVariantGenome
                         |
                         v
        TAKE / KEEP / DEVELOP / BREAK / RETURN
                         |
                         v
            bounded deterministic route
                         |
              +----------+-----------+
              |                      |
              v                      v
      base harmonic identity   vertical color
              |                 (polychord)
              +----------+-----------+
                         v
              ChordProgression WHAT
                         |
                ChordRhythm WHEN
                         |
                         v
             role-aware tonal projection
                         |
                         v
                    materialization
```

This is the selected Option 2 and is the normative direction for the next harmonic-variation work.