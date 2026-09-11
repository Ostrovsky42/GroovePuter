# G4-CC1A Acid Structural Minimum

## Status

`MEASUREMENT_GREEN / CONTRACT_REVIEW_REQUIRED`

This checkpoint is a research/evidence overlay on G4-CC1. It does not change production ownership, tuning, rhythm admission, tonal policy, or historical G4-I6 / G4-CC1 evidence.

The central result is intentionally narrower than the working hypothesis:

> The shipped Acid reference vocabulary carries a generic detached-vs-connected bass-articulation capability upstream, but the current authoritative generated Acid Synth-A path does not preserve or exercise that capability downstream.

Therefore CC1A closes as a successful measurement checkpoint while the proposed portable `BASS + ARTICULATION` contract remains `REVIEW_REQUIRED`.

## Authoritative state

Starting authority was freshly verified before creating the branch:

- base branch: `research/20260911-01-g4-cc1-acid-techno-funk-contracts`
- verified base SHA: `1a6814af48c6df6824f91e46ee3876e49d54f080`
- CC1 exact-head workflow retained from history: `34548074571`
- new branch: `research/20260911-02-g4-cc1a-acid-structural-minimum`
- first intentional RED workflow: `34551923739`
- first RED job: `103116444261`
- first classification GREEN workflow: `34552095107`
- first classification GREEN job: `103116948448`
- workflow permissions: `Contents: read`, `Metadata: read`

The final exact-head SHA/workflow/job are not embedded as mutable literals in this committed file: doing so would create a new SHA and invalidate its own exact-head claim. The final workflow attestation and remote-HEAD equality are recorded in the external GitHub Actions evidence and in the checkpoint completion response. The invariant for closure is `remote branch HEAD == successful workflow head_sha`.

Production remains read-only for this checkpoint. A final diff gate must prove `src/** changes == 0` relative to the verified CC1 base.

## Representation facts

The first task was to separate musical concepts from the fields that happen to encode them today.

| Musical concept | Production representation | Layer / interpretation |
| --- | --- | --- |
| attack / onset | legal `LaneGrammar` onset spaces upstream; `BassRhythmPlan::onsets`; materialized `SynthStep.note >= 0` at onset coordinates | generic rhythm / authoritative bass plan / physical pattern |
| re-articulation | `GateClass::Short` or implicit `Normal` are upstream intent; downstream independent onsets retrigger a note | generic vocabulary; there is no scalar generic gate-length field in `SynthStep` |
| hold | `LaneGrammar::heldGate` | generic upstream intent; not consumed by the current strong Acid Synth-A reconstruction path |
| continuation | `BassRhythmPlan::continuations` → `BassPitchBehaviorPlan::continuations` → tonal adapter extends the active note into the following cell | generic authoritative bass semantics |
| tie / continue | `LaneGrammar::tieGate` / `GateClass::Tie` upstream | generic intent exists, but shipped Acid reference lanes do not use `tieGate`, and the measured strong Acid path does not consume it |
| slide | `BassPitchBehaviorPlan::slideIntoOnsets` → `SynthStep.slide` | generic tonal materialization; current Acid policy selects `PLAIN`, so measured Acid rows contain no generated slide expression |
| accent | `BassPitchBehaviorPlan::accentOnsets` → `SynthStep.accent` | generic tonal materialization; not evidence for the CC1A continuity contract |
| rest | absence from legal/onset/continuation masks; inactive pattern cell | generic negative event state |
| exact note lifetime | **NOT REPRESENTABLE as one generic scalar in the measured `SynthStep` seam** | lifetime is represented structurally through gate intent/continuation where available, not as a universal duration number |
| TB-303 identity | engine/patch-specific data outside the generic projection | explicitly excluded from CC1A proof |

Two representations must not be conflated:

1. `LaneGrammar.shortGate / heldGate / tieGate` is an upstream capability/intention vocabulary.
2. The authoritative strong migration path rebuilds Synth A from the selected composition bass identity through `BassRhythmPlan` and `BassPitchBehaviorPlan`.

For the three Acid owners, that downstream path currently selects four `kBassDrive` identities (`KickLock`, `OffbeatPush`, `RollingDrive`, `SyncopatedHook`), none of which emits continuation in the bounded probe. Their tonal bass policy also exposes only the `PLAIN` pitch-articulation style.

This is the key representation finding: current Acid `LaneGrammar` metadata and current audible/generated Synth-A articulation are not the same seam.

## Structural projection used by CC1A

The test-side projection deliberately contains no genre label, recipe label, weight, engine, FX, kick relationship, or offset constant.

For an upstream `BassRhythm` lane:

- `INDEPENDENT_ATTACK` is available when a legal bass coordinate can use explicit `Short` or implicit `Normal` intent;
- `CONNECTED_EXTENDED` is available when a legal bass coordinate carries `Held` or `Tie` intent;
- an articulation space is called expressive only when both semantic possibilities exist.

