# Atlas Pass #1 — Coverage and extraction audit

**Status:** measured offline audit; no production code  
**Base:** Stage 6A/6B docs candidate `0e81f6a03604609f37cdcc460ed5913f22483046`  
**Atlas evidence snapshot:** schema 2.6.0, pinned SHA-256 `5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd`  
**Purpose:** produce the first evidence-backed candidate/gap map for post-Stage-6 vocabulary work without adding runtime archetypes, enums, generated headers or Scene fields.

This pass deliberately answers:

```text
What does Atlas support strongly?
What is already represented by the current 20-archetype runtime vocabulary?
What appears structurally novel but still needs quantitative extraction?
What is too weak/editorial to admit yet?
```

It does **not** answer by inventing 20 new archetype names.

---

# 1. Evidence classes and decision vocabulary

Evidence classes follow `ATLAS_VOCABULARY_EXTRACTION.md`:

```text
MEASURED
PROJECT_OWNED_EXACT
EDITORIAL_CURATED
RESEARCH_AGGREGATE
INSUFFICIENT
```

Candidate decisions used in this pass:

```text
ACCEPT_BASELINE
    evidence strongly validates a grammar already represented in runtime;
    it is not a new Stage 7 item.

ACCEPT_TRANSFORM
    evidence is strong enough to freeze a phrase/development transform
    candidate for later curated runtime data.

REVIEW
    candidate is plausible and useful, but support/distinctness still needs
    a quantitative extraction or comparison before runtime admission.

HOLD
    evidence is currently too sparse/editorial/recoverability-sensitive for
    runtime admission.
```

`ACCEPT_*` in this audit is **not permission to write production code**. Stage 7 still has its own admission gate.

---

# 2. Corpus baseline

The v2.6 assessment reports:

| Corpus item | Count |
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

Raw pattern count is not a vocabulary metric: duplicate/near-duplicate normalization remains mandatory.

Published P-level distinctness is already asymmetric:

```text
P2: 12 published patterns / 10 structural grids
P3: 12 published patterns / 7 structural grids
```

This is an important first warning for Stage 7 metrics: P3 labels overstate structural variety more than P2 labels do.

The 27 exact duplicate-grid groups cover 59 patterns. Expression differences remain separate evidence and must not create new rhythm archetypes.

---

# 3. Strong measured phrase signal

Across the 12 published recipes, the existing v2.6 assessment measured:

| Transition | Mean added events | Mean removed events | Mean net |
|---|---:|---:|---:|
| P1 -> P2 / Development | 9.33 | 0.58 | +8.75 |
| P1 -> P3 / Break/fill/ending | 2.92 | 11.17 | -8.25 |

Pass #1 therefore accepts two independent transform classes as evidence-backed:

```text
Development:
    preserve phrase identity
    mostly add activity

Break / Ending:
    preserve critical identity
    remove substantial activity
    optionally add a small ending gesture
```

The useful sequence:

```text
BASE -> BASE -> DEVELOPMENT -> ENDING
```

remains a **REVIEW trajectory template**, not a measured universal law. The aggregate transition data supports the functions, while exact four-bar ordering still needs trajectory extraction from the 2/4-bar evidence and composite-sequence policy.

---

# 4. Current runtime baseline that Stage 7 must not duplicate

The Stage 3 reference vocabulary already contains 20 curated one-bar grammars:

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

The current catalog already represents relationship/protected-space ideas such as Coincide, Exclude, Respond, FillGaps, backbeat protected space and shuffle-sensitive role eligibility.

Therefore Stage 7 admission compares Atlas candidates to these grammars, not to the old procedural generator.

---

# 5. Pass A — Rhythm topology

## Published vertical-slice observations

### Acid

Project-owned Chicago Jack P1 uses:

```text
Kick:       1 5 9 13
Backbeat:   5 13
ClosedHat:  3 7 11 15
OpenHat:    7 15
```

Rolling Acid keeps the same four-floor drum frame while materially increasing bass-event density.

This validates that Acid identity is not another `RhythmFamily`; the shared rhythmic frame composes with a distinct bass role/contour.

Current Stage 3 already contains `straight_acid`, `rolling_acid`, `syncopated_acid`, `sparse_acid`, so these exact recipes are baseline/calibration evidence, not automatic Stage 7 additions.

