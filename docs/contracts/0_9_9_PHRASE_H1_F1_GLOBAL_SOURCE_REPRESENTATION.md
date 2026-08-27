# 0.9.9-PHRASE-H1-F1 — Global Progression Source Representation

Status: **CORRECTIVE PRODUCTION CHECKPOINT — FREEZE COMMIT**  
Decision: **A — GLOBAL PROGRESSION SOURCE REPRESENTATION READY**

## Purpose

Make the already-frozen H1 WHAT-source semantics publicly representable without
changing musical policy. P1R-T0 proved that `ChordProgressionPlan` is a finite
expanded consumer window and loses the intrinsic period of a period-3 source
when it is materialized to eight events.

H1-F1 adds a bounded source representation and owner-owned arbitrary-ordinal
accessor. It does not extend progression vocabulary or harmonic time.

## Hardware

No hardware listening is required. H1-F1 changes command-time progression source
representation only; it does not change audio, transport, storage, MIDI, UI, or
hardware I/O.

## Hardware list

None.

## Wiring

None.

## Frozen predecessor

Frozen H1:

- branch: `research/20260826-11-0.9.9-phrase-h1-progression-what-source`
- exact SHA: `74456bcfec0fc74138ec0d8c652dde642c7e16b6`
- decision: **A — PROGRESSION SOURCE CONTRACT READY**

H1 #387 semantic ownership remains valid:

```text
ChordProgression = WHAT harmony advances to
ONE selected progression source per logical phrase
phrase-global harmonic ordinal resolves cyclically through that source
```

## P1R-T0 evidence

Immutable blocker evidence:

- branch: `agent/20260826-15-0.9.9-phrase-p1r-production-execution`
- commit: `d760dfb8623b9cdad00b5a2d9d60c24ef451f738`
- workflow run: `33047917485`
- semantic exit: `42`
- marker: `H1_SOURCE_PERIOD_NOT_REPRESENTABLE`
- production `src/` delta: zero

The old finite-plan projection fails for `TwoFiveOne` after ordinal 7 because an
8-event expanded window is not divisible by intrinsic period 3.

## Ownership

Ownership is unchanged and now representationally explicit:

```text
HarmonicRhythm
owns:
WHEN + number of positions

ChordProgressionSource
owns:
WHAT + intrinsic cycle

ChordProgressionPlan
= finite expanded consumer window

P1R later owns:
projection of phrase-global ordinal
through frozen source
```

There is **ONE authoritative H1 WHAT source** per logical phrase.

`harmonicEventCount` is deliberately absent from
`ChordProgressionSourceRequest`. WHAT source identity continues to use only the
existing H1 selection inputs: requested progression id / Auto, rhythm family,
`GenerationContext`, and `phraseBars`.

## Source vs finite plan distinction

Authoritative source representation:

```text
ChordProgressionSource
  id
  period <= 4
  events[4]
```

It represents the intrinsic selected grammar and supports arbitrary global
harmonic ordinal lookup through the owner-owned accessor.

Existing finite consumer representation remains unchanged:

```text
ChordProgressionPlan
  id
  eventCount <= 8
  events[8]
```

`ChordProgressionPlan::eventCount` remains the finite consumer-window count and
is not reinterpreted as intrinsic period. `kMaxHarmonicEvents` remains 8.

There is still one authoritative private grammar table in
`chord_progression.cpp`; no period/grammar policy table is duplicated in P1R or
another owner.

## Intrinsic period semantics

Frozen current periods, obtained from the same authoritative selected grammars:

```text
StaticModal     1
PedalDrone      1
PopCycle        4
TwoFiveOne      3
ParallelShift   4
MinorFall       4
BorrowedLift    4
```

The public source copies the full selected intrinsic grammar, not a requested
finite prefix.

## Arbitrary ordinal accessor

`chordProgressionSourceEventAt(source, phraseGlobalHarmonicOrdinal, outEvent)`
is owner-owned, allocation-free, stateless, and resolves:

```text
source.events[ordinal % source.period]
```

A malformed/invalid source fails closed. Focused tests cover ordinals `0..31`
for every current progression source and mandatory `TwoFiveOne` ordinals
`0,1,2,7,8,9,11,14,15,17,31`.

`32` is the supported arbitrary-ordinal / representation test range. It is not a
32-element progression expansion and does not enlarge source capacity.

Critical regression:

```text
TwoFiveOne ordinal 8
new source accessor -> intrinsic event 2
old plan.events[8 % plan.eventCount] -> explicitly rejected as source API
```

## Compatibility

Existing `realizeChordProgression()` and `ChordProgressionPlan` semantics are
preserved. The focused runner builds a deterministic legacy-plan corpus for:

- every explicit `ProgressionId` plus `Auto`;
- all production rhythm families;
- `phraseBars = 1/2/4/8`;
- `harmonicEventCount = 0..8`;
- multiple deterministic seeds.

It compiles/runs that same corpus against frozen H1 `74456bc...` in a detached
worktree and requires byte-identical textual semantic output against H1-F1.

No new progression grammar, harmonic policy, F08.1 dependency, 16/32-element
progression expansion, heap allocation, or unbounded state is introduced.

## Memory

