# GF2-I4 — Musical Corridor Consumers

## Scope

GF2-I4 closes two previously declarative `GenerationCorridor` fields without
creating a new generation owner:

- `densityMin` / `densityMax` become profile-level activity intent;
- `gridSteps` is explicitly classified as unsupported variable-resolution
  capacity in Groove Vocabulary Core v1.

GF2-I4 does not change realization-level ownership, phrase policy, Pattern or
Phrase storage, scheduler behaviour, UI, Genre/Recipe schema, failure-masked
migration handling, or legacy fallback handling.

## Final disposition

| Dimension | Before GF2-I4 | After GF2-I4 |
| --- | --- | --- |
| `density_min_and_max` | `DECLARED_ONLY` | `CONNECTED / CAUSALLY PROVEN` |
| `grid_steps` | `DECLARED_ONLY` | `NEGATIVE CAPACITY / INTENTIONAL LIMIT` |
| `realization_level` | unresolved here | `DEFERRED TO GF2-I5` |
| failure-masked migration result | existing concern | `NOT I4` |
| legacy fallback | existing concern | `NOT I4` |

## Ownership

The ownership chain is deliberately singular:

```text
GenerationProfile / GenerationCorridor
        activity intent
              |
              v
StrongRhythmFrozenSelection / PREPARE
        resolve once
              |
              v
phrase/bar execution plumbing
        carry only
              |
              v
RhythmRealizationRequest
              |
              v
RhythmRealizer
        sole structural-density materializer
```

The semantic responsibilities remain:

- **Profile corridor** = activity intent.
- **`RhythmArchetype::DensityContract`** = legal structural-density vocabulary.
- **`RhythmRealizer`** = sole structural-density materialization consumer.
- **GF2-I3 phrase law / BarEvolution** = temporal evolution owner.

No phrase bar independently re-resolves profile density. Phrase execution only
forwards the already-frozen target into the existing I3 `BarEvolutionRequest`.
BarEvolution may then apply its already-authoritative bar-function semantics.

## Density projection

The profile corridor domain remains the documented integer domain `0..16`.
Validation rejects malformed corridors where `densityMin > densityMax` or
`densityMax > 16`; GF2-I4 does not repair malformed production data.

The profile-level activity point is resolved exactly once as the ceiling of the
corridor midpoint:

```text
profileCenter = ceil((densityMin + densityMax) / 2)
              = (densityMin + densityMax + 1) / 2
```

For the selected archetype, that normalized `0..16` point is projected into the
legal structural interval `[structuralMin, structuralMax]`:

```text
range  = structuralMax - structuralMin
scaled = range * profileCenter

target = structuralMin + round(scaled / 16)
       = structuralMin + (scaled + 8) / 16
```

Therefore the resolved target is always inside the selected archetype's legal
structural-density range. `ornamentMax` is not used as a structural target.
There is no second density realizer and no post-generation hit correction.

## Explicit absence and legacy compatibility

`kNoStructuralDensityTarget = 0xFF` is the explicit absence value carried by
`RhythmRealizationRequest`, `BarEvolutionRequest`, and `PhraseEvolutionRequest`.

When the target is absent, `RhythmRealizer` executes the pre-I4 path literally:

```text
kNoStructuralDensityTarget
        |
        v
archetype.density.structuralPreferred
```

No midpoint is synthesized at the realizer boundary. The focused regression
compares the complete `RhythmRealizationResult` for the same catalog,
archetype, seed, phrase length and realization level and requires byte identity
between the no-intent request and the explicit `structuralPreferred` request.
The inherited one-bar Stage 2 suite is also part of the I4 acceptance gate.

## GRID compatibility proof

Before tightening validation, the authoritative shipped production profile and
recipe catalog (`kProfiles`, enumerated through `availableRecipeCount()` and
`availableRecipeAt()`) was checked in full.

Every shipped production profile expected to remain valid uses:

```text
gridSteps = 16
```

No shipped production profile or recipe depends on `gridSteps = 8` or
`gridSteps = 32`. Those values were permissive declarative validation, not a
production semantic or persistence dependency. GF2-I4 therefore does not create
variable-resolution generation.

Core-v1 validation now requires:

```text
16 -> valid
8  -> rejected as unsupported capacity
32 -> rejected as unsupported capacity
```

The executable corpus regression repeats the shipped-catalog proof rather than
relying only on source inspection.

## Frozen density magnitude contract

The pre-implementation RED fixture remains unchanged:

```text
archetype: SparseFastBreak / 415
level:     P2
seed:      identical
FEEL:      no displacement

Lo-Fi base       density 2..8
Drum & Bass base density 7..15
```

Final measured evidence is:

```text
profile centers:                       5 / 11
projected structural density targets: 11 / 16
structural onsets:                    11 / 16   spread 5
materialized structural drum events:  9 / 13   spread 4
canonical primary anchors:             preserved
```

