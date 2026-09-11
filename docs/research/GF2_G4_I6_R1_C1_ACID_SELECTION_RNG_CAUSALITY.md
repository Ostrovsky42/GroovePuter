# G4-I6-R1-C1 — Acid Selection / RNG Causality

## AUTHORITATIVE STATE

Checkpoint: `G4-I6-R1-C1 — ACID SELECTION / RNG CAUSALITY`.

C1 started from remote branch HEAD:

```text
f411300cf6d011caddd5a1d793b39338eba01468
```

Historical committed I6 evidence used as the OLD projection comes from:

```text
47e09186c1ade6cca8ed79c567fca919399fb98e
```

The pre-document causal proof completed on:

```text
SHA      75375e722e4c3a40caf9ae942855130c16f2126a
workflow 34629835551
job      103363676104
```

The workflow token was read-only (`Contents: read`, `Metadata: read`). Production and committed I6 evidence remained frozen. `G4-I6 committed evidence freshness` intentionally remained RED because this checkpoint does not refresh I6 evidence.

## OBSERVED DELTA

Fresh I6 regeneration retained the existing global census invariants:

```text
profiles=33
archetypes=24
effective_edges=122
roots=4224
materializations=12672/12672
reachability=122/122
false_owner=0
admission_orphan=0
overbroad_owner=0
structural_collision=0
shared_admission=24
shared_valid_under_contracts=0
review_required=31
unknown=11904
proven_contracts=4
collision_pairs=0
```

Exactly 948 of 12672 materialization rows changed. All 948 are Acid rows and every changed row changes `bass_identity`.

```text
Acid / BASE          333
Acid / Chicago Jack  294
Acid / Rolling Acid  321
non-Acid               0
```

Field deltas:

```text
materialized_signature 948
bass_identity          948
chord_identity         681
melodic_identity       687
motif_identity         519
```

The exact observed combinations are:

```text
bass Δ | chord Δ | melodic Δ | motif Δ | rows
   Y   |    N    |     N     |    N    |  51
   Y   |    N    |     Y     |    N    |  39
   Y   |    N    |     Y     |    Y    | 177
   Y   |    Y    |     N     |    N    | 210
   Y   |    Y    |     Y     |    N    | 129
   Y   |    Y    |     Y     |    Y    | 342
```

No combination exists with unchanged bass and changed downstream identities.

## SELECTION ORDER

The authoritative preparation path resolves the upstream rhythm/archetype first and then `resolveGenerationComposition()` selects composition identities in this relevant order:

```text
1. upstream rhythm/archetype — already frozen for the composition call
2. feel
3. bass identity
4. chord identity
5. progression
6. melodic identity
7. motif identity
8. phrase law
```

Relevant selector map:

| FIELD | SELECTOR | RNG / SEED SOURCE | READS BASS? | SHARED STATE? |
| --- | --- | --- | --- | --- |
| bass | `selectWeightedIdentityFromView()` | `deriveGenerationSeed(context, archetype, BassRhythmSelection, baseSalt)` | n/a | NO |
| chord | `selectWeightedIdentityFromView()` | `deriveGenerationSeed(context, archetype, ChordRhythmSelection, baseSalt \| bass_id)` | YES | NO |
| melodic | `selectWeightedIdentityFromView()` | `deriveGenerationSeed(context, archetype, MelodicRhythmSelection, baseSalt \| (bass_id << 8) \| chord_id)` | YES | NO |
| motif | `selectWeightedIdentityFromView()` | `deriveGenerationSeed(context, archetype, MotifSelection, baseSalt \| melodic_id)` | indirectly, through melodic result | NO |

`baseSalt` is stable profile identity:

```text
(generativeMode << 24) | (recipe << 16)
```

Each selector owns its candidate view. No selector consumes a mutable PRNG cursor left by a previous selector.

## RNG MODEL

Verdict:

```text
STATELESS_DOMAIN
```

