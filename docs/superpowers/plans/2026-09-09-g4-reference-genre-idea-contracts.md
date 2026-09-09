# G4 Reference Genre / Musical-Idea Contracts Implementation Plan

> Execute on isolated branch `feature/20260909-02-0.9.11-g4-reference-genre-contracts`.

Design: `docs/superpowers/specs/2026-09-09-g4-reference-genre-idea-contracts-design.md`

Authoritative feature base: `8d7713e67431a6bc8eba2f7dfcee4c94b3fe821b`

## Guardrails

- TDD for every production behavior change: test first, observe the intended RED, then minimal GREEN.
- Do not add `ReferenceGenre`, a persisted MusicalIdea, a new sequencer, schema, UI, recipe, or generic DSL.
- `GenerationCompositionResult` remains the frozen ephemeral idea value for this checkpoint.
- Bass-family vocabulary remains owned by `bass_rhythm.*`; composition laws consume that truth rather than duplicating it.
- House and Acid are calibration regressions in this first pass; do not narrow them without new evidence.
- Every remote evidence statement must name exact SHA + workflow run.

## Task 1 — G4-R1 reference contract characterization

**Files:**
- Create `tests/test_gf2_g4_r1_reference_contracts.cpp`
- Create `tests/run_gf2_g4_r1_tests.sh`
- Create `.github/workflows/gf2-g4-r1-reference-contracts.yml`

### Step 1.1 — Add RED characterization for explicit bass-family bypass

Use existing production `realizeBassRhythm()` directly.

Construct a Breakbeat request with a valid breakbeat archetype and explicit `BassRhythmId::RollingDrive`.

Expected contract:

```text
explicit bass identity incompatible with selected family
→ InvalidRequest
```

Current production returns the explicit requested ID directly, therefore this test must fail for the intended reason.

Also add a positive control for a legal Breakbeat identity (`HalfTimePocket` or `SyncopatedHook`) so the test does not accidentally reject all explicit identities.

### Step 1.2 — Add RED characterization for DnB composition coherence

For identities 1..128:

1. Resolve DnB composition through the production selection seam.
2. Resolve selected archetype definition and family.
3. Assert the selected bass identity is compatible with that family according to the intended bass-role contract.
4. Record distinct reachable bass IDs and require at least two so GREEN cannot collapse DnB into one canonical bass pattern.

Before production changes, print direct witnesses of incompatible tuples and fail if any exist.

### Step 1.3 — Add RED characterization for Dub Techno temporal skeleton

For recipe 5 (`GenerativeMode::Reggae`, recipe 5), enumerate/materialize the production-compatible rhythm candidates rather than checking only numeric IDs.

Define the characterization predicate from materialized kick topology:

```text
techno skeleton witness = quarter-note kick anchors at steps 0,4,8,12
```

For this first contract the test requires every allowed recipe-5 rhythm candidate to preserve those anchors. It must also require at least two distinct candidate archetypes, preventing a one-pattern “fix”.

Current recipe-5 candidates 409/411/412 must make this RED.

If the RED reveals that the chosen structural predicate conflicts with an intentionally valid reference sound, STOP before changing production and revise the contract; do not weaken the assertion merely to pass CI.

### Step 1.4 — Add reference non-regression observations

Record, but do not hard-fail on new musical law assumptions for:

- Acid reachable archetypes and role diversity;
- House reachable archetypes including 713;
- P1/P2/P3 composition semantic stability for a small deterministic sample.

Hard assertions should cover existing invariants only (successful resolution, no disappearance of the reference genres, P-level selection identity stability).

### Step 1.5 — Push RED and run exact-head workflow

The workflow builds/runs the focused host test. It must expose the semantic failures, not infrastructure/compiler mistakes.

Expected RED causes:

```text
R1_BASS_EXPLICIT_FAMILY_BYPASS
R1_DNB_BASS_FAMILY_INCOHERENCE
R1_DUB_TECHNO_SKELETON
```