### UK Garage

Classic 2-Step P1 uses a broken kick and clear backbeat:

```text
Kick:       1 7 11
Backbeat:   5 13
ClosedHat:  3 6 8 11 14 16
Bass:       1 6 10 15
```

Dark Skippy P1 is sparser in the kick:

```text
Kick:       1 7
Backbeat:   main anchors remain 5/13 with a ghost at 12
ClosedHat:  3 6 8 11 14 16
Bass:       1 6 10 15
```

Current Stage 3 already contains `classic_2step` and `skippy_2step` with backbeat exclusion/protected space and bass gap-filling tendencies. These Atlas slices therefore validate existing topology coverage; they do not justify duplicate runtime archetypes under recipe labels.

### Jungle

Classic Amen P1 contains a dense break-oriented drum surface and an eight-event sampler break layer. The v2.6 assessment separately notes that Jungle exact recipes are sampler-dependent enough that exact runtime expansion should wait for a sampler strategy.

Current Stage 3 already has four breakbeat grammars, but the Atlas observation may still contain structural novelty after sampler-independent normalization. That comparison has not yet been measured in Pass #1.

### Lo-Fi and Boom-Bap

The published vertical slices show stable backbeat identity with softer/syncopated kick and expression differences. However only two editorial/project-owned variants per family are available in this vertical slice set, and expression is an important part of their audible distinction.

These are candidates for topology extraction, not yet accepted archetypes.

## Rhythm candidate decisions

| Candidate | Domain | Evidence | Distinctness vs current 20 | Confidence | Runtime owner | Decision |
|---|---|---|---|---|---|---|
| `classic_2step` evidence slice | Rhythm | PROJECT_OWNED_EXACT | already represented by 417 | high | `RhythmArchetype` | ACCEPT_BASELINE |
| `dark_skippy` evidence slice | Rhythm | PROJECT_OWNED_EXACT | overlaps 418; exact distance not measured | high evidence / medium novelty | `RhythmArchetype` | ACCEPT_BASELINE + REVIEW distance |
| Chicago four-floor acid frame | Rhythm | PROJECT_OWNED_EXACT | already covered by Acid/FourFloor grammars | high | `RhythmArchetype` | ACCEPT_BASELINE |
| Rolling Acid drum frame | Rhythm | PROJECT_OWNED_EXACT | drum topology largely same family; novelty lives strongly in bass density/contour | high | `RhythmArchetype` | ACCEPT_BASELINE |
| Lo-Fi stable-backbeat sparse-kick family | Rhythm | PROJECT_OWNED_EXACT + EDITORIAL_CURATED | comparison to current 20 not measured | medium | `RhythmArchetype` | REVIEW |
| Boom-Bap syncopated-kick strong-backbeat family | Rhythm | PROJECT_OWNED_EXACT + EDITORIAL_CURATED | comparison to current 20 not measured | medium-high | `RhythmArchetype` | REVIEW |
| Jungle break/sampler topology | Rhythm | PROJECT_OWNED_EXACT | likely distinct, but sampler normalization unresolved | medium | `RhythmArchetype` + external sampler policy | HOLD/REVIEW |

### Pass-A gap

We still need a machine calculation of normalized role-topology distance between every candidate structural group and each of the current 20 runtime archetypes. Without that, adding any new Stage 7 archetype would be premature.

---

# 6. Pass B — Lane relationships and protected space

UKG vertical slices repeatedly preserve the main backbeat while kick remains broken. Current runtime already expresses this as hard Backbeat/Kick exclusion plus protected backbeat space for Kick/Bass.

Classic 2-Step exact P1 places Bass on `1,6,10,15` against Kick `1,7,11`. This supports a mixed relationship rather than a simple `Bass == Kick` rule: one onset coincides while several are adjacent/answering/gap-filling.

Acid exact slices combine a stable four-floor kick with denser bass onsets, supporting Coincide anchors plus independent legal Offset/extra movement rather than full lockstep.

