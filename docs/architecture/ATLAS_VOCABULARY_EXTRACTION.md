# Atlas Vocabulary Extraction — Orthogonal Evidence Model

**Status:** Stage 6B architecture + coverage audit; offline/research boundary only  
**GroovePuter audit base:** `agent/20260809-01-groove-vocabulary-stage6-bar-evolution` @ `ee99c5cda41a8e55ea6eba7a1105249fffe0621b`  
**Atlas archive:** SEQTRAK Pattern Atlas schema 2.6.0, pinned SHA-256 `5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd`  
**Purpose:** define which independent musical facts are extracted from Atlas, how coverage/distinctness are measured, which facts may become runtime grammar, and which source material must remain offline.

This document is paired with:

```text
docs/architecture/GENERATION_COMPOSITION_MODEL.md
```

The composition model answers **how independently owned musical knowledge may be combined**. This document answers **what reusable musical knowledge Atlas actually contains and how it may be generalized safely**.

---

# 1. Core decision

Atlas is primarily:

```text
1. offline evidence corpus
2. compiler input for compact generalized vocabularies
3. validation / musical regression corpus
```

Atlas is **not** the new runtime architecture and must not become one monolithic `AtlasVocabulary` object.

The target flow is:

```text
ATLAS CORPUS
    │
    ├── rhythm observations
    │       ↓
    │   Rhythm Vocabulary
    │
    ├── bass rhythm observations
    │       ↓
    │   Bass Relationship Vocabulary
    │
    ├── bass pitch observations
    │       ↓
    │   Bass Contour Vocabulary
    │
    ├── motif / melodic observations
    │       ↓
    │   Motif Vocabulary
    │
    ├── phrase-development observations
    │       ↓
    │   Bar / Phrase Trajectories
    │
    ├── timing / expression observations
    │       ↓
    │   Feel compatibility / expression profiles
    │
    └── character observations
            ↓
        bounded Mood biases
```

Runtime receives small curated grammars and weights, not raw research pattern databases.

---

# 2. Evidence corpus versus firmware-visible subset

These two scopes MUST be reported separately.

## 2.1 Full Atlas v2.6 evidence corpus

The validated archive snapshot used for generation assessment contains:

| Item | Count |
|---|---:|
| Patterns | 413 |
| Pattern events | 9,377 |
| Structural groups | 381 |
| Exact/expressive duplicate groups | 27 |
| One-bar patterns | 302 |
| Two-bar patterns | 93 |
| Four-bar patterns | 17 |
| Composite sequences | 92 |
| Published baseline recipes | 12 |
| Raw-bundle-eligible project-owned/internal-derived patterns | 51 |
| Patterns blocked pending rights review | 362 |
| Grid-skeleton-only patterns | 363 |
| Patterns with expressive detail | 47 |
| Patterns with partial expression | 3 |

Archive validation snapshot:

```text
passed checks  281
failures         0
warnings         1  (duplicate artifact hashes)
```

## 2.2 Current GroovePuter firmware Atlas subset

`tools/atlas/compile_atlas_runtime.py` pins the archive hash above and currently compiles exactly six recipes:

```text
Chicago Jack
Rolling Acid
Classic 2-Step
Dark Skippy
Deep Chord
Minimal Space
```

Each compiler-selected recipe must contain exactly:

```text
P1
P2
P3
```

and the current compiler accepts only:

```text
1 bar
16 steps/bar
```

Therefore the firmware-visible exact Atlas subset is only:

```text
6 recipes × 3 slots = 18 exact one-bar patterns
```

The generated runtime index currently also records:

```text
ignored sampler events               40
ignored unsupported-track events      0
ignored unrepresentable pitch events  0
```

This exact-preset subset is useful compatibility material, but it must not be confused with the 413-pattern evidence corpus.

---

# 3. Evidence quality limits

The Atlas is strongest as structural and phrase evidence, not as a complete performance capture dataset.

Current archive limitations:

