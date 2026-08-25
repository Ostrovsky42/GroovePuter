# 0.9.9-E3R-B — DROP / DISPLACE Graph Value + Musical Review

## Purpose

Freeze the research result for counterfactual DROP and DISPLACE value after the E3a exact execution contract, without changing production policy or production source.

This checkpoint answers whether bounded DROP and DISPLACE materially improve the legal canonical-relative rhythm variant graph, and what restrictions are justified before any future production enablement.

E3R-B is research-only. It does not implement lifecycle, cadence, selection, UI, runtime graph storage, or production mutation enablement.

## Exact ancestry and scope

- E3a execution authority: `2549bbfdda84581a14783a775d3c8577f8855c54`
- Frozen V0R authority: `872fef9331a34e8d48f5703015b1126f11e581c3`
- Two-parent research integration base: `f01c4639e2cf3f485efb94167fce5a4c0a007721`
- Research branch: `research/20260825-02-0.9.9-e3r-b-drop-displace-value`
- Pre-closure research head: `7d490488e0fd95d34c817760d7c1c99c5c0cb770`
- Draft PR: `#372`

The integration commit has exactly the E3a and frozen V0R commits as parents. Production `src/` delta relative to E3a is zero.

## Authority boundaries

- E2a remains the mutation proposal owner.
- E3a `applyRhythmMutationDelta(...)` is the only DROP/DISPLACE materializer used by E3R-B.
- Frozen V0R ADD/GHOST materialization remains the research adapter for those existing operations.
- E2b remains the canonical-relative diff/budget authority.
- Every candidate is judged against the original canonical material `C`; legality is never rebased to an intermediate `V`.
- ACCENT is excluded.
- Production mutation policy is unchanged.
- Full graph materialization is research-only and is forbidden as a runtime design.

## Hardware

NONE — host/research only.

## Wiring

NONE.

## Build / validation

Primary and sensitivity validation are run by:

```bash
bash tests/run_0_9_9_e3r_b_tests.sh
bash tests/run_host_tests.sh
```

The E3R-B workflow is:

```text
.github/workflows/research-0-9-9-e3r-b-drop-displace-value.yml
```

Final pre-closure authority run:

```text
workflow: 32823639696
conclusion: SUCCESS
graph-authority: SUCCESS
full-host: SUCCESS
cap=1: PASS
cap=2 sensitivity: PASS
frozen V0R compatibility gate: PASS
core regressions: SUCCESS
```

No cap=3 run is part of this checkpoint. Enumeration ceilings were not raised.

## Frozen V0R baseline

Frozen V0R remained byte-identical through the E3R-B compatibility gate. The authoritative frozen raw digest is:

```text
8f4b70f44a2e6c32dce105b5526d532ba52e99021039774c13738bb772bb85d5
```

This establishes that introducing the E3a executor did not change the pre-existing production variant graph.

## Primary authority versus sensitivity

**Cap=1 is the PRIMARY E3R-B policy-value measurement.**

**Cap=2 is SECONDARY SENSITIVITY evidence only.**

Cap=1 already establishes the first-order topology effect of enabling one DROP and/or one DISPLACE in the counterfactual budget. Cap=2 is used only to determine whether an additional allowance creates qualitatively new topology value or mainly combinatorial expansion.

Cap=2 therefore does not replace or redefine the cap=1 recommendations.

## Counterfactual graph model

The measured matrix contains 48 cap=1 graphs:

```text
6 archetype families
x P2/P3
x BASE, DROP, DISPLACE, DROP_DISPLACE
```

The same research graph semantics are used for cap=2 sensitivity; only the counterfactual DROP/DISPLACE cap changes from 1 to 2 in the sensitivity build copy.

Safety ceilings remain:

```text
nodes:       1,000,000 per graph
transitions: 8,000,000 per graph
```

No cap=1 or cap=2 graph reached a ceiling. Enumeration is complete within these declared bounds.

## Cap=1 primary result — DROP

DROP has strong graph value at cap=1, but the value is not uniform across levels or source classes.

