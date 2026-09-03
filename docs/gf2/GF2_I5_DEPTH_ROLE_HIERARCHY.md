# GF2-I5 — DEPTH / ROLE HIERARCHY MAGNITUDE

## Status

Executable characterization of the authoritative `dev_0.9.10` base:

- base: `13a03f84f8eb650c90e3d0b64fd1fe61f873785b`
- checkpoint: GF2-I5
- production semantic delta: **NONE**
- frozen I3 semantics rewritten: **NO**

## Decision

GF2-I5 does **not** redefine or rewire the existing P1/P2/P3 axis.

The measured contract is:

```text
CURRENT DEPTH CAUSAL                  YES
CURRENT DEPTH DISTINCT                YES

DEPTH = ROLE HIERARCHY                NO
DEPTH -> SECONDARY ROLE               NO

ROLE-HIERARCHY CAPACITY VIA DEPTH     NOT PROVEN
                                       NEGATIVE CAPACITY
```

The existing axis is best described by the evidence as **realization / transformation magnitude**. It is not structurally redundant and must not be classified as a fake control.

## Why I5 is a characterization checkpoint

The previously planned positive I5 interpretation conflicted with already-frozen production semantics:

- `RealizationLevel::P1Canonical` requests canonical realization.
- `P2Variation` and `P3Transformation` change rhythm realization topology and optional/ghost material.
- frozen GF2-I3 intentionally allows P2 and P3 to select different phrase trajectories.

Rewriting those semantics merely to make `DEPTH = role hierarchy` would destroy already-proven musical causality. I5 therefore tests the role-hierarchy hypothesis instead of forcing it into production.

## RED evidence

Intentional RED workflow run:

- run: `33791682032`
- head: `37e1c773be5af8796a06a4c50eafd07bf9e4a1a6`
- failed assertions: exactly two instances of `RED: role-hierarchy DEPTH changes Synth-B participation`
- compile / harness setup: PASS
- `git diff --check`: PASS

The two failures correspond to the two controlled shipped-family comparisons. All diagnostic assertions surrounding them passed.

### Rhythm realization magnitude

Controlled archetype `415` (`sparse_fast_break`), identical deterministic request except P1/P2/P3:

| Family | Level | Structural | Secondary | Ghosts | Total | Topology | Primary anchors |
| --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| Lo-Fi | P1 | 11 | 0 | 0 | 11 | `29aef8ad` | preserved |
| Lo-Fi | P2 | 11 | 0 | 2 | 13 | `9c55186d` | preserved |
| Lo-Fi | P3 | 11 | 3 | 2 | 16 | `cc40a55d` | preserved |
| DnB | P1 | 16 | 0 | 0 | 16 | `698122e7` | preserved |
| DnB | P2 | 16 | 0 | 2 | 18 | `b2a040bc` | preserved |
| DnB | P3 | 16 | 2 | 2 | 20 | `adc005a1` | preserved |

Observed in both families:

```text
primary structural activity      SAME across P1/P2/P3
primary anchors                  PRESERVED
optional/ghost activity          INCREASES with P-level
realization topology             CHANGES with P-level
total realization activity       INCREASES with P-level
```

This is positive evidence for a realization/transformation-magnitude axis.

### I4 density remains independently causal

At fixed P2 on the same controlled archetype:

```text
Lo-Fi structural target result   11
DnB structural target result     16
spread                            5
required witness                 >= 4
```

Result: PASS. DEPTH characterization does not collapse the I4 density axis.

### Frozen I3 trajectory remains level-sensitive

At identical phrase law with only P-level changed:

```text
DevelopReturn   P2=6   P3=7
SparseDrift     P2=3   P3=8
```

Result: P2/P3 trajectories remain distinct exactly as frozen by I3.

## Role-hierarchy diagnostic

### Same genre/profile, only P1/P2/P3 changed

#### Lo-Fi

```text
P1 selected_role=2 materialized_role=2 participation=3 synthB_events=3
P2 selected_role=2 materialized_role=2 participation=3 synthB_events=3
P3 selected_role=2 materialized_role=2 participation=3 synthB_events=3
```

#### Techno

```text
P1 selected_role=1 materialized_role=1 participation=2 synthB_events=2
P2 selected_role=1 materialized_role=1 participation=2 synthB_events=2
P3 selected_role=1 materialized_role=1 participation=2 synthB_events=2
```

For both measured shipped families:

```text
secondary Synth-B role identity      SAME
secondary participation/admission    SAME
materialized Synth-B activity        SAME
rhythm realization topology          CHANGES
phrase trajectory                    CHANGES where I3 defines it
```

Therefore the measured DEPTH axis does not express supporting-role hierarchy.

## Separate secondary-role owner

At fixed `P2Variation`, shipped profiles select distinct secondary roles and the migration/materialization branch follows them:

| Family | profile.secondaryRole | materialized Synth-B role | participation signature |
| --- | --- | --- | ---: |
| Reggae | Chord | Chord | 1 |
| Techno | Melodic | Melodic | 2 |
| Lo-Fi | ChordWithMelodicFill | ChordWithMelodicFill | 3 |

This demonstrates that role identity/participation is not simply missing from the system. It has a separate semantic owner: `GenerationProfileView::secondaryRole`.

## Production decision

No natural existing seam was found where P1/P2/P3 already meant role admission or role hierarchy. Adding a new DEPTH dependency to the secondary-role branch would therefore create a new semantic meaning rather than connect an existing one.

GF2-I5 consequently makes **zero production changes**.

Do not:

- wire `RealizationLevel` into `secondaryRole` merely to satisfy the old I5 label;
- suppress or normalize the frozen I3 P2/P3 trajectory distinction;
- call DEPTH structurally redundant;
- infer a new musician-facing hierarchy control from this checkpoint.

A future product decision may retain the name DEPTH for realization depth, rename it, or introduce a distinct hierarchy axis if Gate B / G1 demonstrates a musical need. That decision is outside GF2-I5.

## Gate-B handoff

Gate B should measure the engine that actually exists:

```text
GENRE / RECIPE   -> musical vocabulary
DENSITY          -> structural activity amount
PHRASE LAW       -> temporal form
SECONDARY ROLE   -> supporting-role identity / participation
P1 / P2 / P3     -> realization / transformation magnitude
```

GF2-I5 therefore closes as a negative-capacity result for **role hierarchy via DEPTH**, while preserving DEPTH itself as a proven causal axis.
