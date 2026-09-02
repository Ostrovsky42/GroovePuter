# 0.9.9-PHRASE-H1 — Progression WHAT Source

Status: **RESEARCH / CONTRACT**  
Base: `f63db9bbc18db32a9cf494dddd6610a3cc403a1b`  
Branch: `research/20260826-11-0.9.9-phrase-h1-progression-what-source`  
Decision: **A — PROGRESSION SOURCE CONTRACT READY**

## Purpose

Resolve the C1 hard stop:

> when a phrase harmonic timeline addresses a harmonic event position beyond the
> eight-entry `ChordProgressionPlan` carrier, what existing owner supplies WHAT?

H1 does not perform production phrase execution. It identifies and freezes the
existing owner rule so PHRASE-P1 can consume it without inventing musical policy.

## Hardware list

Host compiler/runtime only. Cardputer-Adv hardware is not required for H1 because
there is no runtime, storage, UI, MIDI, or audio integration in this checkpoint.

## Wiring

None.

## Existing owner

`ChordProgression` already owns both:

1. selection of one progression grammar from `ProgressionId`,
   `GenerationContext`, and `phraseBars`;
2. mapping a materialized event index onto that selected grammar.

The existing production implementation is:

```cpp
const HarmonicEvent value = selected->events[index % selected->count];
```

This rule is inside `chord_progression.cpp`, the existing WHAT owner. H1 therefore
does not introduce modulo cycling as an M3/Phrase rule. It records that cyclic
grammar lookup is already the progression owner's production semantics.

## Carrier versus source

These are separate capacities:

```text
ChordProgression grammar
    = authoritative WHAT source
    = currently 1 / 3 / 4 events depending on selected grammar

ChordProgressionPlan
    = bounded materialized carrier
    = at most 8 HarmonicEvent values

PhraseHarmonicTimeline
    = WHEN coordinates
    = at most 32 event positions
```

`kMaxHarmonicEvents == 8` remains unchanged. H1 does not enlarge the plan and
does not store 32 harmonic values.

## Source identity

For a fixed request, progression source identity is deterministic.

- explicit `ProgressionId` remains explicit;
- `Auto` selection is deterministic in `GenerationDomain::ChordPitch`;
- grammar variant selection is deterministic in the same domain;
- `GenerationContext` participates in selection;
- `phraseBars` participates in selection;
- `harmonicEventCount` does **not** participate in grammar selection.

Therefore changing the number of values copied into the bounded plan does not
reselect the underlying source.

## Exactly one source per logical phrase

PHRASE-P1 is authorized to resolve exactly one progression source for the logical
phrase, using the phrase's frozen `ProgressionId`, `GenerationContext`, rhythm
family, and effective semantic `phraseBars`. Per-bar materialization must project
from that source and must not reselect it.

The existing one-bar StrongRhythm consumer remains legacy compatibility behavior:

```cpp
progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);
progressionRequest.phraseBars = 1;
```

That local reset is not authoritative phrase-wide WHAT semantics and H1 changes
none of that code.

## 32-position reachability classification

**B — SYNTHETIC BOUNDED-CAPACITY FIXTURE ONLY.**

The semantic/source contract can represent and deterministically resolve:

```text
8 semantic bars
× 4 harmonic WHEN positions/bar
= 32 phrase harmonic event positions
```

including `phraseHarmonicEventOrdinal == 17`, against one selected progression
grammar.

However the current production profile model contains phrase-law admission and
progression vocabulary, but no production field/owner that admits an exact
8-bar + QuarterCycle harmonic-clock combination. The current StrongRhythm
consumer remains bar-local with `progressionRequest.phraseBars = 1`.

Therefore no authoritative production fixture can honestly be named for
`8 bars × QuarterCycle` at this checkpoint.

This is not a blocker for source capacity. It is a reachability classification.
H1 and P1 must not expand harmonic or phrase-law vocabulary merely to make the
32-position test production-reachable.

The focused H1 synthetic fixture uses eight semantic bars with WHEN positions
at steps `0, 4, 8, 12` in every bar, giving 32 global ordinals. It validates all
ordinals `0..31` and explicitly validates deterministic source resolution for
ordinal `17`.

## Phrase-level rule for P1

P1 may execute phrase harmonic WHAT only under the following frozen contract:

1. use one frozen phrase selection / generation identity;
2. use the phrase's effective semantic `phraseBars` value;
3. select the progression id/grammar once according to existing
   `ChordProgression` semantics;
4. resolve each global `phraseHarmonicEventOrdinal` against that same selected
   grammar using the owner's existing cyclic lookup;
5. never derive source identity from `patternAddress`, Song row, storage slot,
   local replay count, or melodic-event presence;
6. preserve the 32-position fixture label as synthetic-only until an existing
   production profile actually admits the exact combination.

A P1 implementation may expose a production arbitrary-ordinal accessor or an
equivalent bounded frozen-source representation, but it must preserve this rule
and preserve legacy 1..8 `ChordProgressionPlan` behavior.

## Explicit non-owners

The following do not own progression WHAT extension:

- `PhraseHarmonicTimeline` — WHEN only;
- `ChordRhythm` — physical articulation only;
- melodic motif/pitch intent — consumer only;
- physical `patternAddress` — destination only;
- Song/storage — publication only;
- PHRASE-C2 lifetime policy — unrelated;
- PHRASE-I2 transaction policy — unrelated.

## Decision

**DECISION A — PROGRESSION SOURCE CONTRACT READY.**

The C1 source-resolution gap is closed because repository production semantics
already define cyclic access to one selected `ChordProgression` grammar. The
prior M3 firewall correctly prohibited M3 from inventing that rule; H1 records
that the rule belongs to `ChordProgression` itself.

The 32-position semantic/source capacity is frozen as
**SYNTHETIC BOUNDED-CAPACITY FIXTURE ONLY**. This does not authorize new harmonic
vocabulary or claim production reachability.

This authorizes PHRASE-P1 to begin only after this exact H1 head passes focused
and normal host validation. It does not authorize Song/storage publication,
lifetime-producing musical policy, runtime integration, or hardware listening.

## Build / validation

From repository root:

```bash
bash tests/run_0_9_9_phrase_h1_tests.sh
```

The gate compiles with GCC, repeats deterministically, checks Clang parity when
available, and executes ASan and UBSan builds.

## Expected behavior

Expected focused output includes:

```text
A explicit_source=DETERMINISTIC
B auto_source=DETERMINISTIC
C materialized_ordinal=SELECTED_GRAMMAR_CYCLE
D carrier_count=DOES_NOT_RESELECT_SOURCE
E plan_capacity=8_SOURCE_POLICY_SEPARATE
F phrase_bars=SOURCE_SELECTION_COORDINATE
G reachability=SYNTHETIC_BOUNDED_CAPACITY_ONLY positions=32 ordinal17=DETERMINISTIC
PHRASE-H1 progression WHAT source: DECISION_A
0.9.9-PHRASE-H1 progression WHAT source gate: OK
```

The source guard must also print:

```text
PHRASE-H1 source contract: owner=ChordProgression grammar
PHRASE-H1 production src delta: ZERO
```

## Troubleshooting

If prefix parity fails, grammar selection has become dependent on materialized
carrier count and H1 Decision A is no longer valid.

If the source guard reports a `src/` delta, move production work to P1 or
reclassify H1 explicitly; do not hide production execution inside this audit.

If `selected->events[index % selected->count]` disappears or changes ownership,
H1 must be reopened before phrase execution continues.

If a future production profile becomes capable of the exact 8-bar + four-events
per-bar harmonic clock, re-audit reachability; do not silently relabel this
frozen synthetic fixture.

## Acceptance checklist

- [x] branch is based exactly on frozen C1 `f63db9b...`;
- [x] H1 has zero `src/` delta;
- [x] explicit source selection is deterministic;
- [x] Auto source selection is deterministic;
- [x] 1..8 materialized prefixes preserve the same source;
- [x] existing selected grammar repeats under the owner rule;
- [x] `ChordProgressionPlan` remains capped at 8;
- [x] phrase length is part of the source-selection coordinate;
- [x] exactly one source per logical phrase is the P1 contract;
- [x] 32 semantic positions are representable against one source;
- [x] ordinal 17 resolves deterministically;
- [x] 32-position reachability classified as synthetic-only;
- [x] no physical destination participates in source identity;
- [ ] final exact-head GCC / repeat / Clang / ASan / UBSan gate passes;
- [ ] final exact-head normal host regression passes;
- [ ] only after both final exact-head jobs are green may PHRASE-P1 start.
