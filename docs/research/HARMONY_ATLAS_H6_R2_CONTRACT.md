# Harmony Atlas H6 R2 Phase 1 Contract

**Status:** normative research decision contract  
**Evidence level:** `R2_CURATED_RUNTIME_CANDIDATE_PROPOSAL`  
**Production runtime admission:** false

## Frozen dependencies

H6 consumes only the frozen research evidence:

```text
H1  4783be1f77784a978f2239d8527214b3912eaf262e92430bff755679fec0734e
H3  f15d127722691789f6c1d1a003028da755e06a1cba1d2c1014c1232697cc456d
H4  68c51f4ca8827167f2b6ff12543ed226164c9c8aa691816caac9102058849e81
H5  df6b05edafd7d0fee34714fe957637ce3c1f5e392829fcfd66bdec7746395060
Stage15 target  fc42763e7798866e61895bf1b8d62339ec59e0a7
```

H6 does not rewrite F0-F6 or mutate the target runtime.

## Coverage boundary

Two values are always kept separate:

```text
CAPABILITY ENVELOPE
  potential exact coverage if every eligible generic template existed

BOUNDED R2 PROPOSAL
  exact coverage of the finite candidate vocabulary actually proposed by H6
```

The envelope is never runtime admission and must not be presented as achieved production coverage.

Baseline:

```text
F3 unique 189, current exact 0
F5 unique  30, current exact 0
F6 unique 945, current exact 0
```

## Exhaustive bounded search

H6 evaluates exactly **7936** candidate bundles before the human decision.

Phase-1 constraints:

```text
harmonic templates <= 8
rhythm grammars <= 4
capability complexity <= 16 points
reference candidate payload <= 160 bytes
rhythm grammars must occupy distinct macro families
```

`capability complexity points` are only a deterministic research-search heuristic. They are **not** an engineering estimate of implementation complexity, CPU, flash or RAM.

`pop` and `pop2` both belong to `ASYMMETRIC_CHANGE`; a phase-1 package may not consume two limited rhythm slots on both near-neighbors.

The predeclared count objective after the diversity gate is:

```text
MAX F6 -> MAX F5 -> MAX F3 -> MIN complexity -> MIN reference payload
```

This objective produces evidence for review; it does not automatically admit the top row.

## Human-reviewed R2 decision

Selected sweep: **`S00617`**.

Capabilities:

```text
QUALITY_RENDERING_CONSUMPTION
TRIAD_POLARITY_OR_EXPLICIT_CONTEXT
MULTI_BAR_CHORD_RHYTHM_IDENTITY
SAME_CHORD_RETRIGGER_WITHOUT_HARMONIC_ADVANCE
```

The bundle intentionally stays within the already established generic limits:

```text
phrase/form <= 4 bars
harmonic events <= 8
no additional quality vocabulary
no generic altered-degree expansion
```

Measured coverage:

```text
capability envelope: F3 98 / F5 18 / F6 465
bounded R2 proposal: F3 8 / F5 13 / F6 25
exact F5 observations in proposal: 675
```

Reference candidate payload: **148 B** (`96 B` harmonic + `52 B` rhythm). This is a research encoding measurement only. It is not compiled flash, linker size, DRAM, stack, heap or CPU cost.

## Mandatory quality-rendering feasibility gate

The frozen Stage15 target has a monophonic tonal output shape:

```text
TonalMaterializationPlan
  onsetCount
  onsetSteps[16]
  midiNotes[16]      // one MIDI note per role onset
```

Pinned target blob:

```text
src/generation/tonal/tonal_materializer.h
370147883faaefcb24f4ce20b6858dec6144ab50
```

The live chord path adapts this plan directly into one `SynthPattern`. Therefore `QUALITY_RENDERING_CONSUMPTION` is **not** approved as a trivial enum-to-pitch patch. H6 does not decide whether audible chord quality should be implemented through bounded polyphonic voicing, a synth-owned chord/quality parameter, or another compatible representation.

Before any harmonic R2 candidate is admitted to production, P1 must prove an explicit bounded audible-quality contract that:

- preserves ChordRhythm onset/continuation ownership;
- keeps Tonal Projector as the sole absolute-pitch projection authority or defines a reviewed compatible extension;
- does not create hidden extra rhythm events;
- has measured flash, internal DRAM, stack/heap and runtime CPU cost;
- has host tests and physical musical acceptance for major/minor polarity and the admitted quality subset.

Until this gate passes, the H6 F3/F6 envelope and bounded proposal are **hypothetical post-capability exactness**, not achieved runtime coverage.

## Candidate vocabulary

Candidate IDs displayed inside the exploratory `MINIMAL` / `BALANCED` / `WIDE` simulation are **bundle-local labels**, not cross-bundle identities. The same local label may refer to a different simulated payload in another bundle. Only the IDs frozen below by the human-reviewed `S00617` decision are stable H6 R2 proposal identities.

Harmonic R2 proposal contains exactly eight generic identities:

```text
R2H_CADENCE_01           provenance Minor:034
R2H_FOUR_CHORD_LOOP_01   provenance Major:025
R2H_MODAL_LOOP_01        provenance Modal:041
R2H_TURNAROUND_01        provenance Major:018
R2H_EXTENDED_PHRASE_01   provenance Minor:040
R2H_THREE_CHORD_LOOP_01  provenance Minor:009
R2H_FOUR_CHORD_LOOP_02   provenance Minor:011
R2H_CADENCE_02           provenance Minor:045
```

Runtime names are generic. Source IDs are evidence provenance only and may not become runtime style labels or probabilities.

Rhythm R2 proposal contains exactly four macro-distinct grammars:

```text
R2R_HELD_PER_CHORD
R2R_ASYMMETRIC_6_10
R2R_GAPPED_RETRIGGER
R2R_RETRIGGERED_COMP
```

`ASYMMETRIC_7_9` remains a valid phase-2 candidate. It ties `ASYMMETRIC_6_10` on measured phase-1 coverage and reference cost; `6/10` is retained only as the more timing-distinct representative from even `8/8`. This is a human diversity decision, not source popularity evidence.

## Deliberately deferred

Raw count winner `S00803` reaches **28** exact F6 versus **25** for `S00617`, but requires `CHORD_RHYTHM_BEYOND_GENERIC_4_BAR_CONTAINER`. The extra architecture surface buys only three F6 identities in the bounded phase-1 proposal, so it is `HOLD_PHASE2` until production cost is measured.

Also deferred:

```text
MORE_THAN_8_HARMONIC_ONSETS
GENERIC_ALTERED_DEGREE_REACHABILITY
ADDITIONAL_CHORD_QUALITY_VOCABULARY
SOURCE_HARMONIC_FORM_GT8
ASYMMETRIC_7_9_SECOND_VARIANT
```

## Production integration boundary

H6 changes no production files and admits no runtime code. Follow-up production work must remain separate and should be split by ownership:

```text
P1 QUALITY_RENDERING_FEASIBILITY_AND_TRIAD_POLARITY
P2 MULTI_BAR_CHORD_RHYTHM_UP_TO_4_BARS
P3 SAME_CHORD_RETRIGGER_WITHOUT_HARMONIC_ADVANCE
P4 BOUNDED_8_HARMONIC_PLUS_4_RHYTHM_R2_VOCABULARY
```

P4 cannot start harmonic admission before P1 proves the audible-quality feasibility gate. Before any R2 candidate reaches firmware, the production PR must measure compiled flash/linker map, internal DRAM, stack/heap headroom, runtime CPU and musical/hardware acceptance. Source incidence, style multiplicity and key materializations remain forbidden as runtime probability weights.