| Field / property | Coverage |
|---|---:|
| `substep_offset != 0` | 0 / 9,377 |
| velocity present | 2,903 / 9,377 |
| microtiming + probability present | 2,788 / 9,377 |
| note length present | 984 / 9,377 |
| ratchet data | 4 events |
| 4/4 patterns | all |
| 16-step patterns | 402 |
| 8-step patterns | 11 |

Interpretation rules:

1. Structural grids can support topology, density, relationship and phrase-function extraction.
2. Humanisation tables are curated/editorial guidance unless a field is actually observed in source events.
3. Tonal/melodic editorial guidance is useful as a transposable constraint vocabulary, not as proof of literal genre melodies.
4. Missing data is not a zero-valued musical preference.
5. Sparse expression coverage must not be promoted into false statistical confidence.

Every future extracted feature must carry an evidence class:

```text
MEASURED
EDITORIAL_CURATED
PROJECT_OWNED_EXACT
RESEARCH_AGGREGATE
INSUFFICIENT
```

---

# 4. Strongest signals already established

## 4.1 Phrase function

Across the 12 published recipes, the existing audit found:

| Transition | Mean added | Mean removed | Mean net |
|---|---:|---:|---:|
| P1 -> P2 / Development | 9.33 | 0.58 | +8.75 |
| P1 -> P3 / Break/fill/ending | 2.92 | 11.17 | -8.25 |

This is a much stronger reusable abstraction than literal P1/P2/P3 event copies:

```text
P1 BASE
    establishes identity / anchors

P2 DEVELOPMENT
    mostly preserves identity
    adds activity

P3 BREAK / FILL / ENDING
    removes significant activity
    optionally adds small ending gestures
```

This is direct evidence for BarTrajectory/Phrase-development vocabulary.

## 4.2 Structure and expression are separate

Atlas already distinguishes structural and expressive hashes/groups. Several variants share a grid while differing in expression.

Therefore runtime vocabularies must keep at least these concerns separate:

```text
structural topology
expression / Feel
phrase-development function
```

A velocity/timing-only difference must not inflate structural vocabulary counts.

---

# 5. Orthogonal extraction dimensions

A single Atlas pattern may contribute evidence to several independent vocabularies.

Example conceptual decomposition:

```text
one UKG observation
    ├─ RhythmFamily = UkTwoStep
    ├─ kick displacement evidence
    ├─ stable backbeat evidence
    ├─ hat complement / protected-space evidence
    ├─ bass Respond/FillGaps evidence
    ├─ bass pitch contour evidence
    ├─ phrase trajectory evidence
    ├─ shuffle-sensitive Feel evidence
    └─ density / ornament evidence
```

The same concrete event must not be copied into every runtime vocabulary. Each extraction dimension stores only the generalized fact it owns.

## Required extraction matrix

| Area | Evidence to derive | Runtime target | Must NOT contain |
|---|---|---|---|
| Rhythm family | coarse temporal organization | `RhythmFamily` / family catalog | genre story/timbre |
| Rhythm archetype | multi-lane topology and identity | `RhythmArchetype` | literal source pattern IDs as behavior |
| Relationships | Coincide / Offset / Respond / Exclude / FillGaps tendencies | lane relationship vocabulary | pitch/timbre |
| Structure | immutable/canonical anchors, preferred/optional zones, protected spaces | lane grammar | expression-only differences |
| Phrase | 1/2/4-bar statement/development/break/return trajectories | `BarTrajectory` / future phrase vocabulary | Scene destination |
| Bass rhythm | relation to kick/backbeat/gaps | Bass relationship strategy | concrete bass notes |
| Bass pitch | pedal/chord-tone/octave/stepwise/acid-contour tendencies | Bass pitch strategy | new onset topology |
| Melody | motif length, contour, repetition/answer/sequence behavior | Motif Vocabulary | fixed literal melodies |
| Harmony | root/chord-tone gravity, approach targets, pitch-class budget | harmonic strategy | synth TYPE |
| Feel | swing-sensitive roles/subdivisions, timing/velocity corridors | Feel compatibility/expression profiles | pitch/topology |
| Density | structural versus ornament corridors | archetype/role budgets | timbre |
| Variation | what actually changes P1 -> P2 -> P3 | transformation budgets/trajectories | treating every slot as independent seed |
| Duplicates | structural/expressive fingerprints | audit metrics / dedupe gate | extra vocabulary count |
| Character | repetition/novelty, register, tension, energy trends with explicit evidence class | future bounded Mood profiles | direct events/physical engine |

