# Atlas Pass #1 — Evidence basis

**Status:** provenance note for the offline Pass #1 audit  
**Scope:** explain which Pass #1 statements are carried forward measurements, which are exact published vertical-slice observations, and which remain hypotheses for Pass #2.

## Normative evidence class

`ATLAS_PASS1_CANDIDATES.csv` has exactly one normative evidence field:

```text
primary_evidence_class
```

Allowed values are only:

```text
MEASURED
EDITORIAL_CURATED
PROJECT_OWNED_EXACT
RESEARCH_AGGREGATE
INSUFFICIENT
```

`supporting_evidence` is descriptive provenance/context and is **not** a second evidence class. It may refer to an editorial guideline, current Stage 3 runtime baseline, sampler dependency, or the fact that a recommendation is derived rather than directly measured.

The machine-readable `decision` field is also bounded to one value from:

```text
ACCEPT_BASELINE
ACCEPT_TRANSFORM
REVIEW
HOLD
```

No compound decision value is allowed.

## Non-normative review confidence

`confidence` is a **human review annotation only**. It is not an admission score, evidence weight, distance threshold, probability, or sortable quality metric.

Current labels are deliberately bounded for readability:

```text
high
medium_high
medium
low_medium
low
```

They have no numerical mapping. Automated Stage 7 admission MUST ignore `confidence` and use measured fields from the computational pass instead: observation counts, distinct structural groups, normalized distances, relationship support, rights status, determinism/cost gates and the final explicit `decision`.

Pass #2 may replace this editorial label with measured confidence/coverage statistics, but must not silently reinterpret the Pass #1 labels as numeric data.

## Carried-forward measured corpus facts

The following values are not recomputed in this PR because the pinned v2.6 ZIP is not stored in the GroovePuter repository. They are carried forward from the validated `atlas_generation_assessment.md` for archive SHA-256:

```text
5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd
```

including:

```text
413 patterns
9,377 events
381 structural groups
27 exact/expressive duplicate groups
302 one-bar patterns
93 two-bar patterns
17 four-bar patterns
92 composite sequences
12 published baseline recipes
362 rights-blocked research patterns
P1->P2 aggregate add/remove statistics
P1->P3 aggregate add/remove statistics
```

These are `MEASURED` only where the assessment itself reports a calculation. Recommendations contained in that assessment, such as a default `BASE -> BASE -> DEVELOPMENT -> ENDING` arrangement, remain recommendations and are not promoted into directly measured four-bar evidence.

## Published exact vertical slices

Concrete role masks/onsets and pitch examples used in Pass #1 are taken from the project-owned published P1/P2/P3 vertical slices in the Atlas exports (`track_patterns.csv` / researched workbook). Examples include:

```text
Chicago Jack / Rolling Acid
Classic 2-Step / Dark Skippy
Lo-Fi Classic Chill / Drunken Groove
Boom-Bap Golden Era / Dusty Jazz
Dub Deep Chord / Minimal Space
Jungle Classic Amen / Atmospheric Jungle
```

Those concrete observations are classified `PROJECT_OWNED_EXACT`.

If an interpretation also follows an Atlas editorial invariant or guidance row, that is recorded in `supporting_evidence=EDITORIAL_CURATED`; the primary exact observation does not become a compound evidence class.

## Runtime novelty baseline

The comparison baseline for Stage 7 novelty is the current Stage 3 `ReferenceVocabulary` of 20 curated archetypes. A note such as:

```text
current_stage3_417
CURRENT_STAGE3_BASELINE
```

is not Atlas evidence. It only records that an Atlas observation is already represented by current runtime grammar and therefore must not be counted as a new Stage 7 candidate.

The 20 current archetypes are a **calibration baseline, not an objective ground-truth distribution**. Pass #2 must therefore publish separate distance distributions for:

```text
current-runtime <-> current-runtime
Atlas-group <-> Atlas-group
Atlas-group -> nearest current runtime archetype
```

and declare the threshold-combination rule before candidate admission decisions are produced. A novelty floor derived only from pairwise distances among the hand-curated current 20 is not sufficient.

## Hypotheses held for Pass #2

The following remain non-admitted until a computational extraction is run on normalized data:

```text
Lo-Fi / Boom-Bap novelty versus current 20
Jungle sampler-independent topology novelty
per-family relationship support frequencies
real 2-bar / 4-bar trajectory classes
UKG RespondKick prevalence
Bass contour/interval support after transposition normalization
Motif contour/development support
numerical novelty threshold for Stage 7
```

Pass #2 should mount/verify the pinned archive, reproduce the corpus measurements, compute normalized fingerprints/distances, publish the three distance distributions and the predeclared threshold-combination rule, and emit a machine-readable report rather than relying on human confidence labels alone.
