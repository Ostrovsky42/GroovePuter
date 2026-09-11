# GF2 G4-CC1 — Acid / Techno / Funk Contract Coverage Wave

## Status

`EVIDENCE_GREEN / ADVERSARIAL_REVIEW_PENDING`

This checkpoint starts from the closed G4-I6 root:

- branch: `research/20260909-02-0.9.11-g4-i6-global-ownership-census`
- authoritative SHA: `6055114d1a249c73a2acc0e7044456cebf4d8f3c`
- authoritative I6 workflow: `34468078843`
- authoritative I6 job: `102841264401`

CC1 does not rewrite the historical I6 report or its four retained PROVEN statements. It adds a separate evidence layer so new musical contracts can be reviewed before promotion into a later global registry.

## Why these three owners

The wave deliberately spans three different kinds of musical identity instead of repeating one detector shape:

1. **Acid** — cross-role bass/articulation structure.
2. **Techno** — a harmonic prohibition: no harmonic motion.
3. **Funk/Soul** — a downbeat/pocket prohibition: do not lose `the One`.

This follows the project rule that a useful genre contract should survive timbre removal and should preferably state what the genre is not allowed to destroy.

## Contract CC1-A — Acid bass articulation and response

### Applicability

All shipped Acid owners enumerated through the production profile API:

- Acid / BASE
- Acid / Chicago Jack
- Acid / Rolling Acid

No recipe name or candidate weight participates in the predicate.

### Admission predicate

For every effectively admitted rhythm archetype:

1. a `BassRhythm` lane exists;
2. `shortGate != 0`;
3. `heldGate != 0`;
4. there is a `Respond` relationship from `Kick` to `BassRhythm`;
5. that response remains within at most three 16th-note steps (`maxOffset <= 3`).

The contract does **not** require one canonical Acid drum pattern and does not refer to TB-303 timbre. Straight, rolling, syncopated and sparse Acid remain distinct allowed ideas.

### Evidence

```text
owners=3
admitted_candidates=8
violations=0
weight_independence_violations=0
```

Adversarial controls reject removal of the short-gate component, held-gate component, or kick→bass response relation.

### Current evidence status

`EVIDENCE_GREEN / ADVERSARIAL_REVIEW_PENDING`

This checkpoint proves the admission statement only. It does not yet claim a universal downstream SynthStep articulation observer because the current materialized SynthStep carrier does not directly encode the LaneGrammar short/held gate distinction.

## Contract CC1-T — Techno has no harmonic motion

### Applicability

Techno / BASE.

### Profile and selection predicate

Every progression identity admitted by the Techno generation profile must be a static progression:

- `StaticModal`, or
- `PedalDrone`.

Across identities `1..128` and P1/P2/P3 selection, every resolved composition must select one of those static progression identities.

### Materialization predicate

When the selected progression is asked to populate multiple harmonic slots, the chord-progression realizer must return `ValidButStatic` with exactly one harmonic event. Repeated scheduling of the same harmony is not counted as harmonic movement.

This deliberately avoids the false rule “Techno must always be four-on-the-floor”. The current Techno admission space legitimately includes both FourFloor and MachineSyncopation archetypes. The stronger genre prohibition lives in harmonic WHAT, not in one drum mask.

### Evidence

```text
progression_candidates=2
candidate_violations=0
resolved_selections=384
selected_violations=0
```

Adversarial controls accept `StaticModal` and `PedalDrone` and reject moving examples such as `PopCycle` and `BorrowedLift`.

### Current evidence status

`EVIDENCE_GREEN / ADVERSARIAL_REVIEW_PENDING`

## Contract CC1-F — Funk/Soul preserves the One

### Applicability

Funk/Soul / BASE.

### Admission predicate

Every effectively admitted rhythm archetype must declare kick step `0` as a **canonical** anchor. Preferred or optional presence is not enough.

This is intentionally not a `RhythmFamily::Funk16` rule. Funk/Soul currently admits:

- `415 sparse_fast_break`
- `416 halftime_switch`
- `713 funk_house_bridge`

Two of those are Breakbeat-family ideas. The common musical invariant is the protected downbeat, not the family label.

### Materialization predicate

Across identities `1..128` and P1/P2/P3, the materialized kick pattern must contain an onset on step `0`.

### Evidence

```text
admitted_candidates=3
admission_violations=0
weight_independence_violations=0
materializations=384
materialized_violations=0
```

The adversarial control removes step `0` from the canonical kick anchors and requires the detector to reject the mutant.

### Current evidence status

`EVIDENCE_GREEN / ADVERSARIAL_REVIEW_PENDING`

## Mechanical evidence

First complete CC1 run:

- workflow: `34547904354`
- job: `103104384199`
- workflow head: `17a03f2e012e5ccc74b375941194a63db1b282f3`
- conclusion: `SUCCESS`
- permissions: `Contents: read`, `Metadata: read`
- artifact ID: `10179717152`
- artifact SHA256: `96f0bab6a0e1146aa4b42f509d98b8ae0cde216e3f4a016bd09915aa1dacc124`

The same run retained the complete G4-I6 host closure:

```text
profiles=33
archetypes=24
effective_edges=122
roots=4224
materializations=12672
false_owner=0
admission_orphan=0
overbroad_owner=0
structural_collision=0
review_required=31
unknown=11904
I7 candidate=NONE
artifact freshness=UP TO DATE
```

The I6 `review_required=31` value remains unchanged on purpose: CC1 has not mutated the historical I6 contract registry.

## What CC1 does not prove

- It does not prove complete perceptual correctness of Acid, Techno or Funk/Soul.
- It does not claim that these are the only invariants of those genres.
- Acid has no downstream short-vs-held gate observer in this checkpoint.
- Techno's contract does not forbid broken drum topology; that would contradict the current admitted vocabulary and the timbre-removal criterion.
- Funk/Soul's `the One` statement is specifically about canonical kick/downstream kick presence, not a claim that all other roles must attack on beat one.
- Phrase ownership is still outside this wave.
- No production code is changed by CC1.

## Promotion rule

Do not add these statements to a global PROVEN registry merely because the current corpus passes them. Promote a statement only after adversarial review confirms that:

1. the predicate captures a musically meaningful invariant rather than an implementation accident;
2. the negative fixtures remove the relevant musical property rather than unrelated metadata;
3. the quantifier and scope are no broader than the evidence;
4. shared archetypes do not make the statement circular;
5. the contract would still be intelligible as a decision or prohibition to a musician.

## Adversarial review questions

### Acid

- Is short+held gate contrast plus bounded kick→bass response genuinely constitutive of Acid identity after timbre removal, or is one component merely an artifact of the current vocabulary?
- Is `maxOffset <= 3` musically justified, or should the relation be stated more abstractly as local kick/bass dialogue?
- Does the contract accidentally exclude legitimate sparse Acid ideas that should be allowed later?

### Techno

- Is “no harmonic motion” correctly represented as static harmonic WHAT even if harmonic WHEN emits repeated slots?
- Should `PedalDrone` and `StaticModal` remain separate admissible expressions of the same prohibition?
- Is one-bar evidence sufficient for this statement, or should the next test extend across 2/4/8 bars specifically because harmonic motion is a time-level property?

### Funk/Soul

- Is canonical kick on step zero the correct formalization of `the One`, or should the invariant be role-aggregate rather than kick-specific?
- Do the admitted Breakbeat candidates preserve Funk pocket for structural reasons beyond their first kick?
- Does P1/P2/P3 preservation of step zero prove enough, or should accent/velocity hierarchy around the downbeat also enter a later contract?

## Next decision

If adversarial review accepts all three statements, the next engineering checkpoint should promote them into a versioned global contract registry and rerun the global census with domain-aware applicability. If review weakens any predicate, change the contract and its negative controls before touching production ownership tables.