---

# 6. Bass extraction must be two-dimensional

Do not create one `BassStyle` enum that mixes rhythm and pitch.

Extract rhythm relationship independently:

```text
FollowKick
CoincideKick
RespondKick
FillKickGaps
AnticipateKick
PedalPulse
SparseIndependent
```

and pitch behavior independently:

```text
Pedal
ChordTone
OctaveMotion
StepwiseMotion
AcidContour
```

These are candidate vocabulary names, not yet frozen runtime enums. Atlas extraction must first report support, distinctness and confidence for each candidate.

Examples of the intended composition model:

```text
UK Garage
    rhythm tendency: RespondKick / FillKickGaps
    pitch tendency:  ChordTone / StepwiseMotion

Acid
    rhythm tendency: CoincideKick + Offset
    pitch tendency:  AcidContour / Pedal

Synthwave
    rhythm tendency: regular pulse
    pitch tendency:  OctaveMotion
```

The rhythm strategy supplies/legalizes onset topology. Pitch strategy consumes those onset sites and may not invent new ones.

---

# 7. Motif extraction target

The melodic goal is not a larger library of fixed MIDI phrases.

Atlas/research extraction should report grammar candidates such as:

## Contour

```text
Static
Rise
Fall
Arch
Valley
Pendulum
```

## Motif behavior

```text
ExactRepeat
RepeatWithEnding
Answer
SequenceUp
SequenceDown
Compress
Expand
RestateAfterGap
```

## Interval vocabulary

```text
repeated note
step
third
fourth/fifth
octave
chromatic approach
```

## Harmonic behavior

```text
root gravity
chord-tone gravity
approach target
limited pitch-class set
```

## Phrase development

```text
A A A' B
A A' A B
A B A B'
```

A candidate is not accepted merely because it can be named. It requires evidence coverage and structural distinctness.

---

# 8. Offline extraction pipeline

The future Atlas analysis/compiler flow should be reproducible and non-destructive:

```text
1. Verify pinned archive hash + schema + validation report.
2. Load rights/publication metadata before musical extraction.
3. Normalize bars/steps/tracks/roles without destroying provenance.
4. Classify evidence as measured/editorial/project-owned/research aggregate.
5. Compute structural and expressive fingerprints separately.
6. Deduplicate by structural group for topology statistics.
7. Exclude composite sequences from single-pattern distributions.
8. Extract independent feature tables per dimension.
9. Compute support, confidence, entropy and effective distinctness.
10. Propose generalized grammar candidates.
11. Curator review accepts/rejects/merges candidates.
12. Emit compact runtime tables only for accepted generalized rules.
13. Validate generated output against Atlas-derived corridors/fingerprints.
```

The compiler must preserve a traceable route from a runtime grammar candidate back to aggregate evidence/provenance without embedding restricted literal patterns.

---

# 9. Rights and provenance boundary

The 362 blocked patterns may be used only for non-reversible aggregate analysis under the existing project policy.

Allowed aggregate examples:

```text
density quantiles
step occupancy frequencies
kick/snare/hat count corridors
syncopation/asymmetry ranges
relationship frequencies
phrase-transition statistics
structural fingerprint distance distributions
generator regression scoring corridors
```

Forbidden without a separate rights-policy change:

```text
shipping their literal event masks
shipping named reconstructed breaks
embedding recoverable restricted event sequences
turning research patterns into firmware presets
```

