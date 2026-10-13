# GroovePuter Unified Harmonic System v2 — Harmonic Algebra + Semantic Graph + Corpus Evidence

**Date:** 2026-10-13  
**Status:** draft design specification for review  
**Supersedes conceptually:** `2026-10-13-unified-harmonic-system-design.md`  
**Decision:** preserve Option 2 (one unified system), but replace the first-generation edge taxonomy as the foundation with a more general harmonic algebra and relation-feature model. PRL, functional, modal, chromatic and portal relations become semantic labels over mathematically derived relations rather than the whole graph definition.  
**Repository:** `Ostrovsky42/GroovePuter`

---

# 1. Why v2 exists

The v1 design correctly moved GroovePuter away from a growing list of hard-coded chord-progressions toward a unified harmonic graph with genre-specific policies. However, it still treated a small set of named edge families (`DIATONIC`, `PRL`, `FUNCTIONAL`, `MODAL`, `CHROMATIC`, `AUGMENTED`) as the primary description of harmonic space.

That is too narrow.

Public MIDI/chord corpora show a much larger chord vocabulary than the current runtime carries, including suspended, added-tone, sixth, ninth, eleventh, thirteenth, altered, diminished and augmented families. The progression corpus also contains many major, minor and modal Roman-numeral routes that cannot be reduced cleanly to a handful of named transformations.

The correct abstraction is therefore:

```text
Pitch-class algebra
      ↓
Chord / scale vocabulary
      ↓
Mathematical relation features between states
      ↓
Semantic musical labels inferred from those features/context
      ↓
Corpus evidence about which transitions actually occur
      ↓
Genre corridor + SongVariantGenome + section intent
      ↓
Bounded deterministic route selection
```

The graph is no longer defined by a closed enum of musical theories. It is defined by **general mathematical relations**, while musical-theory concepts are annotations, constraints and scoring features.

---

# 2. Source evidence and intended use

This design uses two external open-source corpora as research/input evidence only. They are not firmware dependencies.

## 2.1 Fannon/midi-chords-and-scales-pack

Repository:

```text
https://github.com/Fannon/midi-chords-and-scales-pack
```

License: MIT.

Observed purpose:

- generates roughly 400 MIDI chord clips;
- generates roughly 300 MIDI scales;
- explicitly does **not** aim to provide chord progressions;
- uses `@tonaljs/tonal` to construct named chord/scale content.

Useful for GroovePuter as:

- chord-quality vocabulary evidence;
- scale vocabulary evidence;
- interval-set source material;
- test fixtures for normalized chord/scale formulas.

It should **not** be treated as progression statistics.

## 2.2 ldrolez/free-midi-chords

Repository:

```text
https://github.com/ldrolez/free-midi-chords
```

License: MIT.

Observed purpose:

- 13,000+ MIDI files;
- 195 progressions per key;
- Major, Minor and Modal progression families;
- multiple rhythmic realizations;
- mood-like tags such as `Nostalgic`, `Hopeful`, `Dark`, `Mysterious`, `Triumphant`, etc.;
- source progressions are already represented in Roman-numeral-like text form in `chords.py`.

Useful for GroovePuter as:

- normalized progression-route evidence;
- function-transition statistics;
- modal-route examples;
- chord-quality/extension usage evidence;
- optional corpus tags that may bias generation.

The MIDI files themselves are not needed to recover the harmonic sequence where the textual Roman-numeral source already exists.

## 2.3 Corpus rule

External corpora are **evidence**, not truth.

A corpus may bias a legal edge, but it must not become the only reason an edge is legal. This prevents the engine from being limited to the exact musical vocabulary present in a source pack.

---

# 3. Core design rule

The new system must distinguish six different concepts that are often collapsed under the word "harmony":

```text
1. CHORD FORMULA
   which pitch classes define one chord quality

2. SCALE / TONAL CONTEXT
   which pitch classes and functions define the current tonal field

3. VOICING / BASS REALIZATION
   how a chord is physically distributed across notes/registers

4. PROGRESSION FORMULA
   a path such as I -> vi -> IV -> V

5. RELATION / TRANSFORMATION
   a mathematical or musical relationship between two states

6. GENRE NAVIGATION POLICY
   which parts of the legal harmonic space a style prefers
```

The runtime must not store thousands of concrete chord files when most of them can be reconstructed from a compact formula and a root.

---

# 4. Generic pitch-class representation

Use pitch classes modulo 12:

```text
C  = 0
C# = 1
D  = 2
...
B  = 11
```

A pitch-class collection is represented as a 12-bit set.

Conceptually:

```cpp
using PitchSet12 = uint16_t; // only bits 0..11 are valid
```

Examples:

```text
C major   = {0,4,7}
C minor   = {0,3,7}
C7        = {0,4,7,10}
Cmaj7     = {0,4,7,11}
Cm9       = {0,2,3,7,10}
Cmaj9     = {0,2,4,7,11}
C13       = {0,2,4,7,9,10}
Cdim7     = {0,3,6,9}
Caug      = {0,4,8}
```

