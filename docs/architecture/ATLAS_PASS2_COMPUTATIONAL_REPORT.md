# Atlas Pass 2 Computational Extraction Report

**Status:** computational evidence gate before Stage 7  
**Atlas input:** schema 2.6.0, SHA-256 `5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd`  
**Runtime comparison base:** actual 20-archetype `ReferenceVocabulary` dumped from code on PR #192  
**Production impact:** none. This pass emits offline aggregate evidence only.

## 1. Input gates

The supplied Atlas archive is the exact pinned v2.6 corpus:

```text
patterns                 413
events                  9377
validation failures        0
one-bar eligible          300
one-bar structural groups 269
```

The one-bar topology set excludes composite sequences, internal prototypes and superseded material. It contains 263 source observations and 37 project/editorial recipe patterns.

The complete reproducible extraction command is:

```bash
python3 tools/atlas/run_atlas_pass2.py \
  seqtrak_pattern_atlas_csv_v2_6.zip \
  docs/architecture/atlas_pass2/RUNTIME_RHYTHM_BASELINE.tsv \
  <output-directory>
```

The archive itself is not committed.

## 2. Topology extraction result

Pass 2 groups one-bar observations first by exact **Kick + Backbeat skeleton**. This is deliberately narrower than a runtime archetype: hats, percussion, BassRhythm, protected space, relationships and Feel remain separate evidence dimensions.

Only skeletons recurring in at least three distinct structural groups are surfaced. The full corpus yields:

```text
recurring skeleton candidates  8
NEAR_EXISTING                  5
REVIEW                         2
HOLD                           1
ACCEPT                         0
```

The two REVIEW candidates are:

| Candidate | Structural groups | Distinct source IDs | Directions | Runtime-compatible Kick/Backbeat grammar | Ornament median distance |
|---|---:|---:|---|---:|---:|
| `SKEL_04` | 4 | 2 | Afro-Cuban, Bossa Nova | 0 | 0.455357 |
| `SKEL_05` | 4 | 2 | Electro, Go-Go, Hip-Hop, Miami Bass | 0 | 0.710623 |

`source_count` means distinct Atlas `source_id` values only; it is not treated as proof of fully independent provenance.

`SKEL_08` also has no current compatible Kick/Backbeat grammar, but all three supporting groups come from one source ID and therefore remains HOLD.

No Pass 2 skeleton is admitted directly to Stage 7.

## 3. Distance distributions

Three object types are kept distinct.

### Atlas observation ↔ Atlas observation

Weighted role-wise Jaccard over normalized drum roles:

```text
all structural groups:
p50 0.644380
p75 0.776316
p95 0.902146

research structural groups:
p50 0.629825
p75 0.745614
p95 0.894737
```

Known `VARIATION_OF` edges are much closer:

```text
count 24
p50 0.297807
p75 0.361090
p95 0.528994
```

These values are empirical calibration data, not automatic Stage 7 thresholds.

### Runtime grammar ↔ runtime grammar

The runtime comparison is a **grammar-envelope diagnostic**, not an observation distance:

```text
20 current archetypes / 190 pairs
support-envelope Jaccard:
p50 0.471053
p75 0.623277
p95 0.842105
```

### Atlas observation → nearest runtime grammar

No weighted scalar score is used for admission. Nearest runtime is selected lexicographically by:

```text
required-anchor misses
    ↓
outside-support hits
    ↓
density deviation
    ↓
soft support Jaccard
```

The resulting metrics are stored separately and explicitly marked `diagnostic`.

Observation-to-grammar comparison cannot prove full archetype equivalence because a concrete observation and a runtime grammar are different abstraction levels. Relationships and protected space therefore remain separate evidence passes.

## 4. Negative / protected-space evidence

Pass 2 extracts negative-space observations separately from topology.

For each research direction and drum role it first deduplicates by structural group, then considers **only groups where that role is active somewhere**. An absent role is therefore not counted as evidence that every step is protected.

A row is emitted only when:

```text
active structural groups >= 5
step absence fraction    >= 0.90
```

The full research corpus produces:

```text
negative-space aggregate rows  176
BassRhythm rows                  0
```

These are `RESEARCH_AGGREGATE` step-occupancy observations and are stored in `ATLAS_PASS2_NEGATIVE_SPACE.csv`.

