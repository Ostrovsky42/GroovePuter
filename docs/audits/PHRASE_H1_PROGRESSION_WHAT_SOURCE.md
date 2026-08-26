# 0.9.9-PHRASE-H1 — Progression WHAT Source

Status: **RESEARCH / CONTRACT**  
Base: `f63db9bbc18db32a9cf494dddd6610a3cc403a1b`  
Branch: `research/20260826-11-0.9.9-phrase-h1-progression-what-source`  
Decision: **A — EXISTING CHORDPROGRESSION GRAMMAR IS SUFFICIENT**

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
does not introduce modulo cycling as an M3/Phrase rule. It discovers that cyclic
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

## Frozen old consumer

The current StrongRhythm single-bar migration still does:

```cpp
progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);
progressionRequest.phraseBars = 1;
```

That explains M3-A1's `RESET_TO_LOCAL_EVENT_0` characterization. It is an old
single-bar consumer behavior, not the progression owner's phrase-wide source
policy.

H1 changes none of this code.

## Phrase-level rule for P1

P1 may execute phrase harmonic WHAT only under the following frozen contract:

1. use one frozen phrase selection / generation identity;
2. use the phrase's effective semantic `phraseBars` value;
3. select the progression id/grammar once according to existing
   `ChordProgression` semantics;
4. resolve each global `phraseHarmonicEventOrdinal` against that same selected
   grammar using the owner's existing cyclic lookup;
5. never derive source identity from `patternAddress`, Song row, storage slot,
   local replay count, or melodic-event presence.

A P1 implementation may expose a production arbitrary-ordinal accessor or an
equivalent bounded replay mechanism, but it must preserve this exact rule and
must preserve the legacy 1..8 plan behavior.

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

**DECISION A — EXISTING OWNER SUFFICIENT.**

The C1 source-resolution gap is closed because repository production semantics
already define cyclic access to one selected ChordProgression grammar. The prior
M3 firewall correctly prohibited M3 from inventing that rule; H1 establishes
that the rule already belongs to `ChordProgression` itself.

This authorizes PHRASE-P1 to begin after H1 technical validation is green.
It does not authorize Song/storage publication, lifetime-producing musical
policy, runtime integration, or hardware listening.

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

If the source guard reports a `src/` delta, move that production work to P1 or
reclassify H1 explicitly; do not hide production execution inside this audit.

If `selected->events[index % selected->count]` disappears or changes ownership,
H1 must be reopened before phrase execution continues.

## Acceptance checklist

- [ ] branch is based exactly on frozen C1 `f63db9b...`;
- [ ] H1 has zero `src/` delta;
- [ ] explicit source selection is deterministic;
- [ ] Auto source selection is deterministic;
- [ ] 1..8 materialized prefixes preserve the same source;
- [ ] existing selected grammar repeats under the owner rule;
- [ ] `ChordProgressionPlan` remains capped at 8;
- [ ] phrase length is part of the source-selection coordinate;
- [ ] no physical destination participates in source identity;
- [ ] GCC / repeat / Clang / ASan / UBSan focused gate passes;
- [ ] normal host regression passes;
- [ ] only after all above are green may PHRASE-P1 start.
