# GF2-C1F — final-release static semantic census (Gate A)

**Status:** research / characterization only. No production musical policy is changed by this branch.

**Exact production base:** `v0.9.9` / `0a2a6211ef00dcf2214dfd4704b6c34b424b1c9d`

**Historical comparison base:** `ba7f6944d9fec49ca0903f8df63b9d05108ee03d`

**Production profile rows:** `33` (`16` BASE + `17` non-BASE)

**Question answered:** what musical policy is declared by the frozen 0.9.9 release, not what the renderer audibly realizes.

## Result in one paragraph

The final 0.9.9 release declares 33 unique primary static fingerprints. Among the 16 BASE genres there are **0 exact declarative collisions**, **19 partial-collision pairs**, and **101 strongly distinct static pairs** under the dimensions measured here. All 17 recipes differ from their genre BASE in multiple measured domains; 16 change candidate membership, while Reggae / Dub Techno preserves rhythm membership but changes rhythm weights, corridor, and the canonical drum projection. These are Gate A statements only. They make no claim that production realization preserves the declared distinctions.

The regenerated profile, archetype, and BASE-pair payloads are byte-identical to the original GF2-C1 artifacts after substituting only the recorded base SHA. The 0.9.9 renderer line changed production reachability and execution, but did not change the declarative Genre × Recipe catalog measured by C1.

Machine-readable evidence:

- [`GF2_C1F_FINAL_STATIC_SEMANTIC_CENSUS.tsv`](./GF2_C1F_FINAL_STATIC_SEMANTIC_CENSUS.tsv) — all 33 rows, exact memberships, integer weights, corridor fields, role, tonal payload, drum projections, legacy/runtime traces, and full SHA-256 fingerprints;
- [`GF2_C1F_RHYTHM_ARCHETYPE_PAYLOADS.tsv`](./GF2_C1F_RHYTHM_ARCHETYPE_PAYLOADS.tsv) — normalized semantic and drum projections for all 24 referenced rhythm archetypes;
- [`GF2_C1F_BASE_GENRE_PAIRS.tsv`](./GF2_C1F_BASE_GENRE_PAIRS.tsv) — all 120 unordered BASE pairs and their dimension-by-dimension equality flags.

Regenerate them with `bash tests/run_gf2_c1f_final_static_semantic_census.sh`. The extractor reads production APIs and tables from the final release. It does not sample generated output.

## Owners and scope

| Concern | Exact-base owner used by the census | Treatment |
|---|---|---|
| Profile identity and non-rhythm bags | private `ProfileDefinition` / `kProfiles` in `src/generation/composition/generation_profile.cpp` | authoritative profile rows |
| Rhythm compatibility and weights | `rhythmCompatibilityFor(...)` in `src/generation/composition/rhythm_selection.cpp` | joined to the matching profile row, because `generationProfileFor(...)` does the same |
| Tonal policy | `tonalGenerationProfileFor(...)` and six payloads in `src/generation/composition/tonal_profile.cpp` | source identity and normalized effective payload are both retained |
| Production full-generation drum grammar | weighted rhythm compatibility → `ReferenceVocabulary` → strong-rhythm realizer → `PatternMaterializer` | normalized drum-role projection; all 33 valid rows select a non-`Legacy` strong route |
| Legacy/upstream generation | `GenreCatalog::compiledGenerativeParams`, `behavior`, `drumTemplateOverride`, and `kDrumTemplates` | separate traceability projection; not substituted for the canonical strong-rhythm policy |
| Atlas metadata | six generated Atlas recipe headers used by the full-generation preparation path | metadata only (identity, BPM, swing, variation count); generated event patterns are not compared |
| Persisted/runtime selection | `GenreSettings` recipe/rhythm/feel fields | not redefined; census uses recipe identity with morph disabled and automatic rhythm selection |

`ProfileDefinition` does not itself contain the rhythm bag. A production profile view is the effective join of the private row with `rhythmCompatibilityFor(settings)`. Treating only `kProfiles` as the whole policy would therefore omit a major genre/recipe dimension.

The profile `feels` bag is captured as declared composition policy. The canonical drum fingerprint does **not** absorb the Scene FEEL selection: FEEL remains its existing independent runtime owner.

## Deterministic normalization

No pointer, declaration address, table line, or source ordering is fingerprinted.