This is a capability projection, not a requirement that every rendered bar exercise both states.

The downstream observation is separate and records:

- independent attack count;
- continuation count;
- slide-into count;
- active Synth-A cells;
- number of observed detached/connected classes.

No quality score or diversity threshold is used.

## Hypotheses

### Option A — OLD CC1

Old shape:

```text
short + held
+ Kick→Bass Respond
+ maxOffset <= 3
```

Verdict: `REJECTED_AS_PORTABLE_CONTRACT`.

The old predicate remains a true description of the eight current CC1 admission edges, but the kick relation and literal grid offset are unrelated to the minimal articulation question, while requiring both exact gate fields confuses one representation with the musical statement.

`maxOffset <= 3` is retained only as an old implementation witness. It is superseded for CC1A purposes and does not occur in the new musical predicate.

### Option B — PER-PATTERN CONTRAST

Hypothesis:

```text
every Acid materialization contains
both detached/re-articulated and connected/extended behavior
```

Verdict: `REJECTED_BY_1152_ROW_CORPUS`.

All `1152 / 1152` authoritative materializations in the bounded corpus are detached-only under the semantic observer. Requiring every Acid idea to be mixed would therefore reject the complete currently shipped generated corpus and is not justified as a minimum.

The research fixtures also explicitly admit a uniform-detached pattern when it lives inside an owner space that is capable of both detached and connected expression.

### Option C — OWNER-SPACE CAPABILITY

Hypothesis:

```text
every shipped Acid owner preserves
articulation/continuity as a meaningful bass-line dimension
```

Verdict: `BEST_FORMULATION_UPSTREAM_ONLY / REVIEW_REQUIRED`.

All three owners and all eight admitted reference-archetype edges expose detached and connected intent in their upstream `LaneGrammar` spaces. However, the authoritative composition/migration path does not currently expose connected expression for these owners: the selected bass-identity set is continuation-free in the bounded probe and the tonal policy is `PLAIN` only.

So this formulation survives the musician-decision and representation-independence tests, but current production evidence does **not** establish it end-to-end. It cannot be promoted to `PROVEN` in CC1A.

## Adversarial evidence

The test-side controls separate per-pattern usage from owner-space capability:

| Fixture | Expected meaning | Result |
| --- | --- | --- |
| uniform re-articulated | legal single-expression pattern inside expressive owner space | PASS |
| staccato-dominant | mostly detached, some connected expression | PASS |
| connected-dominant | mostly connected, some re-attacks | PASS |
| mixed | clear detached ↔ connected contrast | PASS |
| `FLATTEN_ARTICULATION` | preserve legal bass coordinates while collapsing all articulation intent to one implicit normal/re-articulated class | detected, PASS |
| missing bass semantic dimension | remove the `BassRhythm` semantic role | detected, PASS |
| owner label mutation | report label does not enter projection | independent, PASS |
| candidate weight mutation | weighting does not enter capability judgment | independent, PASS |
| kick relationship mutation | relationship topology does not enter articulation judgment | independent, PASS |
| relationship offset mutation | grid-window constants do not enter articulation judgment | independent, PASS |

The semantic flattening mutant is the relevant negative witness for CC1A. The historical CC1 mutants (`remove shortGate`, `remove heldGate`, `remove Kick→Bass Respond`) remain historical controls only.

## Candidate-space evidence

Observed through authoritative owner enumeration:

```text
owners assessed                         3
candidate edges assessed                8
candidate edges expressive upstream     8
owners expressive upstream              3
profile bass-identity edges             12
unique profile bass identities           4
continuation-capable bass edges          0
owners allowing non-PLAIN articulation   0
weight-independence violations           0
```

Owner breakdown:

```text
Acid / BASE          candidates=4  upstream expressive=YES  bass ids=4
Acid / Chicago Jack  candidates=2  upstream expressive=YES  bass ids=4
Acid / Rolling Acid  candidates=2  upstream expressive=YES  bass ids=4
```

Candidate capability is intentionally not treated as materialized usage.

## Materialized observations

Bounded denominator was derived from authoritative enumeration:

```text
3 Acid owners
× identities 1..128
× P1/P2/P3
= 1152 attempted rows
```

Result:

```text
attempted       1152
successful      1152
failed             0
all detached     1152
all connected       0
mixed               0
with continuation   0
with slide          0
>1 class            0
```

Per owner and level:

