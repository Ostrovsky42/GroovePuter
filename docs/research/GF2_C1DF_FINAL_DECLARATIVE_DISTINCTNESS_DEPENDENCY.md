# GF2-C1DF — Final Declarative Distinctness Dependency / Loss Map

**Status:** research / characterization only. No production musical policy or renderer code is changed.

**C1DF exact base / frozen C1RF HEAD:** `574b830526c784ffe761286096dd62e22d6361d4`

**Underlying production base:** `v0.9.9` / `0a2a6211ef00dcf2214dfd4704b6c34b424b1c9d`

**Question answered:** which final-release C1F declarative differences have a statically connected production path, and which remain lost, partial, failure-sensitive, or policy-inconsistent?

**Question not answered:** whether any two profiles produce audibly or statistically distinct musical output.

Machine-readable evidence:

- [`GF2_C1DF_BASE_PAIR_DEPENDENCY.tsv`](./GF2_C1DF_BASE_PAIR_DEPENDENCY.tsv) — all 120 unordered BASE comparisons;
- [`GF2_C1DF_RECIPE_BASE_DEPENDENCY.tsv`](./GF2_C1DF_RECIPE_BASE_DEPENDENCY.tsv) — all 17 non-BASE recipes compared with their own BASE;
- `tools/gf2/generate_gf2_c1df_dependency_maps.py` — deterministic C1F × C1RF join;
- `tests/run_gf2_c1df_final_distinctness_dependency.sh` — input, coverage, classification, and byte-for-byte regeneration checks.

## 1. Exact base and methodology

C1DF is stacked only on C1F/C1RF research artifacts:

```text
0a2a6211  v0.9.9 production base
    |
d24ebf42  GF2-C1F final census
    |
574b8305  GF2-C1RF reachability / exact C1DF base
```

GF2-0 and GF2-0R are not in this ancestry. C1DF contains the tagged 0.9.9 release only through its exact C1F/C1RF research stack.

The transform compares the frozen C1 rows field by field:

- every weighted bag distinguishes exact equality, same support with different weights, and different support;
- corridor fields are separated into suggested BPM, BPM bounds, density, and grid;
- tonal policy is separated into bass contour, bass articulation, melodic rhythm operation, melodic contour, melodic motif operation, bass register, and secondary register;
- secondary role and canonical drum grammar remain explicit domains;
- legacy and Atlas trace deltas remain separate from the primary Gate-A vector.

Each primary difference is then mapped to an exact C1RF `(semantic_field, role)` record. Generation fails if a primary C1F domain has no C1RF mapping. `UNKNOWN` is never promoted to `CONNECTED`.

List syntax in both TSVs is deterministic: values are sorted and separated by `;`; `NONE` means an empty set. Bag differences carry `:MEMBERSHIP` or `:WEIGHTS`.

This is a structural join. It does not run the renderer, inspect final patterns, calculate a distance, or produce a corpus.

## 2. C1F + C1RF input contracts

Repository evidence confirms:

| Input | Frozen evidence |
|---|---:|
| Production Genre × Recipe profiles | 33 |
| BASE profiles | 16 |
| Non-BASE recipes | 17 |
| Rhythm archetypes | 24 |
| Unordered BASE pairs | 120 |
| C1RF traces | 39 across 13 domains |

C1RF status counts are reproduced unchanged:

| Status | Traces |
|---|---:|
| `CONNECTED` | 26 |
| `PARTIALLY_CONNECTED` | 2 |
| `BLOCKED` | 0 |
| `DROPPED` | 5 |
| `FAILURE_MASKED` | 1 |
| `LEGACY_FALLBACK` | 1 |
| `DUPLICATE_OWNER` | 1 |
| `AMBIGUOUS_OWNER` | 0 |
| `DECLARED_ONLY` | 3 |
| `UNKNOWN` | 0 |

The primary dependency mapping used here is:

| C1F difference | C1RF interpretation |
|---|---|
| rhythm compatibility and canonical drum grammar | `CONNECTED` |
| bass/chord/progression/melodic/motif bags | `CONNECTED` |
| secondary role | `CONNECTED` |
| tonal masks and register corridors | `CONNECTED` |
| profile FEEL bag | `DROPPED` |
| profile phrase law | `DROPPED` |
| profile phrase-length membership | `PARTIALLY_CONNECTED` through P1R admission/execution |
| corridor BPM bounds, density, grid | `DECLARED_ONLY` |
| corridor suggested BPM | catalog-wide `PARTIALLY_CONNECTED`; C1DF refines it to `CONNECTED` for non-Atlas comparisons and retains partial status for Atlas-backed comparisons |
| harmonic timing interpretation | `CONNECTED` under the final `HarmonicRhythm` owner |
| successful strong-migration publication | normal Genre generation is `FAILURE_MASKED` / `LEGACY_FALLBACK`; PHRASE P1R is fail-closed |

Synth-role depth is also dropped according to C1RF, but it is not a differing Genre × Recipe profile field in C1F. It therefore affects zero current C1DF comparisons while remaining a future intervention/reachability requirement.

## 3. Classification vocabulary

- **DECLARATIVE DISTINCTNESS** — at least one frozen C1 primary semantic field differs.
- **REACHABLE DECLARATIVE DISTINCTNESS** — at least one such difference maps to `CONNECTED`.
- **UNREACHABLE DECLARATIVE DISTINCTNESS** — a difference maps to `DROPPED`, `DECLARED_ONLY`, or `BLOCKED`.
- **UNRELIABLE DECLARATIVE DISTINCTNESS** — a difference/path is partial, failure-masked, fallback-dependent, or owner-unresolved.
- **REALIZED DISTINCTNESS** — deliberately not measured here.

Comparison tags are non-exclusive:

| Tag | Exact C1DF rule |
|---|---|
| `DECLARATIVE_COLLISION` | zero primary C1 differences |
| `REACHABLE_POLICY_DIFFERENCE` | one or more primary differences are `CONNECTED` |
| `MIXED_REACHABILITY` | connected differences coexist with unavailable, partial, or owner-unresolved differences |
| `REACHABILITY_DEPENDENT` | differences exist, but none are `CONNECTED` |
| `PARTIALLY_REACHABLE` | a differing field maps to `PARTIALLY_CONNECTED` |
| `FAILURE_SENSITIVE` | strong migration failure/fallback can invalidate attribution |
| `OWNER_UNRESOLVED` | a differing domain depends on an unresolved owner; no current comparison has this tag after final harmonic-owner recovery |
| `POLICY_INCONSISTENCY_AFFECTED` | the comparison includes Minimal Space tempo policy |

No aggregate reachability or Genre distance is calculated.

## 4. BASE genre dependency matrix

The complete matrix is in `GF2_C1DF_BASE_PAIR_DEPENDENCY.tsv`.

| Classification | BASE pairs |
|---|---:|
| `DECLARATIVE_COLLISION` | 0 |
| `REACHABLE_POLICY_DIFFERENCE` | 120 |
| `MIXED_REACHABILITY` | 120 |
| `REACHABILITY_DEPENDENT` | 0 |
| `FAILURE_SENSITIVE` | 120 |
| `OWNER_UNRESOLVED` | 0 |
| `PARTIALLY_REACHABLE` | 69 |

Every BASE pair differs in weighted rhythm compatibility and canonical drum grammar. Both domains have connected successful paths. Therefore no current BASE distinction depends entirely on dropped/unconsumed policy.

Every BASE pair also differs in corridor BPM bounds, which have no material consumer. This makes all 120 comparisons mixed rather than fully connected. Suggested BPM differs in 116 pairs; because neither side is Atlas-backed, those 116 differences refine to `CONNECTED`. Corridor density differs in 113 pairs and is declared-only. Profile phrase-length support differs in 69 pairs and is partially connected through final P1R.

One pair has no connected primary difference outside rhythm/drum:

- Breaks vs UK Garage.

This does not make them collisions. It means their non-rhythm distinctions currently add no trustworthy connected path.

### Generated House vs Techno example

The actual C1F × C1RF join is:

| Domain | Difference | Reachability |
|---|---|---|
| Rhythm compatibility | membership differs | `CONNECTED` |
| Canonical drum policy | differs | `CONNECTED` |
| Progression | membership differs | `CONNECTED`, including the final harmonic WHEN owner |
| Profile FEEL | membership differs | `DROPPED` |
| Corridor suggested BPM | 122 vs 134 | `CONNECTED` on both non-Atlas paths |
| Corridor BPM bounds | 112–128 vs 124–146 | `DECLARED_ONLY` |
| Corridor density | 5–13 vs 5–14 | `DECLARED_ONLY` |
| Tonal policy | shared `kStaticProfile` | no tonal difference |

Both also prohibit every bass contour except `ROOT ANCHOR` and every melodic contour except `STATIC`. Their declarative distinction is reachable through rhythm/drums/progression/tempo request and has no remaining harmonic-owner ambiguity. The comparison is still mixed and failure-sensitive because FEEL/corridor gaps and normal Genre publication masking remain.

## 5. Recipe-vs-BASE dependency matrix

| Classification | Recipes |
|---|---:|
| `DECLARATIVE_COLLISION` | 0 |
| `REACHABLE_POLICY_DIFFERENCE` | 17 |
| `MIXED_REACHABILITY` | 17 |
| `REACHABILITY_DEPENDENT` | 0 |
| `FAILURE_SENSITIVE` | 17 |
| `OWNER_UNRESOLVED` | 0 |
| `PARTIALLY_REACHABLE` | 9 |
| `POLICY_INCONSISTENCY_AFFECTED` | 1 |

All 17 recipes retain at least one connected difference: 16 change rhythm membership; Dub Techno keeps the same support but changes rhythm weights. All 17 consequently change the weighted canonical drum grammar.

Eight recipes have connected primary distinctions only in rhythm/drum:

- Acid / Chicago Jack, Rolling Acid;
- Rave / Psytrance;
- Dub/Reggae / Deep Chord, Minimal Space;
- Breaks / UK Garage, Classic 2-Step;
- Lo-Fi / Classic Chill.

Chicago Jack remains in this group even though its Atlas BPM happens to equal the profile suggestion: Atlas still replaces the profile-owned request, so the ownership path is partial rather than connected.

Four recipes add another connected role distinction:

| Recipe | Additional connected difference |
|---|---|
| Breaks / Drum&Bass | bass rhythm membership |
| Breaks / Dark Skippy | bass rhythm membership |
| Lo-Fi / Drunken Groove | bass rhythm membership |
| Lo-Fi / Lo-Fi House | bass, chord, and progression membership |

Unreachable/partial recipe deltas are:

- all 17: corridor BPM bounds and density;
- 13: corridor suggested BPM differs; 8 non-Atlas comparisons are connected and 5 Atlas-backed comparisons remain partial;
- 6: phrase/evolution membership is different but dropped; all six also change phrase-length support, which is partially connected through P1R;
- 5: profile FEEL differs (four membership, one weights-only) but is dropped.

There are **zero corridor-only recipes** in the frozen C1F model. No recipe is declaratively distinct only through a field without a material consumer.

## 6. Distinctions relying entirely on unreachable semantics

Current result:

| Comparison set | `REACHABILITY_DEPENDENT` |
|---|---:|
| BASE pairs | 0 / 120 |
| Recipe vs own BASE | 0 / 17 |

This is not proof that the existing distinctions survive realization. It proves only that every comparison has at least one static connected path, always including rhythm/drum.

The absence of a fully reachability-dependent comparison must not hide the large unreachable sub-vectors:

| Lost semantic difference | BASE pairs | Recipes |
|---|---:|---:|
| Corridor BPM bounds | 120 | 17 |
| Corridor density | 113 | 17 |
| Phrase/evolution | 94 | 6 |
| Profile FEEL | 88 | 5 |
| Corridor grid | 0 | 0 |

Profile phrase-length membership is not in the lost table: it is now partially connected for **69 BASE pairs** and **6 recipes**. The selected evolution law remains dropped even where its length is admissible and materialized.

Thus current profile tables contain many real Gate-A differences which cannot be used as Gate-B hypotheses until their intended consumer contract exists.

## 7. Mixed-reachability distinctions

All 137 comparisons are mixed. That result has three different causes and must not be reduced to a score:

1. **Connected plus declared-only:** every comparison combines connected rhythm/drum differences with unconsumed corridor bounds.
2. **Connected plus dropped:** many comparisons also differ in profile FEEL or phrase/evolution.
3. **Connected plus partial:** suggested BPM can be replaced in five Atlas-backed recipe comparisons, while profile phrase-length support differs on 69 BASE pairs and six recipes and is connected only through the P1R surface.

The one BASE pair and eight recipes listed above are the most dependent on one connected family: only their rhythm/drum sub-vector is statically connected. That is a future harness priority, not a recommendation to strengthen their tables.

C1F weights remain explicit. For example, Dub Techno is not a membership change in rhythm compatibility; it is a weights-only rhythm difference plus corridor/drum differences. C1DF preserves that rather than flattening both cases into “rhythm differs.”

## 8. Failure-sensitive comparisons

All 120 BASE pairs and all 17 recipe comparisons are marked `FAILURE_SENSITIVE` if a future Gate-B experiment uses the normal Genre full-generation surface.

Reason:

```text
connected semantic selection
  -> migrateStrongRhythmMaterial fails
  -> normal Genre preparePlayingCandidate ignores status
  -> PLAY or STOP commit proceeds
  -> Atlas / legacy / previously captured material survives
  -> corpus metadata may still attribute the row to the requested profile
```

This warning does not downgrade the successful-path role consumers from `CONNECTED`. It says that successful static reachability is insufficient to trust an observed corpus row unless migration and publication success are captured or failure is closed.

The final P1R PHRASE route is different: it checks selection, semantic probes, and every bar's preflight before persistent commit. C1DF therefore does **not** attribute the normal Genre fallback to that fail-closed surface. A future harness must record which production surface it exercises.

Fallback trace deltas are preserved separately in the TSV. They are not folded into the primary Gate-A fingerprint and are not treated as additional reliable distinctions.

## 9. Shared expressive restrictions

Every BASE pair shares these three single-option policy restrictions:

- bass articulation: `PLAIN` only;
- melodic rhythm operation: `PRESERVE` only;
- melodic motif operation: `NONE` only.

More severe shared restrictions:

| Restriction | BASE pairs |
|---|---|
| bass contour `ROOT ANCHOR` only | Rave/House, Rave/Techno, House/Techno |
| melodic contour `STATIC` only | Rave/Dub-Reggae, Rave/House, Rave/Techno, Dub-Reggae/House, Dub-Reggae/Techno, House/Techno |

Shared tonal payload groups are unchanged from C1F:

- `kSynthProfile`: Synthwave, Darksynth, Chip — 3 pairs;
- `kBrokenProfile`: Electro, Breaks, UK Garage, Drum&Bass — 6 pairs;
- `kStaticProfile`: Rave, House, Techno — 3 pairs;
- `kSlowProfile`: Trip-Hop, Hip-Hop, Funk/Soul, Lo-Fi — 6 pairs.

These are shared prohibitions, not declarative collisions. A pair can differ strongly in rhythm while sharing a severely collapsed tonal vocabulary.

## 10. Minimal Space inconsistency impact

The C1RF finding remains unchanged:

```text
Minimal Space Atlas BPM       = 116
GenerationCorridor            = 72–102
classification                = DECLARATIVE_POLICY_INCONSISTENCY
status                        = OPEN
```

Within the required C1DF comparison sets:

- BASE pairs affected: **0** — Minimal Space is not a BASE row;
- recipe comparisons affected: **1** — Dub/Reggae / Minimal Space vs Dub/Reggae BASE.

That recipe retains connected rhythm membership and drum-policy differences. Its tempo/corridor interpretation is nevertheless unreliable: the profile requests suggested 86, while Atlas replaces it with 116 and no arbitration contract explains the out-of-corridor result.

C1DF does not select a winning value and does not change Atlas or corridor data.

## 11. Harmonic-rhythm owner recovery impact

Final v0.9.9 separates the two contracts:

- `ChordRhythm` owns physical chord articulation;
- `HarmonicRhythm` owns progression-event WHEN;
- one-bar and P1R phrase materialization consume the accepted harmonic plan.

Consequently, no current C1DF comparison is tagged `OWNER_UNRESOLVED`:

| Comparison set | Owner-unresolved comparisons |
|---|---:|
| BASE pairs | 0 |
| Recipe vs own BASE | 0 |

