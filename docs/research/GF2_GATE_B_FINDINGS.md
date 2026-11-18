# GF2 Gate B — Materialized Musical Capacity

## 1. Exact base

Frozen measurement base: `9c01b0c34b80aacb1dd6be66bb07c5cef3ad1c38`.
Observed production source: `22df93080a7827684d5b11be09b7516e31828724`.

## 2. Corpus

- Production profiles: **37**.
- Frozen seeds: **55**.
- DEPTH coverage: **P1 / P2 / P3** for every matched profile+seed.
- Main deterministic realizations: **6105**.
- Applied materializations: **6105**; failed/non-applied: **0**.
- Unordered profile pairs: **666**.

## 3. Determinism

**PASS by Gate B contract.** The focused runner requires byte-identical repeated GCC raw dumps, GCC/Clang equality when Clang is available, deterministic Python ordering, and byte-for-byte equality with committed review artifacts. No timestamps or UUIDs are emitted.

## 4. Measured axis capacity

### Genre / Recipe

The production catalog materialized **37** profile identities. Labels alone are not counted as musical capacity.

### Density

Profile corridor minima observed in the frozen production selection are **[1, 2, 3, 4, 5, 6, 7, 8]** and corridor maxima are **[5, 7, 8, 9, 10, 11, 12, 13, 14, 15]**.
The actually resolved internal `structuralDensityTarget` values are **[9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24]**. These are different quantities; corridor bounds must not be reported as resolved materialized targets.
All **37/37** profiles materialize more than one RhythmSignature at at least one identical resolved density target, so a resolved target is not a unique topology identity.
Exact RhythmSignature reuse across different resolved density targets occurs in **13** profiles: `Acid/BASE`, `Darksynth/BASE`, `Drum&Bass/Atmos`, `Drum&Bass/BASE`, `Drum&Bass/Dance`, `Drum&Bass/Funk`, `Drum&Bass/Neuro`, `Dub/Reggae/BASE`, `Dub/Reggae/Dub Techno`, `Dub/Reggae/Minimal Space`, `House/BASE`, `Synthwave/BASE`, `Techno/BASE`.
Conclusion: **DENSITY is materialized and causal, but is not a unique structural-identity owner.**

### Feel

Actual timing displacement is present in **903** realizations across **13/37** profiles and zero in **5202** applied realizations.
The corpus contains **1336** displaced physical events; maximum observed absolute displacement is **1 transport tick**.
Profiles with materialized displacement: `Dub/Reggae/BASE`, `Dub/Reggae/Deep Chord`, `Dub/Reggae/Dub Techno`, `Dub/Reggae/Minimal Space`, `Hip-Hop/BASE`, `Hip-Hop/Dusty Jazz`, `Hip-Hop/Golden Era`, `Lo-Fi/BASE`, `Lo-Fi/Classic Chill`, `Lo-Fi/Drunken Groove`, `Lo-Fi/Lo-Fi House`, `Lo-Fi/Minimal Sleep`, `Trip-Hop/BASE`.
The other **24/37** profiles have **NO MATERIALIZED EFFECT IN THE FROZEN CORPUS** for FEEL; this is characterization, not an automatic bug classification.
`kFeelTicksPerStep = 24` is the grid-step size at 96 PPQN, not the measured displacement magnitude; the observed offset field is already expressed in transport ticks.
Resolved FEEL values on displaced rows: `2`=903.

### Phrase law

- `DEVELOP/RETURN`: selected **1527**, admitted **1527**, materialized with bar-to-bar change in **1510** realizations.
- `LOOP`: selected **1416**, admitted **1416**, materialized with bar-to-bar change in **878** realizations.
- `REPEAT/REPLY`: selected **2469**, admitted **2469**, materialized with bar-to-bar change in **2400** realizations.
- `SPARSE DRIFT`: selected **693**, admitted **693**, materialized with bar-to-bar change in **669** realizations.

Production-valid phrase admission is **6105 / 6105**.
`declared_phrase_law` in the raw Gate B seam is the weighted production law selected for that realization. It is not a static one-law-per-profile owner.
PhraseSignature encodes relative temporal form: equality classes, base/previous structural deltas, material-change locations, functions, and final return. Absolute bar hashes are not part of the musical signature. Trajectory ID remains provenance only.

### secondaryRole

Actual semantic-role distribution over applied realizations: CHORD=825, MELODIC=4290, HYBRID=990.
RoleSignature exclusively owns secondary-role identity plus DRUMS/BASS/CHORD/MELODIC and Synth-B participation. HarmonySignature contains only observed harmonic material.

### DEPTH

Matched profile+seed P1/P2/P3 triplets: **2035**.
`secondaryRole` semantic identity is observed and stable in **2035 / 2035** triplets; identity changes: **0 / 2035**.
Actual Synth-B active/inactive participation changes while the semantic role identity remains stable in **0 / 2035** triplets; supporting activity is sufficiently observed in **2035 / 2035** triplets.
RhythmSignature changes across P1/P2/P3 in **1737 / 2035** matched triplets; TransformationSignature separately measures relative P1→P2→P3 intervention magnitude.
Conclusion: **DEPTH itself is a causal realization/transformation-magnitude axis. ROLE IDENTITY VIA DEPTH is negative capacity in this frozen corpus. No DEPTH-driven change in materialized supporting-role activity was observed in this frozen corpus.**