| Candidate | Domain | Evidence | Distinctness | Confidence | Runtime owner | Decision |
|---|---|---|---|---|---|---|
| Backbeat excludes UKG kick | Relationship | PROJECT_OWNED_EXACT | already represented | high | `LaneRelationship` | ACCEPT_BASELINE |
| UKG backbeat protected space | Protected space | PROJECT_OWNED_EXACT | already represented | high | `ProtectedSpace` | ACCEPT_BASELINE |
| UKG Bass `Respond/FillGaps` | Bass rhythm relation | PROJECT_OWNED_EXACT + EDITORIAL_CURATED | existing runtime has `FillGaps`, exact relation mix not quantified | medium-high | future Bass relationship plan | REVIEW |
| Acid Kick/Bass Coincide + Offset | Bass rhythm relation | PROJECT_OWNED_EXACT | concept already represented in Acid grammars | high | relationship plan | ACCEPT_BASELINE |
| Boom-Bap kick leaves vocal/sample space | Protected space | EDITORIAL_CURATED + exact slices | masks/corridor not aggregated | medium | `ProtectedSpace` | REVIEW |
| Jungle bass leaves break transients clear | Protected space / relationship | EDITORIAL_CURATED | exact quantitative window not extracted | low-medium | future Bass relationship plan | HOLD |

### Pass-B gap

The next raw-table extraction must report per-family support for Coincide, Offset windows, Respond windows, Exclude and FillGaps by **distinct structural group**, not raw pattern rows.

---

# 7. Pass C — 2–4 bar development

Evidence strength differs sharply by length:

```text
1-bar patterns: 302
2-bar patterns:  93
4-bar patterns:  17
```

There are also 92 composite sequences, but derived/composite sequences must not be counted as independent single-pattern observations.

The P1/P2/P3 transition aggregate is currently stronger evidence than any one named four-bar trajectory.

| Candidate | Domain | Evidence | Distinctness | Confidence | Runtime owner | Decision |
|---|---|---|---|---|---|---|
| `Development = preserve + add` | Phrase transform | MEASURED across 12 published recipes | high functional distinction | high | `BarEvolution` / phrase development | ACCEPT_TRANSFORM |
| `Break/Ending = remove + small ending cue` | Phrase transform | MEASURED across 12 published recipes | high functional distinction | high | `BarEvolution` / phrase development | ACCEPT_TRANSFORM |
| `BASE BASE DEVELOPMENT ENDING` | 4-bar trajectory | derived recommendation from measured transforms | not measured against 17 four-bar patterns | medium | `BarTrajectory` | REVIEW |
| `Statement -> Response` topology transform | Phrase trajectory | no quantitative transform evidence yet | unknown | low | `BarEvolution` | HOLD |
| `Return` after break/development | Phrase trajectory | plausible; not yet quantified in corpus pass | unknown | low-medium | `BarEvolution` | REVIEW/HOLD |

### Pass-C gap

Atlas Pass #1 does **not** justify making every Stage 6 BarFunction production-reachable. The next extraction must classify the 93 two-bar and 17 four-bar patterns by normalized bar-to-bar topology delta and compare them with the BarFunction vocabulary.

---

# 8. Pass D — Bass rhythm

Bass rhythm is evaluated independently of pitch.

UKG Classic P1:

```text
Kick: 1 7 11
Bass: 1 6 10 15
```

Dark Skippy P1:

```text
Kick: 1 7
Bass: 1 6 10 15
```

The repeated Bass topology across two different kick densities is evidence that the bass line is not simply regenerated from kick coincidence. The editorial invariant also describes the sub as answering drums / not masking kick.

Acid slices show four-floor kick plus dense independent bass-onset activity; this supports a mixed Coincide+Offset relation rather than FollowKick.

Jungle exact slices use sparse longer bass notes while sampler/drum surface remains dense, but sampler dependency prevents a confident generalized relationship from this pass alone.

