# G4-C3 Broken/DnB Admission Equivalence Design

## Goal

Prove one narrow structural claim: `Broken / Drum&Bass` (mode=7, recipe=2) owns the same plural Breakbeat rhythm-admission set `{413,414,415,416}` as the already-PROVEN canonical `Drum&Bass / BASE` owner (mode=14, recipe=0).

## Why this checkpoint exists

G4-C2 ranked `Broken / Drum&Bass` first because its fully observed admitted archetype set exactly aliases the proven DnB admission set. C2 deliberately blocked promotion because owner equivalence was not proven and the observed bass vocabulary differs.

C3 resolves only the rhythm-admission part of that blocker.

## In scope

- exact owner `mode=7, recipe=2`;
- exact reference owner `mode=14, recipe=0`;
- one-bar, `pattern_address=0`, identities `1..128`, P1/P2/P3, matching the current I6 corpus boundary;
- effective candidate count remains 4 for both owners;
- all four admitted archetypes `{413,414,415,416}` are observed for both owners;
- every selected archetype in both owners is `RhythmFamily=Breakbeat`;
- the Broken owner may be promoted from `REVIEW_REQUIRED` to a new PROVEN admission contract if and only if those predicates hold;
- C1 DnB materialized/bass contract must remain unchanged.

## Explicit non-claims

C3 does **not** prove full owner equivalence.

C3 does **not** inherit the DnB bass contract into Broken / Drum&Bass.

C3 does **not** assert that equal rhythm admission implies equal genre identity.

C3 does **not** use genre labels, recipe names, weights, or timbre as evidence.

C3 does **not** modify production generator behavior or `src/`.

C3 does **not** assess phrase ownership or temporal/section development.

## Required negative boundary

The checkpoint must preserve and explicitly verify the observed bass-space difference:

- canonical DnB: `{3,4,8,9}`;
- Broken / Drum&Bass: `{2,5,7,9}`.

If a future implementation silently copies the canonical DnB bass contract into Broken / Drum&Bass, C3 must fail.

## Contract result

If proven, add one contract layer only:

`G4-C3-BROKEN-DNB-ADMISSION-BREAKBEAT-ALIAS`

Semantics:

> For the exact Broken / Drum&Bass owner, the effective rhythm-admission space is the same four-member Breakbeat archetype set `{413,414,415,416}` as canonical DnB.

The downstream bass semantics remain unproven/independent.

## Expected census effect

Before C3:

- proven contract layers: 6;
- REVIEW_REQUIRED owners: 30;
- UNKNOWN materialized evaluations: 11520.

After C3 admission promotion:

- proven contract layers: 7;
- REVIEW_REQUIRED owners: 29;
- UNKNOWN materialized evaluations: 11136;
- no `src/` changes.

The reduction by 384 rows is not a claim that Broken bass semantics are proven; those rows are now evaluated only against the proven admission predicate.

## Verification requirements

1. RED must fail because the C3 promoter/contract is absent, after retained G4 gates pass.
2. Positive test proves exact admission equality and Breakbeat family.
3. Negative/boundary test proves Broken and canonical DnB bass vocabularies remain distinct and C1 stays intact.
4. Deterministic I6 run A/B remains byte-identical after C1+C3 promotion.
5. Committed I6 evidence freshness remains fail-closed.
6. C2 ranking is regenerated or updated so the promoted Broken owner is no longer ranked among REVIEW_REQUIRED owners; the next candidates move up without automatic promotion.
7. Final unified workflow uses `contents: read` only.
8. Final artifact includes updated I6 and C2 evidence.
9. Remote branch HEAD equals the exact successful workflow SHA.
