# Atlas Pass #1 — Coverage and extraction audit

**Status:** measured offline audit; no production code  
**Base:** Stage 6A/6B docs candidate `d39dcd99b66b65183ca48d3a322522085a37180c`  
**Atlas evidence snapshot:** schema 2.6.0, pinned SHA-256 `5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd`  
**Purpose:** identify evidence-backed candidates and gaps before Stage 7 without inventing runtime vocabulary from genre labels.

Paired files:

```text
docs/architecture/ATLAS_PASS1_CANDIDATES.csv
docs/architecture/ATLAS_PASS1_EVIDENCE_BASIS.md
```

## 1. Pass boundary

Pass #1 uses three evidence inputs:

```text
validated v2.6 assessment for the pinned archive
project-owned published P1/P2/P3 vertical slices
current 20-archetype Stage 3 ReferenceVocabulary as novelty baseline
```

The raw pinned ZIP is not stored in the GroovePuter repository and was not available in the current file set. Pass #1 therefore does **not** claim to recompute all 413 patterns from source CSVs. That full normalized calculation is the next computational extraction gate.

No runtime archetype, enum, generated header, Scene field or generator routing is added here.

## 2. Evidence and decisions

Every machine-readable candidate has exactly one `primary_evidence_class`:

```text
MEASURED
EDITORIAL_CURATED
PROJECT_OWNED_EXACT
RESEARCH_AGGREGATE
INSUFFICIENT
```

Context belongs in `supporting_evidence`, not in a compound class.

Decision meanings:

```text
ACCEPT_BASELINE  validates knowledge already represented in runtime
ACCEPT_TRANSFORM strong transform evidence; not production permission
REVIEW           useful hypothesis, quantitative support/distinctness incomplete
HOLD             evidence too weak/editorial/dependency-sensitive for admission
```

## 3. Corpus baseline

The pinned v2.6 assessment reports:

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
| Rights-blocked research patterns | 362 |

Duplicate structure already matters:

```text
27 duplicate grid groups cover 59 patterns
P2: 12 published patterns / 10 structural grids
P3: 12 published patterns / 7 structural grids
```

Raw pattern, slot or seed count is not a vocabulary metric. Expression-only differences never create a new RhythmArchetype.

## 4. Strongest measured signal — phrase function

Across 12 published recipes:

| Transition | Mean added | Mean removed | Mean net |
|---|---:|---:|---:|
| P1 -> P2 / Development | 9.33 | 0.58 | +8.75 |
| P1 -> P3 / Break/fill/ending | 2.92 | 11.17 | -8.25 |

Evidence-backed transform candidates:

```text
Development
    preserve identity
    mostly add activity

Break / Ending
    preserve critical identity
    remove substantial activity
    optionally add a small ending cue
```

Decisions:

```text
Development preserve+add        ACCEPT_TRANSFORM
Break/Ending remove+small cue   ACCEPT_TRANSFORM
```

`BASE -> BASE -> DEVELOPMENT -> ENDING` remains `REVIEW`. Its **primary evidence is `EDITORIAL_CURATED`** because the four-bar ordering is a derived recommendation, while the measured P1->P2/P3 statistics are supporting evidence for its component transforms.

`Statement -> Response` topology change remains `HOLD` until a real bar-to-bar response transform is extracted. This matches Stage 6.1 keeping Response metadata-only.

## 5. Novelty baseline — current runtime vocabulary

Stage 7 must compare candidates against the existing 20:

```text
straight_drive
offbeat_open_hat
hypnotic_sparse
broken_techno
straight_acid
rolling_acid
syncopated_acid
sparse_acid
one_drop_space
steppers
sparse_skank
chord_response
two_step_roll
ghosted_roll
sparse_fast_break
halftime_switch
classic_2step
skippy_2step
shuffled_4x4
machine_syncopation
```

The current grammar already contains core operators such as Coincide, Exclude, Respond, FillGaps, protected backbeat space and shuffle-sensitive timing eligibility.

A candidate is new only if normalized musical structure is not already covered by this set.

---

# Pass A — Rhythm topology

### Acid