1. Every weighted bag is sorted by numeric semantic identity and serialized as `id:name@rawIntegerWeight`. A second support-only form removes weights, allowing membership and weight differences to be distinguished.
2. The corridor payload contains every actual `GenerationCorridor` field on the final release: `bpmMin`, `suggestedBpm`, `bpmMax`, `gridSteps`, `densityMin`, and `densityMax`.
3. The tonal payload fingerprints masks for allowed and preferred bass contours/articulations, melodic rhythm operations/contours/motif operations, and both register corridors field by field.
4. Each rhythm archetype payload normalizes family, suggested BPM, allowed phrase lengths, active roles, lane grammar, protected spaces, lane relationships, anchor transforms, trajectories, timing policy, density, and mutation budgets. Collections are sorted by semantic contents.
5. The canonical drum payload projects each weighted archetype to drum roles and drum-affecting relations. It is genre/recipe-owned grammar, not a realized pattern.
6. `primary_static_fingerprint` combines weighted composition, corridor, secondary role, tonal payload, and canonical drum projection. `full_trace_fingerprint` additionally includes legacy params/drums/behavior/timbre and Atlas metadata. Full 64-character hashes are in the TSV; 12-character prefixes below are only labels for reading.

Raw integer weights are retained because the production selector consumes those exact integers and total-weight modulus. Support equality and weighted equality are separately available in the TSV.

## Genre × Recipe census

Rhythm entries below are `archetypeId@weight`. Corridor is `BPM min/suggested/max; density min–max; grid`. All other exact weighted bags are present, by name and ID, in the main TSV.

