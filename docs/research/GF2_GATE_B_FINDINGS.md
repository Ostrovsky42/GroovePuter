# GF2 Gate B — Materialized Musical Capacity

## 1. Exact base

Frozen measurement base: `9c01b0c34b80aacb1dd6be66bb07c5cef3ad1c38`.

## 2. Corpus

- Production profiles: **33**.
- Frozen seeds: **55**.
- DEPTH coverage: **P1 / P2 / P3** for every matched profile+seed.
- Main deterministic realizations: **5445**.
- Applied materializations: **5445**; failed/non-applied: **0**.
- Unordered profile pairs: **528**.

## 3. Determinism

**PASS by Gate B contract.** The focused runner requires byte-identical repeated GCC raw dumps, GCC/Clang equality when Clang is available, deterministic Python ordering, and byte-for-byte equality with committed review artifacts. No timestamps or UUIDs are emitted.

## 4. Measured axis capacity

### Genre / Recipe

The production catalog materialized **33** profile identities. Labels alone are not counted as musical capacity.

### Density

Profile corridor minima observed in the frozen production selection are **[1, 2, 3, 4, 5, 6, 7, 8]** and corridor maxima are **[5, 7, 8, 9, 10, 11, 12, 13, 14, 15]**.
The actually resolved internal `structuralDensityTarget` values are **[9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24]**. These are different quantities; corridor bounds must not be reported as resolved materialized targets.
All **33/33** profiles materialize more than one RhythmSignature at at least one identical resolved density target, so a resolved target is not a unique topology identity.
Exact RhythmSignature reuse across different resolved density targets occurs in **8** profiles: `Acid/BASE`, `Darksynth/BASE`, `Dub/Reggae/BASE`, `Dub/Reggae/Dub Techno`, `Dub/Reggae/Minimal Space`, `House/BASE`, `Synthwave/BASE`, `Techno/BASE`.
Conclusion: **DENSITY is materialized and causal, but is not a unique structural-identity owner.**

### Feel

Actual timing displacement is present in **912** realizations across **13/33** profiles and zero in **4533** applied realizations.
The corpus contains **1282** displaced physical events; maximum observed absolute displacement is **1 transport tick**.
Profiles with materialized displacement: `Dub/Reggae/BASE`, `Dub/Reggae/Deep Chord`, `Dub/Reggae/Dub Techno`, `Dub/Reggae/Minimal Space`, `Hip-Hop/BASE`, `Hip-Hop/Dusty Jazz`, `Hip-Hop/Golden Era`, `Lo-Fi/BASE`, `Lo-Fi/Classic Chill`, `Lo-Fi/Drunken Groove`, `Lo-Fi/Lo-Fi House`, `Lo-Fi/Minimal Sleep`, `Trip-Hop/BASE`.
The other **20/33** profiles have **NO MATERIALIZED EFFECT IN THE FROZEN CORPUS** for FEEL; this is characterization, not an automatic bug classification.
`kFeelTicksPerStep = 24` is the grid-step size at 96 PPQN, not the measured displacement magnitude; the observed offset field is already expressed in transport ticks.
Resolved FEEL values on displaced rows: `2`=912.

### Phrase law

- `DEVELOP/RETURN`: selected **1533**, admitted **1533**, materialized with bar-to-bar change in **1467** realizations.
- `LOOP`: selected **951**, admitted **951**, materialized with bar-to-bar change in **459** realizations.
- `REPEAT/REPLY`: selected **2256**, admitted **2256**, materialized with bar-to-bar change in **2049** realizations.
- `SPARSE DRIFT`: selected **705**, admitted **705**, materialized with bar-to-bar change in **682** realizations.

Production-valid phrase admission is **5445 / 5445**.
`declared_phrase_law` in the raw Gate B seam is the weighted production law selected for that realization. It is not a static one-law-per-profile owner.
PhraseSignature encodes relative temporal form: equality classes, base/previous structural deltas, material-change locations, functions, and final return. Absolute bar hashes are not part of the musical signature. Trajectory ID remains provenance only.

### secondaryRole

Actual semantic-role distribution over applied realizations: CHORD=825, MELODIC=3135, HYBRID=1485.
RoleSignature exclusively owns secondary-role identity plus DRUMS/BASS/CHORD/MELODIC and Synth-B participation. HarmonySignature contains only observed harmonic material.