Chicago Jack P1 exposes a four-floor frame:

```text
Kick       1 5 9 13
Backbeat   5 13
ClosedHat  3 7 11 15
OpenHat    7 15
```

Rolling Acid keeps the broad drum frame while Bass onset density increases.

Orthogonal decision:

```text
Chicago/Rolling drum topology   -> RhythmArchetype baseline -> ACCEPT_BASELINE
Rolling denser Bass onsets      -> BassRelationshipStrategy -> REVIEW
Rolling pitch motion            -> BassPitchStrategy        -> REVIEW
```

No combined Rhythm+Bass candidate exists.

### UK Garage

Classic 2-Step P1:

```text
Kick       1 7 11
Backbeat   5 13
ClosedHat  3 6 8 11 14 16
Bass       1 6 10 15
```

Dark Skippy P1:

```text
Kick       1 7
Backbeat   main anchors 5/13, ghost near 12
ClosedHat  3 6 8 11 14 16
Bass       1 6 10 15
```

Current runtime already has `classic_2step` and `skippy_2step`. These slices validate coverage; they do not create recipe-named duplicates.

### Lo-Fi / Boom-Bap

Published slices show stable backbeat identity with sparse/syncopated kick behavior:

```text
Lo-Fi sparse/stable-backbeat topology       REVIEW
Boom-Bap syncopated-kick/backbeat topology REVIEW
```

Normalized distance to the current 20 is not yet measured.

### Jungle

Published Jungle slices are break-dense and sampler-heavy. The assessment reports 53 of 253 Jungle recipe events on sampler track.

```text
Jungle sampler-independent topology HOLD
```

Reconsider it only after a sampler-normalization policy and normalized topology measurement exist.

### Rhythm gap

Pass #2 must compute:

```text
Atlas structural group
 -> semantic role topology
 -> nearest current archetype
 -> normalized distance
 -> covered or genuinely new cluster
```

No Stage 7 archetype should be added before this calculation.

---

# Pass B — Lane relationships / protected space

Observed baseline:

```text
UKG stable backbeat + broken Kick
    -> Backbeat/Kick exclusion
    -> protected backbeat space

UKG Bass 1,6,10,15 across different Kick densities
    -> not simple FollowKick
    -> Respond/FillGaps hypothesis

Acid four-floor Kick + denser Bass
    -> Coincide anchors + independent Offset movement
```

| Candidate | Primary evidence | Owner | Decision |
|---|---|---|---|
| UKG Backbeat/Kick exclusion | PROJECT_OWNED_EXACT | `LaneRelationship` | ACCEPT_BASELINE |
| UKG protected backbeat space | PROJECT_OWNED_EXACT | `ProtectedSpace` | ACCEPT_BASELINE |
| UKG `FillKickGaps` | PROJECT_OWNED_EXACT | `BassRelationshipStrategy` | ACCEPT_BASELINE |
| UKG `RespondKick` | PROJECT_OWNED_EXACT | `BassRelationshipStrategy` | REVIEW |
| Acid Coincide+Offset | PROJECT_OWNED_EXACT | `BassRelationshipStrategy` | ACCEPT_BASELINE |
| Boom-Bap protected vocal/sample space | EDITORIAL_CURATED | `ProtectedSpace` | REVIEW |
| Jungle bass/transient protected space | EDITORIAL_CURATED | `BassRelationshipStrategy` | HOLD |

Pass #2 must count relationship support by **distinct structural group**, not raw rows.

---

# Pass C — 2–4 bar development

Evidence volume:

```text
1 bar  302
2 bar   93
4 bar   17
```

The 92 composite sequences are derived arrangements and are not independent observations.

| Candidate | Primary evidence | Supporting evidence | Owner | Decision |
|---|---|---|---|---|
| Development preserve+add | MEASURED | 12 published recipe transitions | `BarEvolution` | ACCEPT_TRANSFORM |
| Break/Ending remove+cue | MEASURED | 12 published recipe transitions | `BarEvolution` | ACCEPT_TRANSFORM |
| BASE BASE DEVELOPMENT ENDING | EDITORIAL_CURATED | measured component transforms | `BarTrajectory` | REVIEW |
| Statement -> Response topology transform | INSUFFICIENT | none | `BarEvolution` | HOLD |
| Return after development/break | INSUFFICIENT | none | `BarEvolution` | HOLD |

