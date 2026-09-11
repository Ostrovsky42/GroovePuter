# G4-C3 Broken/DnB Admission Equivalence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Promote exactly one new structural contract proving that Broken / Drum&Bass owns the same plural Breakbeat admission set `{413,414,415,416}` as canonical DnB, without inheriting canonical DnB bass semantics.

**Architecture:** Extend the existing I6 post-processing chain after C1 with a C3 promoter that validates both exact owners, proves the admission alias, and rewrites only the Broken owner rows to a PROVEN admission contract. Keep the C1 canonical DnB materialized/bass contract unchanged. Regenerate committed I6 evidence and C2 ranking from the new census so provenance remains exact-SHA and fail-closed.

**Tech Stack:** Python 3 stdlib (`csv`, `pathlib`, `re`, `tempfile`, `subprocess`), Bash, GitHub Actions, existing C++17 G4/I6 host corpus.

**Spec:** `docs/superpowers/specs/2026-09-11-g4-c3-broken-dnb-admission-equivalence-design.md`

## Global Constraints

- Start from exact C2 SHA `902ce152c6224c25a2ee374801b23631f6757259`.
- Exact Broken owner: `mode=7, recipe=2`.
- Exact canonical DnB reference: `mode=14, recipe=0`.
- Exact rhythm set: `{413,414,415,416}`.
- Both owners must remain `RhythmFamily=Breakbeat` across the scoped materialized rows.
- Canonical DnB bass IDs stay `{3,4,8,9}`.
- Broken / Drum&Bass bass IDs stay `{2,5,7,9}` and must not be rewritten to the canonical set.
- No production `src/` changes.
- No genre labels, recipe names, weights, or timbre as proof inputs.
- Phrase ownership remains out of scope.
- Final workflow permissions remain `contents: read`.

---

### Task 1: Define the C3 RED contract test

**Files:**
- Create: `tests/test_gf2_g4_c3_broken_dnb_admission_equivalence.py`
- Modify: `.github/workflows/gf2-g4-c0-materialized-corpus.yml`

**Interfaces:**
- Consumes: I6 `run-a` census after C1 promotion.
- Produces: a fail-closed executable contract test expecting C3 promotion and preserving the bass boundary.

- [ ] **Step 1: Write the failing test**

Create a Python test that accepts the I6 run directory and asserts:

```python
BROKEN = ("7", "2")
DNB = ("14", "0")
EXPECTED_ARCHETYPES = {"413", "414", "415", "416"}
BROKEN_BASS = {"2", "5", "7", "9"}
DNB_BASS = {"3", "4", "8", "9"}
C3_ID = "G4-C3-BROKEN-DNB-ADMISSION-BREAKBEAT-ALIAS"
C1_ID = "G4-C1-DNB-MATERIALIZED-BASS-SPACE"
```

For each exact owner require 384 rows, one `effective_candidate_count=4`, full observation of the exact four archetypes, and only `Breakbeat` family. Require Broken rows to use `C3_ID`, `PROVEN`, `SATISFIED`; canonical DnB rows must still use `C1_ID`, `PROVEN`, `SATISFIED`. Require `broken_bass == BROKEN_BASS`, `dnb_bass == DNB_BASS`, and `broken_bass != dnb_bass`. Require summary coverage `proven=7`, `review_required=29`, `unknown=11136`, and a C3 marker saying admission is PROVEN while bass is independent/unproven. Require the report registry to contain C3 admission contract but no C3 bass/materialized contract.

- [ ] **Step 2: Wire the test into the unified workflow before retained regressions**

Add the feature branch to the workflow branch list and add C3 test/promoter paths. Add a focused step after I6 and before C2 ranking that runs the new test against `build/host-tests/gf2-g4-i6/run-a`. At RED time the test must fail because the C3 promoter/contract is absent, not because an earlier retained gate fails.

- [ ] **Step 3: Commit RED**

Commit only the test and workflow wiring with message:

```text
test(g4-c3): define Broken DnB admission equivalence RED
```

- [ ] **Step 4: Verify RED in GitHub Actions**

Expected: C0/R1/I3/I4/I5/I6 all succeed; C3 fails with a specific missing/unpromoted contract condition; C2/retained steps after C3 are skipped.

---

### Task 2: Implement minimal C3 admission promoter

**Files:**
- Create: `tools/gf2/promote_gf2_g4_c3_broken_dnb_admission.py`
- Modify: `tests/run_gf2_g4_i6_tests.sh`

**Interfaces:**
- Consumes: I6 run directory after `promote_gf2_g4_c1_dnb_contract.py`.
- Produces: updated census/report/summary where only Broken / Drum&Bass is promoted to the C3 admission contract.

- [ ] **Step 1: Implement exact-owner validation**

The promoter must fail closed unless both owners have exactly 384 rows, candidate count 4, observed archetypes exactly `{413,414,415,416}`, and every row is `RhythmFamily=Breakbeat`.

Before promotion Broken rows must still be `I6-REVIEW-RHYTHM-OWNERSHIP / REVIEW_REQUIRED / UNKNOWN`; canonical DnB rows must already be `G4-C1-DNB-MATERIALIZED-BASS-SPACE / PROVEN / SATISFIED`.

- [ ] **Step 2: Enforce the bass non-equivalence boundary**

Require exact observed bass sets:

```python
BROKEN_BASS = {"2", "5", "7", "9"}
DNB_BASS = {"3", "4", "8", "9"}
```

Fail if the sets become equal or either set drifts. Never rewrite `bass_identity`.

- [ ] **Step 3: Promote only Broken admission rows**

