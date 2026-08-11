# Harmony Atlas H6 — Human-reviewed R2 Phase 1 Decision

**Decision:** `RECOMMEND_R2_PHASE1`  
**Selected sweep:** `S00617`  
**Production impact:** none in H6

## Selected bounded bundle

```text
MULTI_BAR_CHORD_RHYTHM_IDENTITY
QUALITY_RENDERING_CONSUMPTION
SAME_CHORD_RETRIGGER_WITHOUT_HARMONIC_ADVANCE
TRIAD_POLARITY_OR_EXPLICIT_CONTEXT
```

Harmonic templates: **8**  
Rhythm grammars: **4**  
Macro families: **HELD, ASYMMETRIC_CHANGE, GAPPED_RETRIGGER, RETRIGGERED_COMP**

| Coverage | F3 | F5 | F6 |
|---|---:|---:|---:|
| Capability envelope | 98 | 18 | 465 |
| Bounded R2 proposal | 8 | 13 | 25 |

Exact F5 observations in proposal: **675**.  
Reference candidate payload: **148 B** (harmonic 96 B + rhythm 52 B).  
Capability complexity: **12 points**.

## Why not the raw count winner

`S00803` reaches 28 F6 but adds `CHORD_RHYTHM_BEYOND_GENERIC_4_BAR_CONTAINER`. It gains only **3** exact F6 over the selected bundle while crossing the generic 4-bar architecture boundary. It is held for phase 2 until production cost is measured.

## Harmonic R2 candidates

- `R2H_CADENCE_01` — CADENCE; family=Minor; events=3; encoding=ROOT_PATH_OVERLAY; provenance=Minor:034
- `R2H_FOUR_CHORD_LOOP_01` — FOUR_CHORD_LOOP; family=Major; events=4; encoding=ROOT_PATH_OVERLAY; provenance=Major:025
- `R2H_MODAL_LOOP_01` — MODAL_LOOP; family=Modal; events=6; encoding=NEW_GENERIC_TEMPLATE; provenance=Modal:041
- `R2H_TURNAROUND_01` — TURNAROUND; family=Major; events=5; encoding=NEW_GENERIC_TEMPLATE; provenance=Major:018
- `R2H_EXTENDED_PHRASE_01` — EXTENDED_PHRASE; family=Minor; events=8; encoding=NEW_GENERIC_TEMPLATE; provenance=Minor:040
- `R2H_THREE_CHORD_LOOP_01` — THREE_CHORD_LOOP; family=Minor; events=3; encoding=ROOT_PATH_OVERLAY; provenance=Minor:009
- `R2H_FOUR_CHORD_LOOP_02` — FOUR_CHORD_LOOP; family=Minor; events=4; encoding=NEW_GENERIC_TEMPLATE; provenance=Minor:011
- `R2H_CADENCE_02` — CADENCE; family=Minor; events=3; encoding=NEW_GENERIC_TEMPLATE; provenance=Minor:045

## Rhythm R2 candidates

- `R2R_HELD_PER_CHORD` — `HELD_PER_CHORD`; provenance-style=default
- `R2R_ASYMMETRIC_6_10` — `ASYMMETRIC_6_10`; provenance-style=pop
- `R2R_GAPPED_RETRIGGER` — `GAPPED_RETRIGGER`; provenance-style=hiphop2
- `R2R_RETRIGGERED_COMP` — `RETRIGGERED_COMP`; provenance-style=soul

`ASYMMETRIC_7_9` is not rejected musically; it is deferred because it ties 6/10 on measured phase1 coverage/cost but is closer to an even 8/8 split. The choice is diversity curation, not popularity weighting.

## Production boundary

H6 admits no production code. Actual compiled flash, linker map, DRAM, runtime CPU and hardware musical acceptance remain mandatory in separate production PRs.
