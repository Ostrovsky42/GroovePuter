# Harmony Atlas Research Protocol

**Status:** planning / research only  
**Integration line:** `dev_0.9.1`  
**Source project:** `ldrolez/free-midi-chords`  
**Pinned source revision:** `baf0896694de6b09ac00250722f2414202e668ed`  
**Evidence class:** `EDITORIAL_CATALOG_EVIDENCE`  
**Runtime impact:** none  

---

## 1. Purpose

Define a falsifiable research protocol for a future GroovePuter Harmony Atlas.

The goal is not to import a large chord pack into firmware and not to create another chord generator. The goal is to extract, normalize, classify and review useful harmonic evidence that may later strengthen the existing Stage 15 harmonic vocabulary.

The research boundary is:

```text
source catalog
    ↓
offline extraction
    ↓
normalized / deduplicated evidence
    ↓
functional and rhythmic analysis
    ↓
human-reviewed runtime candidates
    ↓
existing ChordProgression / ChordRhythm owners
    ↓
shared Tonal Projector
```

The source corpus is evidence material, not firmware payload and not a runtime authority.

---

## 2. Architecture boundary

Harmony Atlas must preserve the existing ownership chain:

```text
ChordProgression -> what harmonic degrees / qualities occur
ChordRhythm      -> when harmonic events occur
Tonal Projector  -> absolute MIDI projection
materialization  -> physical Synth A/B pattern writes
Scene            -> authoritative concrete state after commit
```

Harmony Atlas may provide evidence for:

- progression identities;
- compatible chord qualities;
- modal / chromatic alterations;
- loop and cadence classes;
- chord-rhythm identities;
- progression-family coverage;
- later selection priors.

Harmony Atlas MUST NOT:

- emit absolute MIDI notes directly;
- become a second Tonal Projector;
- own Scene, Song, page, bank, slot or physical synth destinations;
- choose synth TYPE as a side effect of harmonic selection;
- turn mood tags into direct event-writing logic;
- make corpus frequency equal runtime selection probability;
- bypass current Stage 15 ownership with a parallel generator.

Research representation is explicitly allowed to be richer than the current runtime representation. An observation that current Stage 15 cannot represent must remain valid deferred evidence rather than being silently simplified to fit current C++ structures.

---

## 3. Source facts that constrain the methodology

The pinned `free-midi-chords` source is a generated editorial catalog, not an observational corpus of independent songs.

At the pinned revision:

- progression definitions are authored as Roman-numeral strings in source data;
- the repository advertises more than 13,000 MIDI files and roughly 190 chord progressions per key;
- generation iterates over 12 major / relative-minor key pairs;
- each progression can be emitted using multiple rhythmic styles;
- current progression styles are the default long/basic form plus `pop`, `pop2`, `hiphop2` and `soul`;
- progression definitions carry human-authored descriptors such as `Hopeful`, `Nostalgic`, `Dark`, `Mysterious`, `Cadence` and `New`;
- Major, Minor and Modal source families use related but not identical Roman-numeral conventions;
- Modal entries include chromatically altered roots such as flat or sharp degrees.

Therefore:

> A generated MIDI file is not an independent harmonic observation.

and:

> The number of files carrying a progression must never be interpreted as real-world musical popularity.

The primary source identity is the logical progression definition before transposition and rhythmic materialization.

---

## 4. Evidence class

This project assigns the source the explicit class:

```text
EDITORIAL_CATALOG_EVIDENCE
```

This class supports conclusions of the form:

- a harmonic construction is present in a curated utility catalog;
- a family has more or less editorial coverage inside this source;
- several source definitions collapse to the same normalized structural identity;
- a progression uses a given alteration, quality, cadence type or loop shape;
- the source provides several rhythmic materializations for one harmonic identity.

It does NOT support conclusions of the form:

- progression X is used N times more often than progression Y in released music;
- mood tag X predicts listener emotion with measured probability;
- source file count is a popularity prior;
- one source family is inherently more important because it generated more MIDI payloads.

