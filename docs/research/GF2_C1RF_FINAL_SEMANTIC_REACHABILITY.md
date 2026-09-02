# GF2-C1RF — Final-Release Semantic Reachability Audit

**Status:** research / characterization only. This audit does not change production musical policy and does not measure realized or audible distinctness.

**C1RF branch base:** `d24ebf42ba48c50d2057af055807dd2c1ec6f096`

**Underlying production base:** `v0.9.9` / `0a2a6211ef00dcf2214dfd4704b6c34b424b1c9d`

**C1F input:** 33 production Genre × Recipe rows and their normalized final-release fingerprints from `GF2_C1F_FINAL_STATIC_SEMANTIC_CENSUS.tsv`.

Machine-readable evidence is in [`GF2_C1RF_FINAL_SEMANTIC_REACHABILITY.tsv`](./GF2_C1RF_FINAL_SEMANTIC_REACHABILITY.tsv). Its 39 records use symbols and paths rather than line numbers or addresses. The table is a companion to C1F; it does not alter or duplicate the complete 33-row census.

## 1. Exact base

C1RF is stacked only on the completed final-release C1F research commit. It does not contain GF2-0 or GF2-0R. The relationship is:

```text
0a2a6211  v0.9.9 exact production base
    |
d24ebf42  GF2-C1F exact HEAD / C1RF frozen base
```

Unlike C1R, this audit reads the complete frozen 0.9.9 release directly. Historical branch names are not used as evidence. The old C1R result is retained only as a comparison point for identifying which reachability contracts changed.

## 2. Method

For every C1 semantic domain, the audit followed the normal Genre generation entry point and recorded these stages:

1. **DECLARED** — authoritative policy/data exists.
2. **RESOLVED** — an effective Genre × Recipe policy can be obtained.
3. **SELECTED** — production derives a concrete semantic identity.
4. **PROPAGATED** — a downstream carrier contains that decision.
5. **CONSUMED** — a role/materializer/executor reads it.
6. **MATERIALIZATION-CAPABLE** — the consumer contains a conversion into musical material.
7. **EXECUTION-CAPABLE** — no obvious later replacement/drop was found on the successful path.

The last two stages prove only static capability. They do not prove audible effect, effect size, or Genre distinctness. No pattern corpus was generated or compared.

The normal one-bar Genre trace used as one audit spine is:

```text
GenreSettings
  -> generationProfileFor / rhythmCompatibilityFor / tonalGenerationProfileFor
  -> resolveGenerationComposition
  -> StrongRhythmMigrationResult
  -> role-specific requests
  -> role realizers + tonal/feel materializers
  -> transactional candidate patterns
  -> quantized or immediate publication
```

The final PHRASE production trace is audited separately:

```text
explicit 1/2/4/8-bar request
  -> profile phrase-length admissibility + exact law selection
  -> PreparedPhraseExecution
  -> HarmonicRhythm projection + phrase-global progression source
  -> per-bar strong materialization preflight
  -> bounded persistent commit
```

The C1F weighted support, raw weights, support fingerprints, and weighted fingerprints remain the declarative-distance evidence. C1RF adds reachability status; it does not recalculate a realized distance or pretend that source-path equality is a musical metric.

## 3. Reachability status definitions

| Status | Meaning in this audit |
|---|---|
| `CONNECTED` | Owner → resolution → carrier → authoritative consumer is statically demonstrated on the successful path. |
| `PARTIALLY_CONNECTED` | Only some modes, surfaces, or roles consume the semantic data. |
| `BLOCKED` | A complete contract exists but a proven production prerequisite is not satisfied on the audited release. |
| `DROPPED` | A decision is selected/received but not copied into the relevant downstream request. |
| `FAILURE_MASKED` | A failure result is ignored and processing/publication can continue. |
| `LEGACY_FALLBACK` | Old, Atlas, or pre-migration material can remain authoritative on a path. |
| `DUPLICATE_OWNER` | Two owners independently provide one semantic decision. |
| `AMBIGUOUS_OWNER` | The code does not establish one authoritative owner. |
| `DECLARED_ONLY` | Policy exists, but no production material consumer was found. |
| `UNKNOWN` | Static evidence is insufficient. |
| `NOT_APPLICABLE` | No such semantic axis is declared for the role. |

