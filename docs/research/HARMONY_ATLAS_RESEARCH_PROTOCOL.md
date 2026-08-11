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

The goal is not to import a chord pack into firmware and not to create another runtime chord generator. The goal is to extract, normalize, classify and review useful harmonic evidence that may later strengthen the existing Stage 15 harmonic vocabulary.

Research flow:

```text
source catalog
    ↓
offline extraction
    ↓
normalized / deduplicated evidence
    ↓
functional + rhythmic analysis
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
- later bounded selection priors.

Harmony Atlas MUST NOT:

- emit absolute MIDI notes directly;
- become a second Tonal Projector;
- own Scene, Song, page, bank, slot or physical synth destinations;
- choose synth TYPE as a side effect of harmonic selection;
- turn mood tags into direct event-writing logic;
- make corpus frequency equal runtime selection probability;
- bypass current Stage 15 ownership with a parallel generator.

Research representation may be richer than current runtime representation. An observation that Stage 15 cannot currently represent must remain valid deferred evidence rather than being silently simplified to fit current C++ structures.

---

## 3. Source facts that constrain methodology

The pinned `free-midi-chords` source is a generated editorial catalog, not an observational corpus of independent songs.

At the pinned revision:

- progression definitions are authored as Roman-numeral strings in source data;
- the repository advertises more than 13,000 MIDI files and roughly 190 chord progressions per key;
- generation iterates over 12 major / relative-minor key pairs;
- each progression can be emitted using several rhythmic styles;
- current styles are the default long/basic form plus `pop`, `pop2`, `hiphop2` and `soul`;
- progression definitions carry human-authored descriptors such as `Hopeful`, `Nostalgic`, `Dark`, `Mysterious`, `Cadence` and `New`;
- Major, Minor and Modal source families use related but not identical Roman-numeral conventions;
- Modal entries include chromatically altered roots such as flat or sharp degrees.

Therefore:

> A generated MIDI file is not an independent harmonic observation.

and:

> Generated file count must never be interpreted as real-world musical popularity.

The primary harmonic source identity is the logical progression definition before transposition and rhythmic materialization.

---

## 4. Evidence class

The source is classified as:

```text
EDITORIAL_CATALOG_EVIDENCE
```

This class supports conclusions such as:

- a harmonic construction is present in a curated utility catalog;
- a family has more or less editorial coverage inside this source;
- several source definitions collapse to the same normalized structural identity;
- a progression uses a given alteration, quality, cadence type or loop shape;
- one harmonic identity has several source rhythm materializations.

It does NOT support conclusions such as:

- progression X is used N times more often than progression Y in released music;
- mood tag X predicts listener emotion with measured probability;
- source file count is a popularity prior;
- source ratios should be copied directly into firmware selection weights.

Reports must use terms such as `catalog incidence`, `source coverage` or `editorial support`, not `musical frequency`, unless a later observational corpus independently supports that claim.

---

## 5. Unit of analysis

The canonical harmonic research unit is:

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

Required grouping model:

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

Rhythmic styles may provide `ChordRhythm` evidence, but must not inflate harmonic support counts.

---

## 6. Research schema

The research schema is intentionally independent from the final embedded representation.

### 6.1 Functional degree

Do not collapse altered/modal degrees into a plain `0..6` identity.

Conceptual shape:

```text
FunctionalDegree
  diatonic_degree       0..6
  alteration_semitones  signed bounded integer
  notation_class        source-normalization metadata
```

Examples:

```text
I      -> degree 0, alteration  0
bIII   -> degree 2, alteration -1
#IV    -> degree 3, alteration +1
bVII   -> degree 6, alteration -1
```

The exact encoding is not frozen here. The invariant is that altered and unaltered functional roots remain distinguishable.

### 6.2 Normalized harmonic observation

Conceptual shape:

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
  ...

catalog_tags
  New
  ...
```

Normative rules:

```text
New     != mood
Cadence != mood
```

Unknown descriptors must be reported explicitly rather than silently entering the mood set.

### 6.4 Chord rhythm

Harmonic identity and chord timing are orthogonal dimensions.

Conceptual shape:

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

The final embedded `ChordRhythm` representation remains separate work.

---

## 7. Normalization rules

Normalization must be deterministic and sufficiently reversible for diagnostics.

Required rules:

1. Preserve source family before canonicalization.
2. Parse Roman root identity separately from chord quality.
3. Preserve accidental alteration.
4. Preserve explicit rests.
5. Preserve event order.
6. Preserve repeated harmonic events before dedup classification.
7. Preserve source tags with their typed category.
8. Do not infer absolute MIDI pitch into normalized harmonic identity.
9. Do not use filenames as canonical identity when source definitions are available.
10. Reject or quarantine unknown notation rather than silently mapping it to a familiar degree or quality.

Parser diagnostics must make unsupported syntax enumerable and testable.

---

## 8. Fingerprint and deduplication model

Harmony Atlas MUST NOT use one universal fingerprint.

Fingerprint namespaces are `F0..F6`; research stage namespaces are separately `H0..H6`.

Required fingerprint levels:

```text
F0 SourceIdentity
   exact normalized source definition including source identity metadata

F1 TranspositionInvariant
   progression identity independent of absolute key projection

F2 RootSequence
   ordered functional roots including chromatic alterations

F3 RootQualitySequence
   ordered functional roots + chord qualities

F4 FunctionalClassSequence
   optional tonic / predominant / dominant / borrowed analysis

F5 ChordRhythmIdentity
   ordered timing / rest / continuation identity independent of pitch

F6 CombinedIdentity
   F3 harmonic identity + F5 chord-rhythm identity
```

Every duplicate count must name the fingerprint used.

### 8.1 Forbidden naive equivalences

Do not automatically treat these as equal:

```text
I V vi IV
I V vi IV I V vi IV
```

The second may encode phrase repetition or a longer explicit form.

Do not automatically use cyclic rotation equivalence:

```text
I V vi IV
V vi IV I
```

They share cyclic content but differ in phrase start and potentially in functional closure.

Do not collapse chord qualities when the question concerns harmonic color.

Near-duplicate reports may compare such forms, but must preserve the reason for grouping.

---

## 9. Functional-analysis dimensions

After exact normalization is validated, research may derive higher-level features.

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

Candidate loop classes:

```text
OPEN_LOOP
CLOSED_TONIC_LOOP
DOMINANT_LOOP
MODAL_AMBIGUOUS_LOOP
TURNAROUND
CADENTIAL
```

Derived labels must not replace normalized source identity.

Any heuristic classifier requires:

- deterministic rules;
- reason codes or confidence when ambiguity exists;
- falsification tests;
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

Forbidden shortcut:

```text
source says 31% -> runtime weight = 31
```

A later runtime prior may combine:

```text
source evidence
    + Genre corridor
    + harmonic / voice role
    + P-level / phrase function
    + diversity target
    + human review
    -> bounded runtime selection prior
```

Evidence and runtime weighting are separate decisions.

---

## 11. Provenance, support and confidence

Every derived result intended for later admission must answer:

- what source revision produced it;
- which logical definitions support it;
- which extractor version produced it;
- which normalization/fingerprint class was used;
- how much logical support exists;
- whether it is exact or heuristic;
- whether it has human review.

Conceptual provenance:

```text
ResearchSource
  source_repo
  source_commit
  evidence_class
  extractor_version
  local_artifact_hash
  license_class
```

Conceptual derived evidence:

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

`support_source_definitions` counts logical definitions, not transposed output files.

---

## 12. Runtime representability is a report

Current Stage 15 is deliberately bounded. Research must not distort source evidence to fit it.

After normalization, classify observations such as:

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
What can current Stage 15 represent exactly?
What information would current representation lose?
Which missing capability has the strongest logical-source support?
```

A missing capability is not automatically a request to expand production code.

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

- R0 may contain source-specific identity needed for reproducibility.
- R1 contains normalized/generalized evidence.
- R2 must be generic, bounded and explicitly reviewed.
- production runtime may consume only R2 artifacts or manually encoded equivalents approved by a separate production PR.
- R0/R1 artifacts do not gain runtime ownership merely because they are generated automatically.

---

## 14. H0-H6 research sequence

### H0 — Source audit

Goal: establish the factual source shape before writing an extractor that assumes it.

Required outputs:

```text
source revision
source definition inventory
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

Goal: parse every progression definition into a loss-aware functional representation.

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
- complete coverage or an explicit quarantine report.

### H2 — Structural fingerprinting and dedup

Goal: quantify distinct identities under several explicit equivalence models.

Required outputs:

```text
F0..F6 duplicate reports
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
- ambiguous cases may remain `UNKNOWN`;
- no heuristic rewrites normalized evidence.

### H4 — ChordRhythm extraction

Goal: separate timing vocabulary from progression vocabulary.

Required outputs:

```text
source rhythm-style inventory
normalized rhythm fingerprints
rest / continuation statistics
style-to-rhythm duplicate report
harmonic-identity vs rhythm-identity cross table
```

Gate:

- one harmonic identity may reference several rhythm identities without duplicating harmonic support;
- ChordRhythm remains an independent dimension.

### H5 — Stage 15 representability report

Goal: compare evidence against current production contracts without changing production.

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

Candidate organization classes may include:

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

These are candidate runtime organization classes, not claims about source-native taxonomy.

Gate:

- candidates increase effective musical coverage rather than only nominal ID count;
- every candidate has R1 evidence and human review;
- source-specific names are not required at runtime;
- runtime memory impact is measured;
- absolute pitch still passes only through Tonal Projector;
- production integration is a separate PR.

---

## 15. Required tests for future extractor PRs

Future Harmony Atlas tooling must test at least:

```text
same source revision + extractor version -> byte-identical export
key transposition does not increase logical harmonic support
style materialization does not increase logical harmonic support
bIII != III
#IV != IV
quality information survives normalization
New cannot enter mood_tags
Cadence cannot enter mood_tags
unknown tag/token is reported
repeated phrase != automatically deduplicated phrase
cyclic rotation != automatically identical phrase
harmonic fingerprint independent from chord-rhythm fingerprint
no normalized identity requires absolute MIDI pitch
```

Mutation tests should demonstrate failure if:

- accidental signs are discarded;
- quality is discarded;
- key-generated files count as independent progression support;
- source style variants inflate harmonic support;
- tag categories are merged;
- fingerprint ordering is lost;
- repeated phrases are collapsed unconditionally.

---

## 16. License and repository boundary

The pinned source project is MIT-licensed.

Research tooling must record source revision and license provenance. If source code is copied or adapted into repository tooling, required copyright/license notices must be retained.

Preferred repository policy:

- parse the small source-data representation or a controlled research export;
- keep external corpus payload outside firmware;
- do not commit the generated 13k MIDI payloads to normal GroovePuter repository history;
- commit only bounded research tooling, manifests, normalized reports and reviewed generic candidates where appropriate.

This protocol does not import or redistribute the external MIDI pack.

---

## 17. Risk register

| Risk | Likelihood | Impact | Required control |
|---|---|---|---|
| Generated MIDI files counted as independent observations | High | Critical | logical-definition analysis unit |
| Catalog incidence described as real-world popularity | High | High | evidence-class terminology |
| Flat/sharp degree alteration lost | Medium | Critical | alteration-preserving schema/tests |
| `Mood`, `New`, `Cadence` mixed into one tag axis | High | High | typed tag taxonomy |
| One fingerprint used for every dedup question | High | High | F0-F6 fingerprint levels |
| Phrase repetition collapsed as duplicate | Medium | High | repetition-aware comparison |
| Cyclic rotations treated as identical | Medium | High | similarity only, no default equivalence |
| Chord rhythm coupled to progression identity | Medium | High | orthogonal schemas/support counts |
| Research schema constrained by current Stage 15 | High | High | deferred representability classes |
| Source ratios copied directly into runtime weights | Medium | High | separate admission policy |
| Atlas becomes a new runtime owner | Medium | Critical | R0/R1/R2 + existing ownership chain |
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

The Harmony Atlas research foundation is ready for extractor implementation when:

```text
[ ] pinned source revision recorded
[ ] evidence class is EDITORIAL_CATALOG_EVIDENCE
[ ] logical progression definition is the canonical analysis unit
[ ] source-file multiplicity is explicitly non-statistical
[ ] altered-degree representation is loss-aware
[ ] tag taxonomy separates mood / structural / catalog metadata
[ ] harmonic and chord-rhythm identities are orthogonal
[ ] F0-F6 fingerprint model is documented
[ ] H0-H6 research sequence is documented
[ ] popularity claims are prohibited without observational evidence
[ ] provenance/support/confidence contract is documented
[ ] runtime representability is a post-extraction report
[ ] R0/R1/R2 admission boundary is documented
[ ] production ownership remains ChordProgression -> ChordRhythm -> Tonal Projector
```

After this foundation is accepted, the next PR should implement **H0 source audit only**. It must not jump directly to runtime vocabulary admission.
