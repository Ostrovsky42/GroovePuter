# GroovePuter 0.9.14 — D0-B Normative Semantic Contract

Status: D0-A = REVISE / CLOSED; D0-B = OPEN.

## Scope

D0-B is a semantic-contract and falsification checkpoint. It must not redesign production transformations, replace Material ownership, or promote `src/dsp/musical_development.h` into the 0.9.14 semantic authority.

The 0.9.13 classifier is treated as a legacy classifier plus evidence producer and migration input.

## Central boundary

    MATERIAL / DOMAIN STATE
            ↓
       DOMAIN EVIDENCE
            ↓
       SEMANTIC FACTS
            ├─ LINEAGE
            ├─ STATE RELATION(reference)
            ├─ TRAJECTORY ROLE
            ├─ GENRE STATUS
            ├─ OPERATION CONFORMANCE
            └─ CAPABILITY(claim/domain)
                    ↓
            PUBLICATION POLICY
                    ↓
          PUBLISH / HOLD / REFUSE
                    ↓
              MATERIAL OWNER

No semantic fact is itself a publication command. Publication policy may consume facts but must not rewrite them.

## D0 laws

### D0-L1 — Development is not mutation magnitude

`DEVELOPMENT` is a temporal/formal relation. Mutation count, similarity, distance, P-level, number of changed domains, or any other scalar magnitude must not define it.

### D0-L2 — Lineage and state relation are different questions

`LINEAGE` asks whether a candidate still belongs to the source idea lineage. `STATE RELATION(reference)` asks how the candidate relates to one explicitly named reference.

### D0-L3 — Repeat and return are temporal/formal roles

`REPEAT` and `RETURN` are not lineage classifications. Exact material may be an immediate repeat in one history and a return in another.

### D0-L4 — Genre, operation, capability, request/policy and publication are independent

A genre violation, a failed operation promise, unavailable analytical capability, a request-policy refusal, and a publication decision are not interchangeable facts.

### D0-L5 — Return and development require context

`RETURN` requires an explicit `RETURN_TARGET`. `DEVELOPMENT` requires trajectory context. A source/candidate pair is not sufficient in the general case.

### D0-L6 — State relation always names its reference

There is no context-free SAME/VARIATION verdict. Relevant references currently include SOURCE, PREDECESSOR and RETURN_TARGET. A candidate may have several simultaneous relations.

### D0-L7 — Unknown preserves evidence

Missing evidence, unavailable capability, or unresolved interpretation must not be silently converted into NO, VIOLATION, NEW_IDEA or OPERATION VIOLATED. UNKNOWN remains local to the unsupported claim.

## Normative factorization

### LINEAGE

Question: does CANDIDATE remain part of the SOURCE idea lineage?

Research vocabulary: CONTINUES / NEW_IDEA / UNKNOWN.

`NEW_IDEA` requires failure of required lineage-preservation claims. It must not be inferred merely because both pitch and onset changed, or because a distance threshold was crossed.

### STATE RELATION(reference)

Reference roles: SOURCE / PREDECESSOR / RETURN_TARGET.

Research vocabulary: EXACT / VARIATION / UNKNOWN / NOT_APPLICABLE.

No reference role is globally privileged.

### TRAJECTORY ROLE

Question: what temporal/formal role does the candidate occupy in the current sequence?

Research vocabulary: NONE / REPEAT / DEVELOPMENT / BREAK / RETURN / UNKNOWN.

Trajectory role requires history/formal context.

### GENRE STATUS

Question: does the candidate satisfy the applicable Genre Contract?

Research vocabulary: ALLOWED / VIOLATION / UNKNOWN.

Only genre-bearing claims may determine this axis.

### OPERATION CONFORMANCE

Question: did the candidate honor the semantic promise of the requested operation?

Research vocabulary: HONORED / VIOLATED / UNKNOWN.

For example, a contour-preserving transform that flips the required contour is OPERATION VIOLATED. Genre remains independently evaluated.

### CAPABILITY(claim/domain)

Capability is claim-scoped rather than candidate-wide.

Research vocabulary: AVAILABLE / UNAVAILABLE / UNKNOWN.

Example:

    capability(HARMONIC_ROOT_PRESERVATION) = UNAVAILABLE
    capability(RHYTHM_TOPOLOGY)            = AVAILABLE

D0-B deliberately does not introduce PARTIAL. Narrower claims should be used instead.

### MATERIAL LIFECYCLE

CURRENT/NEXT/ACCEPT/DISCARD/publication/persistence/Undo remain under the existing Material contract. D0-B creates no new owner.

## Preservation claims

Lineage, return, operation and genre evaluation consume explicit claims, not a global IdeaFingerprint.

A claim conceptually names PROPERTY, DOMAIN, REFERENCE, REQUIREMENT and EVIDENCE STATUS.