The strongest P3 effect is reversal of material that was previously introduced by ADD. Measured P3 added-secondary DROP edges are direct reverse edges of ADD, creating reversible topology without inventing a second mutation owner.

This is the primary evidence for a future restricted DROP policy:

```text
P3 noncanonical / previously-added material DROP
PROMOTE WITH RESTRICTIONS — PENDING LISTENING
```

The evidence does **not** justify unrestricted canonical structural DROP.

Ghost DROP is executor-supported by E3a, but no graph-value evidence was produced for it in this measured corpus:

```text
GHOST DROP
EXECUTOR SUPPORTED
GRAPH VALUE NOT MEASURED
```

## Cap=2 sensitivity — DROP diminishing return

P3 aggregate:

| Metric | cap=1 | cap=2 |
| --- | ---: | ---: |
| nodes | 61,408 | 126,864 |
| transitions | 446,064 | 1,002,833 |
| largest SCC | 176 | 176 |
| reverse reachable to canonical | 591 | 591 |

A second allowed DROP approximately doubles the state space and more than doubles transition count, but it does **not** increase the maximum SCC and does **not** increase canonical reverse reachability.

The main reversible topology value was already captured by cap=1.

Therefore higher DROP budget shows diminishing topology return with substantial state-space growth. E3R-B does **not** recommend `maxDrops=2` as a production default.

## P2 DROP negative result

P2 DROP sensitivity is specifically negative for general canonical structural enablement:

| Metric | cap=1 DROP | cap=2 DROP |
| --- | ---: | ---: |
| nodes | 850 | 1,611 |
| largest SCC | 1 | 1 |
| reverse reachable to canonical | 6 | 6 |

Additional P2 DROP expands destructive canonical-state space without creating reversible topology.

Final research recommendation:

```text
P2 DROP
NOT RECOMMENDED
```

This is strong evidence against general P2 canonical structural DROP enablement.

## Cap=1 primary result — DISPLACE

DISPLACE has positive graph value without increasing onset density and is archetype-dependent.

Across the measured cap=1 space:

- executor rejections are zero;
- identity violations are zero;
- measured DISPLACE movement is density-neutral;
- ghost DISPLACE remains unsupported;
- some archetypes, including Breakbeat/HalfTime cases, may have no legal DISPLACE space under the current grammar;
- existing archetype grammar remains the authority.

The measured movement is distance 2. Distance 1 produced zero measured edges.

Therefore E3R-B must not claim or invent a radius-1 policy.

```text
DISPLACE DISTANCE1
NOT MEASURED

DISPLACE DISTANCE2
MEASURED
```

Structural and secondary displacement both exist in the measured space, but structural musical acceptability remains listening-dependent.

Primary recommendation:

```text
DISPLACE
PROMOTE WITH RESTRICTIONS — PENDING LISTENING
```

Do not force DISPLACE into an archetype with no legal displacement space.

## Cap=2 sensitivity — DISPLACE

P3 aggregate:

| Metric | cap=1 | cap=2 |
| --- | ---: | ---: |
| nodes | 26,847 | 31,875 |
| transitions | 141,408 | 181,604 |
| largest SCC | 10 | 12 |
| reverse reachable to canonical | 13 | 17 |

A second DISPLACE allowance produces modest additional topology value with moderate graph growth. This is a quantitative improvement, but not a qualitative policy reversal.

All cap=1 restrictions remain authoritative:

- distance1 value is not established;
- distance2 is the actual measured movement;
- ghost DISPLACE is unsupported;
- archetype grammar remains authoritative;
- archetypes with no legal DISPLACE space remain untouched.

## Combined DROP + DISPLACE

The combined graph shows strong complementary value, but also strongly multiplicative state-space growth.

P3 aggregate:

| Metric | cap=1 | cap=2 |
| --- | ---: | ---: |
| nodes | 179,727 | 398,856 |
| transitions | 1,710,027 | 4,415,276 |
| largest SCC | 4,209 | 6,564 |
| reverse reachable to canonical | 4,804 | 7,174 |

Largest single cap=2 graph:

```text
Rolling/P3 DROP_DISPLACE
nodes:        324,241
transitions:  3,823,258
largest SCC:  6,564
```