Reports must use terms such as `catalog incidence`, `source coverage` or `editorial support`, not `musical frequency`, unless a later observational corpus independently justifies that claim.

---

## 5. Unit of analysis

The canonical research unit is:

```text
LogicalProgressionDefinition
```

not:

```text
MIDI file
key-transposed file
rhythmic-style file
folder entry
filename
```

A source progression may generate many physical files. Those files belong to one harmonic source identity plus zero or more rhythm materializations.

Required grouping hierarchy:

```text
LogicalProgressionDefinition
    ├─ key projection C / Db / D / ...
    ├─ default rhythm materialization
    ├─ pop materialization
    ├─ pop2 materialization
    ├─ hiphop2 materialization
    └─ soul materialization
```

Key transpositions must not increase harmonic support counts.

Rhythmic styles may increase `ChordRhythm` evidence counts only when the report explicitly states that it is counting distinct source style definitions rather than independent musical observations.

---

## 6. Research schema

The research schema is intentionally independent from the final embedded representation.

### 6.1 Functional degree

Do not collapse altered/modal degrees into plain `0..6` degree identity.

Conceptual schema:

```text
FunctionalDegree
  diatonic_degree      0..6
  alteration_semitones signed bounded integer
  notation_class       source-normalization metadata
```

Examples:

```text
I      -> degree 0, alteration  0
bIII   -> degree 2, alteration -1
#IV    -> degree 3, alteration +1
bVII   -> degree 6, alteration -1
```

The exact encoding is not frozen here. The invariant is that normalization MUST preserve the distinction between altered and unaltered functional roots.

### 6.2 Normalized progression

Initial research shape:

```text
HarmonyObservation
  source_id
  source_family             Major | Minor | Modal
  event_count
  roots[]                   FunctionalDegree
  chord_qualities[]
  structural_tags[]
  mood_tags[]
  catalog_tags[]
  source_notation
```

### 6.3 Tag taxonomy

Source descriptors must be typed before analysis.

At minimum:

```text
mood_tags
  Hopeful
  Nostalgic
  Dark
  Mysterious
  Romantic
  ...

structural_tags
  Cadence
  ...future structural labels

catalog_tags
  New
  ...future source-maintenance labels
```

`New` is not a mood.

`Cadence` is not a mood.

Unknown descriptors must be reported explicitly instead of silently entering the mood set.

### 6.4 Chord rhythm

Harmonic identity and chord timing are orthogonal research dimensions.

Conceptual schema:

```text
ChordRhythmObservation
  source_progression_id
  source_style
  event_count
  onset_coordinates[]
  durations[]
  rest_mask
  continuation_mask
  phrase_length
```

The exact runtime `ChordRhythmDefinition` representation remains separate work.

---

## 7. Normalization rules

Normalization must be deterministic and reversible enough for diagnostics.

Required rules:

1. Preserve source family before canonicalization.
2. Parse Roman root identity separately from chord quality.
3. Preserve accidental alteration.
4. Preserve explicit rests.
5. Preserve event order.
6. Preserve repeated harmonic events before dedup classification.
7. Preserve source tags with their typed category.
8. Do not infer absolute MIDI pitch into normalized harmonic identity.
9. Do not use filename text as the canonical identity when source definitions are available.
10. Reject or quarantine unknown notation rather than silently mapping it to a familiar degree or quality.

Parser diagnostics must make unsupported syntax enumerable and testable.

---

## 8. Deduplication model

Harmony Atlas MUST NOT use one universal fingerprint.

Different questions require different equivalence classes.

Required fingerprint levels:

```text
H0 SourceIdentity
   exact normalized source definition including tags where relevant

H1 TranspositionInvariant
   progression identity independent of absolute key projection

H2 RootSequence
   ordered functional roots including chromatic alterations

H3 RootQualitySequence
   ordered functional roots + chord qualities

H4 FunctionalClassSequence
   optional higher-level tonic / predominant / dominant / borrowed analysis

H5 ChordRhythmIdentity
   ordered timing/rest/continuation identity independent of pitch

H6 CombinedIdentity
   H3 harmonic identity + H5 chord-rhythm identity
```