This representation is the common mathematical carrier for both chord and scale analysis.

## 4.1 ChordQualityDefinition

A quality definition is root-independent:

```text
ChordQualityDefinition
  stable quality id
  interval mask relative to root
  cardinality
  semantic family
  optional triadic skeleton
  complexity rank
```

Examples:

```text
maj     -> {0,4,7}
min     -> {0,3,7}
sus2    -> {0,2,7}
sus4    -> {0,5,7}
maj7    -> {0,4,7,11}
min7    -> {0,3,7,10}
dom7    -> {0,4,7,10}
maj9    -> {0,2,4,7,11}
min9    -> {0,2,3,7,10}
dim7    -> {0,3,6,9}
aug     -> {0,4,8}
```

Chord vocabulary growth therefore costs entries in a compact definition table rather than twelve copies per root.

## 4.2 ScaleDefinition

Likewise:

```text
ScaleDefinition
  stable scale id
  interval mask relative to tonic
  mode/family metadata
  optional parent-scale relation
  complexity / exoticism rank
```

Examples:

```text
Ionian      {0,2,4,5,7,9,11}
Dorian      {0,2,3,5,7,9,10}
Phrygian    {0,1,3,5,7,8,10}
Lydian      {0,2,4,6,7,9,11}
Mixolydian  {0,2,4,5,7,9,10}
Aeolian     {0,2,3,5,7,8,10}
Locrian     {0,1,3,5,6,8,10}
```

The first implementation does not need hundreds of scales in firmware. The offline compiler may know a broad vocabulary while each Genre admits only a compact runtime subset.

---

# 5. Harmonic state model

A harmonic node is richer than one pitch-set but remains compact.

Conceptually:

```text
HarmonicState
  rootPc         0..11
  qualityId      stable chord-quality id
  pitchMask      derived/cacheable PitchSet12
  bassPc         optional slash/inversion bass
```

`bassPc` is optional because slash chords appear in progression corpora and can materially affect harmonic motion. However, bass realization is not allowed to mutate the core chord pitch-set identity.

The design deliberately distinguishes:

```text
set-theoretic inversion I_n
```

from:

```text
voicing inversion / slash bass
```

They are not the same operation.

---

# 6. Tonal context model

A route also has a current tonal context:

```text
TonalContext
  tonicPc
  scaleId
  scaleMask
  homeTonicPc
  homeScaleId
```

The local tonal context may change during a modulation while the original home identity remains available for `RETURN`.

Chord-state identity and tonal-context identity stay separate because the same chord may serve different functions in different contexts.

---

# 7. Fundamental mathematical operations

These are the true foundation of the graph.

## 7.1 Transposition

For pitch class `p`:

```text
T_n(p) = (p + n) mod 12
```

For pitch set `S`:

```text
T_n(S) = { (p + n) mod 12 | p in S }
```

Example:

```text
Cmaj7 = {0,4,7,11}
T_2(Cmaj7) = {2,6,9,1} = Dmaj7
```

This is why a chord quality needs one interval formula rather than twelve separate stored chords.

## 7.2 Pitch-class inversion

```text
I_n(p) = (n - p) mod 12
```

For a set:

```text
I_n(S) = { I_n(p) | p in S }
```

This provides a generic reflection operation and can represent families of relationships sometimes described separately in music-theory systems.

No dedicated "negative harmony" subsystem is required to represent such reflections. If a genre or experiment later wants a named negative-harmony rule, it can be expressed as a constrained `I_n` transformation plus tonal-context semantics.

---

# 8. Relation feature vector

An edge is not primarily an enum. It is a compact feature vector computed between source and target states under a tonal context.

Conceptually:

```text
HarmonicRelationFeatures
  commonToneCount
  setDifferenceCount
  pcVoiceLeadingCost
  intervalVectorDistance
  rootSemitoneDistance
  rootFifthsDistance
  outOfScaleTargetTones
  sourceOutOfScaleTones
  complexityDelta
  bassMotionDistance
  sameScale
  sameTonic
  dominantToTarget
  diatonicFunctional
  borrowedFromParallel
  prlClass
  chromaticMediant
  tritoneRootRelation
  augmentedPortal
  diminishedPortal
  pivotCompatibility
```

The concrete runtime struct should use small integers and bit flags rather than booleans padded to machine words.

---

# 9. General chord-set similarity metrics

## 9.1 Common tones

```text
Common(A,B) = popcount(A & B)
```

High common-tone count usually supports perceptual continuity.

Example:

```text
Cmaj7 = {C,E,G,B}
Em7   = {E,G,B,D}
Common = 3
```

## 9.2 Symmetric set difference

```text
SetDiff(A,B) = popcount(A XOR B)
```