Rewrite only Broken rows:

```text
contract_id=G4-C3-BROKEN-DNB-ADMISSION-BREAKBEAT-ALIAS
contract_status=PROVEN
contract_scope=one-bar address-0; identities 1..128; P1/P2/P3
contract_quantifier=forall materialized rows for exact Broken/DnB owner under admission predicate
expected_witness=Breakbeat+admission={413,414,415,416}
evaluation=SATISFIED
offending_stage=NONE
```

`actual_witness` must record the selected Breakbeat archetype only; it must not claim a bass contract.

- [ ] **Step 4: Update report and summary deterministically**

Update:

```text
proven_contract_owners: 3 -> 4
proven_contracts: 6 -> 7
review_required: 30 -> 29
unknown_evaluations: 11520 -> 11136
```

Add one registry row for `G4-C3-BROKEN-DNB-ADMISSION-BREAKBEAT-ALIAS`. Add a summary marker:

```text
G4_C3_BROKEN_DNB_ADMISSION equivalence=PROVEN rows=384 archetypes=4 broken_bass_ids=4 dnb_bass_ids=4 bass_semantics=INDEPENDENT
```

- [ ] **Step 5: Insert promoter after C1 in both deterministic I6 passes**

In `tests/run_gf2_g4_i6_tests.sh`, run C3 immediately after C1 for both `RUN_A` and `RUN_B`, then run the C3 focused test after deterministic comparison.

- [ ] **Step 6: Commit minimal GREEN implementation**

Commit promoter + runner changes with message:

```text
feat(g4-c3): prove Broken DnB admission equivalence
```

---

### Task 3: Add counterfactual/boundary controls

**Files:**
- Modify: `tests/test_gf2_g4_c3_broken_dnb_admission_equivalence.py`

**Interfaces:**
- Consumes: the C3 promoter as a subprocess plus copied run-dir fixtures.
- Produces: tests proving the promoter reacts to structural drift and refuses bass-contract inheritance.

- [ ] **Step 1: Add an admission drift negative fixture**

Copy a valid pre-C3 run directory, mutate one Broken row so its selected archetype/family no longer belongs to the exact expected Breakbeat set, run the promoter, and require nonzero exit with a structural admission failure marker.

- [ ] **Step 2: Add a bass-collapse negative fixture**

Copy a valid pre-C3 run directory and rewrite Broken bass identities so the observed set equals canonical DnB `{3,4,8,9}`. The promoter must reject the fixture rather than silently proving owner equivalence.

- [ ] **Step 3: Add non-structural independence fixture**

Mutate Broken/DnB display labels and weight provenance only. C3 promotion result for contract/rank/coverage must remain unchanged.

- [ ] **Step 4: Commit controls**

Commit with message:

```text
test(g4-c3): guard admission and bass boundaries
```

---

### Task 4: Regenerate C2 against the promoted census

**Files:**
- Modify: `tests/test_gf2_g4_c2_contract_candidate_ranking.py`
- Modify: `docs/research/GF2_G4_C2_CONTRACT_CANDIDATES.tsv`
- Modify: `docs/research/GF2_G4_C2_CONTRACT_CANDIDATES.md`
- Modify: `docs/research/GF2_G4_I6_OWNERSHIP_CENSUS.tsv`
- Modify: `docs/research/GF2_G4_I6_OWNERSHIP_CENSUS.md`
- Modify: `.github/workflows/gf2-g4-c0-materialized-corpus.yml`

**Interfaces:**
- Consumes: C3-promoted I6 census.
- Produces: fresh C2 ranking over exactly 29 REVIEW_REQUIRED owners and committed evidence matching current generators/promoters.

- [ ] **Step 1: Update C2 expected owner count and top list**

After C3 promotion, Broken / Drum&Bass must disappear from the REVIEW_REQUIRED ranking. The expected leading candidates become:

```python
EXPECTED_TOP4 = [
    ("7", "8"),
    ("4", "4"),
    ("5", "11"),
    ("5", "0"),
]
```

The test must still forbid all automatic promotion.

- [ ] **Step 2: Regenerate I6 and C2 evidence deterministically**

Generate exact outputs from the current toolchain and commit them. Do not hand-edit numeric census totals without matching generated evidence.

- [ ] **Step 3: Extend workflow freshness checks and artifact upload**

Ensure the final workflow byte-compares regenerated C2 TSV/MD against committed files and uploads C3/I6/C2 evidence. Keep permissions `contents: read`.

- [ ] **Step 4: Commit evidence/provenance closure**

Commit with message:

```text
ci(g4-c3): close Broken DnB admission provenance
```

---

### Task 5: Final exact-SHA verification

**Files:**
- No production changes.

**Interfaces:**
- Consumes: final feature branch SHA.
- Produces: authoritative C3 checkpoint only if every gate is fresh on that exact SHA.

- [ ] **Step 1: Run unified workflow on final SHA**

Require success for C0 deterministic repeat, R1, I3, I4, I5, I6+C1+C3, C2 freshness, retained GF2-I3/Stage-12/FEEL regressions, and artifact upload.

- [ ] **Step 2: Verify final metrics**

Require:

```text
proven=7
review_required=29
unknown=11136
C3 admission=PROVEN
C3 bass_semantics=INDEPENDENT
C2 auto_promoted=0
```

- [ ] **Step 3: Verify branch and production boundaries**

Remote branch HEAD must equal workflow head SHA. Compare `902ce152...` to final SHA and require zero changed files under `src/`.

- [ ] **Step 4: Record artifact provenance**

Record final workflow run/job IDs, artifact ID, size, and SHA-256 digest.
