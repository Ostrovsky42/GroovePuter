# 0.9.9-PHRASE-H1-F1 — Global Progression Source Representation

## Purpose

Expose the frozen H1 progression WHAT source as a complete bounded public value without changing harmonic policy, phrase clocks, melodic lifetime, or the existing finite `ChordProgressionPlan` contract.

The predecessor gap marker is `H1_SOURCE_PERIOD_NOT_REPRESENTABLE`: a finite eight-event plan cannot be the authoritative phrase-global source for intrinsic periods such as `TwoFiveOne = 3`.

## Hardware

Host-level representation contract. No physical hardware wiring changes. Cardputer ADV build is inherited compatibility validation only.

This checkpoint does not create new audible behavior.

## Wiring

None. No GPIO, I2C, SPI, MIDI, audio, or storage wiring changes are involved.

## Ownership

`src/generation/roles/chord_progression.*` remains the sole owner of progression WHAT vocabulary and source selection.

The private `Grammar` / `GrammarSet`, `selectId(...)`, and `selectGrammar(...)` remain authoritative. H1-F1 adds no progression IDs, grammar events, candidate tables, period tables, seed derivation, family policy, or phrase-clock policy.

`harmonicEventCount` remains HarmonicRhythm WHEN cardinality and is intentionally absent from `ChordProgressionSourceRequest`.

## Source vs Plan

`ChordProgressionSource` is the complete selected WHAT source:

```text
id
period
complete selected grammar events[4]
```

`ChordProgressionPlan` remains the existing finite consumer window:

```text
id
eventCount
events[8]
```

`kMaxHarmonicEvents` remains exactly 8. `plan.eventCount` is not reinterpreted as the intrinsic source period.

## Intrinsic Period

Current authoritative grammar periods are:

```text
StaticModal     1
PedalDrone      1
PopCycle        4
TwoFiveOne      3
ParallelShift   4
MinorFall       4
BorrowedLift    4
```

The source capacity is therefore exactly four events. The public source copies `selected->count` and the complete selected private grammar. No downstream period reconstruction is required.

## Arbitrary Ordinal Access

`chordProgressionEventAt(source, globalHarmonicOrdinal)` performs deterministic random access with no cursor, heap, or mutable iteration state:

```text
source.events[globalHarmonicOrdinal % source.period]
```

The accessor validates the explicit progression ID, intrinsic period against the existing authoritative grammar set, and every active `HarmonicEvent` before returning a value.

The blocker case is explicit:

```text
TwoFiveOne period = 3
eventAt(source, 8) = source.events[2]
```

It is not `finitePlan.events[8 % 8]`.

## Compatibility

Existing `realizeChordProgression(const ChordProgressionRequest&)` externally observable semantics are frozen.

The focused runner compiles the exact predecessor implementation from `74456bcfec0fc74138ec0d8c652dde642c7e16b6` with `git show`, runs a semantic corpus, runs the candidate implementation over the same corpus, and diffs their output.

The corpus covers every explicit progression ID, every current `RhythmFamily` through Auto selection, phrase lengths `1/2/4/8`, harmonic event counts `0..8`, repeated realization, status, selected ID, finite event count, every materialized event, and the legacy plan/result sizes.

No raw `memcmp` is used for semantic equality.

## Memory

All new values are fixed-capacity and trivially copyable. Compile-time budgets are enforced for `HarmonicEvent`, `ChordProgressionSource`, `ChordProgressionSourceRequest`, `ChordProgressionSourceResult`, and `ChordProgressionEventResult`.

The focused test prints exact `sizeof(...)` values for:

```text
HarmonicEvent
ChordProgressionSource
ChordProgressionSourceRequest
ChordProgressionSourceResult
ChordProgressionEventResult
ChordProgressionPlan
ChordProgressionResult
```

The base-vs-candidate compatibility corpus separately proves that the existing `ChordProgressionPlan` and `ChordProgressionResult` sizes did not change.

No `std::vector`, `std::map`, `std::function`, dynamic string ownership, or heap allocation is introduced.

## Build / Test

Focused host gate:

```bash
bash tests/run_0_9_9_phrase_h1_f1_tests.sh
```

It requires and compares:

```text
GCC
repeat GCC
Clang
ASan
UBSan
frozen-H1 vs candidate finite-plan semantic parity
```

After the focused gate is green, the exact candidate SHA must also pass inherited pull-request gates, including Core host, SDL, Cardputer ADV, fixed DRAM, SEQTRAK MIDI-only, and relevant harmonic/tonal/Stage15 matrices.

## Expected Behavior

Focused output must establish:

```text
periods = 1 / 3 / 4
ordinal range = 0..31
TwoFiveOne ordinal 8 = modulo-3 source event 2
explicit and Auto selection = deterministic
harmonicEventCount = not source identity
invalid request/source/event = fail closed
finite ChordProgressionPlan base parity = PASS
```

No screen, transport, MIDI, storage, or audible behavior change is expected.

## Troubleshooting

If the production firewall reports any `src/` file outside `chord_progression.h/.cpp`, stop and reclassify the change instead of broadening H1-F1.

If base parity fails, do not update a golden: the existing finite plan contract has changed and H1-F1 is not additive.

If `TwoFiveOne` ordinal 8 resolves to finite-plan event 0, the predecessor blocker remains unresolved.

If a new grammar needs period greater than four, do not silently increase the source capacity in this checkpoint; perform a new bounded-memory contract review.

## Provenance

Frozen H1 predecessor:

```text
74456bcfec0fc74138ec0d8c652dde642c7e16b6
```

P1R-T0 blocker evidence:

```text
d760dfb8623b9cdad00b5a2d9d60c24ef451f738
```

Semantic blocker marker:

```text
H1_SOURCE_PERIOD_NOT_REPRESENTABLE
```

H1-F1 exists only to make the frozen H1 WHAT-source contract publicly representable without downstream knowledge of private grammar periods.

## Acceptance Checklist

- [x] production ownership remains local to `chord_progression.*`;
- [x] one complete selected source is publicly representable;
- [x] source request contains no `harmonicEventCount`;
- [x] source capacity remains fixed at the evidenced maximum period of 4;
- [x] arbitrary ordinal API is stateless and heap-free;
- [x] periods 1/3/4 and TwoFiveOne ordinal 8 have focused test coverage;
- [x] explicit IDs and Auto families have repeat-determinism coverage for phrase bars 1/2/4/8;
- [x] invalid IDs/family/phrase length/period/events fail closed in focused tests;
- [x] `kMaxHarmonicEvents == 8` is compile-time guarded;
- [x] finite-plan compatibility is compared against exact frozen H1 code;
- [ ] focused GCC / repeat / Clang / ASan / UBSan gate is terminal green on the final SHA;
- [ ] inherited Core host / SDL / Cardputer ADV / fixed DRAM / SEQTRAK MIDI-only gates are terminal green on the final SHA;
- [ ] relevant harmonic / tonal / Stage15 gates are terminal green on the final SHA.

## Decision

The implementation is eligible for:

```text
DECISION A —
GLOBAL PROGRESSION SOURCE REPRESENTATION READY
```

only after every exact-head acceptance gate above is terminal green. Until then the candidate remains un-frozen and does not authorize W1 replay.