This is a cheap, embedded-safe measure of harmonic content replacement.

## 9.3 Jaccard similarity

Useful offline and optionally runtime if quantized:

```text
J(A,B) = |A intersect B| / |A union B|
```

Runtime selection does not require floating point; numerator and denominator may be compared or quantized.

---

# 10. Pitch-class voice-leading cost

For two pitch classes:

```text
delta(a,b) = min(|a-b|, 12-|a-b|)
```

For equal-cardinality pitch sets, use the minimum assignment cost:

```text
D_vl(A,B) = min over bijections pi of
            sum_i delta(a_i, b_pi(i))
```

For unequal-cardinality sets, use a minimum partial matching plus a small unmatched-tone penalty:

```text
D_pcvl(A,B) = min_M [
    sum_(a,b in M) delta(a,b)
    + lambda * (|A| + |B| - 2|M|)
]
```

where `M` is a partial one-to-one matching and `lambda` is a small integer constant tuned offline.

Important naming rule:

> This is **pitch-class voice-leading cost**, not final voiced MIDI-note distance.

Concrete octave/register voice leading belongs downstream to voicing/role realization.

For common triads:

```text
C -> Cm  cost ~1
C -> Em  cost ~1 with two common tones
C -> Am  cost ~2 with two common tones
```

This explains the smoothness of classical PRL relations without requiring PRL to be the mathematical foundation.

---

# 11. Interval-class vector similarity

For each unordered pair of pitch classes in a set, compute interval class:

```text
ic(a,b) = min((b-a) mod 12, (a-b) mod 12)
```

which lies in `1..6`.

A chord/set receives a six-element interval-class vector:

```text
ICV(S) = [c1,c2,c3,c4,c5,c6]
```

where `ck` counts interval-class `k` pairs.

Relation metric:

```text
D_icv(A,B) = sum_k |ICV(A)_k - ICV(B)_k|
```

This measures similarity of internal interval structure even when root labels or exact pitch classes differ.

It is useful for:

- identifying structurally related chord colors;
- comparing extended chords;
- distinguishing common-tone similarity from interval-content similarity;
- offline clustering of large chord vocabularies.

---

# 12. Root-space distances

## 12.1 Chromatic/semitone root distance

```text
D_root12(a,b) = min(|a-b|, 12-|a-b|)
```

## 12.2 Circle-of-fifths distance

Because one fifth is `+7 mod 12`, the fifth-step displacement is:

```text
k = (7 * (b-a)) mod 12
D_fifths(a,b) = min(k, 12-k)
```

This gives a different notion of tonal proximity from semitone distance.

Example consequence:

- C and G are far by one chromatic scale step count convention but adjacent on the circle of fifths;
- C and C# are adjacent chromatically but remote by fifth-space.

Genre policies may value these distances differently.

---

# 13. Scale and chromaticity relations

Given current scale mask `K` and chord mask `C`:

```text
outOfScale(C,K) = popcount(C & ~K)
```

A chord can therefore be:

- fully diatonic;
- partially borrowed;
- strongly chromatic.

This is superior to a single `CHROMATIC` edge flag because it expresses degree.

A genre may allow:

```text
0 outside tones  strongly
1 outside tone   occasionally
2+ outside tones only in BREAK
```

without needing a separate progression recipe.

---

# 14. Scale-space / modulation graph

The unified harmonic system contains **two related graphs**:

```text
Chord-state graph
Scale/tonal-context graph
```

Scale states also use `PitchSet12`, so the same set-similarity machinery applies.

Candidate tonal-context transitions may be scored by:

```text
scale common-tone count
scale symmetric difference
tonic fifth-distance
tonic semitone-distance
parallel/relative relation
shared pivot chords
corpus support
genre modulation policy
```

## 14.1 Pivot compatibility

A chord `C` is a strong pivot between scales `K1` and `K2` if:

```text
C subset-of K1
and
C subset-of K2
```

A softer pivot score can use common-tone coverage when extensions contain one foreign tone.

This lets modulation emerge from shared material rather than arbitrary key jumps.

## 14.2 Scale transitions by section

Typical rule:

```text
TAKE     no modulation
KEEP     no or parallel/relative only
DEVELOP  adjacent mode / fifth-related / corpus-supported
BREAK    broader tonal-context candidates if genre permits
RETURN   explicitly targets stored home context
```

---

# 15. Semantic musical labels are derived annotations

Named theories remain valuable because humans and genre policies reason with them. They are simply no longer the whole graph definition.

A relation may carry zero, one or several semantic tags.

Examples:

```text
PRL_P
PRL_R
PRL_L
DIATONIC_FUNCTIONAL
DOMINANT_TO_TARGET
SECONDARY_DOMINANT
BORROWED_PARALLEL
MODAL_MIXTURE
CHROMATIC_MEDIANT
TRITONE_ROOT
AUGMENTED_PORTAL
DIMINISHED_PASSING
COMMON_TONE_HIGH
FIFTH_RELATED
PIVOT_CHORD
```