No ceiling was hit:

```text
node ceiling:       1,000,000
transition ceiling: 8,000,000
ENUMERATION COMPLETE
```

The combined improvement is real, but the graph expansion is sufficiently large to make the runtime design constraint explicit:

```text
PRODUCTION MUST NEVER MATERIALIZE THE FULL VARIANT GRAPH.
```

Future runtime work must remain bounded and local: produce a bounded candidate set, validate candidates against original canonical `C`, and select without caching or materializing the full closure.

## Execution and identity result

Across cap=1 and cap=2:

```text
executor rejections: 0
identity violations: 0
canonical budget basis: original C
production src delta vs E3a: ZERO
```

The result supports the E3a execution contract and E2b canonical-relative legality model; it does not change production policy.

## Determinism and artifacts

### Cap=1 primary artifacts

```text
summary CSV SHA-256
9c9d3983f456e8ef3ffe19422c08cba0dbaafcb9470ab4f1d33d9b6482b98ed1

summary JSON SHA-256
bbad8865638cc3dc620680b806cd4a6c00da13fb1f335383b6be0d569356abe5
```

### Cap=2 sensitivity artifacts

```text
summary CSV SHA-256
baabca1b011541796489e0743bda9ffa3656a8b992c9453f76a2ae12d07b6b7a

summary JSON SHA-256
2ce0c2daf0b37e6f0183ab33be11ef7f50c22818fc74cbe1041bf07bc5926adc

nodes CSV SHA-256
013f09f678cd741a082eeb2237777e705b5b0163d0951f7d3d6b7e8528ebbcab

edges CSV SHA-256
604b3c71f282df303b359a4a122d0e9854a48fff541bcb97a5019c26928bee80
```

GitHub Actions artifact:

```text
artifact id: 9554997458
artifact name: e3r-b-drop-displace-value
archive SHA-256: 66c1bc4fafe63e3ca56255e997df1e73da50dad50a4e856169695fed343472c3
workflow run: 32823639696
```

The artifact contains the cap=1 authority output, cap=2 sensitivity output, and frozen baseline evidence retained by the workflow.

## Musical review corpus and listening boundary

The deterministic cap=1 review corpus contains:

```text
DROP:      12 cases
DISPLACE:  12 cases
COMBINED:   8 cases
```

Each case preserves canonical `C`, current `V`, candidate `W`, and exact canonical-relative diff for manual musical review.

Musical listening remains:

```text
MUSICAL LISTENING
PENDING
```

Reason: the current production code does not expose an authoritative production-neutral interface that accepts arbitrary E3R-B `C/V/W RhythmPhrasePlan` values and renders those exact plans to MIDI/audio.

Existing audition paths generate/evolve their own production material rather than accepting the arbitrary measured research plans as an exact rendering input. Creating a new renderer would introduce new infrastructure and semantics outside this closure checkpoint.

Therefore E3R-B intentionally does not create a renderer and does not claim musical PASS.

## Final recommendations

### DROP

```text
DROP GRAPH VALUE
STRONG AT CAP1

DROP CAP2 VALUE
DIMINISHING TOPOLOGY RETURN / LARGE STATE GROWTH

P2 DROP
NOT RECOMMENDED

P3 NONCANONICAL DROP
PROMOTE WITH RESTRICTIONS — PENDING LISTENING

GHOST DROP VALUE
NOT MEASURED
```

The strongest supported future policy direction is P3 DROP of noncanonical / previously-added material because it directly reverses ADD. Do not enable unrestricted canonical structural DROP from this evidence.

### DISPLACE

```text
DISPLACE GRAPH VALUE
POSITIVE / ARCHETYPE-DEPENDENT

DISPLACE CAP2
MODEST ADDITIONAL VALUE

DISPLACE DISTANCE1
NOT MEASURED

DISPLACE DISTANCE2
MEASURED

GHOST DISPLACE
UNSUPPORTED
```

Keep the current archetype displacement grammar as authority. Structural displacement remains listening-dependent. Do not invent radius1 policy and do not force DISPLACE where the grammar yields no legal space.

### Combined