This is static owner/reachability evidence, not proof that progression timing is audibly distinct. C1DF changes no F-08 policy or owner.

## 12. Final-release gap impact

This table is impact evidence, not an implementation schedule:

| Semantic gap | BASE pairs | Recipes | Current status | Static evidence | Proven 0.9.9 task |
|---|---:|---:|---|---|---|
| Strong migration publication/fallback | 120 | 17 | `FAILURE_MASKED` + `LEGACY_FALLBACK` | normal Genre `preparePlayingCandidate` discards the result; P1R is fail-closed | `UNKNOWN` |
| Corridor BPM bounds | 120 | 17 | `DECLARED_ONLY` | no clamp/arbitration material consumer | `UNKNOWN` |
| Corridor density | 113 | 17 | `DECLARED_ONLY` | no role/materializer consumer | `UNKNOWN` |
| Suggested BPM replacement | 0 | 5 | `PARTIALLY_CONNECTED` | non-Atlas comparisons consume the request; Atlas-backed comparisons replace it | `UNKNOWN` |
| Profile phrase law propagation | 94 | 6 | `DROPPED` | selected law has no production reader | `UNKNOWN` |
| Profile phrase-length support | 69 | 6 | `PARTIALLY_CONNECTED` | P1R admits and materializes supported lengths; normal Genre remains one bar | final P1R contract, exact task mapping not re-adjudicated here |
| Profile FEEL propagation | 88 | 5 | `DROPPED` | selected suggestion; Scene FEEL drives consumers | `UNKNOWN` |
| Corridor grid | 0 | 0 | `DECLARED_ONLY` | no material consumer; all rows currently 16 | `UNKNOWN` |
| Synth-role depth | 0 | 0 current catalog comparisons | `DROPPED` for bass/chord/melodic | level reaches drums only | `UNKNOWN` |

Harmonic WHEN is no longer listed as a gap: C1RF proves a `CONNECTED` final owner. Historical branch names alone are not evidence of any other ownership fix, so no remaining E/F task is assigned by intuition.

## 13. Future GF2-C2 eligibility requirements

Every comparison remains globally:

```text
GF2-C2 = BLOCKED
```

The historical dependency chain remains provenance for the frozen release:

```text
D3 -> E0 -> H1' -> E1 -> E1.5 -> E2 -> E3 -> E4 -> E5 -> E6
```

plus `F-13 final` and `F-08 final`.

This C1DF base is the exact completed `v0.9.9` release, so the audit does not pretend to re-adjudicate each historical task label independently. Starting Gate B still requires a dedicated C2-V0 harness validation, pair-specific reachability oracles, and a trustworthy failure/publication contract for whichever production surface the harness uses. Minimal Space additionally remains ineligible for a tempo/corridor hypothesis until its open declaration conflict has an explicit arbitration decision.

Pair-specific eligibility also requires that every domain used in the intended hypothesis is trustworthy. Examples:

- a FEEL hypothesis requires profile FEEL propagation, not merely Scene FEEL;
- a phrase-law hypothesis requires a production reader; a phrase-length hypothesis may use the final P1R path only after V0 proves it;
- a corridor hypothesis requires an owner and consumer for the exact corridor field;
- a chord/progression timing hypothesis must observe the now-connected `HarmonicRhythm` path;
- every normal Genre full-generation comparison requires observable successful migration/publication or fail-closed handling.

Before any pair comparison, future **GF2-C2-V0** must prove:

1. deterministic replay;
2. within-policy stochastic baseline;
3. intervention isolation;
4. semantic reachability for the domains actually distinguishing that pair.

C1DF strengthens item 4 by providing an explicit per-comparison domain vector. It does not implement the harness or calculate within/between-policy distances.

## 14. No-policy-change statement

**Semantic delta: NONE.**

C1DF adds only deterministic research tooling, generated dependency maps, validation, and this report. It changes no production file, profile, weight, corridor, tonal/drum policy, Genre, Recipe, persistence identity, Phrase behavior, FEEL behavior, depth behavior, or harmonic owner.

`GF2-C2`, `GF2-G1`, and `GF2-R2` are not started. Existing PHRASE UI/production behavior is not modified by C1DF.