### GRID negative capacity

Observed structural GRID values: **[16]**. Core-v1 remains GRID=16; GRID 8/32 remain intentionally unsupported **NEGATIVE CAPACITY**.
## 5. Pairwise results

- STRUCTURALLY DISTINCT: **642**
- PARTIALLY DISTINCT: **24**
- TIMBRE-DEPENDENT: **0**
- STRUCTURALLY REDUNDANT: **0**
- INSUFFICIENT EVIDENCE: **0**

Classification is dimension-level. NegativeSignature is evidence but never a second positive structural vote. TIMBRE-DEPENDENT requires positive observed timbre evidence; this neutral observation seam supplies none, so profile labels cannot produce that class.

## 6. Same-Genre Recipe collisions

- `Acid/Chicago Jack` ↔ `Acid/Rolling Acid` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=SAME.
- `Breaks/Classic 2-Step` ↔ `Breaks/Footwork` — PARTIALLY DISTINCT; rhythm=DISJOINT;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Classic 2-Step` ↔ `Breaks/UK Garage` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Dark Skippy` ↔ `Breaks/UK Garage` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=DISJOINT;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Drum&Bass` ↔ `Breaks/Footwork` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Footwork` ↔ `Breaks/UK Garage` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Drum&Bass/BASE` ↔ `Drum&Bass/Dance` — PARTIALLY DISTINCT; rhythm=SAME;bass=SAME;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Dub/Reggae/BASE` ↔ `Dub/Reggae/Deep Chord` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Dub/Reggae/BASE` ↔ `Dub/Reggae/Dub Techno` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Dub/Reggae/BASE` ↔ `Dub/Reggae/Minimal Space` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Dub/Reggae/Deep Chord` ↔ `Dub/Reggae/Minimal Space` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Hip-Hop/Dusty Jazz` ↔ `Hip-Hop/Golden Era` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Lo-Fi/Classic Chill` ↔ `Lo-Fi/Drunken Groove` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Rave/BASE` ↔ `Rave/Psytrance` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=SAME.

## 7. Cross-Genre collisions

- `Acid/Chicago Jack` ↔ `Rave/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=SAME.
- `Acid/Chicago Jack` ↔ `Rave/Psytrance` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=SAME.
- `Acid/Rolling Acid` ↔ `Rave/Psytrance` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=SAME.
- `Breaks/Classic 2-Step` ↔ `Rave/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=DISJOINT;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Classic 2-Step` ↔ `Rave/Psytrance` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Drum&Bass` ↔ `Rave/BASE` — PARTIALLY DISTINCT; rhythm=DISJOINT;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Footwork` ↔ `Chip/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Chip/BASE` ↔ `Rave/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.
- `Dub/Reggae/BASE` ↔ `Lo-Fi/Lo-Fi House` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=DISJOINT;transformation=OVERLAP;negative=OVERLAP.
- `Dub/Reggae/Dub Techno` ↔ `Trip-Hop/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=SAME;transformation=OVERLAP;negative=OVERLAP.

## 8. One-dimensional profiles

- No pair differed on exactly one fully-observed positive structural axis.

## 9. Negative capacity

- Core-v1 structural GRID 8/32: **NEGATIVE CAPACITY / intentionally unsupported**.
- DEPTH realization/transformation magnitude: **causal/distinct frozen capacity**, not negative capacity.
- ROLE IDENTITY VIA DEPTH: **NEGATIVE CAPACITY in the frozen corpus**; 0/2035 semantic secondaryRole identity reassignments are observed.
- DEPTH → supporting-role activity: no Synth-B active/inactive change was observed in 2035 matched triplets.
- Inert FEEL means **NO MATERIALIZED EFFECT IN THE FROZEN CORPUS** for the affected profiles; it is evidence, not automatically a production defect.
- NegativeSignature remains first-class prohibition/absence evidence but never double-votes the positive source dimension.
## 10. Observation limitations

- Physical note duration is not exposed: **NOT_OBSERVED**, never zero.
- Physical DrumStep does not expose structural/secondary/ghost importance; Gate B does not reconstruct it from velocity or declarations.
- Timbre/engine/oscillator/sample/kit/FX identity is intentionally removed, and the V0R seam does not expose separate positive timbre evidence; therefore TIMBRE-DEPENDENT cannot be inferred from labels.
- Relative pitch is observed as pitch class relative to tonal root plus contour; richer harmonic function is not fabricated.
- `resolved_density` is the internal production `structuralDensityTarget`; profile corridor bounds (`density_min` / `density_max`) are related selection inputs, not aliases for that resolved target.
- `declared_phrase_law` records the production-weighted law selected for the individual realization; Gate B does not invent a single canonical law per profile.
- FEEL timing offsets in the observation seam are already transport ticks. The 24-tick sixteenth-note step size is not a displacement multiplier.
- Pairwise results characterize the frozen deterministic 55-seed matched corpus, not every possible RNG identity.
## 11. Gate B conclusion

Gate B is measurement only. It makes no recommendation about deleting profiles, merging Genres, promoting Recipes, exposing controls, renaming DEPTH, redesigning UI, or building a next feature. Interpretation belongs to **GF2-G1 — INTERPRET ACTUAL CAPACITY**.