The tag registry is append-only and extensible.

## 15.1 PRL becomes a detected special case

For major/minor triadic skeletons:

```text
Major(r):
  P -> Minor(r)
  R -> Minor(r+9)
  L -> Minor(r+4)

Minor(r):
  P -> Major(r)
  R -> Major(r+3)
  L -> Major(r+8)
```

Extended chords may still receive a PRL-like tag if their triadic skeletons satisfy the relation and the extensions remain compatible.

Derived neo-Riemannian transformations do not need separate hard-coded formulas when they can be represented as bounded compositions of P/R/L or recognized by the general relation metrics.

## 15.2 Dominant-to-target

For edge `A -> B`:

```text
A is dominant quality
and
A.root == B.root + 7 mod 12
```

then tag:

```text
DOMINANT_TO_TARGET
```

Secondary-dominant interpretation depends on the active tonal context.

## 15.3 Chromatic mediant

A candidate may receive a chromatic-mediant tag when roots are separated by a major/minor third and the target is not the ordinary diatonic mediant relation in context.

The exact semantic predicate is context-sensitive and belongs in the offline-tested theory helper rather than in ad-hoc genre code.

---

# 16. Vertical color / polychord model

Polychords remain a separate vertical layer.

Base-route identity:

```text
HarmonicState base
```

Optional color:

```text
UpperStructure
  second chord/set relation to base
```

The union is:

```text
ColorSet = BaseSet OR UpperSet
```

But the base route fingerprint does not change just because an upper structure is added.

The relation engine can score an upper structure using the same metrics:

```text
common tones
interval-vector effect
added pitch classes
out-of-scale count
complexity delta
role capability
```

For monophonic engines, the upper structure changes pitch-class weights, not simultaneous polyphony requirements.

---

# 17. Corpus compiler

The external corpora belong in an **offline compiler pipeline**, not in firmware runtime.

```text
External source
    ↓
parse
    ↓
normalize
    ↓
canonical harmonic tokens
    ↓
extract transitions / n-grams / quality usage / tags
    ↓
quantize
    ↓
compact generated tables
    ↓
firmware/runtime tests
```

## 17.1 Roman-numeral normalization

Progressions such as:

```text
I V vi IV
D A Bm G
E B C#m A
```

must normalize to the same relative harmonic route.

Canonical token fields should include:

```text
scale-relative degree/root offset
quality family
extension family
alteration flags
slash-bass relation
modal/borrowed spelling metadata when relevant
```

The parser must preserve meaningful distinctions such as:

```text
V
V7
Vsus4
Vadd9
iv
IV
bVII
#IVdim
```

while still allowing coarse grouping during statistics.

## 17.2 Transition statistics

At minimum compile:

```text
unigram function frequencies
bigram edge counts       P(B | A)
trigram route counts     P(C | A,B)
quality-transition counts
cadence-ending counts
modal/borrowed edge counts
```

No runtime logarithms are required. Offline probabilities may be converted into small integer prior weights.

## 17.3 Unseen edge rule

An edge with zero corpus count is **not automatically illegal**.

Corpus prior is one scoring term only:

```text
math legality
+ genre legality
+ section legality
+ corpus evidence
```

This preserves discovery and prevents corpus lock-in.

## 17.4 Mood-like tags

Source labels such as:

```text
Nostalgic
Hopeful
Dark
Mysterious
Triumphant
Tender
Rebellious
Spiritual
```

may be compiled into optional route/tag priors.

Normative interpretation:

> These are dataset annotations, not universal mappings from harmony to human emotion.

They may influence a future bounded Mood bias, but must never override Genre legality or stable harmonic identity.

---

# 18. Runtime candidate generation

Do not enumerate every chord quality times all 12 roots at each event.

Candidate generation is bounded and staged.

## 18.1 Candidate roots

Start from a small union of admitted root sources:

```text
current scale degrees
parallel/relative candidates
+/-1 semitone if chromatic policy permits
+/-2 semitones if genre permits
third-related roots
fifth-related roots
tritone only if admitted
portal roots derived from augmented/diminished logic
RETURN-specific target roots
```

Deduplicate into a fixed small array.

## 18.2 Candidate qualities

For each root, select only qualities admitted by:

```text
Genre chord vocabulary
current scale/context
section complexity budget
SongVariantGenome color budget
role/materializer capability
```

Target first implementation goal:

```text
<= 24 or <= 32 candidates per harmonic decision
```

The exact cap is chosen by benchmark, then protected by static assertions/tests.

## 18.3 Candidate relation analysis

For each candidate:

```text
compute feature vector
infer semantic tags
look up compact corpus priors
apply hard filters
score
```

No graph traversal object is stored persistently.

---

# 19. Unified scoring model

A candidate is chosen from a legal set by integer score.

