# Atlas Pass 2 Computational Extraction Report

**Status:** adversarially hardened computational evidence gate before Stage 7  
**Atlas input:** schema 2.6.0, SHA-256 `5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd`  
**Production impact:** none. Pass 2 remains offline/audit-only.

The original Pass 2 candidate `4d41980b...` passed its first internal review, but an independent adversarial methodology audit found additional requirements. That finding invalidated the previous `3/3` certification and reset the review counter. The hardened pass below is the new normative basis.

## 1. Input and corpus reconciliation

The supplied archive is the exact pinned Atlas v2.6 corpus:

```text
patterns                  413
events                   9377
validation failures         0

302 x 1-bar
 93 x 2-bar
 17 x 4-bar
  1 x 5-bar derived composite
------------------------------
413 patterns
```

The 5-bar outlier remains in corpus accounting and is excluded from Core-v1 1-4-bar phrase admission. It is never silently truncated.

Canonical hardened extraction command:

```bash
python3 tools/atlas/run_atlas_pass2_hardened.py \
  seqtrak_pattern_atlas_csv_v2_6.zip \
  docs/architecture/atlas_pass2/RUNTIME_RHYTHM_BASELINE.tsv \
  <output-directory>
```

The raw archive and private per-pattern detail are not committed.

## 2. Versioned semantic role normalization

Normative mapping schema:

```text
ATLAS_ROLE_MAPPING_V2
```

`track_role` is not accepted as the sole source of drum semantics. Exact track identity takes precedence:

```text
KICK                         -> Kick
SNARE                        -> Backbeat
CLOSED_HAT / HAT1            -> ClosedHat
OPEN_HAT / HAT2              -> OpenHat
PERC1/PERC2/RIM/TOM/COWBELL… -> Percussion
```

For research observations, `CLAP/HAND_CLAP` is conservatively kept as Percussion unless source metadata explicitly establishes backbeat ownership. Ambiguous `CYMBAL`, `RIDE`, `MID_HAT`, `HAT_FOOT` and unknown drum-like tracks remain `UNMAPPED`; they are not guessed into a lane.

On the hardened one-bar research corpus this leaves 664 event occurrences ambiguous/unmapped. Candidate rows publish that uncertainty instead of hiding it.

## 3. Source topology vs normalized musical topology

Atlas `structural_group_id` remains a source-lineage/deduplication key. It is **not** treated as the Stage 7 musical identity.

Hardened one-bar research scope:

```text
263 SOURCE_OBSERVATION patterns
239 source structural groups
```

After source deduplication, observations are re-normalized through the semantic role mapping before musical clustering.

## 4. Observation -> runtime grammar semantics

Runtime topology schema:

```text
GROOVEPUTER_RUNTIME_RHYTHM_TOPOLOGY_V2
```

The V2 runtime dump is generated directly from the current 20 `ReferenceVocabulary` archetypes and includes:

```text
lane required anchors
preferred/optional legal support
forbidden coordinates
lane density bounds
protected spaces
hard/soft relationships
```

Observation-to-grammar comparison is asymmetric. A concrete Atlas observation is never compared to `canonical|preferred|optional` as though that union were one concrete rhythm.

### Hard feasibility

For the drum domain an observation is `hard_covered` only when all of these are zero:

```text
required-anchor misses
outside-legal hits
protected-space hits
lane-density deviation
hard-relationship violations
```

Hard `Exclude`, `Coincide`, `Offset` and `Respond` semantics follow the runtime resolver. Cross-domain Bass/Chord/Melodic relationships are not used to reject a drum-only evidence observation.

### Typicality

Only after feasibility, support Jaccard is reported as a separate typicality diagnostic. Typicality cannot compensate for an illegal observation.

The reported minimum lane edit value is explicitly a **lower bound**: repairing a hard relationship can require additional edits.

## 5. Independent evidence support

Support schema:

```text
ATLAS_PASS2_SUPPORT_V2
```

Candidate support is not reduced to one `N`. Each recurring cluster reports separately:

```text
structural_group_count
independent_provenance_root_count
content_deduped_artifact_count
direction_count
hard-covered / hard-uncovered members
cluster dispersion
ambiguous/unmapped event count
```

Derived composites add zero independent provenance confirmations. Artifact support is deduplicated by content hash, so duplicate artifact IDs cannot inflate support.

A single provenance root can never become `AUDITION_REVIEW` merely by producing many rows.

## 6. Hardened topology result

After source-aware normalization, the research corpus contains nine recurring Kick/Backbeat clusters with at least three source structural groups:

```text
AUDITION_REVIEW   2
HOLD_SINGLE_ROOT  7
NOVEL_CANDIDATE   0
ACCEPT            0
```

The two multi-provenance audition candidates are:

```text
HARD_02  5 groups / 2 provenance roots / 2 content-distinct artifacts
         directions: Electro / Go-Go / Hip-Hop / Miami Bass

HARD_05  4 groups / 2 provenance roots / 2 content-distinct artifacts
         directions: Afro-Cuban / Bossa Nova
```