```text
COMBINED GRAPH
STRONG / MULTIPLICATIVE

FULL GRAPH AT RUNTIME
FORBIDDEN BY DESIGN
```

Use only bounded/local runtime candidate generation in any future production work.

## Expected behavior

Running the E3R-B authority pipeline should:

- reproduce the frozen V0R compatibility gate;
- complete the cap=1 primary matrix;
- complete the cap=2 sensitivity matrix without raising ceilings;
- report zero executor rejects and zero identity violations;
- preserve zero production `src/` delta relative to E3a;
- leave production DROP/DISPLACE policy disabled;
- leave musical listening as PENDING.

## Troubleshooting

If the frozen V0R digest changes, stop: E3R-B is no longer measuring against the accepted frozen baseline.

If `src/` differs from E3a, stop: this research branch has taken production ownership.

If an enumeration ceiling is reached, classify the run as `ENUMERATION INCOMPLETE`; do not increase the ceiling in this checkpoint.

If cap=1 or cap=2 artifacts differ across GCC #1/#2, Clang, or ASan/UBSan, treat it as a determinism failure; do not refresh snapshots to hide the mismatch.

If an exact arbitrary C/V/W production-neutral renderer is still unavailable, keep musical listening PENDING; do not create a new renderer as a closure fix.

## Acceptance checklist

- [x] Two-parent E3a + frozen V0R research base preserved.
- [x] Production `src/` delta vs E3a is zero.
- [x] Frozen V0R compatibility gate is byte-identical.
- [x] Cap=1 remains the primary policy-value authority.
- [x] Cap=1 graph authority passes.
- [x] Cap=2 sensitivity passes.
- [x] Cap=2 does not replace cap=1 recommendations.
- [x] No node/transition ceiling reached.
- [x] No cap=3 run performed.
- [x] DROP cap=2 diminishing-return result documented.
- [x] P2 DROP negative result documented.
- [x] DISPLACE cap=2 modest-value result documented.
- [x] Distance1 is not claimed as measured value.
- [x] Distance2 is recorded as measured movement.
- [x] Ghost DROP graph value remains unmeasured.
- [x] Ghost DISPLACE remains unsupported.
- [x] Combined multiplicative growth documented.
- [x] Full runtime graph materialization explicitly forbidden by design.
- [x] Executor rejections = 0.
- [x] Identity violations = 0.
- [x] Musical listening remains PENDING.
- [x] Production policy unchanged.
- [x] E3b not started.
- [x] PR #372 remains open, draft, and unmerged.

## Final classification

```text
PRODUCTION POLICY
UNCHANGED

PRODUCTION SRC DELTA VS E3a
ZERO

FROZEN V0R
BYTE-IDENTICAL

PRIMARY COUNTERFACTUAL CAP
1

CAP2 SENSITIVITY
PASS

CAP2 CEILING
NOT REACHED

DROP GRAPH VALUE
STRONG AT CAP1

DROP CAP2 VALUE
DIMINISHING TOPOLOGY RETURN / LARGE STATE GROWTH

P2 DROP
NOT RECOMMENDED

P3 NONCANONICAL DROP
PROMOTE WITH RESTRICTIONS — PENDING LISTENING

DISPLACE GRAPH VALUE
POSITIVE / ARCHETYPE-DEPENDENT

DISPLACE CAP2
MODEST ADDITIONAL VALUE

DISPLACE DISTANCE1
NOT MEASURED

DISPLACE DISTANCE2
MEASURED

GHOST DROP VALUE
NOT MEASURED

GHOST DISPLACE
UNSUPPORTED

COMBINED GRAPH
STRONG / MULTIPLICATIVE

FULL GRAPH AT RUNTIME
FORBIDDEN BY DESIGN

IDENTITY VIOLATIONS
0

EXECUTOR REJECTIONS
0

MUSICAL LISTENING
PENDING

E3b
NOT STARTED

PR #372
OPEN / DRAFT / UNMERGED

E3R-B
FROZEN AFTER FINAL DOC/CI
```

HARD STOP after E3R-B closure. Do not continue graph research or begin E3b from this checkpoint.
