# GF2 G4-CC1 Acid / Techno / Funk contract coverage

## Purpose

G4-CC1 proves three genre-identity statements that live on different structural axes. It does not change generator production code and it does not open a synthetic G4-I7.

Authoritative parent root for this closure branch: `73abe2dbbc4467133f798ee295193d244efdcc91`.

The contract predicates are structural. Genre labels, candidate weights and timbre are not admissible evidence for a verdict.

## Contract registry

| Contract | Owner scope | Subject | Proven predicate | Evidence domain |
|---|---|---|---|---|
| `G4-CC1-ACID-BASS-ARTICULATION` | Acid / BASE, Chicago Jack, Rolling Acid | cross-role rhythm/bass articulation | every admitted rhythm archetype has a BassRhythm lane with both short and held articulation and a Kick -> BassRhythm Respond relation within three 16th-note steps | all 8 effective admitted candidates across the 3 Acid owners |
| `G4-CC1-TECHNO-NO-HARMONIC-MOTION` | Techno / BASE | harmonic prohibition | the progression space contains only StaticModal or PedalDrone; each candidate realizes as `ValidButStatic` with one harmonic event | both progression candidates plus identities 1..128 at P1/P2/P3 = 384 frozen selections |
| `G4-CC1-FUNK-THE-ONE` | Funk/Soul / BASE | downbeat pocket | every admitted rhythm archetype canonically anchors the kick on step 0, and downstream P1/P2/P3 materialization preserves that onset | 3 admitted candidates plus identities 1..128 at P1/P2/P3 = 384 materializations |

## Acid evidence

The effective Acid admission spaces are intentionally mixed-family and are not reduced to a RhythmFamily assertion:

- BASE: `{405,406,407,408}`.
- Chicago Jack: `{405,408}`.
- Rolling Acid: `{406,407}`.

Across those owner spaces there are 8 admitted candidate edges. The contract inspects only LaneGrammar and LaneRelationship structure: a BassRhythm lane must expose non-zero short and held gates, and a Kick -> BassRhythm `Respond` relationship must stay within offsets 0..3.

Adversarial controls remove the short gate, remove the held gate, and remove the kick-to-bass response. Each mutation must make the predicate fail. A separate reweighting control mutates candidate weights and must not alter the verdict.

This contract says nothing about TB-303 timbre and does not require one canonical Acid drum pattern.

## Techno evidence

Techno / BASE admits multiple rhythm organizations, so four-floor topology is not used as the genre proof. The contract is the prohibition on harmonic motion.

The current progression candidate space contains exactly two candidates and every candidate must be either `StaticModal` or `PedalDrone`. Each requested candidate must realize with `ChordProgressionStatus::ValidButStatic` and exactly one harmonic event even when the request allows the maximum harmonic-event count.

The frozen-selection domain covers 128 identities at P1, P2 and P3, for 384 selections. Every selected progression must remain inside the same static set.

Adversarial boundary fixtures accept `StaticModal` and `PedalDrone` and reject moving progressions such as `PopCycle` and `BorrowedLift`. The predicate does not inspect genre labels, timbre or candidate weights.

## Funk/Soul evidence

Funk/Soul / BASE currently admits `{415,416,713}`. This is deliberately not stated as `RhythmFamily::Funk16`: two of the three candidates are Breakbeat-family ideas.

The structural contract is The One: the Kick lane's canonical anchors must include step 0. Downstream materialization is then checked over 128 identities at P1/P2/P3, for 384 materializations, and the rendered kick onset mask must still include step 0.

The adversarial control removes step 0 from the Kick lane and must make the contract fail. A reweighting control mutates candidate weights and must not alter the verdict.

## I6 schema boundary

CC1 is an orthogonal contract layer and does **not** rewrite the existing I6 census totals. At parent root `73abe2db...`, I6 remains `proven=11`, `review_required=25`, `unknown=9600`.

This is intentional. The current I6 materialized row has one `contract_id` slot and its REVIEW_REQUIRED accounting describes missing rhythm-ownership/admission proof. The Techno CC1 proof is harmonic, while Funk also includes downstream preservation. Overwriting that single slot with an orthogonal contract would make the owner look closed for the wrong reason and would prevent multiple simultaneous contracts from being represented truthfully.

Therefore CC1 records these three statements as PROVEN in its own registry/test layer while retaining I6 unchanged. A later coverage-schema checkpoint may introduce multi-contract owner coverage; CC1 itself does not smuggle that architecture change into a genre checkpoint.

## Scope limits

- No claim of universal genre correctness.
- No timbral or production-style proof.
- No phrase/section ownership claim.
- Acid admission is proven through structural cross-role relations, not a unique rhythm family.
- Techno proves static harmony, not a unique kick topology.
- Funk/Soul proves protection of The One, not that every other onset is fixed.