The research report must state which fingerprint is used for every duplicate count.

### 8.1 Forbidden naive equivalences

Do not automatically treat these as equal:

```text
I V vi IV
I V vi IV I V vi IV
```

The second may encode phrase repetition or an explicit longer form.

Do not automatically use cyclic rotation equivalence:

```text
I V vi IV
V vi IV I
```

They share cyclic content but have different phrase starts and potentially different functional closure.

Do not collapse chord qualities when the question concerns harmonic color.

Near-duplicate reports may compare these forms, but admission decisions must retain the exact reason why two entries were grouped.

---

## 9. Functional-analysis dimensions

After exact normalization is validated, the research may derive higher-level features.

Candidate features:

```text
tonic_return_distance
dominant_resolution_count
borrowed_chord_count
chromatic_root_count
quality_entropy
unique_degree_count
repetition_period
harmonic_event_density
loop_closure_class
cadence_class
functional_motion_histogram
```

Candidate `loop_closure_class` values:

```text
OPEN_LOOP
CLOSED_TONIC_LOOP
DOMINANT_LOOP
MODAL_AMBIGUOUS_LOOP
TURNAROUND
CADENTIAL
```

These are derived research labels. They must not replace the normalized source identity.

Any heuristic classifier requires:

- deterministic rules;
- confidence or reason codes where ambiguity exists;
- a mutation/falsification test set;
- explicit `UNKNOWN` rather than forced classification.

---

## 10. Coverage, not popularity

Every report must distinguish at least:

```text
logical_source_count
normalized_unique_count
key_materialization_count
rhythm_materialization_count
```

The report MUST NOT rank runtime candidates by generated MIDI file count.

If a later candidate-selection prior is proposed, it must be a separate reviewed policy combining musical goals and evidence. A source catalog ratio may be one input, but it is never copied directly into runtime weights.

Forbidden shortcut:

```text
source says 31% -> runtime weight = 31
```

Allowed model:

```text
source evidence
    + Genre corridor
    + VoiceRole / harmonic role
    + P-level / phrase function
    + diversity target
    + human review
    -> bounded runtime selection prior
```

---

## 11. Research confidence and provenance

Every derived research result intended for later admission must carry enough metadata to answer:

- what source revision produced it;
- which logical source definitions support it;
- which extractor version produced it;
- which normalization/fingerprint class was used;
- how much support exists;
- whether the result was exact or heuristic;
- whether a human reviewed it.

Conceptual provenance shape:

```text
ResearchSource
  source_repo
  source_commit
  evidence_class
  extractor_version
  local_artifact_hash
  license_class
```

Conceptual derived-evidence shape:

```text
HarmonyEvidence
  evidence_id
  evidence_kind
  support_source_definitions
  support_styles
  confidence
  derivation_version
  review_state
```

`support_source_definitions` must count logical definitions, not transposed output files.

---

## 12. Runtime representability is a report, not an extraction constraint

The current Stage 15 runtime vocabulary is deliberately bounded. Harmony research must not distort source evidence to fit it.

After normalization, publish a representability classification such as:

```text
REPRESENTABLE_CURRENT
REQUIRES_NEW_QUALITY
REQUIRES_ALTERED_DEGREE
REQUIRES_LONGER_FORM
REQUIRES_RHYTHM_EXTENSION
DEFERRED_OTHER
```

The report should answer:

```text
What fraction of normalized source identities can current Stage 15 represent exactly?
What information would be lost by current representation?
Which missing capability has the highest evidence support?
```

A missing capability is not automatically a request to expand production code.

Production expansion remains a separate architecture and musical-admission decision.

---

## 13. Research result levels

Harmony Atlas uses three result levels:

```text
R0 RAW OBSERVATION
   exact parser/extractor output tied to source definitions

R1 GENERALIZED EVIDENCE
   normalized, deduplicated, classified statistics and relationships

R2 CURATED RUNTIME CANDIDATE
   small generic human-reviewed vocabulary proposed for production
```