Next calculation: classify actual 2/4-bar normalized bar-to-bar deltas against Stage 6 BarFunctions.

---

# Pass D — Bass rhythm

Bass rhythm is independent from Bass pitch.

```text
UKG Classic:     Kick 1 7 11 / Bass 1 6 10 15
UKG Dark Skippy: Kick 1 7    / Bass 1 6 10 15
```

Bass topology survives a meaningful Kick-density change, arguing against `Bass = Kick copy`.

Acid likewise combines four-floor Kick anchors with denser independent Bass onsets.

Decisions:

```text
UKG FillKickGaps                 ACCEPT_BASELINE
Acid Coincide+Offset            ACCEPT_BASELINE
UKG RespondKick                 REVIEW
Rolling denser Bass onset plan  REVIEW
Sparse long-note break Bass     HOLD
AnticipateKick                  HOLD
SparseIndependent               HOLD
```

Before Stage 8, compute Bass/Kick/Backbeat relation histograms deduplicated by structural group.

---

# Pass E — Bass pitch / contour

Pitch evidence is weaker than onset topology because Atlas tonal/melodic guidance is partly editorial and transposable.

Published exact slices still support hypotheses for repeated-note/interval Acid motion, Rolling octave motion, UKG low-register phrases, root/chord-tone gravity and sparse long sub notes.

| Candidate | Primary evidence | Owner | Decision |
|---|---|---|---|
| AcidContour | PROJECT_OWNED_EXACT | `BassPitchStrategy` | REVIEW |
| OctaveMotion | PROJECT_OWNED_EXACT | `BassPitchStrategy` | REVIEW |
| Pedal/root gravity | PROJECT_OWNED_EXACT | `BassPitchStrategy` | REVIEW |
| ChordTone gravity | PROJECT_OWNED_EXACT | `BassPitchStrategy` | REVIEW |
| StepwiseMotion | INSUFFICIENT | `BassPitchStrategy` | HOLD |

No BassPitchStrategy is admitted by Pass #1. Pass #2 must normalize transposition and count contour/interval support.

---

# Pass F — Motif / melodic development

This is the weakest domain. The v2.6 assessment treats tonal/melodic guidance as editorial/transposable rather than evidence for literal genre melodies.

```text
short motif reuse       REVIEW
ExactRepeat             HOLD
RepeatWithEnding        HOLD
Arch / Rise / Fall      HOLD
SequenceUp/Down         HOLD
Compress / Expand       HOLD
```

Stage 9 still needs motif segmentation, transposition-normalized interval/contour fingerprints, bar-to-bar motif identity and multi-bar development evidence.

The correct response to this gap is more evidence/curation, not more random-note entropy.

---

## 6. Gap map

| Domain | Evidence strength | Main missing measurement |
|---|---|---|
| Rhythm topology | strong | distance of Atlas groups to current 20 |
| Relationships | medium-high | per-family support by structural group |
| Protected space | medium | aggregate masks/corridors |
| Phrase transform function | **high** | map real 2/4-bar deltas to functions |
| Four-bar trajectory catalog | low-medium | only 17 independent four-bar patterns |
| Bass rhythm | medium-high | complete relation histogram |
| Bass pitch | medium | transposition-normalized contour support |
| Motif | low | segmentation + identity/development extraction |
| Feel/expression | low-medium | measured coverage sparse; much guidance editorial |
| Jungle exact identity | medium | sampler-independent normalization |

Summary:

```text
Strong enough now:
    rhythm structural baseline
    relationship operator foundation
    Development / Break phrase-function evidence

Promising but not admitted:
    Lo-Fi / Boom-Bap topology
    UKG RespondKick
    Rolling Bass-rhythm density behavior
    Acid bass contour

Weak / HOLD:
    broad Motif grammar
    universal 4-bar trajectory catalog
    detailed measured Feel priors
```