They are **not** automatically converted to runtime `ProtectedSpace`. Stage 7 curation must still decide whether repeated absence is a musical constraint, a consequence of another relationship, or merely low density.

## 5. Phrase development

Measured multi-bar evidence is kept separate from derived composites.

### Measured source observations

There are 19 two-bar source observations. Eleven use an 8-step source grid; they are normalized to the runtime 16-step coordinate system by mapping source step `n` to runtime step `2n`.

The phrase pass accepts both the normal drum role labels and the Atlas literal `PERCUSSION` used by `PERC1/PERC2` on several measured source patterns. Those events materially affect the transition counts and are covered by a regression.

Measured adjacent-bar deltas:

```text
EXACT_REPEAT   4
ADD_ONLY       3
DROP_ONLY      2
MIXED         10
```

This proves the presence of repeat/add/drop/mixed transition primitives. It does **not** by itself prove one universal named four-bar trajectory.

### Derived four-bar composites

There are 17 derived four-bar composites. Their transition-class sequences are reported as `EDITORIAL_CURATED`, not `MEASURED`.

The most common derived sequence is:

```text
EXACT_REPEAT > ADD_ONLY > MIXED    7/17
```

The report therefore supports BarEvolution transformation vocabulary, but does not promote a derived `A A A' B` ordering into measured fact.

## 6. Bass evidence

One-bar BassRhythm evidence is not research-measured in this corpus.

```text
BassRhythm one-bar patterns  35
origin                        project/editorial recipe patterns
evidence class                PROJECT_OWNED_EXACT
```

Kick→BassRhythm relationship statistics are therefore emitted only as project-owned exact evidence.

No Bass v2 runtime strategy is admitted by Pass 2.

## 7. Bass pitch and motif evidence

Pitched one-bar material is likewise project/editorial rather than research-measured.

```text
Bass pitch contour eligible patterns   35
MelodicRhythm one-bar patterns          35
Motif contour eligible patterns         23
```

Contour and interval aggregates are emitted for hypothesis generation, but every row remains `HOLD`.

`pattern_count` in this table is descriptive coverage only. It is not an independent-support score and cannot be used for runtime admission; several rows come from related P1/P2/P3 project material.

Pass 2 must not claim that Atlas v2.6 independently proves a BassPitchStrategy or MotifVocabulary.

## 8. Effective variation baseline

Across the 12 published baseline recipes:

| Slot | Realizations | Distinct structural groups | Effective variation ratio |
|---|---:|---:|---:|
| P1 | 12 | 12 | 1.000000 |
| P2 | 12 | 10 | 0.833333 |
| P3 | 12 | 7 | 0.583333 |

This becomes the first source-corpus baseline for the future Stage 7 `effective_variation_count` gate.

It is not yet the generated-runtime metric. Stage 7 must measure generated P1/P2/P3 variants separately after normalization.

## 9. Rights / reversibility guard

Committed Pass 2 outputs contain only aggregate statistics.

They intentionally omit:

```text
pattern IDs
structural group IDs
structural/expressive hashes
source locators
literal event lists
literal Kick/Backbeat masks
per-pattern normalized fingerprints
```

The two REVIEW skeleton candidates are represented only by opaque candidate IDs and aggregate support/compatibility statistics.

Direction×role×step occupancy frequencies are allowed aggregate evidence under the Stage 6B rights contract; no per-pattern topology is published.

CI checks both output values and output field names for restricted identifier/hash/mask tokens. The canonical runner normalizes generated text to UTF-8/LF before producing a SHA-256 manifest so reproducibility is independent of CSV platform line endings.

## 10. Stage 7 gate

Stage 7 remains **CLOSED** after Pass 2.

A recurring skeleton is only a candidate observation cluster. Before it becomes a `RhythmArchetype`, curation must still establish:

```text
full multi-lane grammar
anchors / optional space
protected space
relationships
density corridor
compatibility
effective generated variation
between-archetype distance
listening acceptance
embedded cost
```

Current computational result:

```text
0 ACCEPT
2 REVIEW
1 HOLD novel skeleton
5 NEAR_EXISTING recurring skeletons
```

This is a successful Pass 2 result: it reduces the space for Stage 7 instead of manufacturing vocabulary.
