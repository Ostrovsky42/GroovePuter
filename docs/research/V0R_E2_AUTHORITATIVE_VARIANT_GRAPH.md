# 0.9.9-V0R — E2 Authoritative Rhythm Variant Graph

## Purpose

Measure the complete legal neighboring-variant closure for the six selected production rhythm archetypes at P1/P2/P3 using the frozen E2 contracts. This is host/research tooling only. It does not define lifecycle, cadence, mutation budgets, or new production musical semantics.

The measured result is frozen by committed compact CSV/JSON snapshots. The authority runner must regenerate the graph result and compare both snapshots byte-for-byte; snapshot drift is a hard failure and is never auto-refreshed.

## Hardware

NONE — host/research only.

## Wiring

NONE.

## Exact base / authority inputs

- Exact base: `0992f3d8b7af317d91ff166b641b893f806a921c`.
- Research branch: `research/20260824-02-0.9.9-e2-authoritative-variant-graph`.
- Pre-freeze snapshot/tooling checkpoint: `54939c5e8c4c68070ccd1ad0a9935415d9b325c8`.
- E2c owns the mutation delta grammar and ordering.
- E2a owns canonical mutation candidate production.
- E2b owns canonical diff/budget legality.
- `rhythm_realizer` remains the existing structural/archetype validity owner through `rhythmMutationPlanValid()`.
- Canonical realization uses the selected production `ReferenceVocabulary` mappings, `projectSeed=0xE2A09901`, `phraseOrdinal=7`, one bar, and a Statement trajectory.
- E2t Activity is not an input.

The six measured report families map to the existing production references: StraightFour/StraightDrive, OffbeatPulse/OffbeatOpenHat, Breakbeat/SparseFastBreak, HalfTime/HalftimeSwitch, Sparse/HypnoticSparse, and Rolling/RollingAcid.

## Node identity

Node identity is field-wise and padding-independent. The key includes:

- `barCount`, `trajectoryId`, `level`, `intent`;
- each bar's `function`;
- for every rhythm role: `structural`, `secondary`, `ghosts`, `shortGate`, `heldGate`, `tieGate`, and `accents`.

The graph does not serialize or compare raw `RhythmPhrasePlan` struct bytes and does not use `memcmp()` for node identity.

## Enumeration algorithm

For each of six production archetypes and each RealizationLevel P1/P2/P3:

1. Realize the canonical one-bar material `C` from the production reference vocabulary.
2. Normalize the root to the requested level without changing its musical material.
3. Traverse discovered nodes breadth-first using a stable field-wise node key.
4. For current node `V`, call the existing E2a producer with canonical `C` and current material `V`.
5. Materialize only operations already emitted by these production paths. Current measured paths use ADD and/or GHOST.
6. Validate every materialized target `W` through E2b against the original canonical `C`.
7. Reject invalid/budget-exceeding targets; deduplicate legal material nodes by the stable key; emit the directed `V -> W` transition record.
8. Continue until no new legal node remains.

KEEP is not a graph alternative. If the selected production producer begins emitting DROP, DISPLACE, or ACCENT without an already-owned materialization path, V0R fails closed with a contract-gap error rather than inventing research-only semantics.

Weak connectivity and canonical reachability are therefore closure invariants of this enumeration procedure, not standalone quality claims. Directed SCC and reverse-reachability measurements provide the useful topology distinction.

## Canonical-relative legality

Every target is checked as:

```text
E2a(V) -> W
E2b legal(C, W)
```

The canonical material never changes to `V`. The graph never refreshes the mutation budget after an intermediate hop.

`legal(C,A) + legal(A,B)` does not imply `legal(C,B)`. A second mutation may be small relative to `A` while the cumulative delta from original canonical `C` exceeds the level's E2b budget. The measured graph exposes this directly: P3 generated 129,838 materializable proposals, and E2b rejected 70,688 of them against `C`.

## Why E2t Activity is excluded