| Candidate | Evidence | Distinctness | Confidence | Future owner | Decision |
|---|---|---|---|---|---|
| `RespondKick` for UKG | PROJECT_OWNED_EXACT + EDITORIAL_CURATED | plausible distinct from FollowKick | medium-high | Bass relationship strategy | REVIEW |
| `FillKickGaps` for UKG | PROJECT_OWNED_EXACT + existing Stage3 relation | already partly represented | high baseline | Bass relationship strategy | ACCEPT_BASELINE |
| Acid Coincide+Offset | PROJECT_OWNED_EXACT | distinct from pure follow | high | Bass relationship strategy | ACCEPT_BASELINE |
| Sparse long-note break bass | PROJECT_OWNED_EXACT + EDITORIAL_CURATED | likely distinct | medium-low | Bass relationship strategy | HOLD |
| `AnticipateKick` | no measured aggregate in current pass | unknown | low | Bass relationship strategy | HOLD |
| `SparseIndependent` | concept visible in sparse styles, not quantified | unknown | low-medium | Bass relationship strategy | HOLD |

### Pass-D gap

Before Stage 8, compute relation histograms using transposition-independent bass onset masks and kick/backbeat masks, deduplicated by structural group.

---

# 9. Pass E — Bass pitch / contour

The Atlas assessment explicitly warns that tonal/melodic guidance is partly editorial and transposable. Pitch candidates therefore receive stricter decisions than rhythm topology.

Project-owned exact slices nevertheless demonstrate useful candidate behavior:

```text
Acid Chicago / Rolling:
    monophonic repeated-note + interval movement
    Rolling has markedly higher note density
    accent/slide semantics exist

UKG:
    short low-register line over four bass onsets

Lo-Fi / Boom-Bap / Dub:
    sparse low-register chord-tone/root-oriented lines in published slices

Jungle:
    long sparse sub notes in the exact vertical slices
```

| Candidate | Evidence | Distinctness | Confidence | Future owner | Decision |
|---|---|---|---|---|---|
| `AcidContour` | PROJECT_OWNED_EXACT + editorial invariant | likely distinct | medium-high | `BassPitchStrategy` | REVIEW |
| `OctaveMotion` | PROJECT_OWNED_EXACT/EDITORIAL_CURATED in Rolling Acid | likely useful | medium | `BassPitchStrategy` | REVIEW |
| `Pedal` / repeated-note gravity | PROJECT_OWNED_EXACT across multiple slices, not aggregated | unknown support count | medium | `BassPitchStrategy` | REVIEW |
| `ChordTone` gravity | PROJECT_OWNED_EXACT + editorial harmony guidance | not quantitatively extracted | medium | `BassPitchStrategy` | REVIEW |
| `StepwiseMotion` | insufficient measured evidence in current pass | unknown | low | `BassPitchStrategy` | HOLD |

No Bass pitch candidate is admitted to runtime by Pass #1. The required next measurement is transposition-normalized contour/interval fingerprinting over every eligible bass line.

---

# 10. Pass F — Motif / melodic development

This is the weakest evidence domain in the current pass.

The v2.6 assessment states that tonal/melodic guidance is editorial and transposable and should not be treated as evidence for literal genre melodies. Published DX/melody lines are often sparse one-bar examples.

| Candidate | Evidence | Distinctness | Confidence | Future owner | Decision |
|---|---|---|---|---|---|
| short motif reuse | PROJECT_OWNED_EXACT + EDITORIAL_CURATED | not normalized | medium | future `MotifVocabulary` | REVIEW |
| `ExactRepeat` | plausible, no corpus-level normalized count yet | unknown | low-medium | future `MotifVocabulary` | HOLD |
| `RepeatWithEnding` | phrase evidence supports endings generally, not melodic motif operation specifically | unknown | low | future `MotifVocabulary` | HOLD |
| `Arch` / `Rise` / `Fall` | sparse/editorial melodic evidence | unknown | low | future `MotifVocabulary` | HOLD |
| sequence/compress/expand | not measured in Pass #1 | unknown | low | future `MotifVocabulary` | HOLD |

### Pass-F gap

Stage 9 cannot be designed from this evidence alone. It needs transposition-normalized motif segmentation and bar-to-bar motif identity analysis. This is a genuine evidence gap, not something to hide with more random notes.

---

# 11. Gap map