No heap is introduced. All new carriers are trivially-copyable fixed-capacity
values. Intrinsic source capacity is four events; legacy plan capacity remains
eight events.

Focused terminal CI measured:

```text
sizeof(HarmonicEvent)                 = 3
sizeof(ChordProgressionSource)        = 14
sizeof(ChordProgressionSourceRequest) = 16
sizeof(ChordProgressionSourceResult)  = 15
sizeof(ChordProgressionPlan)          = 26
sizeof(ChordProgressionResult)        = 27
```

Compile-time guards keep `ChordProgressionSource <= 20` bytes and
`ChordProgressionSourceRequest <= 32` bytes. The legacy plan remains 26 bytes.

Cardputer fixed-DRAM validation is a static/linker memory gate only. It is not a
runtime largest-free-block measurement.

## Build / validation

Focused:

```bash
bash tests/run_0_9_9_phrase_h1_f1_tests.sh
```

The gate runs:

- production-delta firewall;
- GCC;
- deterministic GCC repeat;
- Clang when available;
- ASan;
- UBSan;
- all source periods and ordinals 0..31;
- Auto / variant determinism;
- count-independence;
- P1R-T0 blocker supersession;
- direct frozen-H1 finite-plan compatibility corpus.

Required exact-final-head validation after this doc-only freeze commit:

- focused H1-F1;
- Core host;
- SDL;
- Cardputer ADV;
- fixed DRAM;
- SEQTRAK MIDI-only;
- Stage15 tonal baseline;
- tonal materializer;
- tonal integration;
- final tonal acceptance;
- Tonal Projector;
- global scale.

Queued/running jobs are not PASS. A changed freeze SHA must be validated as an
exact head even though production `src/` is unchanged.

## Expected behavior

Focused output must include:

```text
H1-F1 production delta firewall: OK
H1-F1 TwoFiveOne ordinal8=new_source_event2 old_plan_mod8=rejected
H1-F1 periods static=1 pedal=1 pop=4 twofiveone=3 parallel=4 minorfall=4 borrowed=4
H1-F1 frozen H1 ChordProgressionPlan corpus parity: OK
0.9.9-PHRASE-H1-F1 focused GCC/repeat/Clang/ASan/UBSan gate: OK
```

No screen or audible behavior should change.

## Troubleshooting

If finite-plan corpus parity fails, do not update goldens: characterize the exact
legacy semantic change and classify Decision B.

If `TwoFiveOne` ordinal 8 does not resolve to intrinsic source event 2, the P1R-T0
blocker remains unresolved.

If any production file outside `src/generation/roles/chord_progression.h/.cpp`
changes, stop and justify ownership before proceeding.

If fixed embedded memory requires heap/unbounded state, classify Decision C.

## Provenance / supersession

- H1 #387 semantic WHAT ownership: **VALID / UNCHANGED**.
- H1-F1 source representation: **SUPERSEDES the old representation limitation**.
- P1R-T0 `d760dfb...`: **IMMUTABLE BLOCKER EVIDENCE; RESOLVED BY H1-F1**.
- #390 / H2: downstream only; **NOT REPLAYED YET**.
- W1: **NOT REPLAYED YET**.
- fresh P1R: **NOT STARTED**.
- #388: untouched.

## Acceptance checklist

- [x] exact base is frozen H1 `74456bc...`;
- [x] production delta limited to ChordProgression owner files;
- [x] additive bounded source representation exists;
- [x] intrinsic period is explicit and capped at 4;
- [x] source request has no `harmonicEventCount`;
- [x] arbitrary ordinal accessor is stateless and allocation-free;
- [x] legacy `kMaxHarmonicEvents` remains 8;
- [x] old finite-plan modulo projection is explicitly not the source API;
- [x] focused tests cover all current source periods and ordinals 0..31;
- [x] `TwoFiveOne` mandatory boundary cases are covered;
- [x] deterministic repeat / count-independence are covered;
- [x] frozen-H1 legacy-plan corpus comparison is wired and green on implementation candidate;
- [x] bounded memory sizes are measured and recorded;
- [ ] exact-final-head focused H1-F1 terminal green;
- [ ] exact-final-head Core host terminal green;
- [ ] exact-final-head SDL terminal green;
- [ ] exact-final-head Cardputer ADV terminal green;
- [ ] exact-final-head fixed DRAM terminal green;
- [ ] exact-final-head SEQTRAK MIDI-only terminal green;
- [ ] exact-final-head required Stage15 tonal matrix terminal green;
- [ ] exact final H1-F1 SHA frozen after terminal validation.

## Decision

**Decision A — GLOBAL PROGRESSION SOURCE REPRESENTATION READY.**

The H1-F1 contract is frozen by this documentation-only commit. The exact freeze
SHA is accepted only after every exact-head gate above is terminal green. No
production or test-semantic changes are permitted during that validation pass.

After exact-head terminal green:

```text
H1
semantic WHAT ownership
VALID / unchanged

H1-F1
source representation
SUPERSEDES old representation limitation

P1R-T0
d760dfb...
immutable blocker evidence
RESOLVED BY H1-F1

W1 replay
NOT STARTED

H2 replay
NOT STARTED

fresh P1R
NOT STARTED
```

Then hard stop. Do not create or replay W1/H2/P1R in this checkpoint.
