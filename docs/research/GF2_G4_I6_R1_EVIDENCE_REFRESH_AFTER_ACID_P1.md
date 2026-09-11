# G4-I6-R1 — Deterministic Evidence Refresh After Acid P1

## Scope

This checkpoint refreshes the retained G4-I6 deterministic evidence after the approved Acid P1 production change. It does not change production code, I6 predicates, I6 methodology, ownership classification, collision projection, reachability logic, contract registry state, RNG seed derivation, or selection semantics.

## Historical I6 state

The original authoritative I6 evidence remains historical at commit `47e09186c1ade6cca8ed79c567fca919399fb98e` (`docs(g4-i6): commit deterministic census evidence`). At that checkpoint the retained projection reported `profiles=33`, `archetypes=24`, `effective_edges=122`, `roots=4224`, `materializations=12672`, `proven_contracts=4`, `review_required=31`, and `unknown=11904`.

The refreshed files below are the current retained I6 projection after the later approved Acid P1 production change. They do not imply that `SustainAndDrop` was present during the historical I6 checkpoint.

## Why freshness became RED

Acid P1 changed the three shipped Acid owners (`BASE`, `Chicago Jack`, `Rolling Acid`) from the historical four-candidate bass space to the Acid-local `kBassAcid` space that additionally admits the existing `SustainAndDrop` bass identity. The historical deterministic census therefore no longer byte-matched a fresh projection even though the I6 semantic, methodology, reachability, ownership, and collision gates remained healthy.

## C1 causality authority

Causality was closed separately by `G4-I6-R1-C1` at SHA `e1ecb6a28bf60aaa42fb705bfc9302c2116052ba`.

Provenance:

- workflow: `34630032272`
- job: `103364312436`
- artifact: `10276170826`
- artifact SHA-256: `c8929e491b5cd36aebe17a498183b6e48b663e3e5c64ea809b0caa5e499b6e21`
- RNG model: `STATELESS_DOMAIN`

C1 proved that the downstream identity drift is deterministic semantic reseeding from the changed selected bass identity, not shared PRNG cursor contamination. This refresh references that proof rather than duplicating its full trace.

## Historical-to-current delta

Fresh comparison against the historical I6 census is exact:

```text
changed_rows=948
non_acid_delta=0

Acid / BASE          333
Acid / Chicago Jack  294
Acid / Rolling Acid  321

materialized_signature=948
bass_identity=948
chord_identity=681
melodic_identity=687
motif_identity=519
```

Classification:

```text
bass_identity    → EXPECTED_P1_DELTA
chord_identity   → DERIVED_EXPECTED_DELTA
melodic_identity → DERIVED_EXPECTED_DELTA
motif_identity   → DERIVED_EXPECTED_DELTA

DERIVED_EXPECTED_DELTA=1887 field changes
UNEXPECTED_DELTA=0
UNEXPLAINED=0
```

No non-Acid materialization row changed. `GF2_G4_I6_OWNERSHIP_ANOMALIES.tsv` remains byte-identical to the historical projection.

## Stable current I6 invariants

The refreshed projection preserves:

```text
profiles=33
archetypes=24
effective_edges=122
roots=4224
materializations=12672
successful=12672/12672
reachability=122/122

false_owner=0
admission_orphan=0
overbroad_owner=0
structural_collision=0
shared_admission=24
shared_valid_under_contracts=0
collision_pairs=0

proven_contracts=4
review_required=31
unknown=11904
```

Full existing I6 self-tests and corrected methodology controls remain unchanged and are required to pass in the exact-head R1 workflow.

## Refreshed canonical evidence

Current generated authority is copied directly from the existing I6 generator:

- `docs/research/GF2_G4_I6_OWNERSHIP_CENSUS.tsv`
- `docs/research/GF2_G4_I6_OWNERSHIP_CENSUS.md`
- `docs/research/GF2_G4_I6_OWNERSHIP_ANOMALIES.tsv` remains byte-identical and therefore has no semantic/content change.

The generator remains the sole authority. No hand normalization was applied.

## Contract status

`G4-ACID-BASS-ARTICULATION-CAPABILITY` remains:

```text
status=REVIEW_REQUIRED
```

This refresh does not promote Acid and does not modify the contract registry. Current I6 coverage therefore remains `proven=4`, `review_required=31`, `unknown=11904` until final C3 ratification.

## Determinism and freshness

`tests/run_gf2_g4_i6_tests.sh` generates complete run A and run B projections and byte-compares the canonical outputs. Final G4-I6-R1 acceptance requires:

```text
G4-I6 full derived corpus deterministic repeat: PASS
G4-I6 committed evidence freshness: UP TO DATE
```

The final read-only exact-head workflow additionally byte-compares run A with all committed freshness-controlled evidence files.

## Production freeze

G4-I6-R1 is evidence-only. Relative to C1 start SHA `e1ecb6a28bf60aaa42fb705bfc9302c2116052ba`, final acceptance requires `src/**` diff to be empty.

## Impact on C3A

Once the final read-only exact-head G4-I6-R1 workflow succeeds and remote HEAD equality is proven, the stale-I6-evidence blocker is removed and `C3A READY TO RETRY = YES`. C3A itself is deliberately not run by this checkpoint.

## Final exact-head verification

This report is finalized as an input to the read-only G4-I6-R1 exact-head verification workflow. No further evidence mutation is permitted inside final verification.
