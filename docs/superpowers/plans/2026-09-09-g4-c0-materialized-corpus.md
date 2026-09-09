# G4-C0 Materialized Corpus Research Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Measure whether Acid, House, Dub Techno and Drum & Bass produce genuinely distinct materialized musical structures after production/timbre information is removed, without changing production semantics.

**Architecture:** Reuse the existing strong-rhythm production path and the C2-V0R observation conventions. Materialize deterministic identities into real `DrumPatternSet` / `SynthPattern` objects, derive three evidence layers (surface, role-preserving structural, monophonic-click structural), then summarize collapse and phrase-development evidence. BPM is not a topology input to `StrongRhythmMigrationContext`; the corpus records the profile corridor once and treats BPM as a later perceptual-time projection rather than multiplying identical structural cases.

**Tech Stack:** C++17 host tests, existing GroovePuter generation/materialization sources, Bash, GitHub Actions.

**Spec:** Research checkpoint established in the G4 conversation; no production-code change is permitted.

## Global Constraints

- Exact base: `fcd0d77da5ed6ef38419547477ab26e77ec6ff26`.
- Branch: `research/20260909-01-g4-c0-materialized-corpus`.
- Allowed repository paths: `tests/`, `.github/workflows/`, `docs/superpowers/plans/`, and the final research report.
- Forbidden: changes under `src/`, persisted Scene/Recipe schemas, UI, runtime ownership, Pattern/Phrase storage, scheduler semantics, generator policy retuning.
- Pilot set: Acid base, House base, Reggae recipe 5 (current Dub Techno route), Drum & Bass base.
- Identity coordinates: phrase-generation identities 1..128.
- Realization levels: P1, P2, P3.
- Generation attempt ordinal: 0 for identity corpus; repeated-G analysis remains variation evidence from the existing F-07 contract.
- Timbre stripping must exclude velocity, timing, FX, probability, absolute pitch and instrument patch identity.

---

### Task 1: Characterization corpus

**Files:**
- Create: `tests/test_gf2_g4_c0_materialized_corpus.cpp`
- Create: `tests/run_gf2_g4_c0_tests.sh`

**Interfaces:**
- Consumes: `resolveStrongRhythmFrozenSelection(...)`, `migrateStrongRhythmFrozenMaterial(...)`, `preparePhraseExecution(...)`, `materializePreparedPhraseBar(...)`, and `GF2Measurement::materialFingerprint(...)`.
- Produces: deterministic summary rows beginning with `G4_C0 PILOT`, `G4_C0 LEVEL`, `G4_C0 PLEVEL`, `G4_C0 PHRASE`, plus witness rows for semantic/material collapse.

- [ ] **Step 1: Add the characterization test before any production change**

The test is observational: it must fail only on inability to materialize the declared corpus or on nondeterministic/invalid measurement state. It must not encode the desired future G4 production behaviour as a passing requirement.

- [ ] **Step 2: Compile and run the focused test**

Run:

```bash
bash tests/run_gf2_g4_c0_tests.sh
```

Expected: host compilation succeeds and the program emits deterministic corpus summaries for all four pilots.

- [ ] **Step 3: Repeat and diff output**

Run:

```bash
bash tests/run_gf2_g4_c0_tests.sh > /tmp/g4-c0-a.txt
bash tests/run_gf2_g4_c0_tests.sh > /tmp/g4-c0-b.txt
diff -u /tmp/g4-c0-a.txt /tmp/g4-c0-b.txt
```

Expected: no diff.

### Task 2: Remote exact-head evidence

**Files:**
- Create: `.github/workflows/gf2-g4-c0-materialized-corpus.yml`

**Interfaces:**
- Consumes: `tests/run_gf2_g4_c0_tests.sh`.
- Produces: exact-head GitHub Actions log and `g4-c0-summary.txt` artifact.

- [ ] **Step 1: Add a branch-scoped workflow**

The workflow must trigger only on `research/20260909-01-g4-c0-materialized-corpus` and `workflow_dispatch`, install the same host compiler dependencies used by the Lo-Fi adversarial gate, run the focused corpus twice, diff the outputs, and upload the accepted summary.

- [ ] **Step 2: Verify exact-head run**

Expected: workflow conclusion `success`; both deterministic runs byte-identical.

### Task 3: Research verdict

**Files:**
- Create: `docs/research/GF2_G4_C0_MATERIALIZED_IDEA_CORPUS.md`

**Interfaces:**
- Consumes: exact-head G4-C0 workflow log.
- Produces: evidence/inference-separated verdict for Acid, House, Dub Techno and DnB.

- [ ] **Step 1: Record measured counts**

For each pilot and level record successful samples, unique surface fingerprints, unique role-structure fingerprints, unique click-structure masks and semantic-signature cardinality.

- [ ] **Step 2: Record pairwise collapse evidence**

Classify at least one witness, when present, for each of:

```text
surface differs / role structure same
semantic signature differs / role structure same
role structure differs / click structure same
same identity P1/P2/P3 semantic selection stable / realization differs
```

- [ ] **Step 3: Record phrase evidence**

For every pilot, report ready phrases, multi-bar role-structural development and multi-bar click-structural development separately. Do not infer a new genre branch from a single witness.

- [ ] **Step 4: Freeze the G4-C0 verdict**

The report must distinguish measured evidence from musical interpretation and must not propose production implementation until the corpus result is known.
