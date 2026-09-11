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

### 1. Acid / Chicago Jack (mode=0, recipe=6)

- Tier: `C_MIXED_FAMILY_PLURAL`
- Candidate predicate: effective admitted set spans RhythmFamily={FourFloor,SparsePulse}; a family-only predicate is insufficient
- Evidence: observed 2/2 archetypes; family_count=2; rows=384
- Observed bass IDs: 2,5,7,9
- Promotion: `BLOCKED` — `FAMILY_ONLY_PREDICATE_WOULD_BE_OVERBROAD`

### 2. Acid / Rolling Acid (mode=0, recipe=7)

- Tier: `C_MIXED_FAMILY_PLURAL`
- Candidate predicate: effective admitted set spans RhythmFamily={FourFloor,MachineSyncopation}; a family-only predicate is insufficient
- Evidence: observed 2/2 archetypes; family_count=2; rows=384
- Observed bass IDs: 2,5,7,9
- Promotion: `BLOCKED` — `FAMILY_ONLY_PREDICATE_WOULD_BE_OVERBROAD`

### 3. Broken / Dark Skippy (mode=7, recipe=9)

- Tier: `C_MIXED_FAMILY_PLURAL`
- Candidate predicate: effective admitted set spans RhythmFamily={MachineSyncopation,UkTwoStep}; a family-only predicate is insufficient
- Evidence: observed 2/2 archetypes; family_count=2; rows=384
- Observed bass IDs: 3,4,6,8,10
- Promotion: `BLOCKED` — `FAMILY_ONLY_PREDICATE_WOULD_BE_OVERBROAD`

### 4. Hip-Hop / Dusty Jazz (mode=11, recipe=17)

- Tier: `C_MIXED_FAMILY_PLURAL`
- Candidate predicate: effective admitted set spans RhythmFamily={Breakbeat,Funk16}; a family-only predicate is insufficient
- Evidence: observed 3/3 archetypes; family_count=2; rows=384
- Observed bass IDs: 3,4,6,8,9
- Promotion: `BLOCKED` — `FAMILY_ONLY_PREDICATE_WOULD_BE_OVERBROAD`

### 5. Funk/Soul / BASE (mode=12, recipe=0)

- Tier: `C_MIXED_FAMILY_PLURAL`
- Candidate predicate: effective admitted set spans RhythmFamily={Breakbeat,Funk16}; a family-only predicate is insufficient
- Evidence: observed 3/3 archetypes; family_count=2; rows=384
- Observed bass IDs: 3,4,6,8,9
- Promotion: `BLOCKED` — `FAMILY_ONLY_PREDICATE_WOULD_BE_OVERBROAD`

## Non-claims

- C2 does not infer genre identity from a RhythmFamily label.
- C2 does not use weights as evidence.
- C2 does not turn repeated observation into musical necessity.
- C2 does not change the census or any production generator behavior.