| # | Genre | Recipe | Rhythm compatibility | Corridor | Secondary role | Tonal source | Primary FP |
|---:|---|---|---|---|---|---|---|
| 0 | Acid | BASE | 405@120, 406@110, 407@100, 408@90 | 118/132/150; d6–14; g16 | Melodic | kAcidProfile | `55898004bfe8` |
| 1 | Acid | 6 Chicago Jack | 405@120, 408@100 | 118/124/132; d6–13; g16 | Melodic | kAcidProfile | `09a76859bc9c` |
| 2 | Acid | 7 Rolling Acid | 406@120, 407@110 | 126/136/145; d8–15; g16 | Melodic | kAcidProfile | `e9e63334c9cb` |
| 3 | Outrun | BASE | 401@90, 403@120, 711@110, 713@100 | 88/108/125; d4–11; g16 | Melodic | kSynthProfile | `4b470097f208` |
| 4 | Darksynth | BASE | 401@120, 402@110, 403@100, 404@100, 711@80 | 122/134/148; d5–13; g16 | Melodic | kSynthProfile | `b1fdbaba59f9` |
| 5 | Electro | BASE | 404@80, 420@120, 712@120, 714@110 | 102/116/132; d4–12; g16 | Melodic | kBrokenProfile | `752848e30a7d` |
| 6 | Rave | BASE | 401@120, 402@110, 404@90, 419@80 | 132/145/160; d7–15; g16 | Melodic | kStaticProfile | `6be63d445020` |
| 7 | Rave | 4 Psytrance | 401@120, 402@110, 406@100 | 138/145/150; d8–15; g16 | Melodic | kStaticProfile | `fa706461de16` |
| 8 | Reggae | BASE | 409@120, 410@110, 411@110, 412@100 | 68/82/105; d2–9; g16 | Chord | kDubProfile | `327e75f27a49` |
| 9 | Reggae | 5 Dub Techno | 409@110, 410@100, 411@110, 412@120 | 112/120/128; d2–8; g16 | Chord | kDubProfile | `34da30a241a7` |
| 10 | Reggae | 10 Deep Chord | 412@120 | 108/116/124; d2–8; g16 | Chord | kDubProfile | `a7df848f9e4d` |
| 11 | Reggae | 11 Minimal Space | 409@100, 411@120, 412@90 | 72/86/102; d1–7; g16 | Chord | kDubProfile | `93c95f73e8f0` |
| 12 | TripHop | BASE | 415@120, 416@130, 713@90, 714@75 | 66/80/98; d2–9; g16 | Chord | kSlowProfile | `42dd77121608` |
| 13 | Broken | BASE | 413@100, 414@100, 415@90, 416@90, 417@110, 418@110, 419@90, 420@80 | 118/132/148; d5–14; g16 | Melodic | kBrokenProfile | `ffb7847c1139` |
| 14 | Broken | 1 UK Garage | 417@120, 418@110, 419@100, 420@80 | 125/132/138; d5–13; g16 | Melodic | kBrokenProfile | `a8dcb533669f` |
| 15 | Broken | 2 Drum&Bass | 413@120, 414@110, 415@100, 416@90 | 160/174/180; d7–15; g16 | Melodic | kBrokenProfile | `14a5e3649766` |
| 16 | Broken | 3 Footwork | 415@90, 416@100, 420@120, 714@110 | 145/158/165; d6–15; g16 | Melodic | kBrokenProfile | `213fd0537bc7` |
| 17 | Broken | 8 Classic 2-Step | 417@120, 419@90 | 126/132/136; d5–12; g16 | Melodic | kBrokenProfile | `4a7b8c23c86a` |
| 18 | Broken | 9 Dark Skippy | 418@120, 420@100 | 128/134/140; d3–11; g16 | Melodic | kBrokenProfile | `7b98bfe47f8a` |
| 19 | Chip | BASE | 420@120, 711@80, 712@100, 714@90 | 96/128/170; d5–15; g16 | Melodic | kSynthProfile | `fafa8b1d4d4d` |
| 20 | House | BASE | 401@90, 402@90, 419@70, 711@125, 713@130 | 112/122/128; d5–13; g16 | Melodic | kStaticProfile | `01968d952172` |
| 21 | Techno | BASE | 401@120, 402@110, 403@105, 404@105, 420@90 | 124/134/146; d5–14; g16 | Melodic | kStaticProfile | `56bca709b31b` |
| 22 | HipHop | BASE | 415@105, 416@125, 712@90, 713@85, 714@110 | 76/90/104; d3–10; g16 | ChordWithMelodicFill | kSlowProfile | `d0ae6a326dcd` |
| 23 | HipHop | 16 Golden Era | 416@135, 712@90, 713@85, 714@100 | 82/92/100; d4–10; g16 | ChordWithMelodicFill | kSlowProfile | `5c9bf0476bdd` |
| 24 | HipHop | 17 Dusty Jazz | 415@85, 416@115, 713@135 | 70/84/94; d3–9; g16 | ChordWithMelodicFill | kSlowProfile | `58206d772a62` |
| 25 | FunkSoul | BASE | 415@75, 416@90, 713@150 | 88/102/116; d4–11; g16 | ChordWithMelodicFill | kSlowProfile | `c65d6db1222f` |
| 26 | UkGarage | BASE | 417@125, 418@120, 419@105, 420@75 | 126/132/140; d5–13; g16 | Melodic | kBrokenProfile | `d74035d4a6d6` |
| 27 | DrumAndBass | BASE | 413@125, 414@115, 415@105, 416@95 | 160/174/180; d7–15; g16 | Melodic | kBrokenProfile | `926c04fe098f` |
| 28 | LoFi | BASE | 403@55, 415@115, 416@135, 713@95, 714@65 | 54/72/90; d2–8; g16 | ChordWithMelodicFill | kSlowProfile | `d26b55decb13` |
| 29 | LoFi | 12 Classic Chill | 415@115, 416@145, 713@85 | 58/72/82; d2–7; g16 | ChordWithMelodicFill | kSlowProfile | `89eccbf08f5d` |
| 30 | LoFi | 13 Drunken Groove | 415@70, 416@105, 713@130, 714@90 | 66/82/92; d3–9; g16 | ChordWithMelodicFill | kSlowProfile | `5250923a9e34` |
| 31 | LoFi | 14 Lo-Fi House | 402@65, 419@85, 711@115, 713@140 | 92/106/118; d4–11; g16 | ChordWithMelodicFill | kSlowProfile | `42a28e237fce` |
| 32 | LoFi | 15 Minimal Sleep | 403@85, 415@70, 416@155 | 42/54/66; d1–5; g16 | ChordWithMelodicFill | kSlowProfile | `50bd06407c0d` |

The canonical row order above is `(genre ID, recipe ID)`, not private `kProfiles` declaration order. It is a research enumeration coordinate, never musical or persisted identity.

## Non-BASE classification