Commit:

```text
test(g4): characterize reference genre contracts
```

Do not touch `src/` in this commit.

## Task 2 — G4-I1 bass-family compatibility boundary

**Files:**
- Modify `src/generation/roles/bass_rhythm.h`
- Modify `src/generation/roles/bass_rhythm.cpp`
- Modify `src/generation/composition/generation_profile.cpp`
- Optionally modify `src/generation/composition/genre_structural_laws.h` only to compose existing predicates; do not copy family candidate tables.
- Update focused test only if adding additional positive/negative cases, never to weaken RED.

### Step 2.1 — Expose one pure bass-role predicate

Extract the existing family candidate truth into a public allocation-free function, for example:

```cpp
bool isBassRhythmCompatibleWithFamily(
    BassRhythmId id,
    RhythmFamily family);
```

The implementation must use the same `candidatesFor(family)` data already used by Auto selection.

No new table.

### Step 2.2 — Make explicit role requests truthful

Before realizing an explicit requested bass ID:

```text
invalid id/family relation → InvalidRequest
legal id/family relation   → existing realization path
```

This closes the semantic bypass at the role boundary.

### Step 2.3 — Filter composition bass candidates before weighted selection

`resolveGenerationComposition()` knows the selected archetype. Resolve its `ReferenceVocabulary::Definition` and family.

Select bass only from profile candidates for which `isBassRhythmCompatibleWithFamily()` is true.

Requirements:

- preserve existing deterministic domain/seed/salt behavior;
- preserve candidate weights;
- canonicalize by stable ID as existing selector does;
- fixed-capacity only;
- no retry loop;
- no heap;
- fail `InvalidProfile` only if no compatible bass candidate remains.

Prefer a small selector/view helper that skips incompatible candidates while applying existing canonical weighted-selection semantics, rather than copying the entire selector.

### Step 2.4 — Verify focused GREEN

Expected:

- explicit incompatible Breakbeat `RollingDrive` rejected;
- legal explicit Breakbeat identity still works;
- all 128 DnB identities select family-compatible bass;
- >=2 DnB bass identities remain reachable;
- Dub Techno RED still fails at this stage, proving tasks are causally separated.

Commit:

```text
feat(g4): enforce bass family coherence
```

## Task 3 — G4-I2 Dub Techno temporal compatibility

**Files:**
- Modify `src/generation/composition/rhythm_selection.cpp`
- Focused test remains authoritative.

### Step 3.1 — Choose minimal existing techno-skeleton candidate vocabulary

Do not create new archetypes.

Inspect existing materialized archetypes and select a recipe-5 candidate set that:

- preserves quarter-note kick anchors under P1 structural realization;
- contains at least two distinct rhythm statements;
- is compatible with Dub Techno tempo corridor;
- leaves dub identity to the existing bass/chord/progression profile.

Candidates should come from existing techno/four-floor vocabulary. Do not blindly alias the whole `kTechnoBase` if some candidates fail the structural test.

### Step 3.2 — Change recipe-5 compatibility data only

Replace the reggae/dub-pulse temporal candidate set for recipe 5 with the tested techno-skeleton set and intentional weights.

Do not change `GenerativeMode`, recipe ID, persistence, or UI taxonomy.

### Step 3.3 — Verify focused GREEN

All three hard R1 contracts must now pass:

- bass explicit-family truth;
- DnB composition coherence;
- Dub Techno structural skeleton + >=2 candidates.

Commit:

```text
feat(g4): restore Dub Techno temporal skeleton
```

## Task 4 — G4-I3 phrase-law causal truth

**Files:**
- Modify `tests/test_gf2_g4_r1_reference_contracts.cpp` or create focused `tests/test_gf2_g4_i3_phrase_truth.cpp`
- Modify `src/generation/composition/generation_profile.cpp` and/or a narrowly scoped composition helper
- Modify `src/generation/migration/phrase_execution.cpp` only if admitting an already-existing tested trajectory; do not move Phrase ownership.