No row is labeled `BLOCKED` merely from a task name. Because the relevant later branch objects were not locally available, uncertain task mappings remain `UNKNOWN` rather than inferred.

## 4. Domain matrix

| Domain | Static result | Terminal production effect / gap |
|---|---|---|
| Rhythm compatibility | `CONNECTED` for drums, bass, chord, melodic | Selects an archetype; its family, protected spaces and role grammar reach each role request. |
| Profile FEEL bag | `DROPPED` | `suggestedFeel` is selected and copied to diagnostics, but materialization reads Scene FEEL instead. |
| Scene FEEL | `CONNECTED` for all four roles | Drum materializer and semantic synth adapters read timing profile and amount. |
| Bass rhythm bag | `CONNECTED` | Concrete ID reaches `BassRhythmRequest` and produces bass topology. |
| Chord rhythm bag | `CONNECTED` | Concrete ID reaches `ChordRhythmRequest` and produces chord topology. |
| Progression bag | `CONNECTED` | Concrete ID reaches the progression plan used by tonal materialization. |
| Melodic rhythm bag | `CONNECTED` | Concrete ID reaches melodic onset/continuation realization. |
| Motif bag | `CONNECTED` | Shape reaches motif source order and secondary pattern projection. |
| Profile phrase law | `DROPPED` | The selected law is retained in composition/migration results but final phrase execution never reads it. |
| Profile phrase bars | `PARTIALLY_CONNECTED` | Final P1R uses the profile as length admissibility and materializes the accepted count; normal Genre generation remains one bar. |
| Explicit PHRASE length | `CONNECTED` | The 1/2/4/8-bar request is profile-filtered, preflighted, and committed as bounded Song material. |
| Secondary role | `CONNECTED` | Selects Synth B chord, melodic, or chord-priority hybrid admission. |
| Corridor suggested BPM | `PARTIALLY_CONNECTED` | Used as an APPLY TEMPO request for non-Atlas paths; Atlas metadata replaces it for Atlas-backed recipes. |
| Corridor BPM bounds | `DECLARED_ONLY` | Validated and displayed; no runtime clamp/arbitration consumer exists. |
| Corridor density/grid | `DECLARED_ONLY` | Validated/displayed, but no production role/materializer reads them. |
| Tonal policy | `CONNECTED` for bass and secondary | Masks select operations/contours; register corridors constrain absolute pitch projection. |
| Drum policy | `CONNECTED` on success | Weighted archetype grammar is realized and physically materialized. |
| Harmonic rhythm | `CONNECTED` | Final `HarmonicRhythm` owns WHEN independently of chord articulation and feeds both one-bar and phrase materialization. |
| Depth/mutation | drums `CONNECTED`; bass/chord/melodic `DROPPED` | `RealizationLevel` reaches rhythm mutation budgets, but is absent from all three synth-role request families. |
| Failure propagation | normal Genre `FAILURE_MASKED` + `LEGACY_FALLBACK`; PHRASE P1R `CONNECTED` | The main one-bar preparer still discards migration status, while final phrase preparation rejects failed strong materialization before commit. |

Record counts in the companion table:

| Status | Records |
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
| `NOT_APPLICABLE` | 0 |

The table can still use `UNKNOWN` in the blocker column while the axis status is known. That means the final-release gap is proven but no future production task has been assigned by evidence.

## 5. Role-specific production paths

### Drums

```text
rhythmCompatibilityFor
  -> resolveRhythmSelection
  -> RhythmRealizationRequest(archetypeId, level, phraseBars=1)
  -> realizeRhythmPhrase
  -> applyFeelToMaterializedPattern(Scene FEEL)
  -> materializeRhythmPattern
  -> DrumPatternSet
```

Drums receive rhythm identity, archetype mutation budget/`RealizationLevel`, cross-role grammar, and Scene FEEL. In normal Genre generation they remain one bar. In final PHRASE P1R, the explicit length is filtered by the profile and each accepted bar is materialized through the same strong owner. Drums still do not receive profile `suggestedFeel` or corridor density/grid.

### Bass

