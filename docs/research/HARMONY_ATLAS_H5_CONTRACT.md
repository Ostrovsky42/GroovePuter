# Harmony Atlas H5 Representability Contract

**Status:** normative research contract for H5  
**Runtime impact:** none

## Evidence and target

H5 consumes exact frozen research evidence:

```text
H1  4783be1f77784a978f2239d8527214b3912eaf262e92430bff755679fec0734e
H4  68c51f4ca8827167f2b6ff12543ed226164c9c8aa691816caac9102058849e81
```

and compares it with `TARGET_CONTRACT_EVIDENCE`:

```text
Ostrovsky42/GroovePuter @ fc42763e7798866e61895bf1b8d62339ec59e0a7
```

Seven target source blobs are verified before analysis. H5 does not merge, copy or mutate the target code.

## Exact identity summary

The unit of harmonic support is always `LogicalProgressionDefinition`.

```text
logical definitions       190
unique H4 F3 identities   189
exact current F3            0

unique H4 F5 identities    30
exact current F5            0

unique H4 F6 identities   945
exact current F6            0
```

`0 exact` is not equivalent to `0 useful overlap`. H5 separately reports field capacity, root-path overlap and grid compatibility.

## Harmonic representability levels

These levels are intentionally non-equivalent:

1. `RAW_FIELD_SHAPE_ENCODEABLE` — degree/root-offset/event-count fit and no completely absent quality class. Context-dependent `Triad` is allowed at this level only.
2. `EXACT_QUALITY_FIELD_ENCODEABLE` — every H1 quality has an unambiguous target `ChordQuality` label; generic `Triad` does not independently preserve MAJOR/MINOR polarity.
3. `CURRENT_PROGRESSION_CATALOG_EXACT` — an existing frozen Stage 15 progression grammar can emit the complete ordered degree/alteration/quality sequence.
4. `CURRENT_ROOT_PATH_EXACT_IGNORING_QUALITY` — diagnostic only; this is never F3 equality.
5. `AUDIBLE_F3_EXACT` — current target must also consume the quality distinction during tonal materialization.

The frozen target validates `ChordQuality`, but its TonalMaterializer calculates pitch from `degree + rootOffsetSemitones` and does not branch/project by `ChordQuality`. Therefore enum presence must not be called audible quality coverage.

## Quality normalization boundary

H5 keeps H1 loss-awareness. In particular:

```text
I7     != Idom7
I9     != a silently inferred dominant/major ninth
```

Unambiguous target-label matches include explicit `Minor7`, `Major7`, `Dominant7`, `Sus4`, `Minor9`, `Major9` and `Diminished` semantics.

Plain MAJOR/MINOR triads are `CONTEXT_DEPENDENT_TRIAD`: the target has only `ChordQuality::Triad`, so the quality field itself does not preserve triad polarity. `sus2`, power-5, 6/69, add9, generic 7/9 and flat-fifth quality remain gaps unless a later production design explicitly represents them.

## Altered-degree boundary

`rootOffsetSemitones` can numerically carry the observed ±1 source alterations and TonalMaterializer consumes that field. This field capacity is not current catalog reachability: frozen `realizeChordProgression()` permits nonzero offsets only through specific hard-coded progression grammars. H5 therefore reports altered-degree support separately from exact catalog coverage.

## Rhythm representability

F5 exactness is not merely grid compatibility.

The current target has a 16-step one-bar `ChordRhythmPlan`. H5 compares exact H4 rational durations and actions against fixed current ChordRhythm masks.

```text
CHORD_ONSET      may map to a target onset
REST             may map to silence
CHORD_RETRIGGER  cannot be replaced by a new harmonic advance
```

All 950 H4 observations are compatible with the current 16th-grid precision, so there is no finer-grid capability request from this source. Exact F5 still remains zero because source phrases are longer than the live one-bar contract; 380 style observations additionally require same-chord retrigger semantics.

## Capability ranking

Deferred capability support is counted by logical source definition. Style observations are secondary diagnostics only; key projections are never support.

Capability support sets overlap and **must not be summed**. Rows with equal logical support are support ties; their displayed deterministic order is not an implementation-priority decision. H6 must consider incremental coverage, complexity, memory/runtime cost and musical value before choosing a production batch.

## H5/H6 boundary

H5 may measure and rank gaps but may not select runtime candidates. H6 may propose a small human-reviewed R2 vocabulary/capability batch. Production integration remains a later separate PR.
