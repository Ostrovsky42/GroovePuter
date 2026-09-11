# G4-CC1A-P1 Acid Articulation Authority Restoration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore a demonstrably reachable authoritative choice between detached/re-articulated and connected/extended bass motion for all three shipped Acid owners without changing rhythm ownership, existing candidate weights, P-level semantics, Phrase, FEEL, UI, engines, patches, or FX.

**Architecture:** Keep `BassRhythmPlan` as the owner of onset/continuation topology and keep tonal/SynthStep layers as faithful materializers. Prove first that Acid composition profiles expose only detached-capable bass identities while the generic bass role already contains a connected-capable identity. Restore capability at the narrowest data-driven selection boundary, then verify two-sided reachability through `resolveStrongRhythmFrozenSelection()` and `migrateStrongRhythmFrozenMaterial()`.

**Tech Stack:** C++17 host tests, existing GroovePuter generation APIs, Bash test runners, GitHub Actions.

**Spec:** `docs/research/GF2_G4_CC1A_ACID_STRUCTURAL_MINIMUM.md`

## Global Constraints

- Start from exact verified `8331735080f25acbc809ef97b9881fd9e17c86a2`.
- Work only on `feature/20260911-01-g4-cc1a-p1-acid-articulation-authority`.
- RED must inspect authoritative generated bass output, not LaneGrammar metadata.
- Contract quantifier is owner-space capability, never per-pattern mixed articulation.
- No rhythm admission changes, no existing candidate-weight changes, no genre-weight changes, no new genre owner, no new articulation framework, no arbitrary grid constants.
- Connected evidence must reach `BassRhythmPlan::continuations` and authoritative Synth-A materialization; slide alone is not sufficient.
- Preserve detached reachability for every shipped Acid owner.
- Run relevant lifetime/source regressions because continuation affects note lifetime semantics.
- Final CI must use `permissions: contents: read` and be rerun on the final exact SHA.

---

### Task 1: Characterize the authoritative RED

**Files:**
- Create: `tests/test_gf2_g4_cc1a_p1_acid_articulation_authority.cpp`
- Create: `tests/run_gf2_g4_cc1a_p1_tests.sh`
- Create: `.github/workflows/gf2-g4-cc1a-p1-acid-articulation-authority.yml`

**Interfaces:**
- Consumes: `generationProfileFor`, `resolveStrongRhythmFrozenSelection`, `migrateStrongRhythmFrozenMaterial`, `realizeBassRhythm`, `realizeBassPitchBehavior`.
- Produces: deterministic per-owner capability evidence with `DEFINED`, `SELECTABLE`, `AUTO_OBSERVED`, and `MATERIALIZED` states plus concrete detached/connected witnesses.

- [ ] **Step 1: Write the failing capability test**

The test must iterate `Acid / BASE`, `Acid / Chicago Jack`, and `Acid / Rolling Acid`; enumerate each profile's authoritative bass identities; classify an identity as connected-capable only if a production `realizeBassRhythm()` result has non-zero `continuations`; search deterministic generation identities and materialize through `resolveStrongRhythmFrozenSelection()` + `migrateStrongRhythmFrozenMaterial()`; require both detached and connected materialized witnesses per owner.

- [ ] **Step 2: Run the RED workflow on the unmodified production tree**

Expected result on the start tree: compile/setup succeeds; all three owners report detached reachable and connected unreachable; workflow fails specifically on the connected side of the contract.

- [ ] **Step 3: Record RED provenance**

Capture exact RED SHA, workflow run, job, and per-owner witness output before touching production code.

---

### Task 2: Restore capability at the causal owner

**Files:**
- Modify only the production file proven by Task 1 to own the missing selector capability. Current hypothesis: `src/generation/composition/generation_profile.cpp`.
- Test: `tests/test_gf2_g4_cc1a_p1_acid_articulation_authority.cpp`

**Interfaces:**
- Consumes: existing generic `BassRhythmId::SustainAndDrop`, whose `BassRhythmPlan` already emits continuations.
- Produces: an Acid-specific authoritative bass-identity space containing both detached-capable identities and an existing connected-capable identity, without changing any pre-existing candidate weight.

