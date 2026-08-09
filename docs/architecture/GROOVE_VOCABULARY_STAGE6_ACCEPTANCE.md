# Groove Vocabulary — Stage 6 BarEvolution Acceptance

Status: implementation candidate / not complete until CI + three clean reviews.

Stage 6 adds a bounded transient 1–4 bar evolution core on top of the Stage 1–5 Groove Vocabulary stack. It does not change normal GENRE MATERIALIZE behavior and does not grant generator code ownership of Scene storage, Song layout or PhraseCore slots.

## Base

Stage 6 is stacked on:

`agent/20260808-12-groove-vocabulary-stage5-strong-path-migration`

at Stage 5 correction head:

`f719f01f8e051ea81f58798f282cdd377614aa37`

The already-hardware-approved Stage 5 one-bar routes remain unchanged.

## Purpose

Add deterministic multi-bar grammar expressed as:

```text
RhythmArchetype
    + eligible BarTrajectory refs
    + RealizationLevel
    + GenerationContext
    ↓
BarEvolution
    ↓
RhythmPhrasePlan[1..4 bars]
```

Stage 6 is a transient planning core. A later caller may materialize the returned phrase into explicit destinations, but BarEvolution never chooses those destinations itself.

## API boundary

New files:

```text
src/generation/rhythm/bar_evolution.h
src/generation/rhythm/bar_evolution.cpp
```

Primary request:

```text
catalog
archetypeId
phraseBars
RealizationLevel
GenerationContext
optional requestedTrajectoryId
optional read-only PhraseRhythmIdentity reuse
```

Primary result:

```text
BarEvolutionStatus
base RealizationStatus
selected TrajectoryId
PhraseRhythmIdentity
RhythmPhrasePlan
```

No heap allocation is introduced.

## Ownership contract

BarEvolution MAY own:

- weighted deterministic trajectory selection;
- BarFunction application to transient rhythm value objects;
- deterministic BarEvolution RNG-domain usage;
- final rhythm invariant validation.

BarEvolution MUST NOT own or choose:

- page;
- bank;
- pattern index;
- Song row;
- PhraseCore slot;
- Scene mutation;
- physical Synth A/B role assignment;
- synth engine TYPE;
- pitch generation;
- persistence/provenance fields.

## BarFunction semantics

Stage 6 v1 uses conservative semantics that preserve the Stage 2 hard rhythm contract.

### Statement

Keep the independently realized bar unchanged.

### Repeat

Repeat the previous legal bar exactly at the rhythm-plan level.

### RepeatWithGhosts

Repeat the previous bar and add bounded legal ghost ornamentation when space/budget permits.

### Response

Keep the independently realized bar for that phrase coordinate. It therefore shares phrase identity but is not forced to byte-copy the statement.

### Reduction

Remove ghost ornamentation and then deterministically remove only non-anchor structural/secondary events while lane minima, density minima and hard relationships remain legal.

### Build

Add bounded legal ghost cues without changing hard structural topology.

### Turnaround

Add bounded legal late-bar ghost cues in steps 12–15 when the lane grammar permits them.

### Break

Apply a stronger reduction budget: clear ghost ornamentation and remove legal non-anchor structural/secondary events while preserving all required/canonical anchors and hard invariants.

### Return

Restore the first Statement bar at the rhythm-plan level.

## Deliberate conservative limit

Stage 6 does not yet suppress or displace canonical anchors through `AnchorTransformRule`.

The existing rule types remain available for later refinement, but this stage first proves multi-bar ownership, deterministic trajectory selection and safe bounded reduction/build behavior. This avoids changing established archetype identity while Stage 5 listening acceptance is still recent.

## Determinism

Trajectory selection uses only:

```text
GenerationContext
RhythmArchetypeId
GenerationDomain::BarEvolution
phraseBars
RealizationLevel
```

The same request must return the same trajectory and plan.

Different `phraseOrdinal` values may select different eligible weighted trajectories while remaining deterministic.

No global `rand()` stream is allowed.

## Transactionality

All work is performed in request/result value objects and scratch copies.

If:

- request validation fails;
- no eligible trajectory exists;
- base realization fails;
- any evolved bar violates lane/protected-space/hard-relationship constraints;

then Stage 6 returns a failure status and has no external storage side effects.

## Stage 5 compatibility

Stage 5 production migration remains explicitly:

```cpp
request.phraseBars = 1;
```

and does not include or call BarEvolution.

Therefore Stage 6 introduces no normal firmware sound/UI behavior change by itself.

## Automated acceptance

The Stage 6 host matrix must pass under GCC, Clang and ASan+UBSan and verify:

- catalog validation for 1–4 bar trajectories;
- deterministic weighted trajectory selection;
- useful selection variation across phrase ordinals;
- explicit trajectory override;
- Statement semantics;
- Repeat semantics;
- RepeatWithGhosts semantics;
- Response semantics;
- Reduction semantics;
- Build semantics;
- Turnaround late-bar cue semantics;
- Break semantics;
- Return semantics;
- final protected-space invariants;
- final lane/density bounds;
- final hard relationship invariants;
- read-only PhraseRhythmIdentity reuse;
- invalid phrase length rejection;
- RealizationLevel/trajectory eligibility rejection;
- no Stage 5 production phrase-length change;
- no storage/runtime ownership imports or mutation calls.

Repository gates on the same final SHA:

- Stage 1 focused suite;
- Stage 2 focused suite;
- Stage 3 reference vocabulary suite;
- Stage 4 materializer/shadow suite;
- Stage 5 migration suite;
- Stage 6 BarEvolution suite;
- SDL build;
- Cardputer ADV normal build + DRAM budget;
- SEQTRAK MIDI-only build;
- Phrase Core;
- Four-axis UI;
- synth persistence.

Known inherited failures must be identified by exact log line and must not be misreported as Stage 6 regressions.

## Hardware acceptance

No new hardware listening gate is required for Stage 6 core itself because this stage is intentionally not connected to normal materialization/storage.

Hardware listening becomes mandatory when a later production caller materializes multi-bar plans.

A firmware smoke check is still required to ensure the added code does not change existing one-bar Stage 5 behavior or memory stability.

## Out of scope

Stage 6 does not implement:

- automatic Song composition;
- automatic PhraseCore writes;
- VoiceRole runtime;
- VoiceRole persistence;
- Bass Generator v2;
- Bass Performance Policy;
- pitch/motif vocabulary;
- weak-genre rehabilitation;
- new Scene fields.

These remain later stages.

## Completion rule

After all required gates are stable on one unchanged SHA, perform three consecutive clean reviews:

1. scope / ownership / diff review;
2. state safety / determinism / runtime-memory review;
3. CI / embedded / acceptance review.

Any finding resets the count to `0/3` and requires a new final SHA before reviews restart.