Example:

    property    = RHYTHM_TOPOLOGY
    domain      = RHYTHM
    reference   = SOURCE
    requirement = REQUIRED_FOR_LINEAGE
    evidence    = PASS

Exact voicing may simultaneously be NOT_REQUIRED for a transformed return.

## Mandatory adversarial witnesses

### D0-B1 — A → A

Expected: relation(candidate, PREDECESSOR)=EXACT; LINEAGE=CONTINUES; TRAJECTORY=REPEAT.

### D0-B2 — A → B → A

Expected: relation(candidate, RETURN_TARGET=A)=EXACT; LINEAGE=CONTINUES; TRAJECTORY=RETURN.

### D0-B3 — A → B → A′

Required anchors of A restored but non-required surface properties differ.

Expected: relation(candidate, RETURN_TARGET=A)=VARIATION; LINEAGE=CONTINUES; TRAJECTORY=RETURN.

### D0-B4 — A → A′ isolated

Expected: state relation VARIATION; LINEAGE=CONTINUES; TRAJECTORY=NONE.

### D0-B5 — A → A′ → A″ directional

Expected: local state relations may be VARIATION; LINEAGE=CONTINUES; TRAJECTORY=DEVELOPMENT when formal direction is established.

### D0-B6 — A → A′ → A″ random drift

Repeated local variation is not sufficient for DEVELOPMENT. Expected trajectory is non-DEVELOPMENT or UNKNOWN.

### D0-B7 — tiny change destroys The One

Lineage may still continue while GENRE=VIOLATION. Mutation magnitude is irrelevant.

### D0-B8 — EXTEND without tonal-root authority

Expected: capability(HARMONIC_ROOT_PRESERVATION)=UNAVAILABLE. This alone is neither GENRE VIOLATION nor OPERATION VIOLATED.

### D0-B9 — requested contour preservation is broken

Expected: OPERATION=VIOLATED. Genre remains independently evaluated.

### D0-B10 — unrelated but genre-valid phrase

Expected: LINEAGE=NEW_IDEA and GENRE=ALLOWED is representable.

## Canonical magnitude falsification pair

MANY MUTATIONS plus no supported formal role does not imply DEVELOPMENT.

ONE SMALL STRUCTURAL CHANGE with an established phrase/temporal role may participate in DEVELOPMENT.

This pair is permanent 0.9.14 regression material.

## Legacy 0.9.13 classifier boundary

The legacy implementation may continue to provide observations such as pitch changed, onset changed, articulation changed, contour preserved, The One present, density delta and pitch-class preservation.

The following shortcuts are explicitly non-normative:

    anchorPitchDiff && anchorOnsetDiff -> NEW_IDEA
    anythingChanged                    -> VARIATION
    GenreResult::Pass                  -> Publish
    GenreResult::Fail                  -> omnibus rejection reason

D0-B must firewall these assumptions rather than incrementally growing `evaluateClassificationAndG4()` into the new authority.

## Publication policy

D0-B does not freeze final publication policy. The semantic evaluator emits independent facts; publication policy consumes them.

No normative semantic result may itself be named or interpreted as Publish/Hold/Refuse.

## RED test rule

The first wave is test/docs-only. Production `src/` delta is forbidden.

Required artifacts:

- `docs/0.9.14/D0_B_NORMATIVE_SEMANTIC_CONTRACT.md`
- `tests/test_0_9_14_d0b_semantic_contract.cpp`
- `tests/test_0_9_14_d0b_legacy_classifier.cpp`
- `tests/test_0_9_14_d0b_source_regressions.py`
- `tests/run_0_9_14_d0b_tests.sh`

The test-local model may use local vocabulary. Tests assert witness distinctions, not future production enum spellings.

## Source firewall

D0-B must not be made green by adding DevelopmentDistance, similarity percentages, mutation-magnitude thresholds, a global IdeaFingerprint, a global admissibility enum, or by reinterpreting existing `PhraseEvolutionLawId`, `BarFunction`, `P2Variation` or `P3Transformation` as the new semantic verdicts.

Material publication ownership must remain unchanged.

## Acceptance gate

D0-B GREEN requires all of the following:

1. D0-L1…L7 remain non-contradictory.
2. B1…B10 are representable without collapsing independent questions.
3. Immediate repeat and exact return are distinguishable.
4. Transformed return is representable.
5. Isolated variation and developmental variation are distinguishable.
6. Random drift is not automatically DEVELOPMENT.
7. GENRE VIOLATION, OPERATION VIOLATION and CAPABILITY UNAVAILABLE remain distinct.
8. GENRE ALLOWED + NEW_IDEA is representable.
9. UNKNOWN remains claim-local.
10. No global similarity/mutation scalar is required.
11. No new musical-truth owner is introduced.
12. Production transformation algorithms remain unchanged during D0-B.

Possible verdicts: D0-B GREEN / D0-B REVISE / D0-B EVIDENCE GAP.

Only after D0-B GREEN may the project open a semantic-adapter mapping checkpoint.