```text
profile bass bag + selected rhythm archetype
  -> BassRhythmRequest
  -> realizeBassRhythm
  -> BassPitchBehaviorRequest(tonal bass policy)
  -> realizeBassPitchBehavior
  -> materializeRole(bass register + progression)
  -> applyFeelToSemanticPattern(Scene FEEL)
  -> Synth A
```

The bass contour and articulation masks are real selection prohibitions, and the bass register corridor reaches pitch projection. `RealizationLevel` is not present in `BassRhythmRequest` or `BassPitchBehaviorRequest`.

### Chord

```text
profile chord + progression bags
  -> ChordRhythmRequest / HarmonicRhythmRequest / ChordProgressionRequest
  -> chord topology + independent harmonic clock + progression plan
  -> materializeRole(secondary register, harmonicEventOnsets)
  -> applyFeelToSemanticPattern(Scene FEEL)
  -> Synth B chord material
```

`HarmonicRhythm` now owns progression-event timing independently of the physical chord-onset mask. The one-bar path realizes it directly; PHRASE P1R projects the same accepted owner once per semantic bar and pairs it with one phrase-global progression source.

### Melodic / secondary

```text
profile melodic + motif bags
  -> MelodicMotifRequest
  -> melodic topology + source order
  -> MelodicPitchIntentRequest(tonal melodic policy)
  -> contour/rhythm/motif operation
  -> materializeRole(secondary register + progression)
  -> Synth B melodic or chord-gap fill
```

`secondaryRole` controls whether this path is primary, absent, or admitted only into chord-free steps. `RealizationLevel` is not propagated into the melodic requests.

## 6. Declared-but-unreachable semantics

Three separate cases must not be collapsed into one diagnosis:

1. **Selected then dropped:** profile `suggestedFeel`, profile phrase law, and synth-role depth are concrete selected/request semantics that do not enter the relevant downstream carrier.
2. **Partially connected:** profile phrase-bar vocabulary constrains and supplies the final PHRASE path, while normal Genre generation remains one bar.
3. **Declared only:** corridor BPM bounds, density, and grid are valid profile data but have no production material consumer. UI display is not musical consumption.
4. **Connected but policy-collapsed:** a consumer exists, but the allowed set itself contains one option.

The third case is negative capacity, not a renderer defect:

- all 33 rows: bass articulation allows `PLAIN` only;
- all 33 rows: melodic rhythm operation allows `PRESERVE` only;
- all 33 rows: melodic motif operation allows `NONE` only;
- the four `kStaticProfile` rows: bass contour `ROOT ANCHOR` only and melodic contour `STATIC` only;
- `kStaticProfile` plus `kDubProfile`, eight rows total: melodic contour `STATIC` only;
- Reggae / Deep Chord: the composition rhythm bag contains only archetype 412.

Those axes are **CONNECTED BUT EXPRESSIVELY COLLAPSED BY POLICY**. A future C2 result showing low diversity there must not be mislabeled renderer collapse.

## 7. Partial and failure-masked paths

### Profile FEEL versus Scene FEEL

`resolveGenerationComposition` selects `suggestedFeel` from the profile weighted bag. `migrateStrongRhythmDrums` copies it to `StrongRhythmMigrationResult`, but every actual feel adapter receives `StrongRhythmMigrationContext.feelProfile`, built from `scene.feel.timingProfile`. No bridge from the selected profile feel to that context exists.

This is a proven `DROPPED` declarative distinction. It is not labeled duplicate ownership because “suggested profile feel” and the explicit Scene FEEL selector may intentionally be different concepts; the missing arbitration contract is the open issue.

### Phrase length

Final P1R changes the old finding. `resolveGenerationCompositionForPhraseBars(...)` derives admissible lengths from the Genre × Recipe phrase bag, rejects unsupported requests, selects a law with the exact requested length, and carries `effectivePhraseBars` through preparation, harmonic projection, preflight, and persistent Song commit. That makes profile phrase length `PARTIALLY_CONNECTED`: connected for PHRASE, not a global default for normal one-bar Genre generation.

The selected `phraseLaw` itself remains `DROPPED`. No final production consumer reads `GenerationCompositionResult.phraseLaw`; rhythm-archetype trajectory/evolution policy remains a separate owner.

### Depth