Conceptually:

```text
Score(candidate) =
    GenreScore(features, tags)
  + GenomeScore(features, tags)
  + SectionScore(features, tags)
  + CorpusPrior(context -> candidate)
  + VoiceLeadingPreference
  + Home/CadenceScore
  + ModulationScore
  + VerticalColorCompatibility
  - RecentEdgePenalty
  - RecentRoutePenalty
  - ComplexityPenalty
```

No floating point is required.

A useful implementation shape is a quantized dot product:

```text
score = bias + sum_i weight_i * feature_i
```

where feature values are small bounded integers.

This gives the project a single harmonic decision engine while letting genres define radically different corridors.

---

# 20. Genre policy v2

The v1 family weights remain useful but become a higher-level view of more general features.

Genre now controls:

```text
allowed chord-quality families
allowed scale families
max out-of-scale tones
preferred pc voice-leading range
preferred fifth-distance range
preferred root-motion classes
common-tone preference
functional-tag weights
modal/borrowed-tag weights
chromatic-mediant weight
portal permission
modulation permission
extension/complexity budget
vertical-color budget
harmonic change-rate budget
corpus-conservatism weight
```

Examples:

## Acid

```text
small quality vocabulary
very high home gravity
low modulation
low extension complexity
low functional cadence weight
moderate chromatic root motion
strong repetition tolerance
harmony changes slowly
```

## Deep House

```text
medium/high extension vocabulary
high common-tone preference
medium PRL-like relations
medium dominant/borrowed support
moderate vertical color
low/medium modulation
strong anti-repeat on full route shape
```

## Ambient

```text
large common-tone and interval-color space
weak home gravity
high extension allowance
portal/modulation allowed
long chord durations
route smoothness may matter more than functional cadence
```

## EBM

```text
small/medium chord vocabulary
higher chromatic and tritone root relations
low ornate extension budget
strong pedal/home behavior
low ii-V style cadence preference
```

## Salsa

```text
strong functional tags
strong fifth-related movement
secondary dominants admitted
higher cadence weight
low arbitrary chromatic-mediant/portal weight
```

---

# 21. SongVariantGenome v2

The genome now biases mathematical dimensions, not only named edge families.

Recommended conceptual fields:

```text
SongVariantGenome
  homeGravity
  harmonicMobility
  smoothnessBias
  commonToneBias
  functionalBias
  modalBias
  chromaticRisk
  modulationRisk
  portalRisk
  extensionComplexity
  verticalColor
  corpusConservatism
  repetitionTolerance
  cadenceStrength
```

All are small quantized values.

The genome remains subordinate to Genre hard limits.

A song cannot use a forbidden scale/chord family merely because its genome rolled high `extensionComplexity` or `chromaticRisk`.

---

# 22. Section policy v2

## TAKE

```text
current context == home context
low complexity delta
low out-of-scale count
low tonal distance
high corpus/common-tone confidence
```

## KEEP

```text
prefer high common-tone / low pc-voice-leading cost
allow nearby functional/PRL/diatonic movement
keep same tonal context
```

## DEVELOP

```text
increase admissible complexity
allow borrowed/modal relations
allow secondary dominant / chromatic mediant if genre permits
allow one adjacent tonal-context shift in mobile genres
```

## BREAK

```text
largest legal harmonic distance for this Genre/Genome
portal relations may unlock
modulation candidates may expand
vertical-color budget may peak
```

Static-harmony genres are explicitly allowed to keep the same harmony and move other musical dimensions instead.

## RETURN

RETURN is goal-directed.

Candidate scoring receives a dominant home-target term and may use:

```text
pivot back to home scale
dominant-to-home
secondary preparation
common-tone path
augmented/diminished connector
bounded shortest legal path in relation-cost space
```

`RETURN` targets stored home tonal identity, not merely the root of the most recent chord.

---

# 23. Structural anti-repetition v2

One exact route hash is not enough because near-duplicates matter.

Store several normalized sub-signatures per section/song:

```text
rootMotionFingerprint
qualitySequenceFingerprint
semanticRelationFingerprint
cadenceFingerprint
scaleContextFingerprint
```

All root motion is stored relative to the starting/home tonic so transpositions normalize together.

Example:

```text
C  Am  F  G
D  Bm  G  A
E  C#m A  B
```

must share the same normalized root-motion/functional signature.

Similarity penalty can be a small integer sum:

```text
+4 same root-motion signature
+3 same semantic-relation sequence
+2 same quality sequence
+2 same cadence class
+1 same modulation pattern
```

This avoids expensive general sequence-distance algorithms on embedded hardware while catching the repetitions humans notice most quickly.

A constrained genre such as Acid receives a lower harmonic-repeat penalty so it is not punished for legitimate static harmony.

---

# 24. Current GroovePuter representation gap

The current `HarmonicEvent` representation is intentionally small:

```text
degree
ChordQuality
rootOffsetSemitones limited to +/-2
```

