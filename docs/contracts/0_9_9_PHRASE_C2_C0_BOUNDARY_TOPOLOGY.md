# 0.9.9 PHRASE-C2-C0-R — Corrected Boundary-Topology Replay

Status: **CHARACTERIZATION REPLAY CANDIDATE**

## Purpose

Replay the frozen C2-C0 cross-bar melodic boundary characterization on the authoritative H1-F1 -> W1R -> H2R -> P1R ancestry, without changing production policy. The sole question is whether finalized P1R, including its corrected H1-F1 progression-source accessor adapter, reproduces the already-validated C2-C0 topology and corpus results.

## Exact ancestry

Authoritative base:

```text
H1-F1  eae498dc5b6377ddc4a45c2e62a7c33afab92e6c
W1R    329dcb91e40feb734182f437a8a50f2b61b40fd2
H2R    06ffcdc01969eb73b6bd8a452cc9b261a5b51e28
P1R    a413561136b274a1b16b079f95f8d3ce3353fac5
```

Exact C2-C0-R base: `a413561136b274a1b16b079f95f8d3ce3353fac5`.

Validated old C2-C0 reference only: `cd0e77a8acdf62e449792964f14899cfa118120b`.

The old `016bcd6 -> cd0e77a -> 2f9b6c7 -> a847cb8` ancestry is an oracle only. It must not be merged or rebased into the authoritative line.

## Research-only owner set

C2-C0-R changes no production source. The replay delta is limited to:

```text
.github/workflows/0_9_9_phrase_c2_c0_boundary_topology.yml
docs/contracts/0_9_9_PHRASE_C2_C0_BOUNDARY_TOPOLOGY.md
tests/run_0_9_9_phrase_c2_c0_tests.sh
tests/test_0_9_9_phrase_c2_c0_boundary_topology.cpp
tests/test_0_9_9_phrase_c2_c0_corpus.cpp
```

The two C++ characterization oracles are the exact old Git blobs:

```text
boundary topology  840cbbfd6edb598ca64f136d2af2ec3665cb8450
exhaustive corpus  39de5dc185c87237dd5083a6993fa611a673fedc
```

`src/` must remain byte-identical to finalized P1R.

## Frozen characterization result to reproduce

```text
A_ONSET         reachable
A_CONTINUATION  zero
A_OVERLAP       zero

next producer scope:
A-ONSET ONLY
```

Authoritative old attempt-0 corpus:

```text
request tuples                         75,496,320
active settings                               288
legacy settings                                  0
admitted phrases                       41,418,120
adjacent intra-phrase boundaries       96,729,660
unique adjacent boundary signatures       294,725
pure-melodic boundaries                52,296,930
pure-melodic, non-empty incoming       41,306,411

A_ONSET      raw 17,530,610   unique 30,408
A_CONTINUATION raw 0          unique 0
A_OVERLAP      raw 0          unique 0
B            raw 22,115,006   unique 33,632
H            raw  9,276,932   unique 80,113
N0           raw  1,644,348   unique  1,408
N1           raw 21,660,687   unique 87,948
N3           raw    909,477   unique  6,091
OTHER        raw 23,592,600   unique 55,125

N2 terminal controls 41,418,120
unique N2 terminal signatures 104,104

signature collision groups             290,961
collision replays                       729,398
profile-diverse collision groups        147,476
```

`294,725` and `104,104` are different denominators: the first is unique adjacent-boundary signatures; the second is unique terminal N2 controls.

## Why corrected replay matters

Finalized H1-F1 exposes `chordProgressionEventAt(...) -> ChordProgressionEventResult`. Finalized P1R preserves the frozen old execution algorithm through a local adapter in `strong_rhythm_migration.h`. C2-C0-R runs the byte-identical old boundary and exhaustive corpus oracles against this corrected P1R tree. Any changed topology count, classification, witness, or deterministic output is a replay failure.

No finite `ChordProgressionPlan` modulo fallback is permitted.

## Hardware list

None. This checkpoint is host characterization only. Normal repository CI may compile Cardputer ADV and SEQTRAK targets as inherited regression evidence, but C2-C0-R claims no physical listening result.

## Wiring

None. Existing Cardputer ADV and MIDI wiring is unchanged.

## Build / flash steps

Focused replay:

```sh
bash tests/run_0_9_9_phrase_c2_c0_tests.sh
```

The runner verifies the P1R `src/` firewall, old-oracle Git blob identity, finalized H1-F1 adapter presence, GCC deterministic replay, Clang equivalence when available, exact frozen corpus cardinalities, ASan/UBSan smoke coverage, physical-gate source characterization, and finalized P1R compatibility.

No firmware flashing is required for this research checkpoint.

## Expected behavior

The focused output must reproduce:

```text
C2-C0 DECISION A: NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE
C2-C0 NEXT PRODUCER SCOPE: A-ONSET ONLY
```

and must retain zero `A_CONTINUATION` and zero `A_OVERLAP`. The exact old corpus cardinalities above must match on corrected ancestry.

## Troubleshooting

- If the `src/` firewall fails, stop: C2-C0-R is not allowed to change production code.
- If an oracle blob SHA differs, restore the exact frozen #395 test object instead of editing the oracle.
- If corpus counts differ, treat it as a corrected-ancestry compatibility failure; do not update expected values to make the test pass.
- If the progression accessor is unresolved, inspect the finalized P1R-local H1-F1 adapter. Do not restore the old finite-plan API or modulo fallback.
- If a sanitizer differs only because the full exhaustive census is too expensive, keep sanitizer scope on the frozen smoke mode; the optimized exhaustive GCC/Clang run remains the cardinality authority.

## Acceptance checklist

- [ ] branch is `research/20260828-06-0.9.9-phrase-c2-c0-replay`
- [ ] merge-base is exact P1R `a413561136b274a1b16b079f95f8d3ce3353fac5`
- [ ] exactly one replay commit above P1R
- [ ] exactly five changed research/test/docs/workflow files
- [ ] zero `src/` delta
- [ ] both C++ oracle blobs are byte-identical to old C2-C0
- [ ] request tuples = 75,496,320
- [ ] admitted phrases = 41,418,120
- [ ] adjacent boundaries = 96,729,660
- [ ] unique adjacent signatures = 294,725
- [ ] A_ONSET = 17,530,610 / 30,408 unique
- [ ] A_CONTINUATION = 0
- [ ] A_OVERLAP = 0
- [ ] terminal N2 = 41,418,120 / 104,104 unique
- [ ] corrected H1-F1 adapter result parity exact
- [ ] focused deterministic GCC / Clang / ASan / UBSan gates green
- [ ] inherited exact-head required workflows terminal green

## Decision gate

Only after one exact final SHA has the required terminal-green matrix:

```text
PHRASE-C2-C0-R FINAL

DECISION A — NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REPRODUCED
NEXT PRODUCER SCOPE — A-ONSET ONLY

C2-C0-R FREEZE COMPLETE
```

Then, and only then, branch C2-R from that exact frozen SHA. C2-R owns the A-ONSET lifetime producer; C2-C0-R does not.
