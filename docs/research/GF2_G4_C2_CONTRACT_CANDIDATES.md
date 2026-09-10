# GF2 G4-C2 Contract Candidate Ranking

## Boundary

C2 does not promote contracts. It ranks existing REVIEW_REQUIRED owners by structural evidence only.
Genre labels, recipe names, and weights are display metadata and do not affect ranking.
A high rank is not a claim of musical correctness; every candidate remains BLOCKED until an explicit musical contract is justified and tested.

## Ranking method

Priority is lexicographic, not a musical quality score:

1. exact admitted-archetype-set alias to an owner that already has PROVEN evidence;
2. fully observed, plural admitted space contained in one RhythmFamily;
3. fully observed plural spaces spanning multiple RhythmFamily values;
4. singleton admitted spaces last because they cannot distinguish an intentional rule from a degenerate implementation choice.

The `score` column is only the count of four evidence flags: fully observed, plural, single-family, exact proven alias.

## Top five candidates

### 1. Broken / Drum&Bass (mode=7, recipe=2)

- Tier: `A_PROVEN_ADMISSION_ALIAS`
- Candidate predicate: effective admitted archetype set={413,414,415,416}; all RhythmFamily=Breakbeat; admitted space plural
- Evidence: fully_observed=1; exact archetype-set alias to mode=14,recipe=0; rows=384
- Observed bass IDs: 2,5,7,9
- Promotion: `BLOCKED` — `OWNER_EQUIVALENCE_NOT_PROVEN;BASS_VOCABULARY_DIFFERS_FROM_PROVEN_ALIAS`

### 2. Broken / Classic 2-Step (mode=7, recipe=8)

- Tier: `B_SINGLE_FAMILY_PLURAL`
- Candidate predicate: all effective admitted candidates are RhythmFamily=UkTwoStep; admitted space remains plural (2 candidates)
- Evidence: observed all 2/2 admitted archetypes; one RhythmFamily across 384 materialized rows
- Observed bass IDs: 3,4,5,9
- Promotion: `BLOCKED` — `MUSICAL_NECESSITY_NOT_PROVEN`

### 3. Rave / Psytrance (mode=4, recipe=4)

- Tier: `B_SINGLE_FAMILY_PLURAL`
- Candidate predicate: all effective admitted candidates are RhythmFamily=FourFloor; admitted space remains plural (3 candidates)
- Evidence: observed all 3/3 admitted archetypes; one RhythmFamily across 384 materialized rows
- Observed bass IDs: 2,5,7,9
- Promotion: `BLOCKED` — `MUSICAL_NECESSITY_NOT_PROVEN`

### 4. Reggae / Minimal Space (mode=5, recipe=11)

- Tier: `B_SINGLE_FAMILY_PLURAL`
- Candidate predicate: all effective admitted candidates are RhythmFamily=DubPulse; admitted space remains plural (3 candidates)
- Evidence: observed all 3/3 admitted archetypes; one RhythmFamily across 384 materialized rows
- Observed bass IDs: 3,4,6,10
- Promotion: `BLOCKED` — `MUSICAL_NECESSITY_NOT_PROVEN`

### 5. Reggae / BASE (mode=5, recipe=0)

- Tier: `B_SINGLE_FAMILY_PLURAL`
- Candidate predicate: all effective admitted candidates are RhythmFamily=DubPulse; admitted space remains plural (4 candidates)
- Evidence: observed all 4/4 admitted archetypes; one RhythmFamily across 384 materialized rows
- Observed bass IDs: 3,4,6,10
- Promotion: `BLOCKED` — `MUSICAL_NECESSITY_NOT_PROVEN`

## Highest-value next check

Rank 1 is structurally special: its admitted archetype set exactly matches the PROVEN reference owner at mode=14, recipe=0.
That supports a focused admission-equivalence test, but not inheritance of downstream bass semantics.
The observed bass vocabulary differs, so a future checkpoint must keep rhythm admission and bass selection as separate claims.

## Non-claims

- C2 does not infer genre identity from a RhythmFamily label.
- C2 does not use weights as evidence.
- C2 does not turn repeated observation into musical necessity.
- C2 does not change the I6/C1 census or any production generator behavior.