| Owner | Level | Rows | All detached | All connected | Mixed | Continuation | Slide | >1 class |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| BASE | P1 | 128 | 128 | 0 | 0 | 0 | 0 | 0 |
| BASE | P2 | 128 | 128 | 0 | 0 | 0 | 0 | 0 |
| BASE | P3 | 128 | 128 | 0 | 0 | 0 | 0 | 0 |
| CHICAGO_JACK | P1 | 128 | 128 | 0 | 0 | 0 | 0 | 0 |
| CHICAGO_JACK | P2 | 128 | 128 | 0 | 0 | 0 | 0 | 0 |
| CHICAGO_JACK | P3 | 128 | 128 | 0 | 0 | 0 | 0 | 0 |
| ROLLING_ACID | P1 | 128 | 128 | 0 | 0 | 0 | 0 | 0 |
| ROLLING_ACID | P2 | 128 | 128 | 0 | 0 | 0 | 0 | 0 |
| ROLLING_ACID | P3 | 128 | 128 | 0 | 0 | 0 | 0 | 0 |

The row-level TSV is generated as workflow evidence rather than committed as a new canonical corpus. It records owner, recipe, identity, P-level, selected archetype, selected bass identity, selected pitch-articulation style, attack count, continuation count, slide-into count, active cells, class count, and a compact articulation signature.

The runner executes the CC1A measurement twice and byte-compares both the summary and TSV. The first classification GREEN run passed deterministic repeat.

## P1 / P2 / P3 finding

P-level changes other realization surfaces, but it does not rescue connected Acid bass expression in this bounded seam: every one of the nine owner×level buckets is detached-only.

Therefore CC1A cannot claim that P-level merely varies articulation expression while preserving downstream articulation capability. The only proven statement is narrower: upstream reference metadata preserves an expressive distinction while the current authoritative generated path does not expose it.

## Verdict

CC1A measurement verdict:

`GREEN`.

Portable contract promotion verdict:

`REVIEW_REQUIRED`.

This is not contradictory. The measurement successfully falsified one hypothesis and exposed the seam that prevents promotion.

### Contract candidate

```text
contract_id:
G4-ACID-BASS-ARTICULATION-CAPABILITY

musical_statement:
Within each shipped Acid owner, the authoritative bass idea space should
preserve a real musical choice between independent re-articulation and
connected/extended motion. An individual materialization is not required
to use both choices.

domains:
BASS
ARTICULATION

scope:
owner-space capability for Acid / BASE, Acid / Chicago Jack,
Acid / Rolling Acid

quantifier:
forall shipped Acid owners,
exists an admissible authoritative detached/re-articulated possibility
AND
exists an admissible authoritative connected/extended possibility

predicate_kind:
CAPABILITY

status:
REVIEW_REQUIRED
```

Why not `PROVEN`: the upstream reference vocabulary satisfies this capability shape, but the current authoritative bass identity + pitch behavior + materialization path does not demonstrate the connected half of it.

No production correction is made here. If the project later chooses to make this contract normative, restoring authoritative connected/extended expression is a separate ownership/calibration checkpoint.

## What is actually established

After removing TB-303 timbre, CC1A can still identify a plausible structural Acid axis: the musical distinction between independently re-articulating a bass event and connecting/extending bass motion. The upstream Acid reference vocabulary already models that distinction across all shipped Acid owner edges.

CC1A does **not** establish that this axis currently survives into generated Acid Synth A. In fact, the bounded corpus establishes the opposite for the measured path: current materialization is articulation-collapsed to detached-only behavior.

Accordingly:

- mixed articulation is **not** a per-pattern minimum;
- expressive owner-space is the better contract shape;
- evidence is **not yet sufficient** for the first portable `PROVEN BASS + ARTICULATION` contract because the authoritative owner space does not currently realize the connected side of the proposed capability.

## Limitations

CC1A does not claim:

- a complete definition of Acid;
- that all historical Acid obeys this proposed minimum;
- that satisfying this minimum is sufficient to classify music as Acid;
- that timbre is irrelevant to Acid identity;
- that TB-303-specific slide/accent/engine behavior is generic genre proof;
- an Acid contour contract;
- Phrase-development truthfulness for Acid articulation;
- a production fix for the detected collapse.

Contour, pitch span, repeated pitch classes, and local pitch repetition were not made PASS/FAIL dimensions of CC1A.

## Historical provenance

G4-I6 remains the immutable historical snapshot at `6055114d...` with `PROVEN=4`, `REVIEW_REQUIRED=31`, `UNKNOWN=11904`.

G4-CC1 remains a separate mechanical evidence overlay at `1a6814af...`; CC1A does not rewrite its report or historical counts.

Final closure requires the exact-head workflow to retain G4-R1, G4-I3, G4-I4, G4-I5, G4-I6, and G4-CC1 alongside CC1A, with read-only permissions and a zero-`src/**` diff from the verified CC1 base.

## Stop

After the final exact-head provenance gate succeeds, CC1A stops. It does not begin Techno CC1B, Funk CC1C, registry implementation, production correction, or another genre wave.