Rules:

- R0 may contain source-specific identifiers needed for reproducibility.
- R1 may contain normalized source references and aggregate evidence.
- R2 must be generic, bounded and explicitly reviewed.
- production runtime may consume only R2 artifacts or manually encoded equivalents approved by a production PR.
- R0/R1 artifacts do not gain runtime ownership merely because they are generated automatically.

---

## 14. H0-H6 research sequence

### H0 — Source audit

Goal: establish the factual source shape before writing an extractor that assumes it.

Required outputs:

```text
source revision
source file / definition inventory
logical progression count
Major / Minor / Modal counts
tag vocabulary and typed classification
chord-quality vocabulary
alteration vocabulary
rhythm-style vocabulary
generation multiplicity by key/style
unknown / malformed syntax inventory
```

Gate:

- source revision is pinned;
- all logical progression definitions are enumerable;
- physical MIDI multiplicity is separated from logical source count;
- no unsupported notation is silently accepted.

### H1 — Canonical parser and normalization

Goal: parse every progression definition into loss-aware functional representation.

Required tests:

- every admitted source definition parses deterministically;
- flat/sharp alterations survive normalization;
- chord quality survives normalization;
- rest semantics survive normalization;
- `New`, `Cadence` and mood descriptors cannot cross tag categories;
- malformed/unknown tokens return explicit diagnostics;
- repeated runs produce byte-identical normalized export.

Gate:

- zero silent parser fallback;
- complete source coverage or an explicit quarantine report.

### H2 — Structural fingerprinting and dedup

Goal: quantify how many distinct identities exist under several explicit equivalence models.

Required outputs:

```text
H0..H6 duplicate reports
near-duplicate candidate report
repetition-expansion report
rotation-similarity report without automatic equivalence
```

Gate:

- every duplicate cluster names its equivalence rule;
- no one fingerprint is presented as universal identity.

### H3 — Functional analysis

Goal: derive loop/cadence/function/color features without changing source identity.

Required outputs:

```text
closure-class distribution
cadence-class report
chromatic / borrowed degree report
quality-diversity report
functional-motion report
uncertain / unknown classification report
```

Gate:

- heuristic classes are falsifiable;
- ambiguous cases can remain `UNKNOWN`;
- no heuristic is used to rewrite normalized evidence.

### H4 — ChordRhythm extraction

Goal: separate timing vocabulary from progression vocabulary.

Required outputs:

```text
source rhythm-style inventory
normalized rhythm fingerprints
rest/continuation statistics
style-to-rhythm duplicate report
harmonic-identity vs rhythm-identity cross table
```

Gate:

- one harmonic identity can reference multiple rhythm identities without duplication of harmonic support;
- ChordRhythm remains an independent dimension.

### H5 — Stage 15 representability report

Goal: compare evidence against the current production contract without changing production.

Required outputs:

```text
exactly representable identities
quality gaps
altered-degree gaps
form-length gaps
rhythm-representation gaps
ranked deferred capabilities by logical-source support
```

Gate:

- no production file changes;
- current runtime is measured, not silently expanded.

### H6 — Curated candidate vocabulary

Goal: propose the first small runtime batch only after H0-H5 evidence is stable.

Candidate categories may include:

```text
STATIC / PEDAL
TWO_CHORD_LOOP
THREE_CHORD_LOOP
FOUR_CHORD_POP_CYCLE
MINOR_LOOP
MODAL_LOOP
TURNAROUND
CADENCE
BORROWED_COLOR
EXTENDED_PHRASE
```

These are candidate runtime organization classes, not claims about source-native categories.

Gate:

- candidates increase effective musical coverage rather than only nominal ID count;
- every candidate has R1 evidence and human review;
- source-specific names are not required at runtime;
- runtime memory impact is measured;
- absolute pitch still passes only through Tonal Projector;
- production integration is a separate PR.

---

## 15. Required tests for future extractor PRs

Future Harmony Atlas tooling must include tests for at least:

```text
same source revision + extractor version -> byte-identical export
key transposition does not increase logical harmonic support
style materialization does not increase logical harmonic support
bIII != III
#IV != IV
I != i when quality/root semantics differ
New cannot enter mood_tags
Cadence cannot enter mood_tags
unknown tag/token is reported
repeated phrase != automatically deduplicated phrase
cyclic rotation != automatically identical phrase
harmonic fingerprint independent from chord-rhythm fingerprint
no normalized identity requires absolute MIDI pitch
```

Mutation tests should demonstrate that the suite fails if:

- accidental signs are discarded;
- quality is discarded;
- key-generated files are counted as independent progression support;
- source style variants inflate harmonic support;
- tag categories are merged;
- fingerprint ordering is lost;
- repeated phrases are collapsed unconditionally.

---

## 16. License and repository boundary

The pinned source project is MIT-licensed.

Research tooling must record the source revision and license provenance. If source code is copied or adapted into repository tooling, required copyright/license notices must be retained.

Preferred approach:

- independently parse the small source data representation or exported research input;
- keep external corpus payload outside firmware;
- do not commit generated 13k MIDI payloads to normal GroovePuter repository history;
- commit only bounded research tooling, manifests, normalized reports and reviewed generic candidates where appropriate.

This protocol does not itself import or redistribute the external MIDI pack.

---

## 17. Risk register

| Risk | Likelihood | Impact | Required control |
|---|---|---|---|
| Generated MIDI files counted as independent observations | High | Critical | logical-definition unit of analysis |
| Catalog incidence described as real-world popularity | High | High | evidence-class terminology |
| Flat/sharp degree alteration lost | Medium | Critical | alteration-preserving schema/tests |
| `Mood`, `New`, `Cadence` mixed into one tag axis | High | High | typed tag taxonomy |
| One fingerprint used for every dedup question | High | High | H0-H6 fingerprint levels |
| Phrase repetition collapsed as duplicate | Medium | High | repetition-aware comparison |
| Cyclic rotations treated as identical | Medium | High | similarity only, no default equivalence |
| Chord rhythm coupled to progression identity | Medium | High | orthogonal schemas and support counts |
| Research schema constrained by current Stage 15 | High | High | deferred representability classes |
| Source ratios copied directly into runtime weights | Medium | High | separate human-reviewed admission policy |
| Atlas becomes a new runtime owner | Medium | Critical | R0/R1/R2 boundary + existing ownership chain |
| Embedded catalog expands without effective diversity | Medium | High | small H6 batch + musical coverage gate |

---

## 18. Non-goals

This protocol does not:

- add or change production chord progressions;
- add new `ProgressionId` values;
- modify `ChordProgression`, `ChordRhythm` or Tonal Projector APIs;
- modify Genre / Mood / Feel / Scene / Song / UI;
- add voicing, inversions, SATB or voice leading;
- import the external MIDI archive;
- claim source popularity statistics;
- freeze a final embedded representation;
- make Harmony Atlas a release gate for 0.9.1.

---

## 19. Definition of done for the research foundation

The Harmony Atlas research foundation is ready for extractor implementation when all of the following are true:

```text
[ ] pinned source revision recorded
[ ] evidence class recorded as EDITORIAL_CATALOG_EVIDENCE
[ ] logical progression definition is the canonical analysis unit
[ ] source-file multiplicity is explicitly non-statistical
[ ] altered-degree representation is loss-aware
[ ] tag taxonomy separates mood / structural / catalog metadata
[ ] harmonic and chord-rhythm identities are orthogonal
[ ] H0-H6 fingerprint model is documented
[ ] popularity claims are prohibited without observational evidence
[ ] provenance/support/confidence contract is documented
[ ] runtime representability is a post-extraction report
[ ] R0/R1/R2 admission boundary is documented
[ ] H0-H6 implementation sequence is documented
[ ] production ownership remains ChordProgression -> ChordRhythm -> Tonal Projector
```

After this foundation is accepted, the next PR should implement **H0 source audit only**. It should not jump directly to runtime vocabulary admission.