and the current `ChordQuality` enum admits only a small subset of the vocabulary described by this v2 design.

Therefore:

> v2 MUST NOT pretend that the existing carrier can losslessly represent the full new harmonic algebra.

Migration has two phases.

## Phase A — representable subset

The graph engine may initially generate only states that project cleanly into the current carrier.

This validates:

- relation scoring;
- genre policies;
- anti-repeat;
- section behavior;
- corpus priors;

without changing production representation.

## Phase B — richer semantic carrier

Only after listening evidence proves value, introduce a small versioned semantic carrier, conceptually:

```text
HarmonicDescriptor
  rootPc
  qualityId
  bassRelation
  flags
```

and project from it to role-specific pitch policies/materialization.

This is safer than expanding `rootOffsetSemitones` and `ChordQuality` ad hoc until the old struct becomes an accidental God object.

---

# 25. Relationship to ChordProgression and ChordRhythm

The existing ownership contract remains normative:

```text
ChordProgression / harmonic strategy = WHAT
ChordRhythm / harmonic timeline      = WHEN
```

The v2 system replaces/augments the WHAT selection backend only.

It does not:

- invent chord onsets;
- retime harmonic events;
- mutate rhythmic identities;
- assume a physical synth destination.

Conceptual integration:

```text
User/Song intent
    ↓
Genre
    ↓
SongVariantGenome
    ↓
TonalContext + current HarmonicState
    ↓
Candidate generator
    ↓
Relation feature engine
    ↓
Corpus prior lookup
    ↓
Section scorer / anti-repeat
    ↓
selected HarmonicState
    ↓
WHAT carrier
    +
ChordRhythm WHEN
    ↓
role-aware pitch projection
```

---

# 26. Offline tooling architecture

Add an offline harmony toolchain under a dedicated namespace/directory rather than embedding corpus parsing in production C++.

Recommended conceptual structure:

```text
tools/harmony/
  import_fannon_vocab.py
  import_ldrolez_progressions.py
  normalize_chords.py
  normalize_scales.py
  normalize_roman.py
  build_transition_stats.py
  build_quality_table.py
  build_scale_table.py
  build_harmony_model.py
  validate_harmony_model.py
```

Generated outputs should be deterministic, checksum-protected and versioned similarly to the existing Atlas extraction workflow.

Generated runtime artifacts may include:

```text
quality definitions
scale definitions
compact transition priors
compact trigram priors for selected genres/contexts
mood/tag priors if admitted later
source/version/checksum metadata
```

---

# 27. Corpus statistics must remain compact

Do not ship a sparse NxN matrix of every observed chord symbol.

Normalize aggressively.

Preferred statistics:

```text
relative functional token -> token counts
relation-tag transition counts
quality-family transition counts
root-motion class counts
cadence-class counts
modal-borrowed transition counts
```

For trigram context, keep only top-N successors for admitted normalized contexts.

Example compact row:

```text
context: [I, V]
successors:
  vi  weight 9
  I   weight 4
  IV  weight 3
  ii  weight 1
```

The exact N is benchmarked, not guessed.

---

# 28. Determinism

The v2 design keeps the existing stable-seed philosophy.

Required independent domains:

```text
HarmonicGenome
HarmonicCandidateTieBreak
HarmonicRoute
HarmonicModulation
HarmonicVerticalColor
```

If implementation prefers fewer domains, they must still preserve downstream independence through stable salts.

Rules:

- same semantic inputs => same route;
- corpus table reordering must not alter results when content/version is unchanged;
- adding vertical color must not perturb the base route;
- extending chord vocabulary must not perturb old replay unless the model version intentionally changes;
- released enum/domain values are append-only.

A harmonic-model schema/version must participate in deterministic compatibility decisions.

---

# 29. Embedded budget

Targets:

- no heap during generation/playback;
- no stored full universal graph;
- fixed candidate arrays;
- integer-only scoring;
- fixed small relation feature struct;
- no recursion in runtime graph search;
- bounded RETURN search;
- generated static tables in flash where appropriate.

Useful approximate design envelope:

```text
PitchSet12             2 bytes
HarmonicState          target <= 8 bytes
RelationFeatures       target <= 16 bytes
Candidate              target <= 24 bytes
32 candidates          target <= ~768 bytes transient worst case
SongVariantGenome      target <= 16 bytes
route fingerprint set  target tens of bytes
```

These are design targets, not accepted measurements. Actual `sizeof` and DRAM deltas must be verified in implementation gates.

---

# 30. Candidate scoring example

Suppose current state is `Cmaj7`, tonal context C Ionian.

Candidates:

```text
Em7
Am7
Fmaj7
C6
Dm9
Dbmaj7
E7
```

The engine may derive approximately:

```text
Em7:
  common tones high
  pc voice-leading low
  diatonic yes
  fifth distance moderate

Am7:
  common tones high
  pc voice-leading low
  diatonic yes

Fmaj7:
  common tones medium/high
  diatonic yes
  root fifth-space near

C6:
  same root
  content delta low
  complexity delta small

Dm9:
  diatonic/extension-rich
  larger content cardinality

Dbmaj7:
  chromatic content high
  root semitone near but fifth-space remote
  out-of-scale high

E7:
  one strong altered/secondary-dominant interpretation depending on target/context
```

Deep House may choose among the first five with occasional E7 setup. EBM may score Db-related chromatic motion much higher. Ambient may favor whichever candidate offers the desired interval/color continuity. The same mathematics serves all three.

---

# 31. Scale-transition example

Current context:

```text
C Ionian
```

Possible DEVELOP/BREAK contexts:

```text
A Aeolian
G Mixolydian
F Lydian
C Aeolian
G Ionian
E Phrygian
```

The engine does not blindly permit all of them.

Each receives:

```text
scale common-tone score
scale-set difference
home tonic distance
fifth-space distance
parallel/relative tag
shared pivot-chord availability
genre policy
section policy
corpus evidence
```

A selected pivot chord can then make the modulation audible as continuity rather than discontinuity.

---

# 32. Why this is more complete than a list of graph formulas

There is no single finite canonical list of all useful harmonic graphs.

Different theories emphasize different spaces:

```text
voice-leading space
Tonnetz / neo-Riemannian space
functional tonal space
circle-of-fifths/root space
pitch-class set space
scale/mode space
corpus transition space
```

The v2 system therefore does not attempt to enumerate every named theory.

Instead it provides a **meta-graph**:

```text
same pair of harmonic states
  -> mathematical relation vector
  -> zero/many semantic labels
  -> genre/context-dependent score
```

This means a future theory can be added as:

```text
new feature
or
new derived semantic tag
or
new policy weight
```

without redesigning the entire graph engine.

---

# 33. Testing strategy

## 33.1 Pitch-set algebra

Test exhaustively where practical:

- mask validity;
- transpose inverse properties;
- inversion properties;
- transposition invariance of set-distance metrics;
- popcount/common-tone correctness;
- interval-vector correctness.

Examples:

```text
T_12(S) == S
T_a(T_b(S)) == T_(a+b)(S)
I_n(I_n(S)) == S
Common(T_k(A), T_k(B)) == Common(A,B)
SetDiff(T_k(A), T_k(B)) == SetDiff(A,B)
```

## 33.2 Relation engine

- PRL examples detected correctly;
- dominant-to-target detection transposition-invariant;
- circle-of-fifths distance correct for all 144 root pairs;
- out-of-scale count correct for every admitted scale/chord fixture;
- chromatic-mediant and portal tags do not over-trigger.

## 33.3 Corpus import

- same input repository snapshot => byte-identical normalized model;
- Roman parser fixtures cover altered degrees, extensions, diminished chords and slash bass;
- malformed rows fail explicitly;
- source checksums recorded;
- no MIDI timing variation accidentally changes normalized harmonic route.

## 33.4 Corpus statistics

- transposed progressions normalize together;
- bigram/trigram totals match source counts;
- unseen mathematically legal edges remain representable;
- quantization preserves ranking of strong priors in fixtures.

## 33.5 Genre gates

Generate deterministic seed sweeps and assert:

- Acid remains harmonically conservative;
- Deep House/UKG show broader route diversity;
- Ambient may travel farther while maintaining smoothness when configured;
- EBM can prefer chromatic/root-tension relations;
- Salsa keeps strong functional/cadential behavior;
- Jungle/DnB are not forced into frequent chord change;
- BREAK has greater allowed distance than TAKE only where Genre permits;
- RETURN reaches stored home or fails explicitly.

## 33.6 Anti-repeat

- transposed copies collide structurally;
- same roots with different quality logic are distinguishable;
- same chord names with different modulation contexts can be distinguished;
- constrained genres are not trapped by novelty penalties.

## 33.7 Build/memory

- GCC / Clang / sanitizers on host where available;
- no runtime allocation;
- static capacity guards;
- Cardputer ADV build;
- DRAM delta recorded;
- largest internal block measured on target when production integration begins.

---

# 34. Listening acceptance v2

The v2 engine succeeds only if the mathematical richness becomes musical variety rather than random complexity.

For priority genres, run fixed-seed A/B suites.

Minimum questions:

1. Are ten consecutive songs in one genre less structurally repetitive?
2. Is genre identity still obvious?
3. Are extended/altered chords used sparingly enough for the genre?
4. Does smoothness improve when common-tone/voice-leading bias is high?
5. Does low smoothness bias produce controlled contrast rather than arbitrary jumps?
6. Are corpus priors noticeable as plausibility without turning generation into corpus replay?
7. Does BREAK explore more without making RETURN implausible?
8. Do modulation-capable genres use pivots convincingly?
9. Are Acid/Jungle/DnB protected from unnecessary harmonic busyness?
10. Do two songs with different genomes feel like different harmonic personalities rather than seed noise?