E2t answers **when** an evolution attempt may occur. V0R measures **which material states and directed neighboring transitions are legal** under the frozen E2 mutation contracts. Including temporal Activity would make graph topology depend on scheduling/context and would mix cadence ownership into a material-space measurement. Activity/cadence is therefore intentionally outside the graph.

## Why synthetic DROP/DISPLACE/ACCENT fixture edges are excluded

E2a contract tests may use synthetic fixtures to exercise the full E2c vocabulary. Those fixtures are not evidence that the selected production `ReferenceVocabulary` paths currently emit those operations. V0R measures the production producer over production canonical material only, so synthetic fixture edges are excluded.

Measured production raw proposal counts for DROP, DISPLACE, and ACCENT are all zero. V0R therefore provides no production evidence for those operations.

## Enumeration ceilings

Safety ceilings are fail-closed:

- unique nodes per graph: `1,000,000`;
- legal transition records per graph: `8,000,000`.

Reaching either ceiling reports `ENUMERATION INCOMPLETE` and fails the run. No measured graph reached a ceiling. The largest measured graph is Rolling/P3 with 5,382 nodes and 22,646 legal transition records.

## Determinism evidence

The frozen authoritative raw dataset SHA-256 is:

```text
8f4b70f44a2e6c32dce105b5526d532ba52e99021039774c13738bb772bb85d5
```

The authoritative reporter CSV SHA-256 is:

```text
410e2da8cf6f3fdc6197259e992f8a703160d11bf2e5ddb454288ee22e897a75
```

Detailed artifact SHA-256 values:

```text
nodes CSV:       630ac63f9daf4de4ff80355df85516af9ebf2d3ab2cbb25c55c1dd9f6b28b202
edges CSV:       632591aec6c2452e2eca197bda32ba389b7836d632d27c049ff1da3d8ff1d779
full metrics:    f4f80d7520d5acf36b327e06c23d963752cdd2b301e81647911c9cde9f91fed0
```

The measured authority run produced byte-identical raw and report artifacts for GCC run #1, GCC run #2, Clang, and ASan/UBSan. The compact snapshot format additionally freezes fields needed for research review without changing the authoritative reporter output.

Committed compact snapshot SHA-256 values for this freeze format are:

```text
compact CSV:  86bcf38c907ee5cef60729ed09db4c50d5f66ee7db41d249e8e8a6973bf12993
compact JSON: 17156a89837c11c60de574e66212e9bc5841ca357784b6b76224a2b7060d39c2
```

## 18-graph result table

| Archetype | Level | Nodes | Alternatives | Canon out | Mean out | Dead ends | SCCs | Depth | ADD | GHOST | Identity |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| StraightFour | P1 | 1 | 0 | 0 | 0.000 | 1 | 1 | 0 | 0 | 0 | 100% |
| StraightFour | P2 | 46 | 45 | 10 | 1.739 | 35 | 46 | 2 | 0 | 80 | 100% |
| StraightFour | P3 | 4015 | 4014 | 20 | 4.108 | 1542 | 4015 | 5 | 9852 | 6642 | 100% |
| OffbeatPulse | P1 | 1 | 0 | 0 | 0.000 | 1 | 1 | 0 | 0 | 0 | 100% |
| OffbeatPulse | P2 | 19 | 18 | 6 | 1.579 | 12 | 19 | 2 | 0 | 30 | 100% |
| OffbeatPulse | P3 | 157 | 156 | 10 | 2.968 | 12 | 157 | 5 | 244 | 222 | 100% |
| Breakbeat | P1 | 1 | 0 | 0 | 0.000 | 1 | 1 | 0 | 0 | 0 | 100% |
| Breakbeat | P2 | 42 | 41 | 9 | 1.738 | 32 | 42 | 2 | 0 | 73 | 100% |
| Breakbeat | P3 | 2685 | 2684 | 18 | 4.028 | 940 | 2685 | 5 | 6411 | 4403 | 100% |
| HalfTime | P1 | 1 | 0 | 0 | 0.000 | 1 | 1 | 0 | 0 | 0 | 100% |
| HalfTime | P2 | 34 | 33 | 8 | 1.706 | 25 | 34 | 2 | 0 | 58 | 100% |
| HalfTime | P3 | 1655 | 1654 | 16 | 3.913 | 500 | 1655 | 5 | 3864 | 2612 | 100% |
| Sparse | P1 | 1 | 0 | 0 | 0.000 | 1 | 1 | 0 | 0 | 0 | 100% |
| Sparse | P2 | 26 | 25 | 7 | 1.654 | 18 | 26 | 2 | 0 | 43 | 100% |
| Sparse | P3 | 637 | 636 | 14 | 3.538 | 108 | 637 | 5 | 1291 | 963 | 100% |
| Rolling | P1 | 1 | 0 | 0 | 0.000 | 1 | 1 | 0 | 0 | 0 | 100% |
| Rolling | P2 | 53 | 52 | 10 | 1.774 | 42 | 53 | 2 | 0 | 94 | 100% |
| Rolling | P3 | 5382 | 5381 | 20 | 4.208 | 2352 | 5382 | 5 | 13534 | 9112 | 100% |