## 7. Stage 7 admission rule

A new `RhythmArchetype` is eligible only if:

```text
1. one primary evidence class is explicit;
2. generalized output is rights-safe;
3. semantic role-topology fingerprint is not already covered by current 20;
4. nearest-current distance exceeds an evidence-derived novelty floor;
5. difference is structural, not velocity/timing/kit/pitch/timbre only;
6. identity-defining relationships/protected spaces are explicit;
7. density corridor is supported by distinct structural groups;
8. candidate survives duplicate/near-duplicate clustering;
9. candidate has one runtime owner;
10. listening acceptance is required once production-reachable.
```

Do not choose the novelty threshold manually. Derive it from the distance distribution among accepted current archetypes and Atlas structural groups.

```text
four-floor variant differing only in velocity/kit -> REJECT
repeated broken relationship cluster meaningfully distant from current 20 -> REVIEW
```

## 8. `effective_variation_count` — central Stage 7 metric

For each archetype and P-level:

```text
N generated realizations
 -> normalize irrelevant expression/timbre
 -> compute dimension-specific fingerprints
 -> count unique musical fingerprints
 -> effective_variation_count
```

Required metrics:

```text
P1_effective_variants
P2_effective_variants
P3_effective_variants
within_archetype_diversity
between_archetype_distance
between_genre_overlap
identity_distance_P1_P2
identity_distance_P1_P3
```

Rhythm structural fingerprints exclude velocity, timing jitter, kit/synth TYPE, absolute pitch and FX. They contain semantic role structural+secondary onset topology.

Phrase fingerprints additionally contain ordered bar fingerprints, bar-function/development relation and normalized delta from phrase identity.

Future Motif fingerprints normalize transposition and compare interval/contour/development identity rather than absolute MIDI notes.

Failure signatures:

```text
100 seeds -> 4 fingerprints        = fake variability
20 archetypes -> same fingerprints = fake vocabulary
P1/P2/P3 too far apart             = phrase identity loss
P1/P2/P3 too often identical       = fake P-level variation
```

Pass #1 freezes metric dimensions, not arbitrary numeric thresholds.

## 9. Input contract for the computational extraction

The next measured pass must mount and verify the pinned Atlas archive and produce:

```text
A. every eligible structural group -> nearest current archetype
B. role-topology distance + relationship signature
C. uncovered candidate clusters
D. per-family relationship support frequencies
E. normalized 2/4-bar delta classes
F. transposition-normalized Bass onset + contour fingerprints
G. transposition-normalized Motif fingerprints where evidence is sufficient
```

Machine fields:

```text
candidate_id
domain
primary_evidence_class
supporting_evidence
observation_count
distinct_structural_groups
covered_families_or_variants
nearest_runtime_owner_or_archetype
normalized_distance
confidence
rights_safe_for_runtime
decision
reason
```

Only after that report exists should Stage 7 select its first approximately 10–20 additions.

## 10. Acceptance checklist

```text
[x] No production code added.
[x] Six extraction domains remain independent.
[x] Each machine candidate has exactly one primary evidence class.
[x] Each machine candidate has one decision from ACCEPT_BASELINE / ACCEPT_TRANSFORM / REVIEW / HOLD.
[x] Each candidate has one runtime owner; Rhythm/Bass/Pitch candidates are not fused.
[x] Full corpus measurements and exact published slices are distinguished.
[x] Current 20 runtime archetypes are the novelty baseline.
[x] Existing UKG/Acid coverage is not duplicated under recipe labels.
[x] Phrase-function measurements are separated from four-bar ordering hypotheses.
[x] Bass rhythm and Bass pitch remain separate.
[x] Motif candidates remain HOLD/REVIEW when evidence is sparse/editorial.
[x] Rights-blocked data remains aggregate-only.
[x] Duplicate/expression-only differences cannot inflate Rhythm vocabulary.
[x] `effective_variation_count` is central to Stage 7 acceptance.
[x] Stage 7 remains blocked pending normalized novelty/distance extraction and Stage 6.1 convergence.
```