`HARD_05` carries substantial ambiguous/unmapped percussion/cymbal evidence and is therefore intentionally a **mapping-sensitive listening candidate**, not a production admission.

The remaining seven recurring clusters are single-root evidence and remain HOLD even where their internal topology is compact.

No Pass 2 output is a runtime archetype.

## 7. Calibration and null corridors

Distance schema:

```text
ATLAS_PASS2_DISTANCE_V2
```

Committed Atlas calibration includes:

```text
same-source-structural-group null:
  29 comparable pairs
  normalized drum distance max = 0.0

within-direction research:
  5645 pairs
  p50 = 0.494737
  p90 = 0.715789
  p95 = 0.789474

recurring-candidate internal:
  66 pairs
  p50 = 0.225877
  p90 = 0.325110
  max = 0.368421
```

The zero null corridor confirms that expression/metadata-only variants do not manufacture rhythm novelty under the hardened normalization.

A separate host calibration uses the actual `RhythmPhraseRealizer` over all 20 current grammars, 64 deterministic P1 seeds each:

```text
current-realizer -> own grammar coverage: 20 x 64 / 64
within-archetype P1 drum-distance distributions
current-realizer -> non-own grammar confusion
```

Aggregate current-realizer calibration on the first hardened run:

```text
within-own-archetype pairs: 40320
p50  0.052632
p90  0.219298
p95  0.289474
max  0.429825

non-own grammar coverage:
335 / 24320 = 0.013775
```

The overlap is sparse rather than zero. Examples include `two_step_roll <-> ghosted_roll` and `sparse_skank <-> chord_response`, plus a smaller straight/hypnotic/rolling corridor. This is useful empirical calibration for Stage 7 and replaces an arbitrary novelty threshold.

No single weighted novelty score is used.

## 8. Negative / protected-space evidence

Negative-space evidence remains a separate pass. The denominator includes only source structural groups in which the role is active somewhere, so a completely absent role cannot manufacture protected-space evidence.

Current research gate:

```text
active structural groups >= 5
step absence fraction    >= 0.90
```

The Pass 2 aggregate contains 176 direction×role×step negative-space observations and zero promoted BassRhythm research rows.

Repeated absence is evidence for curation, not an automatic runtime `ProtectedSpace` contract.

## 9. Phrase development

Measured source observations remain separate from derived/editorial sequences.

Measured adjacent-bar source transitions:

```text
19 two-bar SOURCE_OBSERVATION patterns
EXACT_REPEAT   4
ADD_ONLY       3
DROP_ONLY      2
MIXED         10
```

The measured pass explicitly handles the Atlas `PERCUSSION` role used by PERC1/PERC2.

Derived four-bar composites remain `EDITORIAL_CURATED`; the common derived sequence `EXACT_REPEAT > ADD_ONLY > MIXED` does not become proof of a universal `A A A' B` trajectory.

The single 5-bar derived composite is retained for corpus accounting only and is outside Core-v1 phrase admission.

## 10. Bass, pitch, motif and Feel evidence boundary

The hardened pass preserves provenance instead of escalating it because arithmetic was performed.

```text
BassRhythm one-bar evidence  -> PROJECT_OWNED_EXACT
Bass pitch contours           -> PROJECT_OWNED_EXACT / HOLD
Motif contours                -> PROJECT_OWNED_EXACT / HOLD
editorial microtiming         -> not MEASURED Feel law
```

Pass 2 does not claim independent corpus support for Stage 8 BassPitchStrategy, Stage 9 MotifVocabulary or a generalized Feel law.

## 11. Rights / reversibility boundary

Detailed per-pattern/per-source calculations are local/private only.

Repo-safe Pass 2 hardening outputs contain aggregate/synthetic candidate IDs, counts and distributions. They reject sensitive field names and do not publish:

```text
restricted pattern IDs
source structural-group IDs
source locators
artifact IDs or content hashes
literal masks/event lists
per-pattern normalized fingerprints
per-pattern distance vectors
```

Committed aggregate outputs are pinned by SHA-256 manifests.

## 12. Stage 7 boundary

Production Stage 7 admission remains **CLOSED** at Pass 2.

```text
AUDITION_REVIEW != NOVEL_CANDIDATE
NOVEL_CANDIDATE != ACCEPT
```

`ACCEPT` belongs to Stage 7 curation, generated-runtime effective-variation testing and listening.

A temporary Stage 7A listening harness may contain up to five generalized candidate grammars without changing production `ReferenceVocabulary`:

```text
2 x multi-provenance AUDITION_REVIEW
2 x single-root boundary challengers
1 x single-root control/outlier
```

The purpose is falsification by metrics and listening, not to guarantee five additions.

## 13. Review rule

The independent adversarial findings invalidated the original Pass 2 review sequence. The review counter is reset and must reach three consecutive clean reviews on one unchanged hardened SHA before this evidence gate is frozen.