`M` means candidate membership changed relative to the same genre BASE; `W` means identical membership with different weights. Every row below also changes its corridor. `DRUM` here is the canonical drum-role projection of the changed rhythm grammar. Legacy trace changes are reported separately.

| Recipe | Bag deltas vs BASE | Measured primary domains | Atlas metadata | Legacy trace changes |
|---|---|---|---|---|
| Acid:6 Chicago Jack | rhythms M, phrases M | COMPOSITION + CORRIDOR + DRUM | 124 BPM / 52%, matches suggested BPM | PARAMS; DRUM; BEHAVIOR; TIMBRE |
| Acid:7 Rolling Acid | rhythms M | COMPOSITION + CORRIDOR + DRUM | 128 BPM / 54%, inside corridor | PARAMS; DRUM; BEHAVIOR; TIMBRE |
| Rave:4 Psytrance | rhythms M | COMPOSITION + CORRIDOR + DRUM | — | PARAMS; DRUM |
| Reggae:5 Dub Techno | rhythms W | COMPOSITION + CORRIDOR + DRUM | — | PARAMS; DRUM |
| Reggae:10 Deep Chord | rhythms M, feels M | COMPOSITION + CORRIDOR + DRUM | 120 BPM / 54%, inside corridor | PARAMS; DRUM; BEHAVIOR; TIMBRE |
| Reggae:11 Minimal Space | rhythms M | COMPOSITION + CORRIDOR + DRUM | **116 BPM / 51%, outside profile corridor 72–102** | PARAMS; DRUM; BEHAVIOR; TIMBRE |
| Broken:1 UK Garage | rhythms M | COMPOSITION + CORRIDOR + DRUM | — | PARAMS; DRUM |
| Broken:2 Drum&Bass | rhythms M, feels M, bass M, phrases M | COMPOSITION + CORRIDOR + DRUM | — | PARAMS; DRUM |
| Broken:3 Footwork | rhythms M | COMPOSITION + CORRIDOR + DRUM | — | PARAMS; DRUM |
| Broken:8 Classic 2-Step | rhythms M | COMPOSITION + CORRIDOR + DRUM | 134 BPM / 66%, inside corridor | PARAMS; DRUM; BEHAVIOR; TIMBRE |
| Broken:9 Dark Skippy | rhythms M, bass M, phrases M | COMPOSITION + CORRIDOR + DRUM | 136 BPM / 68%, inside corridor | PARAMS; DRUM; BEHAVIOR; TIMBRE |
| HipHop:16 Golden Era | rhythms M | COMPOSITION + CORRIDOR + DRUM | — | NONE |
| HipHop:17 Dusty Jazz | rhythms M, feels M, phrases M | COMPOSITION + CORRIDOR + DRUM | — | NONE |
| LoFi:12 Classic Chill | rhythms M | COMPOSITION + CORRIDOR + DRUM | — | NONE |
| LoFi:13 Drunken Groove | rhythms M, feels M, bass M, phrases M | COMPOSITION + CORRIDOR + DRUM | — | NONE |
| LoFi:14 Lo-Fi House | rhythms M, feels W, bass M, chord M, progressions M, phrases M | COMPOSITION + CORRIDOR + DRUM | — | NONE |
| LoFi:15 Minimal Sleep | rhythms M | COMPOSITION + CORRIDOR + DRUM | — | NONE |

Classification counts:

| Label | Count | Interpretation |
|---|---:|---|
| BASE-EQUIVALENT | 0 | no recipe is identical to its BASE across measured primary domains |
| WEIGHTS-ONLY | 0 | Dub Techno has a rhythm-weights-only bag delta, but also changes corridor and drum projection |
| MEMBERSHIP-CHANGE | 16 | every recipe except Dub Techno changes at least one bag's support |
| CORRIDOR-ONLY | 0 | all corridor changes coexist with other changes |
| ROLE-CHANGE | 0 | recipes retain the BASE secondary role |
| TONAL-ONLY | 0 | recipes inherit mode-level tonal policy |
| DRUM-ONLY | 0 | drum projection changes coexist with composition/corridor changes |
| MULTI-DOMAIN | 17 | every recipe differs in composition, corridor, and canonical drum projection |

These labels are descriptive census labels, not validity rules. In particular, `0` corridor-only recipes is not a recommendation to create one.

## BASE-genre distinctness

The 16 BASE rows produce 120 unordered pairs:

| Classification | Pairs |
|---|---:|
| EXACT DECLARATIVE COLLISION | **0** |
| PARTIAL COLLISION | **19** |
| STRONG DISTINCTNESS | **101** |

“Strong” means multiple independent declared domains differ. It does not mean audibly distinct.

Eighteen partial pairs are explained by four shared tonal payloads; one additional pair shares all seven non-rhythm composition bags:

| Shared restriction/payload | BASE genres | Pair count | What still differs |
|---|---|---:|---|
| `kSynthProfile` | Outrun, Darksynth, Chip | 3 | rhythm, other composition weights, corridor, legacy drum |
| `kBrokenProfile` | Electro, Broken, UkGarage, DrumAndBass | 6 | rhythm and corridor always; other composition differs for pairs involving DrumAndBass |
| `kStaticProfile` | Rave, House, Techno | 3 | rhythm, other composition weights, corridor; Rave also differs in legacy drum |
| `kSlowProfile` | TripHop, HipHop, FunkSoul, LoFi | 6 | rhythm, other composition weights, corridor; TripHop also differs in role and legacy drum |
| same non-rhythm composition bags | Acid, Techno | 1 | rhythm, corridor, tonal policy, legacy drum |

No two BASE rows share the complete weighted rhythm bag, complete corridor, or canonical drum fingerprint. No two of all 33 rows share `primary_static_fingerprint`.

The exact 19 pair records and the 101 strong pairs are in `GF2_C1F_BASE_GENRE_PAIRS.tsv`; this avoids replacing a complete pairwise result with hand-selected examples.

## Tonal policy and prohibitions

The hypotheses were confirmed from the final release:

- Rave / House / Techno share `kStaticProfile` (and Rave / Psytrance inherits it);
- TripHop / HipHop / FunkSoul / LoFi share `kSlowProfile` (including all HipHop and LoFi recipes).

All six source rows have distinct normalized payloads. There is no different tonal source ID/name with an identical payload on the final release. Sharing groups across all 33 production rows are:

| Tonal source | Rows using the exact same payload | Key effective restriction |
|---|---|---|
| `kAcidProfile` | Acid BASE, Chicago Jack, Rolling Acid | broad bass contour support; melodic Static/Neighbor/RepeatUp/RepeatDown |
| `kSynthProfile` | Outrun, Darksynth, Chip | synth-oriented bass set; broad melodic contour set including arches and leap-return |
| `kBrokenProfile` | Electro; all Broken rows; UkGarage; DrumAndBass | five bass contours; melodic Static/Step/Neighbor/Repeat variants |
| `kStaticProfile` | Rave BASE/Psytrance, House, Techno | bass **RootAnchor only**; melody **Static only**; bass adjacent leap capped at 12 semitones |
| `kDubProfile` | all four Reggae rows | bass RootAnchor/RootFifth/Neighbor/Pedal; melody **Static only** |
| `kSlowProfile` | TripHop; HipHop BASE/recipes; FunkSoul; LoFi BASE/recipes | five bass contours; melody Static/arches/Neighbor/Repeat variants |

Common register policy is bass MIDI `24–47`, secondary MIDI `48–71`, secondary adjacent leap `16`. Bass adjacent leap is `23` except in `kStaticProfile`, where it is `12`.

Single-option or effectively disabled dimensions must be read as prohibitions, not merely as preferences:

- all **33/33** rows allow only bass articulation `PLAIN`;
- all **33/33** rows allow only melodic rhythm operation `PRESERVE`;
- all **33/33** rows allow only melodic motif operation `NONE`;
- the four `kStaticProfile` rows allow only bass contour `ROOT ANCHOR` and only melodic contour `STATIC`;
- the four `kDubProfile` rows also allow only melodic contour `STATIC`, so melodic contour is single-option in **8/33** rows;
- Reggae / Deep Chord is the only row whose composition rhythm bag has one option: archetype `412 chord_response`;
- `gridSteps=16` in **33/33** corridors, so grid is a declared global constant on this profile set rather than a distinguishing dimension.

An empty preferred mask is not treated as an empty allowed set. The allowed masks above are the controlling prohibitions.

## Drum policy

### Canonical full-generation projection

