# G4-C4–C6 Obvious Admission Wave Implementation Plan

**Goal:** Promote the four remaining high-confidence single-family plural owners into separate admission contracts, then regenerate I6/C2 evidence without changing production `src/`.

**Architecture:** Add one focused structural C++ witness test plus one post-I6 Python promoter/test pair. The promoter validates all four owner predicates fail-closed and changes only contract metadata in the generated census/report/summary. C2 is then regenerated over the remaining 25 REVIEW_REQUIRED owners.

## Task 1 — RED

- Add `tests/test_gf2_g4_c4_c6_obvious_admissions.py` expecting four new contract IDs and final totals `proven=11`, `review_required=25`, `unknown=9600`.
- Wire it after existing I6+C3 in the unified workflow.
- Verify retained gates pass first and RED fails only because C4–C6 are still REVIEW_REQUIRED.

## Task 2 — Structural witnesses

- Add `tests/test_gf2_g4_c4_c6_structural_witness.cpp`.
- C4: exact recipe set `{417,419}`; both backbeats anchored on 4/12; 417 kick is broken, 419 kick is quarter-pulse; both shuffle-oriented.
- C5: exact recipe set `{401,402,406}` and all three have quarter-kick anchors 0/4/8/12.
- C6: BASE exact set `{409,410,411,412}`; Minimal Space exact set `{409,411,412}`; verify Minimal Space = BASE − `{410}`.
- Do not use display names or weights as proof.

## Task 3 — Promotion

- Add `tools/gf2/promote_gf2_g4_c4_c6_obvious_admissions.py`.
- Validate exact owners/row counts/candidate counts/archetype sets/families and pre-state REVIEW_REQUIRED/UNKNOWN.
- Promote each owner to its own admission contract ID, never touching bass identities.
- Update report registry and summary deterministically to 11/25/9600.
- Add counterfactual controls for admission drift, Steppers reintroduction, and label/weight independence.
- Run promoter for both I6 deterministic passes after C3.

## Task 4 — C2/provenance

- Update C2 current-state test to 25 owners, zero single-family-plural candidates, and mixed-family first rank.
- Generate updated I6 and C2 evidence using a temporary write-authority publisher; commit only generated `docs/research/*`; delete the publisher.
- Final unified workflow remains `contents: read` and byte-compares committed evidence.

## Task 5 — Exact-SHA closure

- Require unified C0/R1/I3/I4/I5/I6+C1+C3+C4–C6/C2/retained Stage-12+FEEL suite all GREEN on one SHA.
- Require artifact upload and record digest.
- Require remote HEAD == workflow SHA.
- Require C3→final compare has zero `src/` changes.