| Domain | Current evidence | Runtime readiness | Main gap |
|---|---|---|---|
| Rhythm topology | strong structural corpus + published slices | existing 20 are a good baseline | quantitative distance of new groups to current 20 |
| Relationships | strong enough for core operators | baseline operators already exist | per-family support frequencies by structural group |
| Protected space | clear in UKG and curated invariants | partial baseline support | derive masks/corridors beyond hand-curated examples |
| Phrase transforms | **strongest new signal** | Development/Break semantics evidence-backed | map real 2/4-bar deltas to function vocabulary |
| Four-bar trajectories | only 17 four-bar patterns + derived composites | not ready for broad catalog | scarce independent evidence |
| Bass rhythm | promising UKG/Acid evidence | not a dedicated runtime vocabulary yet | full relation histogram |
| Bass pitch | useful exact slices but mixed editorial status | not ready | normalized contour/interval support + dedupe |
| Motif | sparse/editorial one-bar material | not ready | motif segmentation, normalized identity, multi-bar development |
| Feel/expression | only 47 expressive + 3 partial patterns in v2.6 assessment | existing FEEL should stay separate | measured vs editorial coverage is sparse |
| Jungle exact identity | strong musical identity but sampler-heavy | not ready for exact expansion | sampler strategy + sampler-independent topology normalization |

The first pass therefore says:

```text
Strong now:
    structural rhythm
    relationship vocabulary foundation
    P1/P2/P3 phrase-function delta

Promising but needs extraction:
    new Lo-Fi/Boom-Bap rhythm candidates
    UKG/Acid bass relationship profiles
    bass contour strategies

Weak / must not be invented yet:
    broad motif grammar
    universal 4-bar trajectory catalog
    detailed measured Feel priors
```

---

# 12. Stage 7 admission rule

A proposed Stage 7 `RhythmArchetype` is admitted only if all are true:

```text
1. evidence_class is explicit;
2. rights-safe generalized representation is possible;
3. normalized role-topology fingerprint is not already covered by current 20;
4. nearest-current-archetype distance exceeds the evidence-derived novelty floor;
5. difference is structural, not only velocity/timing/kit/pitch/timbre;
6. protected space / relationships that create the identity are explicit;
7. density corridor is backed by distinct structural groups;
8. candidate survives duplicate/near-duplicate clustering;
9. candidate has one clear runtime owner;
10. listening acceptance is scheduled once production-reachable.
```

Do not set the numerical novelty threshold by intuition. Pass #2 must first compute the distance distribution among known accepted archetypes and Atlas structural groups.

Consequences:

```text
"Detroit four-floor #3" differing only in velocity/kit
    -> REJECT

new broken kick/backbeat/protected-space relationship
with repeated structural support
    -> eligible for REVIEW/ACCEPT
```

---

# 13. `effective_variation_count` — central Stage 7 metric

Stage 7 must stop reporting raw seed count as musical variability.

For each archetype and P-level:

```text
N generated realizations
        ↓
normalize irrelevant expression/timbre
        ↓
compute dimension-specific fingerprints
        ↓
count unique musical fingerprints
        ↓
effective_variation_count
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

## Rhythm structural fingerprint

At minimum normalize to semantic roles and record per bar:

```text
structural + secondary onset mask by role
protected-space violations = impossible by acceptance
bar function / phrase coordinate when phrase metric is requested
```

Do not include velocity, timing jitter, kit/synth TYPE, absolute pitch or FX in a rhythm-topology fingerprint.

## Phrase fingerprint

Phrase comparison adds ordered per-bar structural fingerprints, bar-function/development relation and normalized delta from phrase identity.

## Motif fingerprint later

Motif distinctness must normalize transposition and compare interval/contour/development identity rather than absolute MIDI notes.

## Failure signatures

```text
100 seeds -> 4 structural fingerprints
    = fake variability

20 archetypes -> same/near-same normalized fingerprints
    = fake vocabulary

P1/P2/P3 distances exceed identity corridor
    = phrase identity loss

P1/P2/P3 collapse to same fingerprint too often
    = fake P-level variation