Acceptance remains at least **4** structural-onset difference and at least
**3** materialized physical structural drum-hit difference per bar. GF2-I4 did
not weaken either threshold after implementation.

## SparseDrift 8-bar admission diagnosis

The first deterministic phrase fixture combined individually valid shipped
pieces but was not itself a valid trajectory-enabled production combination:

```text
Lo-Fi base
+ Manual SparseFastBreak / 415
+ SparseDrift
+ P2
+ request 8 bars
```

At P2, I3 resolves:

```text
SparseDrift -> trajectory 3
trajectory 3 intrinsic barCount = 2
```

For an eight-bar phrase request, production PREPARE deliberately keeps the
existing BarEvolution vocabulary seam bounded to four bars:

```text
requested phraseBars = 8
bounded BarEvolution phraseBars = 4
```

The exact rejecting predicate is the existing `trajectoryRefEligible()` shape
check:

```text
trajectory->barCount == phraseBars
```

Therefore the old fixture reaches `2 != 4` and the trajectory is correctly
rejected. This is an admission result, not a density failure. Production phrase
admission, I3 trajectory tables, SparseDrift's shipped eight-bar length and the
four-bar BarEvolution vocabulary were left unchanged.

The real deterministic shipped-valid path is:

```text
Lo-Fi base
+ Manual SparseFastBreak / 415
+ SparseDrift
+ P3
+ request 8 bars
+ production PREPARE
```

At P3:

```text
SparseDrift -> trajectory 8
trajectory 8 intrinsic barCount = 4
bounded BarEvolution phraseBars = 4
4 == 4 -> admitted
```

Final executable evidence is:

```text
law=SparseDrift
requested_bars=8
plan_bars=4
target=11
trajectory=8
max_bar_difference=8
```

This proves the shipped eight-bar request is preserved while the existing
four-bar BarEvolution seam is reused across it. One frozen profile density
target is carried throughout, phrase law still changes temporal topology, and
density remains independently causal.

`DevelopReturn` and `RepeatReply` are also preserved as distinct executable
phrase laws after density wiring:

```text
DevelopReturn: requested_bars=4 plan_bars=4 target=17 trajectory=6 max_bar_difference=3
RepeatReply:   requested_bars=4 plan_bars=4 target=17 trajectory=5 max_bar_difference=2
```

## Request initialization compatibility

GF2-I4 adds an optional density carrier to:

- `RhythmRealizationRequest`;
- `BarEvolutionRequest`;
- `PhraseEvolutionRequest`.

All production/test/tool call sites are audited by
`tests/test_gf2_i4_request_initializer_contract.py`. Existing request creation
uses empty/default aggregate initialization followed by explicit member
assignment, so the new field receives the sentinel default and does not rebind
an existing positional value. The final audit reports **41 SAFE / 0 UNSAFE**.

## Embedded memory / representation closure

The initial density carrier placement caused `PreparedPhraseArrangement` to
round from 1024 to 1028 bytes through alignment in `StrongRhythmFrozenSelection`.
That was a representation regression, not a reason to raise the embedded memory
budget.

The byte-sized `structuralDensityTarget` was placed next to the existing
byte-sized route field so it occupies existing alignment space. No density
owner, admission rule or musical behaviour changed, and the 1024-byte guard was
not increased.

Final Cardputer ADV compile and fixed-DRAM budget checks both pass. The two
unrelated EOF-only production hunks were also removed; no substantive code was
changed for formatting.

## Final verification

Validated implementation HEAD before this documentation-only closure commit:

```text
38063fd2ec9f9875c63c24386cfc2d85d080a21d
```

Required verification on that exact implementation state:

```text
GF2-I4 focused runner                    PASS
GF2-I1 tempo/corridor arbitration        PASS
GF2-I2 profile FEEL                      PASS
GF2-I2A FEEL amplitude                   PASS
GF2-I3 phrase-law execution              PASS
one-bar RhythmRealizer compatibility     PASS
aggregate initializer contract           PASS (41 SAFE / 0 UNSAFE)
git diff --check                         PASS

HOST                                     PASS
SDL                                      PASS
CARDPUTER_ADV                            PASS
FIXED_DRAM                               PASS
SEQTRAK_MIDI_ONLY                        PASS
exact-SHA GF2 target-validation matrix   PASS
```

## Explicit exclusions

GF2-I4 introduces no new semantic owner and deliberately leaves these areas
untouched:

- GF2-I5 realization-level semantics;
- Pattern/Phrase storage or scheduler behaviour;
- new phrase policy;
- probability axes;
- UI controls;
- Genre/Recipe fields;
- failure-masked migration result handling;
- upstream legacy fallback handling;
- Performance Instrument work;
- Gate B.