Project-owned/published exact recipes remain a distinct exact-preset compatibility category.

Rights status is part of compiler validation, not a README convention.

---

# 10. Coverage report contract

Before vocabulary expansion, the offline audit must produce one machine-readable report plus a short human-readable summary.

Recommended report sections:

```text
corpus
rights
rhythm_families
rhythm_archetype_candidates
relationship_candidates
protected_space_candidates
bar_trajectory_candidates
bass_rhythm_candidates
bass_pitch_candidates
motif_candidates
harmony_candidates
feel_candidates
density_corridors
variation_analysis
duplicate_analysis
runtime_coverage
weak_evidence
```

Each candidate should expose at least:

```text
candidate_id
source_dimension
evidence_class
observation_count
distinct_structural_groups
covered_genres_or_variants
confidence
structural_fingerprint_summary
runtime_target
rights_safe_for_runtime
curation_status
```

No candidate with `curation_status != ACCEPTED` is emitted into runtime vocabulary tables.

---

# 11. Metrics that replace raw genre/seed counts

Progress must be measured by effective musical coverage, not by the number of enum labels or RNG seeds.

Primary metrics:

```text
rhythm_family_count
rhythm_archetype_count
bar_trajectory_count
bass_rhythm_relation_count
bass_pitch_strategy_count
motif_grammar_count
distinct_structural_fingerprint_count
duplicate_rate
genre_archetype_entropy
effective_variation_count
```

## `effective_variation_count`

Definition:

> Number of generated realizations that remain distinct after the fingerprint resolution relevant to the musical dimension being evaluated.

Examples:

```text
same kick/snare/bass topology + different velocity only
    -> 1 structural rhythm variation

same motif intervals transposed to another root
    -> may remain 1 motif identity, depending on normalized motif fingerprint

same rhythm skeleton + materially different legal bar trajectory
    -> multiple phrase variations
```

The fingerprint resolution must be named in the metric output. There is no universal fingerprint for every musical dimension.

## `genre_archetype_entropy`

This metric should answer whether a Genre genuinely has several meaningful compatible archetypes or merely many aliases/near-duplicates.

High count with low structural entropy is not progress.

---

# 12. Duplicate and near-duplicate gate

The current Atlas snapshot already contains:

```text
381 structural groups for 413 patterns
27 exact/expressive duplicate groups
```

Therefore every vocabulary expansion pass must deduplicate before counting candidate coverage.

Required comparisons:

```text
literal event hash
structural hash
expression hash
normalized role topology fingerprint
phrase/development fingerprint
```

Candidate grammars with small distance should be merged or explicitly justified as distinct musical identities.

A new vocabulary item is rejected when its only distinction is expression that belongs to Feel/Articulation.

---

# 13. Runtime admission gate

An Atlas-derived candidate enters runtime only if all applicable conditions pass:

1. **Evidence:** support is sufficient for the claim being made.
2. **Distinctness:** it adds meaningful structural/phrase/pitch behavior rather than a near-duplicate.
3. **Ownership:** the candidate belongs to exactly one vocabulary/layer.
4. **Compatibility:** adjacent-layer compatibility is explicit.
5. **Rights:** emitted representation is allowed and non-reversible where required.
6. **Determinism:** runtime choice fits the composition RNG contract.
7. **Embedded cost:** flash/DRAM/stack impact is measured.
8. **Tests:** invariants and cross-layer leakage tests exist.
9. **Musical acceptance:** representative hardware/listening gate passes when the new vocabulary becomes production-reachable.
10. **Curation:** candidate is explicitly accepted; extraction alone cannot auto-publish grammar.

This prevents an automatic Atlas -> firmware content dump.

---

# 14. Current coverage gap map

The current state can be summarized as:

| Dimension | Current evidence | Current runtime representation | Gap |
|---|---|---|---|
| Rhythm topology | strong | 20 curated Stage 3 archetypes | expand carefully from Atlas evidence |
| Relationships | meaningful | Core v1 Exclude/Coincide/Offset/Respond/FillGaps | quantify support/coverage by family |
| Protected space | derivable/curated | supported by `RhythmArchetype` | extraction coverage report missing |
| Phrase function | strong P1/P2/P3 signal | Stage 6 API exists; shipped catalog only one-bar Statement | derive/curate real 2–4 bar trajectories |
| Bass rhythm relation | strong conceptual signal, not yet audited systematically | no dedicated runtime Bass relationship vocabulary | Stage 8 target |
| Bass pitch behavior | partial/tonal evidence, requires confidence labels | legacy pitch generation | Stage 8 target |
| Motif grammar | under-extracted | no dedicated vocabulary | Stage 9 target |
| Harmony | editorial + event evidence, mixed confidence | legacy genre/pitch rules | separate transposable constraints |
| Feel | expression evidence sparse/uneven | existing FEEL runtime | derive only evidence-supported compatibility/corridors |
| Mood | character knowledge exists mostly editorially | no Mood subsystem | Stage 10 bounded bias only |
| Exact Atlas presets | 12 published recipes in corpus | 6 recipes / 18 one-bar patterns compiled | compatibility path, not target architecture |
| Duplicate accounting | known | not a primary runtime metric | make mandatory for expansion reports |

---

# 15. Immediate Stage 6B deliverables

Before Stage 7 implementation, the next offline Atlas pass should answer these concrete questions:

```text
Rhythm
- Which additional structural groups are genuinely distinct from the current 20 archetypes?
- Which groups cluster into new families versus variants of existing archetypes?

Relationships
- Which Coincide/Offset/Respond/Exclude/FillGaps relations have repeated support?
- Which are role/family-specific?

Phrase
- Which 2-bar and 4-bar sequences support reusable Statement/Development/Break/Return trajectories?
- How often are P2/P3 structural versus expression-only changes?

Bass
- What is the distribution of bass rhythm relation to kick/backbeat/gaps?
- Which transposition-normalized pitch contours are distinct and repeated?

Motif
- What are motif lengths, normalized contours, interval sets and reuse/development operations?
- Which apparent differences collapse after transposition/normalization?

Feel
- Which timing/expression claims are measured, and which are only editorial?

Duplicates
- What is effective distinctness after structural normalization for every proposed vocabulary family?
```

The result is a coverage report and curated candidate list, **not** generated firmware headers yet.

---

# 16. Revised post-Stage-6 roadmap

This document and the composition model form the architecture gate before further feature expansion:

```text
Stage 6A  Generation Composition Contracts
Stage 6B  Atlas Orthogonal Extraction + coverage audit
Stage 6.1 BarEvolution technical hardening
Stage 7   Groove Vocabulary expansion: 20 -> ~30–40 -> evidence-gated 50–70
Stage 8   VoiceRole + Bass Generator v2
Stage 9   Phrase / Motif Vocabulary
Stage 10  Mood / Vibe bounded bias profiles
Stage 11  Weak genre rehabilitation + hybrids through data/compatibility
```

The exact numerical target is subordinate to effective distinctness. `50–70` is a ceiling/coverage goal, not an acceptance requirement.

---

# 17. Stage 6B acceptance criteria

Stage 6B is complete when:

- evidence corpus and firmware-visible Atlas subset are reported separately;
- every extraction dimension has one target owner;
- rights/provenance classes are explicit;
- structural versus expressive fingerprints are separate;
- duplicate/near-duplicate accounting is mandatory;
- `effective_variation_count` is defined by named fingerprint resolution;
- future Bass rhythm and Bass pitch extraction are independent;
- Motif extraction is transposition/identity oriented, not literal phrase copying;
- Mood is only a future bounded-bias consumer of extracted knowledge;
- no production code, Scene schema, Genre IDs or Texture subsystem are introduced by Stage 6B;
- future Stage 7+ PRs can cite a concrete accepted Atlas candidate/coverage result rather than saying only "Atlas-inspired".