```

Pass #1 defines the metric dimensions. Numerical acceptance thresholds belong to the first measured generator baseline, not this document.

---

# 14. Candidate table — first-pass summary

| Candidate | Domain | Evidence | Distinctness | Confidence | Runtime owner | Decision |
|---|---|---|---|---|---|---|
| Classic 2-Step structural grammar | Rhythm | PROJECT_OWNED_EXACT | already current 417 | high | `RhythmArchetype` | ACCEPT_BASELINE |
| Dark Skippy structural slice | Rhythm | PROJECT_OWNED_EXACT | overlaps current 418; distance pending | medium | `RhythmArchetype` | ACCEPT_BASELINE / REVIEW distance |
| Chicago four-floor Acid frame | Rhythm | PROJECT_OWNED_EXACT | already Acid/FourFloor coverage | high | `RhythmArchetype` | ACCEPT_BASELINE |
| Rolling Acid denser bass identity | Rhythm + Bass | PROJECT_OWNED_EXACT | rhythm baseline; bass behavior distinct | high evidence | split owners | REVIEW Bass only |
| Lo-Fi sparse/stable-backbeat topology | Rhythm | PROJECT_OWNED_EXACT + EDITORIAL_CURATED | not measured vs current 20 | medium | `RhythmArchetype` | REVIEW |
| Boom-Bap syncopated-kick/backbeat topology | Rhythm | PROJECT_OWNED_EXACT + EDITORIAL_CURATED | not measured vs current 20 | medium-high | `RhythmArchetype` | REVIEW |
| Jungle sampler-led break topology | Rhythm | PROJECT_OWNED_EXACT | potentially high but sampler-dependent | medium | `RhythmArchetype` | HOLD/REVIEW |
| Development preserve+add | Phrase | MEASURED | functionally distinct | high | `BarEvolution` | ACCEPT_TRANSFORM |
| Break/Ending remove+cue | Phrase | MEASURED | functionally distinct | high | `BarEvolution` | ACCEPT_TRANSFORM |
| BASE BASE DEVELOPMENT ENDING | Phrase trajectory | derived recommendation | 4-bar comparison pending | medium | `BarTrajectory` | REVIEW |
| UKG RespondKick | Bass rhythm | PROJECT_OWNED_EXACT + EDITORIAL_CURATED | plausible | medium-high | Bass relationship strategy | REVIEW |
| Acid Coincide+Offset | Bass rhythm | PROJECT_OWNED_EXACT | already conceptually covered | high | Bass relationship strategy | ACCEPT_BASELINE |
| AcidContour | Bass pitch | PROJECT_OWNED_EXACT + EDITORIAL_CURATED | normalized dedupe pending | medium-high | `BassPitchStrategy` | REVIEW |
| OctaveMotion | Bass pitch | PROJECT_OWNED_EXACT + EDITORIAL_CURATED | normalized dedupe pending | medium | `BassPitchStrategy` | REVIEW |
| Arch / Rise / Fall contours | Motif | mostly editorial/sparse | unknown | low | future `MotifVocabulary` | HOLD |
| RepeatWithEnding melodic operation | Motif | indirect phrase evidence only | unknown | low | future `MotifVocabulary` | HOLD |

---

# 15. Input contract for Atlas Pass #2 / Stage 7 candidate selection

The next extraction job should be computational rather than editorial and produce:

```text
A. every rights-safe/generalizable structural group -> nearest current archetype
B. role-mask distance and relationship signature
C. candidate clusters not covered by the current 20
D. per-family relationship support frequencies
E. 2/4-bar normalized delta classes
F. transposition-normalized Bass onset + contour fingerprints
G. transposition-normalized Motif fingerprints where evidence is sufficient
```

Output fields:

```text
candidate_id
domain
evidence_class
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

Only after that report exists should Stage 7 choose its first approximately 10–20 additions.

---

# 16. Acceptance checklist

```text
[x] No production code added.
[x] Six extraction passes remain independent.
[x] Full corpus and 12 published vertical slices are not conflated.
[x] Current 20 runtime archetypes are the novelty baseline.
[x] Existing UKG/Acid coverage is not duplicated under recipe labels.
[x] P1->P2 and P1->P3 aggregate signal is separate from 4-bar trajectory hypotheses.
[x] Bass rhythm and Bass pitch remain separate domains.
[x] Motif candidates are held when evidence is sparse/editorial.
[x] Rights-blocked data is limited to non-reversible aggregate evidence.
[x] Duplicate/expression-only differences cannot inflate rhythm vocabulary.
[x] `effective_variation_count` is central to Stage 7 acceptance.
[x] Gap map states where Atlas is strong and where new evidence/design is required.
[x] Stage 7 remains blocked pending quantitative novelty/distance extraction and Stage 6.1 convergence.
```