For every one of the 33 valid profile identities, `selectStrongRhythmRoute(...)` resolves to a non-`Legacy` route. The normal full-generation path resolves the weighted rhythm identity, looks up the normalized `ReferenceVocabulary` archetype, realizes it, and materializes its drum roles. The census therefore fingerprints the weighted drum-role grammar of that path.

Results:

- 24 referenced archetypes, all with distinct full semantic payloads;
- 24 distinct drum-role payloads;
- 33 distinct weighted canonical drum fingerprints across the 33 profile rows;
- no canonical BASE drum sharing group.

Cross-role relationships are retained in the drum projection when either side affects a drum role. Scene FEEL is not absorbed into this fingerprint.

### Legacy/upstream trace

The full-generation preparation path first creates Atlas or legacy material and then calls strong-rhythm migration. The legacy declarations remain relevant traceability and potential failure-path data, so they are recorded separately:

- `GenreCatalog::compiledGenerativeParams`;
- recipe `drumTemplateOverride` or mode `kDrumTemplates`;
- `GenreCatalog::behavior` and its timbre sub-payload.

Legacy drum payload sharing across all 33 rows:

1. Broken / UK Garage (`recipe 1`) and Broken / Classic 2-Step (`recipe 8`) share one legacy override payload.
2. House BASE, Techno BASE, HipHop BASE/Golden Era/Dusty Jazz, FunkSoul BASE, UkGarage BASE, DrumAndBass BASE, and LoFi BASE/all four recipes share an all-zero mode-template payload.

The second group exists because `kDrumTemplates[kGenerativeModeCount]` explicitly initializes only the first nine modes; the seven later modes are zero-initialized. C1F does not “fix” or reinterpret that policy.

The final release still contains call sites that explicitly discard the return value of `migrateStrongRhythmMaterial(...)`. C1F records this only as a trace requiring final-release reachability/failure-propagation review. Its current status and consequence belong to GF2-C1RF; this declarative census does not infer that the canonical projection always reaches output.

### Atlas metadata conflict

Six recipe IDs have an additional generated Atlas metadata declaration. C1 reads metadata only and never compares the generated event patterns:

| Genre / recipe | Atlas identity | BPM | Swing | Variations | Relation to profile corridor |
|---|---|---:|---:|---:|---|
| Acid / Chicago Jack | `REC_ACID_CHICAGO_JACK` | 124 | 52% | 3 | equals suggested BPM |
| Acid / Rolling Acid | `REC_ACID_ROLLING` | 128 | 54% | 3 | inside `126–145`, differs from suggested 136 |
| Broken / Classic 2-Step | `REC_UKG_CLASSIC_2STEP` | 134 | 66% | 3 | inside `126–136`, differs from suggested 132 |
| Broken / Dark Skippy | `REC_UKG_DARK_SKIPPY` | 136 | 68% | 3 | inside `128–140`, differs from suggested 134 |
| Reggae / Deep Chord | `REC_DUB_DEEP_CHORD` | 120 | 54% | 3 | inside `108–124`, differs from suggested 116 |
| Reggae / Minimal Space | `REC_DUB_MINIMAL_SPACE` | **116** | 51% | 3 | **outside profile corridor `72–102`** |

The static declarations still contain both Atlas BPM/swing metadata and the profile corridor. `Minimal Space` therefore retains two incompatible declared tempo values. Whether and how the final production path arbitrates them belongs to GF2-C1RF; the mismatch is not an invitation to change either value in C1F.

## Gate model frozen for GF2

### GATE A — DECLARATIVE DISTINCTNESS

Question:

> Did we declare different musical policies?

Measured by this GF2-C1F census.

### GATE B — REALIZED DISTINCTNESS

Question:

> Does finished production realization preserve those differences into actual musical material?

Measured only by future GF2-C2 after final reachability, dependency-map, and harness validation gates are complete.

Interpretation:

> Gate A distinct + Gate B weak = semantic information is being lost downstream.

That result is **not** evidence to strengthen genre tables. It is evidence to inspect realization/execution owners.

The existing file `tests/data/stage15_tonal_enabled_f13_baseline.tsv.gz.b64` is present and may be reusable as infrastructure evidence later. It is not consumed as musical-capacity evidence by C1F.

## GF2-C2 remains blocked

The historical GF2 contract required completion of:

```text
D3
→ E0
→ H1'
→ E1
→ E1.5
→ E2
→ E3
→ E4
→ E5
→ E6
```

