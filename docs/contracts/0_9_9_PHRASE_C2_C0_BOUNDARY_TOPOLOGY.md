# 0.9.9 PHRASE-C2-C0-R — Corrected I1 Boundary-Topology Replay

Status: **CHARACTERIZATION REPLAY CANDIDATE**

## Purpose

Replay the frozen C2-C0 cross-bar melodic boundary characterization on the authoritative H1-F1 -> W1R -> H2R -> P1R -> I1 ancestry, without changing production policy. This is the final semantic gate before production C2.

The required result is:

```text
A_ONSET          reachable
A_CONTINUATION   unreachable
A_OVERLAP        unreachable
```

Only that result authorizes an A-ONSET-only C2 producer.

## Exact ancestry

```text
H1-F1  eae498dc5b6377ddc4a45c2e62a7c33afab92e6c
W1R    329dcb91e40feb734182f437a8a50f2b61b40fd2
H2R    06ffcdc01969eb73b6bd8a452cc9b261a5b51e28
P1R    a413561136b274a1b16b079f95f8d3ce3353fac5
I1     fb30bbb93f739d8b3e4514ae931cbc73fc8eeb09
```

Exact C2-C0-R base: `fb30bbb93f739d8b3e4514ae931cbc73fc8eeb09`.

Validated references only:

```text
original C2-C0     cd0e77a8acdf62e449792964f14899cfa118120b
P1R-level replay   74cc88a8fd7cb5995d949a25eaf8baaa2c4d39ed
```

Neither reference ancestry is merge/rebase input for the authoritative line.

## Research-only owner set

This checkpoint changes no production source. Its delta is limited to:

```text
.github/workflows/0_9_9_phrase_c2_c0_boundary_topology.yml
docs/contracts/0_9_9_PHRASE_C2_C0_BOUNDARY_TOPOLOGY.md
tests/run_0_9_9_phrase_c2_c0_tests.sh
tests/test_0_9_9_phrase_c2_c0_boundary_topology.cpp
tests/test_0_9_9_phrase_c2_c0_corpus.cpp
```

The two C++ oracles are reused as exact Git blobs:

```text
boundary topology  840cbbfd6edb598ca64f136d2af2ec3665cb8450
exhaustive corpus  39de5dc185c87237dd5083a6993fa611a673fedc
```

`src/` must remain byte-identical to frozen I1. In particular, `src/ui/pages/synth_sequencer_page.cpp` is not modified in C2-C0-R.

I1 remains lifetime-inert before production C2: the P1R lifetime carrier is present/all-false and no C2/R1 producer or consumer semantics are permitted in I1 owners.

## Frozen corpus result to reproduce

```text
request tuples                         75,496,320
active settings                               288
legacy settings                                  0
admitted phrases                       41,418,120
adjacent intra-phrase boundaries       96,729,660
unique adjacent boundary signatures       294,725
pure-melodic boundaries                52,296,930
pure-melodic, non-empty incoming       41,306,411

A_ONSET          raw 17,530,610   unique 30,408
A_CONTINUATION   raw 0            unique 0
A_OVERLAP        raw 0            unique 0
B                raw 22,115,006   unique 33,632
H                raw  9,276,932   unique 80,113
N0               raw  1,644,348   unique  1,408
N1               raw 21,660,687   unique 87,948
N3               raw    909,477   unique  6,091
OTHER            raw 23,592,600   unique 55,125

N2 terminal controls                  41,418,120
unique N2 terminal signatures             104,104

signature collision groups               290,961
collision replays                         729,398
profile-diverse collision groups          147,476
```

`294,725` is the adjacent-boundary-signature denominator. `104,104` is the terminal N2-signature denominator.

## C2 / R1 boundary

If this replay is GREEN and the topology is unchanged, the next production step is strictly:

```text
C2 = A_ONSET producer only
```