---

# 35. Migration plan boundaries

## HGS2-0 — v2 spec freeze

No production changes.

Freeze:

- pitch-set algebra;
- harmonic/scale state boundaries;
- relation-feature vocabulary;
- corpus role;
- representation-gap policy.

## HGS2-1 — Offline vocabulary import

Implement only tooling/tests:

- import chord formulas;
- import scale formulas;
- import ldrolez progression text;
- canonicalize Roman tokens;
- generate fixture reports.

No firmware dependency.

## HGS2-2 — Pitch-set algebra core

Implement:

- `PitchSet12` helpers;
- T_n / I_n;
- set metrics;
- root distances;
- interval-vector helpers;
- bounded pc voice-leading cost.

Pure host tests first.

## HGS2-3 — Relation feature engine

Implement:

- `HarmonicState` analysis;
- scale/chord context relations;
- semantic tag inference;
- fixed candidate feature structs.

No production ChordProgression routing.

## HGS2-4 — Corpus model compiler

Implement:

- normalized bigram/trigram stats;
- compact prior-table generation;
- provenance/checksums;
- quantization tests.

## HGS2-5 — Genre policy + SongVariantGenome v2

Implement feature-weight corridors and deterministic genome derivation.

Still host/offline evaluable.

## HGS2-6 — Bounded route planner

Implement:

- candidate root/quality generation;
- feature scoring;
- structural anti-repeat;
- section policies;
- RETURN targeting;
- optional tonal-context transition planning.

## HGS2-7 — Representable-subset integration

Integrate only harmonic states that map safely to the current runtime carrier.

Preserve:

- legacy `ProgressionId` replay;
- ChordRhythm WHEN ownership;
- deterministic existing tests.

## HGS2-8 — Listening gate

Tune feature weights and compare old/new seed suites.

Only after this gate may the project justify a richer harmonic semantic carrier.

## HGS2-9 — Rich harmonic carrier, if justified

Introduce a new fixed-capacity harmonic descriptor only if listening evidence shows that the current carrier blocks valuable chord vocabulary.

This stage is optional, not assumed.

## HGS2-10 — Vertical color / polychord

Add upper-structure logic after the base route engine is stable.

---

# 36. Explicit architecture invariants

Reject implementation if any becomes true:

```text
Named PRL/etc. tags become the only graph definition              FAIL
Corpus count becomes a legality requirement                       FAIL
Genre directly emits concrete notes                               FAIL
Harmony creates rhythmic onsets                                   FAIL
ChordRhythm chooses chord identity                                 FAIL
Pitch-class inversion is confused with slash/inversion voicing    FAIL
Full 13k MIDI corpus is copied into firmware                       FAIL
Every quality x every root is enumerated each decision             FAIL
Runtime graph traversal is unbounded                              FAIL
Heap allocation enters generation/playback                        FAIL
Current HarmonicEvent is silently overloaded beyond its contract   FAIL
RETURN forgets original home tonal identity                        FAIL
Transposition defeats route anti-repeat                            FAIL
Adding upper structure changes base-route identity                 FAIL
External mood tags are treated as scientific emotion truth         FAIL
Legacy ProgressionId numeric values are renumbered                 FAIL
Existing deterministic replay changes without model-version intent FAIL
```

---

# 37. Final design decision

The selected unified system is now:

```text
                     OFFLINE EVIDENCE

 chord/scale vocabularies        progression corpora
          |                              |
          +-------------+----------------+
                        v
              normalization/compiler
                        |
                        v
        compact vocabulary + corpus priors


                      RUNTIME

                 PitchSet12 algebra
                        |
          +-------------+-------------+
          |             |             |
          v             v             v
   chord relation   scale relation   root spaces
          |             |             |
          +-------------+-------------+
                        v
             relation feature vector
                        |
                        v
              semantic theory tags
       PRL / dominant / modal / portal / ...
                        |
                        v
                  Genre corridor
                        |
                        v
                SongVariantGenome
                        |
                        v
                 Section intent
                        |
                        v
                  Corpus priors
                        |
                        v
             Structural anti-repeat
                        |
                        v
              bounded candidate score
                        |
                        v
                NEXT HARMONIC STATE
                        |
              +---------+---------+
              |                   |
              v                   v
          base route        vertical color
              |                   |
              +---------+---------+
                        v
                  WHAT projection
                        +
                  ChordRhythm WHEN
                        |
                        v
               role-aware realization
```

The project is no longer trying to collect "all harmonic graph formulas" as a finite list.

It defines a compact mathematical basis capable of **deriving and comparing a very large harmonic space**, while named theories, genre behavior and real progression corpora guide which parts of that space become musically relevant.

That is the normative direction for Unified Harmonic System v2.