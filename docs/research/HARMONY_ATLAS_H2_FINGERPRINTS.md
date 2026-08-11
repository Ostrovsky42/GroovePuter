# Harmony Atlas H2 Structural Fingerprints / Dedup

**Status:** generated research evidence / H2 checkpoint  
**Source:** `ldrolez/free-midi-chords @ baf0896694de6b09ac00250722f2414202e668ed`  
**Evidence class:** `EDITORIAL_CATALOG_EVIDENCE`  
**Runtime impact:** none

## Dependency boundary

H2 consumes the verified H1 normalized representation. It does not re-interpret raw Roman notation and it does not remove source definitions.

Frozen H1 normalized JSON SHA-256: `4783be1f77784a978f2239d8527214b3912eaf262e92430bff755679fec0734e`.

The F0-F6 namespace is fully registered, but only levels whose prerequisite evidence exists are computed in H2:

| Level | Name | Status | Unique classes | Duplicate groups | Surplus duplicates |
|---|---|---|---:|---:|---:|
| F0 | SourceIdentity | COMPUTED | 190 | 0 | 0 |
| F1 | TranspositionInvariant | COMPUTED | 189 | 1 | 1 |
| F2 | RootSequence | COMPUTED | 172 | 17 | 18 |
| F3 | RootQualitySequence | COMPUTED | 189 | 1 | 1 |
| F4 | FunctionalClassSequence | DEFERRED (H3_FUNCTIONAL_ANALYSIS) | — | — | — |
| F5 | ChordRhythmIdentity | DEFERRED (H4_CHORD_RHYTHM_EXTRACTION) | — | — | — |
| F6 | CombinedIdentity | DEFERRED (F3_COMPUTED, F5_COMPUTED) | — | — | — |

## Operational fingerprint meanings

```text
F0 SourceIdentity
   source id + family + notation class + exact source-definition SHA-256

F1 TranspositionInvariant
   family + notation class + exact ordered Roman/rest sequence; source id and tags excluded

F2 RootSequence
   ordered degree + alteration + rests; quality/family/spelling excluded

F3 RootQualitySequence
   ordered degree + alteration + semantic quality + rests; source spelling/family excluded
```

F3 intentionally uses H1 semantic quality fields rather than raw suffix spelling. Thus notation variants may collapse only when the preserved H1 semantics are equal; ambiguous `7/9` remain distinct from explicit `dom7`/`M7` through `seventh_flavor`.

## Deferred levels

- **F4 FunctionalClassSequence** — `DEFERRED`: Tonic/predominant/dominant/borrowed labels do not exist before H3 and must not be guessed in H2.
- **F5 ChordRhythmIdentity** — `DEFERRED`: H1 contains harmonic event order but not the timing/rest/continuation evidence required by F5.
- **F6 CombinedIdentity** — `DEFERRED`: Combined harmonic+rhythm identity cannot exist until F5 is available.

## Over-collapse diagnostics

- F2 root-only groups that split into multiple F3 quality classes: **16**.
- F3 semantic groups spanning multiple F1 source-notation/family identities: **0**.
- F1 duplicate groups carrying multiple tag sets: **1**.

These are diagnostics, not automatic merge rules.

## Forbidden naive equivalence checks

Near-relation **pairs** detected but explicitly not deduplicated: **14**.
Connected near-relation components: **7**.
- `CYCLIC_ROTATION`: **14 pair edges**, **7 connected components**

Components (report-only):

- `CYCLIC_ROTATION:C001` — 2 definitions / 1 pair edges: `Major:007`, `Major:032`
- `CYCLIC_ROTATION:C002` — 2 definitions / 1 pair edges: `Major:008`, `Major:030`
- `CYCLIC_ROTATION:C003` — 3 definitions / 3 pair edges: `Major:015`, `Major:040`, `Major:044`
- `CYCLIC_ROTATION:C004` — 4 definitions / 6 pair edges: `Major:019`, `Major:038`, `Major:045`, `Major:047`
- `CYCLIC_ROTATION:C005` — 2 definitions / 1 pair edges: `Major:023`, `Major:046`
- `CYCLIC_ROTATION:C006` — 2 definitions / 1 pair edges: `Minor:038`, `Minor:051`
- `CYCLIC_ROTATION:C007` — 2 definitions / 1 pair edges: `Minor:046`, `Minor:050`

Pair examples (report-only):

- `Major:007` ↔ `Major:032` — CYCLIC_ROTATION (1 steps), **not dedup-equivalent**.
- `Major:015` ↔ `Major:044` — CYCLIC_ROTATION (1 steps), **not dedup-equivalent**.
- `Major:019` ↔ `Major:045` — CYCLIC_ROTATION (1 steps), **not dedup-equivalent**.
- `Major:023` ↔ `Major:046` — CYCLIC_ROTATION (1 steps), **not dedup-equivalent**.
- `Major:045` ↔ `Major:047` — CYCLIC_ROTATION (1 steps), **not dedup-equivalent**.
- `Minor:046` ↔ `Minor:050` — CYCLIC_ROTATION (1 steps), **not dedup-equivalent**.
- `Major:008` ↔ `Major:030` — CYCLIC_ROTATION (2 steps), **not dedup-equivalent**.
- `Major:015` ↔ `Major:040` — CYCLIC_ROTATION (2 steps), **not dedup-equivalent**.
- `Major:019` ↔ `Major:047` — CYCLIC_ROTATION (2 steps), **not dedup-equivalent**.
- `Major:038` ↔ `Major:045` — CYCLIC_ROTATION (2 steps), **not dedup-equivalent**.

## Dedup policy

- every duplicate count names its fingerprint level;
- H2 removes **zero** definitions;
- no representative/canonical winner is selected;
- source tags remain attached to source definitions and do not participate in F1-F3 structural keys;
- cyclic rotations are near-relations, never equality;
- repeated longer forms are near-relations, never equality;
- pair-edge counts are never presented as counts of independent near-duplicate families;
- source incidence never becomes runtime probability;
- F4/F5/F6 remain unavailable until their prerequisite research stages exist;
- production vocabulary admission remains a later human-reviewed R2 step.

Next stage: **H3 functional analysis**. H3 may enable F4, but must not retroactively rewrite F0-F3 identities.