The table is the human-readable projection of the committed compact snapshot; the snapshot also freezes min/median/max outdegree, rates, reachability, per-operation raw proposals, rejection accounting, and explicit identity counts.

## Global proposal/rejection accounting

| Level | Raw proposals | Raw ADD | Raw GHOST | Canonical-budget rejects | Legal transitions | Legal ADD | Legal GHOST |
|---|---:|---:|---:|---:|---:|---:|---:|
| P1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| P2 | 1,170 | 0 | 1,170 | 792 | 378 | 0 | 378 |
| P3 | 129,838 | 69,656 | 60,182 | 70,688 | 59,150 | 35,196 | 23,954 |

Across all 18 graphs:

- materialization failures = `0`;
- structural rejections = `0`;
- duplicate targets = `0`;
- legal DROP edges = `0`;
- legal DISPLACE edges = `0`;
- legal ACCENT edges = `0`.

P1 has six canonical-only graphs. P2 is GHOST-only and has maximum shortest-path depth 2. P3 is ADD/GHOST-only and has maximum shortest-path depth 5.

## Density measurements

Density is descriptive only; no new density policy or threshold is introduced. Full metrics record histograms for total occupied onsets, per-role occupied onsets, ghost count, and accent count.

Across the six production archetypes:

- P1 total occupied-onset range: 11–16; ghosts: 0; accents: 0.
- P2 total occupied-onset range: 11–18; ghost count: 0–2; accents: 0.
- P3 total occupied-onset range: 11–21; ghost count: 0–2; accents: 0.

The detailed per-role/histogram data remains in the authoritative full metrics artifact rather than being converted into production thresholds.

## Identity preservation

Identity preservation is measured with the existing production structural/archetype validator `rhythmMutationPlanValid()` — the same owner E2b uses for `candidatePlanValid`.

All 14,757 measured legal nodes preserve that validator:

```text
identity violations = 0
identity preservation rate = 100%
```

This is a measured property of the current E2 legal space, not a new identity policy.

## Directed topology interpretation

Factual findings from the frozen graph:

1. P1 currently provides no mutation alternatives.
2. P2 is a shallow GHOST-only neighborhood.
3. P3 is materially larger but remains exclusively ADD/GHOST.
4. Every graph has one weak component and 100% canonical reachability by exhaustive closure construction.
5. Every SCC is a singleton; `largest SCC = 1` in all 18 graphs.
6. In every nontrivial graph, only canonical reverse-reaches canonical.
7. Large node count therefore does not imply reversible evolution.
8. E2b canonical-relative budget materially constrains the space: 70,688 P3 proposals are rejected against original canonical `C`.
9. V0R provides no production evidence for DROP, DISPLACE, or ACCENT.
10. Every measured legal node preserves the existing structural validator.