### DEPTH

Matched profile+seed P1/P2/P3 triplets: **1815**.
`secondaryRole` semantic identity is observed and stable in **1815 / 1815** triplets; identity changes: **0 / 1815**.
Actual Synth-B active/inactive participation changes while the semantic role identity remains stable in **15 / 1815** triplets; supporting activity is sufficiently observed in **1815 / 1815** triplets.
RhythmSignature changes across P1/P2/P3 in **1786 / 1815** matched triplets; TransformationSignature separately measures relative P1→P2→P3 intervention magnitude.
Conclusion: **DEPTH itself is a causal realization/transformation-magnitude axis. ROLE IDENTITY VIA DEPTH is negative capacity in this frozen corpus. DEPTH can nevertheless affect the materialized activity magnitude of the already-selected supporting role in a small subset.**

### GRID negative capacity

Observed structural GRID values: **[16]**. Core-v1 remains GRID=16; GRID 8/32 remain intentionally unsupported **NEGATIVE CAPACITY**.
## 5. Pairwise results

- STRUCTURALLY DISTINCT: **381**
- PARTIALLY DISTINCT: **147**
- TIMBRE-DEPENDENT: **0**
- STRUCTURALLY REDUNDANT: **0**
- INSUFFICIENT EVIDENCE: **0**

Classification is dimension-level. NegativeSignature is evidence but never a second positive structural vote. TIMBRE-DEPENDENT requires positive observed timbre evidence; this neutral observation seam supplies none, so profile labels cannot produce that class.

## 6. Same-Genre Recipe collisions