- [ ] **Step 1: Implement the minimum data-driven correction**

Do not alter `bass_rhythm.cpp` behavior unless Task 1 disproves the current root-cause hypothesis. Prefer an Acid-specific profile view that preserves every existing `kBassDrive` candidate and numeric weight while adding the already-existing connected-capable identity. Do not modify `kBassDrive` globally because it is shared by House, Techno, Darksynth, Rave, Broken variants, and Lo-Fi House.

- [ ] **Step 2: Run the new test**

Expected: every Acid owner has at least one detached and one connected `MATERIALIZED` witness; no test requires each individual materialization to mix both classes.

- [ ] **Step 3: Add adversarial path-removal controls**

Test-side removal of connected capability must fail the contract; test-side removal of detached capability must fail the contract. Preserve label, weight, kick-relationship, grid-constant, and semantic-flattening controls.

- [ ] **Step 4: Commit only the causal production change and test update**

No incidental refactor or calibration.

---

### Task 3: Verify corpus, lifetime, monophony, and blast radius

**Files:**
- Update: `tests/test_gf2_g4_cc1a_p1_acid_articulation_authority.cpp`
- Update: `tests/run_gf2_g4_cc1a_p1_tests.sh`
- Create: `docs/research/GF2_G4_CC1A_P1_ACID_ARTICULATION_AUTHORITY.md`
- Optionally create only if generated deterministically by the test: `docs/research/GF2_G4_CC1A_P1_ACID_ARTICULATION_CENSUS.tsv`

**Interfaces:**
- Consumes: final production selector and existing lifetime/source regression runners.
- Produces: 1152-row descriptive corpus, deterministic repeat evidence, no-overlap/continuation checks, retained historical gates.

- [ ] **Step 1: Run the 3 × 128 × 3 corpus**

Report `all_detached`, `all_connected`, `mixed`, `continuation_rows`, `continuation_events`, and `slide_rows`, per owner and P-level. Acceptance is two-sided reachability, not a distribution threshold.

- [ ] **Step 2: Prove Synth-A continuation semantics are monophonic and distinct from fresh attacks**

For every continuation cell, require an active predecessor and no independent onset on the same cell; do not count slide-only onset decoration as connected capability.

- [ ] **Step 3: Run affected generic/source and Pattern/Phrase lifetime regressions**

Reuse existing runners; do not rewrite historical tests. Include gates covering source switching, held-note release, MAKE PHRASE lifetime, and bass/tonal projection semantics.

- [ ] **Step 4: Run retained G4 gates**

Run G4-CC1A, G4-CC1, G4-I6, G4-I5, G4-I4, G4-I3, and G4-R1 on the same tree.

- [ ] **Step 5: Write the research report**

Document exact loss seam, owner decision, RED evidence, production fix, final per-owner witnesses, corpus distribution, adversarial controls, lifetime regressions, retained gates, and the bounded contract verdict.

---

### Task 4: Final exact-SHA provenance gate

**Files:**
- Update: `.github/workflows/gf2-g4-cc1a-p1-acid-articulation-authority.yml`

**Interfaces:**
- Consumes: all tests and report from Tasks 1-3.
- Produces: one read-only final workflow artifact tied to the final exact SHA.

- [ ] **Step 1: Run final workflow with `permissions: contents: read`**

The workflow must run the new contract gate, deterministic repeat, corpus generation, retained G4 gates, relevant lifetime/source regressions, and a diff audit from `8331735080f25acbc809ef97b9881fd9e17c86a2`.

- [ ] **Step 2: Verify workflow/job/artifact**

Require workflow conclusion `success`, capture artifact ID and SHA-256 digest, and inspect logs for the actual evidence lines rather than relying only on step statuses.

- [ ] **Step 3: Re-fetch remote branch**

Require remote branch HEAD to equal the workflow head SHA.

- [ ] **Step 4: Final diff audit**

Classify all changed files as TEST/EVIDENCE, PRODUCTION, CI, or DOCS; explain why every production file owns the defect; reject incidental changes.

- [ ] **Step 5: Stop**

Do not start Techno CC1B, Funk CC1C, registry work, continuation-frequency calibration, Acid contour work, or other genre repairs.