These observations are research findings only. V0R does not convert them into new mutation thresholds or production policy.

## Previous V0 baseline status

The specified earlier branch contained no graph-tooling delta to measure. Therefore there is no previous E1a numeric V0 baseline suitable for a before/after numeric comparison.

Status:

```text
PREVIOUS E1A NUMERIC V0 BASELINE
NOT AVAILABLE
```

## Snapshot authority gate

`tests/run_0_9_9_v0r_variant_graph.sh` regenerates the authoritative reporter outputs, derives deterministic compact CSV/JSON snapshots, and exact-byte compares them with:

```text
tests/data/v0r_e2_variant_graph_summary.csv
tests/data/v0r_e2_variant_graph_summary.json
```

On mismatch the runner prints `V0R snapshot ...: DRIFT`, emits a unified diff, and exits nonzero. It explicitly refuses automatic snapshot refresh. A drift must be classified before any snapshot change as one of:

- committed snapshot incorrect;
- compact helper nondeterministic;
- authority semantics drifted.

## Build/run commands

From repository root:

```bash
python3 tests/test_0_9_9_v0r_source_guard.py
bash tests/run_0_9_9_v0r_variant_graph.sh
bash tests/run_host_tests.sh
```

The V0R runner itself builds/runs GCC twice, Clang, and ASan/UBSan; checks byte-identical authority artifacts; performs compact snapshot `cmp`; and re-runs E1a Stage 6.1, E2c, E2a, and E2b frozen contracts.

## Expected behavior

A valid freeze run must report:

```text
V0R source guard: production semantics unchanged, E2 authority boundary OK
V0R snapshot CSV: PASS
V0R snapshot JSON: PASS
0.9.9-V0R E2 authoritative rhythm variant graph: OK
```

The raw authority digest must remain `8f4b70f4...85d5`; the authoritative reporter CSV must remain `410e2da8...97a75`; no enumeration ceiling may fire; all compiler/sanitizer/cumulative contract executions must succeed.

## Troubleshooting

- `V0R snapshot CSV/JSON: DRIFT`: stop. Do not refresh snapshots. Inspect the unified diff and classify incorrect snapshot vs compact-helper nondeterminism vs authority semantic drift.
- `ENUMERATION INCOMPLETE`: the result is not authoritative; do not report truncated counts.
- `CONTRACT GAP`: the production E2a producer emitted an operation without an existing V0R representation adapter. Do not add research-only musical semantics; resolve ownership separately.
- missing `clang++`: install Clang; cross-compiler authority is mandatory.
- source guard reports a `src/` path: stop; V0R is research-only and production semantics must remain untouched.
- raw/report mismatch across GCC/Clang/sanitizer: treat as determinism failure; do not freeze.

## Acceptance checklist

- [ ] Exact merge-base remains `0992f3d8b7af317d91ff166b641b893f806a921c`.
- [ ] Final compare from exact base contains zero `src/` files.
- [ ] 18 graphs enumerate completely; no safety ceiling is reached.
- [ ] Raw authority digest is `8f4b70f44a2e6c32dce105b5526d532ba52e99021039774c13738bb772bb85d5`.
- [ ] Authoritative reporter CSV digest is `410e2da8cf6f3fdc6197259e992f8a703160d11bf2e5ddb454288ee22e897a75`.
- [ ] GCC #1 == GCC #2 == Clang == ASan/UBSan byte-for-byte.
- [ ] Committed compact CSV exact-byte comparison passes.
- [ ] Committed compact JSON exact-byte comparison passes.
- [ ] E1a Stage 6.1, E2c, E2a, and E2b cumulative contracts pass.
- [ ] Full host regressions pass.
- [ ] Detailed authority artifact uploads successfully.
- [ ] Draft PR #368 remains open, draft, unmerged.