- `Acid/BASE` ↔ `Acid/Chicago Jack` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=SAME.
- `Acid/BASE` ↔ `Acid/Rolling Acid` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=SAME.
- `Acid/Chicago Jack` ↔ `Acid/Rolling Acid` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=SAME.
- `Breaks/BASE` ↔ `Breaks/Classic 2-Step` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/BASE` ↔ `Breaks/Dark Skippy` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=DISJOINT;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/BASE` ↔ `Breaks/Drum&Bass` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/BASE` ↔ `Breaks/Footwork` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/BASE` ↔ `Breaks/UK Garage` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Classic 2-Step` ↔ `Breaks/Footwork` — PARTIALLY DISTINCT; rhythm=DISJOINT;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Classic 2-Step` ↔ `Breaks/UK Garage` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Dark Skippy` ↔ `Breaks/UK Garage` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=DISJOINT;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Drum&Bass` ↔ `Breaks/Footwork` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Drum&Bass` ↔ `Breaks/UK Garage` — PARTIALLY DISTINCT; rhythm=DISJOINT;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/Footwork` ↔ `Breaks/UK Garage` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Dub/Reggae/BASE` ↔ `Dub/Reggae/Deep Chord` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Dub/Reggae/BASE` ↔ `Dub/Reggae/Dub Techno` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Dub/Reggae/BASE` ↔ `Dub/Reggae/Minimal Space` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Dub/Reggae/Deep Chord` ↔ `Dub/Reggae/Dub Techno` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Dub/Reggae/Deep Chord` ↔ `Dub/Reggae/Minimal Space` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Dub/Reggae/Dub Techno` ↔ `Dub/Reggae/Minimal Space` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Hip-Hop/BASE` ↔ `Hip-Hop/Dusty Jazz` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=DISJOINT;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Hip-Hop/BASE` ↔ `Hip-Hop/Golden Era` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Hip-Hop/Dusty Jazz` ↔ `Hip-Hop/Golden Era` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Lo-Fi/BASE` ↔ `Lo-Fi/Classic Chill` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Lo-Fi/BASE` ↔ `Lo-Fi/Drunken Groove` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Lo-Fi/BASE` ↔ `Lo-Fi/Minimal Sleep` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Lo-Fi/Classic Chill` ↔ `Lo-Fi/Drunken Groove` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Lo-Fi/Classic Chill` ↔ `Lo-Fi/Minimal Sleep` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Lo-Fi/Drunken Groove` ↔ `Lo-Fi/Lo-Fi House` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Lo-Fi/Drunken Groove` ↔ `Lo-Fi/Minimal Sleep` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Lo-Fi/Lo-Fi House` ↔ `Lo-Fi/Minimal Sleep` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=DISJOINT;negative=OVERLAP.
- `Rave/BASE` ↔ `Rave/Psytrance` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=SAME.

## 7. Cross-Genre collisions

- `Acid/BASE` ↔ `Breaks/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/BASE` ↔ `Breaks/Classic 2-Step` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=DISJOINT;negative=OVERLAP.
- `Acid/BASE` ↔ `Breaks/Drum&Bass` — PARTIALLY DISTINCT; rhythm=DISJOINT;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/BASE` ↔ `Breaks/Footwork` — PARTIALLY DISTINCT; rhythm=DISJOINT;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/BASE` ↔ `Breaks/UK Garage` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=DISJOINT;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/BASE` ↔ `Chip/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/BASE` ↔ `Darksynth/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=DISJOINT;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/BASE` ↔ `Drum&Bass/BASE` — PARTIALLY DISTINCT; rhythm=DISJOINT;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/BASE` ↔ `House/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/BASE` ↔ `Rave/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=SAME.
- `Acid/BASE` ↔ `Rave/Psytrance` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=SAME.
- `Acid/BASE` ↔ `Techno/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=SAME.
- `Acid/BASE` ↔ `UK Garage/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Chicago Jack` ↔ `Breaks/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Chicago Jack` ↔ `Breaks/Classic 2-Step` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=DISJOINT;negative=OVERLAP.
- `Acid/Chicago Jack` ↔ `Breaks/UK Garage` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=DISJOINT;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Chicago Jack` ↔ `Chip/BASE` — PARTIALLY DISTINCT; rhythm=DISJOINT;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Chicago Jack` ↔ `Darksynth/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=DISJOINT;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Chicago Jack` ↔ `House/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Chicago Jack` ↔ `Rave/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=SAME.
- `Acid/Chicago Jack` ↔ `Rave/Psytrance` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=SAME.
- `Acid/Chicago Jack` ↔ `Techno/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=SAME.
- `Acid/Chicago Jack` ↔ `UK Garage/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Rolling Acid` ↔ `Breaks/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Rolling Acid` ↔ `Breaks/Classic 2-Step` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=DISJOINT;negative=OVERLAP.
- `Acid/Rolling Acid` ↔ `Breaks/Drum&Bass` — PARTIALLY DISTINCT; rhythm=DISJOINT;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Rolling Acid` ↔ `Breaks/Footwork` — PARTIALLY DISTINCT; rhythm=DISJOINT;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Rolling Acid` ↔ `Breaks/UK Garage` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=DISJOINT;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Rolling Acid` ↔ `Chip/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=DISJOINT;negative=OVERLAP.
- `Acid/Rolling Acid` ↔ `Darksynth/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Rolling Acid` ↔ `Drum&Bass/BASE` — PARTIALLY DISTINCT; rhythm=DISJOINT;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Acid/Rolling Acid` ↔ `Rave/Psytrance` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=SAME.
- `Acid/Rolling Acid` ↔ `UK Garage/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=DISJOINT;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/BASE` ↔ `Chip/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/BASE` ↔ `Drum&Bass/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/BASE` ↔ `Electro/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/BASE` ↔ `House/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/BASE` ↔ `Rave/BASE` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/BASE` ↔ `Rave/Psytrance` — PARTIALLY DISTINCT; rhythm=OVERLAP;bass=DISJOINT;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- `Breaks/BASE` ↔ `Synthwave/BASE` — PARTIALLY DISTINCT; rhythm=DISJOINT;bass=OVERLAP;harmony=OVERLAP;phrase=OVERLAP;role=OVERLAP;transformation=OVERLAP;negative=OVERLAP.
- … 75 additional pairs are in the pairwise TSV.

## 8. One-dimensional profiles

- No pair differed on exactly one fully-observed positive structural axis.

## 9. Negative capacity

- Core-v1 structural GRID 8/32: **NEGATIVE CAPACITY / intentionally unsupported**.
- DEPTH realization/transformation magnitude: **causal/distinct frozen capacity**, not negative capacity.
- ROLE IDENTITY VIA DEPTH: **NEGATIVE CAPACITY in the frozen corpus**; 0/1,815 semantic secondaryRole identity reassignments are observed.
- DEPTH → supporting-role activity magnitude is **not** negative capacity: Synth-B active/inactive changes are observed in 15/1,815 matched triplets.
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