`RealizationLevel` reaches `RhythmRealizationRequest` and selects the archetype mutation budget for drums. Bass, chord/progression, melodic/motif, and pitch-intent request types have no level carrier. This is role-specific partial reachability, not evidence that P-level affects the full arrangement.

### Outer publication contract

`migrateStrongRhythmMaterial` itself is transactional: it builds local `nextDrums`, `nextSynthA`, and `nextSynthB`, returns before assignment on failure, and assigns destinations only after every role/materialization/feel stage succeeds.

Final handling is surface-specific. `GeneratedPhraseP1R::prepare` checks selection, semantic probes, and every bar's preflight before commit. Normal Genre `preparePlayingCandidate(...)` still discards the result and returns only target-liveness, so the one-bar publication path remains failure-masked.

## 8. Duplicate or ambiguous ownership

One duplicate-owner finding remains and is recorded without repair:

- **Duplicate tempo declarations:** the profile corridor and Atlas recipe metadata both provide an apply-time BPM for Atlas-backed recipes. Minimal Space proves they can disagree.

The old harmonic-rhythm ambiguity is resolved on the final release: `HarmonicRhythm` explicitly owns WHEN, and `ChordRhythm` remains physical articulation. This is a structural ownership finding only; C1RF makes no claim about audible effectiveness.

The profile FEEL/Scene FEEL split is not promoted to `DUPLICATE_OWNER` without evidence that both fields claim the same semantic contract. It remains a dropped suggestion plus a connected explicit owner.

## 9. Minimal Space tempo inconsistency

**Finding:** `DECLARATIVE_POLICY_INCONSISTENCY`

**Status:** `OPEN`; blocks trustworthy C2 interpretation for Minimal Space; does not block C1F or C1RF.

| Question | Evidence-based answer |
|---|---|
| Atlas BPM owner | `AtlasGenerated::Recipe.bpm` for `REC_DUB_MINIMAL_SPACE`, exposed by `AtlasRuntime::applyRecipe` as `AtlasRuntimeMetadata.bpm = 116`. |
| Corridor owner | Reggae recipe 11 `ProfileDefinition.corridor = {72, 102, 86, 16, 1, 7}`, exposed by `generationProfileFor`. |
| Corridor use | Genre Apply Tempo initially passes suggested 86 as `requestedBpm`; min/max/density/grid are display/validation data only. |
| Atlas use | The shared full-generation preparer replaces the requested BPM with Atlas 116; final STOP and PLAY publication both consume that prepared value when tempo application is enabled. |
| Semantic nature of 116 | Static Atlas identity metadata **and** a runtime tempo request on the full-generation Apply Tempo path. It is not merely an audition label. |
| Can both values coexist? | Yes. GenrePage creates a request from 86, then Atlas replaces it with 116 before/while the candidate is committed. |
| Arbitration | Only an implementation-order override (“Atlas wins”) was found; no owner/arbitration/normalization contract explains why 116 may violate 72–102. |

Exact classification in the table is `DUPLICATE_OWNER`; the musical-policy finding is `DECLARATIVE_POLICY_INCONSISTENCY`. C1RF does not choose whether Atlas, corridor, or the recipe mapping is wrong and changes none of them.

## 10. `migrateStrongRhythmMaterial` failure propagation

**Finding:** `FAILURE_MASKED` with a `LEGACY_FALLBACK` outcome.

The return contract exposes `Applied`, `InvalidContext`, `AttemptUnavailable`, `RealizationFailed`, `MaterializationFailed`, `CompatibilityBindingFailed`, and `FeelApplyFailed` outcomes. The material state is:

| Point | State |
|---|---|
| Before call | The normal Genre candidate contains captured Scene material, then Atlas or legacy generation material. PHRASE P1R uses a fresh deterministic pitch source per preflight/commit bar. |
| After success | Local strong drums/synths replace all requested destinations atomically. |
| After failure | Destination remains unchanged because the migration returns before final assignments. |
| Subsequent action | Normal Genre generation can still commit the unchanged candidate. PHRASE P1R rejects preparation/materialization instead. |

Confirmed final-release masking surface for the current 33-profile catalog:

- `QuantizedGenerationDetail::preparePlayingCandidate` — discards migration status and returns only target-liveness. The final Undo-owned public path uses this shared preparer before both immediate STOP commit and PLAY commit/activation.

The final PHRASE P1R path is explicitly fail-closed: selection, semantic probe, and per-bar preflight statuses are checked before persistent commit. `GeneratedPhraseSong::applyCurrentMigration` still discards status only in the legacy-route helper; all 33 current C1F profile identities resolve a non-`Legacy` strong route and therefore use P1R.

The exact future-C2 risk is:

```text
new semantic selection
  -> strong migration failure
  -> failure result ignored
  -> Atlas / legacy / previously captured material survives
  -> corpus row is attributed to the new selection
```

That can create false similarity or false attribution if C2 uses the normal Genre generation surface without an independent success oracle. It blocks that harness path until the owner fails closed or corpus metadata records migration success. A future C2 harness may instead prove and use the fail-closed P1R path, but C1RF does not choose the harness. No production fix is made here.

## 11. Change from C1R to final 0.9.9

The historical renderer dependency contract was:

```text
D3 -> E0 -> H1' -> E1 -> E1.5 -> E2 -> E3 -> E4 -> E5 -> E6
```

plus `F-13 final` and `F-08 final`.

The final `v0.9.9` code now provides direct evidence, so branch-name inference is no longer needed.

Confirmed structural improvements since C1R:

- profile phrase length: `DROPPED` → `PARTIALLY_CONNECTED`;
- explicit bounded PHRASE length: `CONNECTED` through profile admission, preparation, preflight, and commit;
- harmonic rhythm: `AMBIGUOUS_OWNER` → `CONNECTED` under the recovered `HarmonicRhythm` WHEN owner;
- PHRASE failure propagation: fail-closed for P1R-capable routes.

Confirmed unchanged gaps:

- profile FEEL remains `DROPPED`;
- profile phrase law remains `DROPPED`;
- corridor bounds/density/grid remain `DECLARED_ONLY`;
- synth-role depth remains `DROPPED`;
- normal Genre migration status remains `FAILURE_MASKED` with a possible `LEGACY_FALLBACK` result;
- Minimal Space retains duplicate, inconsistent tempo declarations.

C1RF does not assign new E/F task ownership from names. Any future fix receives a task only after its production owner is proven.

## 12. Consequences for future GF2-C2

GF2-C2 must not begin with Genre comparisons. It first requires **GF2-C2-V0 — HARNESS VALIDATION**:

1. **Deterministic replay:** same effective policy + context + seed produces identical normalized semantic/material output.
2. **Within-policy stochastic baseline:** same policy with different seeds measures ordinary stochastic divergence.
3. **Intervention isolation:** A/B metadata proves that only the investigated owner changed and every other input remained fixed.
4. **Semantic reachability:** the altered declarative policy reaches the intended production consumer, with migration/publication success recorded.

Only then may C2 compare:

```text
WITHIN-POLICY STOCHASTIC DISTANCE
versus
BETWEEN-POLICY CAUSAL DISTANCE
```

If ordinary BASE seed variation is larger than the BASE-to-candidate difference, the candidate has not demonstrated additional expressive capacity.

The existing `tests/data/stage15_tonal_enabled_f13_baseline.tsv.gz.b64` and its deterministic dump path (`tests/run_stage15_tonal_baseline_dump.sh`, `.github/workflows/stage15-tonal-baseline.yml`) exist and may inform future harness reuse. C1RF does not read their musical rows as capacity evidence and does not run a C2 corpus.

The frozen falsification rules are unchanged:

- **F1 — SYSTEMIC COLLAPSE:** Gate A strong + Gate B weak after final-release reachability and V0 validation means GF2-R2 is blocked; return to realization/execution owners.
- **F2 — CATALOG SATURATION:** candidates inside ordinary existing-catalog stochastic variation do not justify expansion.

## 13. No-policy-change statement

**Semantic delta: NONE.**

C1RF adds only this audit, its companion table, and deterministic validation. It changes no `src/` file, Genre, Recipe, profile weight, corridor, tonal policy, drum policy, runtime owner, persistence identity, or UI.

It does not start GF2-C2, GF2-G1, GF2-R2, or modify the existing PHRASE UI.
