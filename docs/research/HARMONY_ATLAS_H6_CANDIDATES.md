# Harmony Atlas H6 — Curated Runtime Candidate Simulation

**Status:** generated R2-proposal research evidence; no production changes  
**Stage15 target:** `fc42763e7798866e61895bf1b8d62339ec59e0a7`

## Boundary

H6 separates a capability envelope from the bounded candidate vocabulary actually proposed. Envelope coverage is not runtime admission.

## Baseline

| Identity | Atlas unique | Current exact |
|---|---:|---:|
| F3 | 189 | 0 |
| F5 | 30 | 0 |
| F6 | 945 | 0 |

## Bundle comparison

| Bundle | Envelope F3/F5/F6 | Proposal F3/F5/F6 | Payload bytes | Complexity |
|---|---|---|---:|---:|
| `MINIMAL` | 98/12/285 | 4/7/6 | 61 | 10 |
| `BALANCED` | 98/18/465 | 8/9/18 | 132 | 12 |
| `WIDE` | 189/30/945 | 12/30/60 | 211 | 27 |

## Bounded candidate details

### MINIMAL

Capabilities:

```text
MULTI_BAR_CHORD_RHYTHM_IDENTITY
QUALITY_RENDERING_CONSUMPTION
TRIAD_POLARITY_OR_EXPLICIT_CONTEXT
```

Harmonic candidates:

- `R2H_CADENCE_01` — CADENCE; family=Minor; events=3; encoding=ROOT_PATH_OVERLAY; provenance=Minor:034
- `R2H_FOUR_CHORD_LOOP_01` — FOUR_CHORD_LOOP; family=Major; events=4; encoding=ROOT_PATH_OVERLAY; provenance=Major:025
- `R2H_MODAL_LOOP_01` — MODAL_LOOP; family=Modal; events=6; encoding=NEW_GENERIC_TEMPLATE; provenance=Modal:041
- `R2H_TURNAROUND_01` — TURNAROUND; family=Major; events=5; encoding=NEW_GENERIC_TEMPLATE; provenance=Major:018

Rhythm candidates:

- `R2R_HELD_PER_CHORD` — source-neutral `HELD_PER_CHORD`; provenance-style=default; F5=2
- `R2R_ASYMMETRIC_7_9` — source-neutral `ASYMMETRIC_7_9`; provenance-style=pop2; F5=5

### BALANCED

Capabilities:

```text
MULTI_BAR_CHORD_RHYTHM_IDENTITY
QUALITY_RENDERING_CONSUMPTION
SAME_CHORD_RETRIGGER_WITHOUT_HARMONIC_ADVANCE
TRIAD_POLARITY_OR_EXPLICIT_CONTEXT
```

Harmonic candidates:

- `R2H_CADENCE_01` — CADENCE; family=Minor; events=3; encoding=ROOT_PATH_OVERLAY; provenance=Minor:034
- `R2H_FOUR_CHORD_LOOP_01` — FOUR_CHORD_LOOP; family=Major; events=4; encoding=ROOT_PATH_OVERLAY; provenance=Major:025
- `R2H_MODAL_LOOP_01` — MODAL_LOOP; family=Modal; events=6; encoding=NEW_GENERIC_TEMPLATE; provenance=Modal:041
- `R2H_TURNAROUND_01` — TURNAROUND; family=Major; events=5; encoding=NEW_GENERIC_TEMPLATE; provenance=Major:018
- `R2H_EXTENDED_PHRASE_01` — EXTENDED_PHRASE; family=Minor; events=8; encoding=NEW_GENERIC_TEMPLATE; provenance=Minor:040
- `R2H_THREE_CHORD_LOOP_01` — THREE_CHORD_LOOP; family=Minor; events=3; encoding=ROOT_PATH_OVERLAY; provenance=Minor:009
- `R2H_FOUR_CHORD_LOOP_02` — FOUR_CHORD_LOOP; family=Minor; events=4; encoding=NEW_GENERIC_TEMPLATE; provenance=Minor:011
- `R2H_CADENCE_02` — CADENCE; family=Minor; events=3; encoding=NEW_GENERIC_TEMPLATE; provenance=Minor:045

Rhythm candidates:

- `R2R_HELD_PER_CHORD` — source-neutral `HELD_PER_CHORD`; provenance-style=default; F5=2
- `R2R_ASYMMETRIC_7_9` — source-neutral `ASYMMETRIC_7_9`; provenance-style=pop2; F5=5
- `R2R_GAPPED_RETRIGGER` — source-neutral `GAPPED_RETRIGGER`; provenance-style=hiphop2; F5=2

### WIDE

Capabilities:

```text
ADDITIONAL_CHORD_QUALITY_VOCABULARY
CHORD_RHYTHM_BEYOND_GENERIC_4_BAR_CONTAINER
GENERIC_ALTERED_DEGREE_REACHABILITY
MORE_THAN_8_HARMONIC_ONSETS
MULTI_BAR_CHORD_RHYTHM_IDENTITY
QUALITY_RENDERING_CONSUMPTION
SAME_CHORD_RETRIGGER_WITHOUT_HARMONIC_ADVANCE
SOURCE_HARMONIC_FORM_GT8
TRIAD_POLARITY_OR_EXPLICIT_CONTEXT
```

Harmonic candidates:

- `R2H_CADENCE_01` — CADENCE; family=Minor; events=3; encoding=ROOT_PATH_OVERLAY; provenance=Minor:034
- `R2H_TURNAROUND_01` — TURNAROUND; family=Major; events=4; encoding=ROOT_PATH_OVERLAY; provenance=Major:026
- `R2H_MODAL_LOOP_01` — MODAL_LOOP; family=Modal; events=5; encoding=NEW_GENERIC_TEMPLATE; provenance=Modal:023
- `R2H_EXTENDED_PHRASE_01` — EXTENDED_PHRASE; family=Major; events=6; encoding=NEW_GENERIC_TEMPLATE; provenance=Major:033
- `R2H_FOUR_CHORD_LOOP_01` — FOUR_CHORD_LOOP; family=Minor; events=4; encoding=ROOT_PATH_OVERLAY; provenance=Minor:016
- `R2H_ALTERED_COLOR_01` — ALTERED_COLOR; family=Modal; events=8; encoding=NEW_GENERIC_TEMPLATE; provenance=Modal:060
- `R2H_THREE_CHORD_LOOP_01` — THREE_CHORD_LOOP; family=Minor; events=3; encoding=ROOT_PATH_OVERLAY; provenance=Minor:009
- `R2H_FOUR_CHORD_LOOP_02` — FOUR_CHORD_LOOP; family=Minor; events=4; encoding=NEW_GENERIC_TEMPLATE; provenance=Minor:011
- `R2H_CADENCE_02` — CADENCE; family=Minor; events=3; encoding=NEW_GENERIC_TEMPLATE; provenance=Minor:045
- `R2H_CADENCE_03` — CADENCE; family=Minor; events=4; encoding=NEW_GENERIC_TEMPLATE; provenance=Minor:038
- `R2H_CADENCE_04` — CADENCE; family=Minor; events=9; encoding=NEW_GENERIC_TEMPLATE; provenance=Minor:023
- `R2H_CADENCE_05` — CADENCE; family=Major; events=3; encoding=ROOT_PATH_OVERLAY; provenance=Major:031

Rhythm candidates:

- `R2R_HELD_PER_CHORD` — source-neutral `HELD_PER_CHORD`; provenance-style=default; F5=6
- `R2R_ASYMMETRIC_6_10` — source-neutral `ASYMMETRIC_6_10`; provenance-style=pop; F5=6
- `R2R_ASYMMETRIC_7_9` — source-neutral `ASYMMETRIC_7_9`; provenance-style=pop2; F5=6
- `R2R_GAPPED_RETRIGGER` — source-neutral `GAPPED_RETRIGGER`; provenance-style=hiphop2; F5=6
- `R2R_RETRIGGERED_COMP` — source-neutral `RETRIGGERED_COMP`; provenance-style=soul; F5=6

## Cost boundary

Reference payload byte counts are measured against the H6 reference encoding only. They are not compiled firmware flash/RAM measurements. Actual linker/map/DRAM impact is mandatory in a later production integration PR.

## Recommendation

Status: **BOOTSTRAP_UNREVIEWED**  
Bundle: **not selected in bootstrap**

H6 never converts source incidence or style materialization multiplicity into runtime probability.
