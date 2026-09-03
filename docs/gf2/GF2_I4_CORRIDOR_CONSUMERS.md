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

Acceptance requires, per bar:

- at least **4** structural-onset difference;
- at least **3** materialized physical structural drum-hit difference;
- canonical kick/backbeat anchors preserved.

GF2-I4 does not weaken these thresholds after implementation.

## Phrase-law compatibility

Density arbitration occurs before phrase execution and is frozen once. The I4
phrase regression proves shipped `DevelopReturn`, `RepeatReply`, and
`SparseDrift` remain causally distinct after density wiring.

`SparseDrift` is tested with its real shipped **8-bar** request. Production I3
continues to use the existing **4-bar BarEvolution vocabulary seam** and maps the
prepared plan across the eight-bar phrase; GF2-I4 does not expand or redesign
BarEvolution vocabulary.

The combined contract is:

```text
one frozen profile density target
+
existing I3 bar-function evolution
=
profile activity remains causal
AND
temporal phrase evolution remains causal
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
an existing positional value. The regression rejects any non-empty positional
aggregate initializer for these request types to prevent a future silent field
shift.

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