plus:

```text
F-13 final
F-08 final
```

This census is now based on the frozen `v0.9.9` release rather than an intermediate renderer commit. C1F does not re-adjudicate the historical task labels; it establishes only that their accumulated production work did not mutate the measured declarative profile payloads.

C2 is nevertheless **not ready** from C1F alone. Before any realized corpus, the final release still requires:

1. **GF2-C1RF** — re-audit semantic reachability and failure propagation on `v0.9.9`;
2. **GF2-C1DF** — regenerate the declarative dependency/loss map from C1F + C1RF;
3. **GF2-C2-V0** — prove deterministic replay, within-policy stochastic baseline, intervention isolation, and semantic reachability.

C1F does not implement or run C2.

## Falsification frozen before C2

### F1 — SYSTEMIC COLLAPSE

If, after the frozen renderer and V0 harness validation:

```text
Gate A = strongly distinct
Gate B = weak/systematically collapsed
```

then:

> **GF2-R2 IS BLOCKED**

Do not compensate by adding recipes or genres. Return to production realization/execution owners.

### F2 — CATALOG SATURATION

If the corrected renderer is working, but existing BASE + recipes already cover nearly all realized musical space and provisional additions produce only insignificant probability reshuffling, catalog expansion is not justified.

Conclusion:

```text
catalog capacity >= engine capacity
```

GF2-R2 is not automatically required.

## Future classification policy, not a runtime abstraction

Default research classification:

```text
new candidate → PROVISIONAL RECIPE
```

Promotion criterion:

```text
cannot honestly be expressed inside an existing genre language
→ candidate for new GENRE
```

This is asymmetric research policy only. It does not create `RecipeFamily`, `Style`, `Dialect`, or `GenreFamily` production owners. Provisional recipe IDs are not released or persisted contracts before classification is frozen.

## PHRASE — GF2 semantic gate only

The existing 0.9.9 PHRASE workflow is not modified here. Before any future GF2 phrase exposure or policy claim, GF2-P1 must prove that, for fixed selection, seed, genre, recipe, rhythm, harmonic context, feel, and `phraseBarOrdinal`, changing only trajectory/form (`AAAA` vs `AABA`) causally changes the semantic output at the relevant fixed ordinal.

Example: `AAAA @ ordinal 2` versus `AABA @ ordinal 2` must differ because of trajectory semantics, not because ordinal changed.

Frozen wording:

> PHRASE selector must causally control phrase topology; barOrdinal alone must not explain the observed difference.

The profile `phraseLaws` bags are catalogued as declared planning metadata, consistent with the production source comment. C1F does not infer their reachability into the final phrase workflow and does not treat them as realized capacity.

## Ubiquitous language audit

There is no single dedicated canonical ubiquitous-language document covering GENRE, RECIPE, RHYTHM, PHRASE, DEPTH, FEEL, EVOLUTION, and ARENA together. `README.md`, `GROOVE_VOCABULARY_MUSICAL_CONTRACTS.md`, stage acceptance documents, and the existing four-axis research documents each define subsets in context.

A future small `CONTEXT.md` could reduce term drift and cross-link those existing normative owners. C1F does not create it and does not propose a broad architecture redesign.

## Open questions and risks

1. Which owner must resolve the `Minimal Space` profile corridor (`72–102`, suggested 86) versus Atlas BPM (`116`) conflict? C1F records both and changes neither.
2. Do the final-release call sites that discard `migrateStrongRhythmMaterial(...)` results permit legacy/previous material to survive failure? GF2-C1RF must answer from the final code path before Gate B.
3. Is mode-level tonal inheritance for every recipe intentional? On the final release no recipe has a tonal-only or tonal-changing delta.
4. Are the seven zero-initialized late-mode legacy drum templates intentionally inert now that their normal full-generation routes are strong, or are they an unfinished fallback policy?
5. Phrase/evolution bags are declared but explicitly marked planning-only in the profile owner. Their causal reachability into the final phrase workflow must not be inferred from this census.
6. The profile corridor and Atlas metadata are two static tempo declarations for six recipes. Future ownership work should state which is authoritative without silently changing musical policy.

## Semantic delta

**NONE.** This branch adds only deterministic research tooling, characterization tests, and regenerated research artifacts. No `src/` production file is modified.