### Step 4.1 — Add RED before production change

For 128 identities per reference genre at P2, prepare real Phrase execution.

For every selected semantic law:

```text
law == Loop
OR
prepared.phraseTrajectory != kNoTrajectoryId
```

Current Acid/House/Dub Techno must produce RED witnesses where a non-Loop label has no trajectory.

DnB is a positive control where real explicit trajectories already exist.

### Step 4.2 — Prefer truthful selection filtering

If an archetype has no validated phrase-evolution programme for a non-Loop law, remove that law from its effective candidate space **before weighted selection**.

Do not change the profile's stored editorial bag globally if another compatible archetype can use it. Derive availability from the selected archetype and the existing phrase-evolution catalog.

If the existing catalog already contains a tested trajectory for an archetype and the only blocker is admission metadata, enabling it is allowed only with a focused trajectory-materialization test.

### Step 4.3 — Verify GREEN

- no non-Loop semantic lie remains in sampled reference compositions;
- DnB trajectories remain active;
- Phrase uses one frozen selection across bars;
- no per-bar idea reroll introduced.

Commit:

```text
feat(g4): make phrase law selection causally truthful
```

## Task 5 — G4-V1 corpus and regression verification

**Files:**
- Reuse `tests/run_gf2_g4_c0_tests.sh`
- If needed, extend report tooling without altering fingerprint semantics.
- Create `docs/gf2/GF2_G4_V1_REFERENCE_GENRE_CONTRACTS.md` after evidence is accepted.

### Step 5.1 — Run focused G4-R1 exact-head gate twice

Require byte-identical deterministic output.

### Step 5.2 — Run full corrected attack-aware G4-C0 corpus on feature head

Compare against baseline report at `8d7713e...`.

Report separately for Acid/House/Dub Techno/DnB:

```text
IDENTITY
IDEA/ROLE DIVERSITY
FALSE DIVERSITY
PHRASE CAUSALITY
```

Do not convert to one score.

Expected directional changes:

- DnB bass-family violations: zero;
- DnB retains multiple role structures;
- Dub Techno four-floor/techno-skeleton contract: 128/128 for allowed material under the chosen predicate;
- Dub Techno retains >1 rhythm archetype;
- House/Acid remain materializable and are not silently collapsed;
- phrase-law false causality: zero for selected non-Loop laws.

### Step 5.3 — Existing regression gates

Run relevant GF2 composition/rhythm/phrase tests and Pattern/Phrase integration regressions touched by the production seams.

At minimum include:

- Stage 15 tonal/composition integration;
- GF2 phrase-law execution;
- G4-C0 corpus;
- focused G4-R1 contracts.

### Step 5.4 — Cardputer ADV / memory evidence

Run the existing embedded compile/link and memory gate appropriate to the current integration line.

Record against base `8d7713e...`:

- `.data + .bss` / fixed DRAM delta;
- binary size delta;
- any heap/runtime allocation delta if detectable.

Expected architectural result: no new persistent allocation; semantic filtering should add code/constexpr data only.

### Step 5.5 — Final report

Create `docs/gf2/GF2_G4_V1_REFERENCE_GENRE_CONTRACTS.md` with:

1. exact base SHA;
2. final exact feature SHA;
3. commits;
4. production files changed;
5. owner changes (expected: none in runtime; only exposed bass-role compatibility truth + composition filtering);
6. RED witnesses;
7. GREEN evidence/run IDs;
8. before/after corpus;
9. per-genre verdicts;
10. memory delta;
11. intentionally untouched genres;
12. unresolved questions, especially House 713 and Acid articulation/lifetime.

Commit:

```text
docs(g4): record reference genre contract verification
```

## Completion rule

Do not call G4-V1 complete until all accepted evidence points to one exact feature candidate SHA, or explicitly distinguish report-only SHA from the measured production SHA.

Do not merge into `dev_0.9.10` as part of this checkpoint. PR/integration policy remains separate.