Do not implement `A_CONTINUATION` or `A_OVERLAP`. R1 remains a separate later runtime consumer checkpoint.

## I1 UI outcome contract

C2-C0-R does not modify UI, but must preserve the already accepted I1 distinction:

```text
ACCEPTED
TYPED REJECTION
EXECUTION FAILURE
```

Typed rejection is a domain outcome, not an execution error. C2/R1 must not collapse these classes.

## Hardware list

None required. This checkpoint is host characterization only. Inherited CI may compile Cardputer ADV and SEQTRAK targets as regression evidence, but C2-C0-R claims no hardware-listening result.

## Wiring

No new wiring. Existing Cardputer ADV and MIDI wiring is unchanged.

## Build / flash steps

Focused replay:

```sh
bash tests/run_0_9_9_phrase_c2_c0_tests.sh
```

No firmware flashing is required for this checkpoint.

The runner verifies:

- zero `src/` delta from exact I1 FINAL;
- exact old oracle blob identity;
- finalized H1-F1 accessor adapter presence;
- frozen I1 C2/R1 semantic firewall;
- deterministic witness replay;
- exhaustive GCC corpus parity;
- Clang equivalence;
- ASan/UBSan smoke coverage;
- existing physical-gate source characterization;
- frozen I1/P1R/D2 compatibility.

## Expected behavior

Focused output must reproduce exactly:

```text
C2-C0 DECISION A: NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE
C2-C0 NEXT PRODUCER SCOPE: A-ONSET ONLY
```

and retain:

```text
A_ONSET          17,530,610 / 30,408 unique
A_CONTINUATION   0 / 0
A_OVERLAP        0 / 0
```

Any changed topology count, witness, classification, or deterministic output is a replay failure. Do not update expected counts to make changed semantics pass.

## Troubleshooting

- `src/` firewall failure: stop; this research checkpoint may not change production code.
- Oracle blob mismatch: restore the exact frozen blob rather than editing the oracle.
- Corpus count mismatch: stop production C2 and investigate the I1-ancestry semantic difference.
- H1-F1 accessor failure: inspect the frozen P1R-local adapter; do not restore finite-plan modulo fallback.
- I1/P1R/D2 compatibility failure: C2 is blocked until the inherited integration regression is resolved.
- Sanitizer-only exhaustive resource issue: keep sanitizers on the frozen smoke domain; exhaustive optimized GCC/Clang output remains the cardinality authority.

## Acceptance checklist

- [ ] branch is `research/20260828-07-0.9.9-phrase-c2-c0-r-corrected-i1-replay`
- [ ] merge-base is exact I1 `fb30bbb93f739d8b3e4514ae931cbc73fc8eeb09`
- [ ] exactly one replay commit above I1
- [ ] exactly five changed research/test/docs/workflow files
- [ ] zero `src/` delta from I1
- [ ] `synth_sequencer_page.cpp` unchanged
- [ ] both C++ oracle blobs byte-identical to #395/#403
- [ ] request tuples = 75,496,320
- [ ] admitted phrases = 41,418,120
- [ ] adjacent boundaries = 96,729,660
- [ ] unique adjacent signatures = 294,725
- [ ] A_ONSET = 17,530,610 / 30,408 unique
- [ ] A_CONTINUATION = 0
- [ ] A_OVERLAP = 0
- [ ] terminal N2 = 41,418,120 / 104,104 unique
- [ ] deterministic GCC / Clang / ASan / UBSan gates GREEN
- [ ] frozen I1/P1R/D2 focused suite GREEN
- [ ] inherited exact-head required workflows terminal GREEN

## Decision gate

Only after one exact final SHA has the required terminal-GREEN matrix:

```text
PHRASE-C2-C0-R FINAL

DECISION A — NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REPRODUCED
NEXT PRODUCER SCOPE — A-ONSET ONLY

C2-C0-R FREEZE COMPLETE
```

Then, and only then, branch production C2 from that exact frozen SHA.