`deriveGenerationSeed()` derives a selector seed from stable inputs including project seed, upstream archetype, phrase ordinal, generation domain and semantic salt. `deterministicValue(seed, coordinate)` is stateless.

Hypotheses:

```text
H1 — SHARED RNG STREAM / CURSOR SHIFT: REJECTED
H2 — DOMAIN-SEPARATED RNG, BASS FEEDS DOWNSTREAM SEED: CONFIRMED
H3 — SEMANTIC ADMISSIBILITY CHANGE: NOT OBSERVED
H4 — OTHER: NOT NEEDED
```

There is no evidence that adding a bass candidate advances a common random stream. The downstream change occurs because the selected upstream musical identity is intentionally encoded into the downstream semantic salt.

This matches the repository composition RNG contract: independent responsibilities use independent domains, while downstream domains may derive from stable identifiers of upstream decisions that semantically affect them. A shared global stream or call-order coupling would violate that contract; this path does not use one.

## BASS SELECTION CAUSALITY

Before P1 the three Acid owners used the same four weighted bass candidates:

```text
KickLock         70
OffbeatPush      90
RollingDrive    110
SyncopatedHook   75
TOTAL           345
```

After P1 the first four identities and weights remain unchanged and Acid-local vocabulary adds:

```text
SustainAndDrop  100
TOTAL           445
```

For the same root, OLD and NEW bass selection receive the same generation context, archetype, domain and semantic salt. Therefore:

```text
bass_seed_old == bass_seed_new
bass_draw_old == bass_draw_new
```

for all 1152 Acid materializations.

The weighted coordinate is computed from that same draw against the candidate-space total:

```text
OLD: draw % 345
NEW: draw % 445
```

That remaps 948 rows. Of those, 234 select the newly admitted `SustainAndDrop`; the other 714 remap deterministically among the pre-existing bass identities. This is a direct consequence of the existing weighted-selector semantics when the legal BASS candidate space changes; it is not a later-domain RNG cursor effect.

The remaining 204 Acid materializations keep the same bass identity.

Classification:

```text
EXPECTED_P1_DELTA
```

## CHORD CAUSALITY

Chord is selected in its own `ChordRhythmSelection` domain. Its semantic salt explicitly contains the selected bass identity:

```text
baseSalt | bass_id
```

For all 948 roots whose bass identity changes, the chord seed therefore changes. There is no shared cursor and no candidate-list structural leakage into CHORD.

A changed seed does not guarantee a changed selected identity: weighted selection maps 267 of those new chord draws back to the same chord identity. The other 681 rows select a different chord identity.

```text
bass changed                    948
chord seed changed              948
chord identity changed          681
chord identity remained same    267
```

Classification:

```text
DERIVED_EXPECTED_DELTA
```

## MELODIC CAUSALITY

Melodic selection uses its own `MelodicRhythmSelection` domain and explicitly encodes both selected bass and chord identities:

```text
baseSalt | (bass_id << 8) | chord_id
```

Because bass changes in every one of the 948 affected roots, the melodic seed changes in all 948 even when chord happens to resolve to its old identity. Weighted mapping retains the old melodic identity in 261 rows and changes it in 687.

```text
bass changed                      948
melodic seed changed              948
melodic identity changed          687
melodic identity remained same    261
```

Classification:

```text
DERIVED_EXPECTED_DELTA
```

## MOTIF CAUSALITY

Motif selection uses its own `MotifSelection` domain. Its semantic salt contains the selected melodic identity:

```text
baseSalt | melodic_id
```

Therefore motif seed changes exactly when melodic identity changes:

```text
melodic identity changed               687
motif seed changed                      687
melodic identity unchanged              261
motif seed unchanged                    261
```

Of the 687 changed motif seeds, 519 map to a different motif identity and 168 map back to the same motif identity.

Classification:

```text
DERIVED_EXPECTED_DELTA
```

## COUNTERFACTUAL CONTROLS

### Same candidate-space change, same resulting bass value

There are 204 Acid materializations where the bass vocabulary changed but the selected bass identity did not.

For every one of them:

```text
chord seed unchanged
melodic seed unchanged
motif seed unchanged
chord identity unchanged
melodic identity unchanged
motif identity unchanged
```

Exact count:

```text
bass_same_downstream_changed=0
```

This is the critical rejection of H1. Merely changing the bass candidate array does not perturb downstream draws.

### Changed bass value with stateless downstream derivation

When bass identity changes, downstream domains are recomputed from the new semantic upstream value. Holding the old semantic downstream input would reproduce the old domain seed because `deriveGenerationSeed()` is a pure derivation from its explicit inputs; there is no mutable RNG state to carry across selectors.

Thus the causal variable is the selected bass VALUE, not the act of traversing a different candidate list.

### Representative deterministic roots

The committed projection contains one representative for every required observed class:

```text
CLASS A  BASE / identity=5  / P1 : bass, chord, melodic, motif change
CLASS B  BASE / identity=12 / P1 : bass + chord change; melodic/motif stay
CLASS C  BASE / identity=1  / P1 : bass + melodic + motif change; chord stays
CLASS D  BASE / identity=9  / P1 : bass only
```

For example, CLASS A keeps exactly the same bass seed and draw but changes its weighted coordinate because `345 -> 445`; after bass changes `9 -> 10`, CHORD receives a different semantic seed, then MELODIC receives the new bass/chord context, then MOTIF receives the new melodic context.

## MUSICIAN-DECISION TEST

The dependency passes the musician-decision test.

The implemented relationship is not:

```text
"a fifth bass candidate consumed an extra random number, so an unrelated motif changed"
```

There is no shared random-number cursor.

It is:

```text
"the selected bass idea changed, so dependent composition choices are deterministically
re-derived from that new upstream musical decision"
```

That is an explicit coherent-idea relationship already encoded by the composition model. CHORD and MELODIC depend directly on bass identity; MOTIF depends on the resulting melodic identity. This preserves domain-separated randomization while allowing a changed upstream musical idea to produce a coherent dependent idea.

Domain independence therefore means independent random domains and no call-order contamination, not semantic independence of all lanes.

## DELTA CLASSIFICATION

```text
BASS
cause:
  Acid-local weighted bass vocabulary changed from total weight 345 to 445;
  identical bass seed/draw maps through the new legal weighted space.
classification:
  EXPECTED_P1_DELTA

CHORD
cause:
  bass value is an explicit component of CHORD semantic salt;
  948 chord seeds change, 681 resolve to a different identity.
classification:
  DERIVED_EXPECTED_DELTA

MELODIC
cause:
  bass + chord values are explicit components of MELODIC semantic salt;
  all 948 affected roots get a new melodic seed, 687 resolve differently.
classification:
  DERIVED_EXPECTED_DELTA

MOTIF
cause:
  melodic value is an explicit component of MOTIF semantic salt;
  687 motif seeds change and 519 resolve differently.
classification:
  DERIVED_EXPECTED_DELTA
```

All 1887 downstream identity-field changes are covered:

```text
681 + 687 + 519 = 1887
unexplained=0
```

## UNEXPECTED DELTA

```text
unexpected=0
unexplained=0
non_acid_changed=0
bass_same_downstream_changed=0
```

No accidental cross-domain RNG coupling was found. No architecture defect was found in this checkpoint.

## I6 REFRESH VERDICT

`G4-I6-R1-C1` verdict:

```text
GREEN
```

The existing stale I6 census delta is fully classified:

```text
EXPECTED_P1_DELTA          = direct BASS identity changes
DERIVED_EXPECTED_DELTA     = CHORD / MELODIC / MOTIF identity changes
UNEXPECTED_DELTA           = 0
UNEXPLAINED                = 0
```

Therefore the I6 evidence is causally safe to refresh in a later `G4-I6-R1` evidence-refresh checkpoint.

This C1 checkpoint deliberately does **not** perform that refresh. The committed I6 census/anomalies evidence remains unchanged and freshness remains intentionally RED here.

Production changes in C1:

```text
NONE
```
