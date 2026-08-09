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

## Reachability with the shipped reference vocabulary

The Stage 6 API supports bounded 1–4 bar plans, but the reference vocabulary shipped with this Stage 6 candidate intentionally does **not** activate multi-bar evolution yet.

Current `ReferenceVocabulary::catalog()` reachability is:

```text
archetypes                         20
trajectories                        1
trajectory 1 barCount               1
trajectory 1 bars[0]        Statement
archetypes allowing >1 bar        0/20
```

Therefore, against the shipped production catalog:

```text
1 bar  -> eligible Statement trajectory -> valid Stage 6 result
2 bars -> archetype length not allowed  -> InvalidRequest
3 bars -> archetype length not allowed  -> InvalidRequest
4 bars -> archetype length not allowed  -> InvalidRequest
```

For the one reachable bar, `Statement` preserves the independently realized events. The Stage 6 output topology is therefore the same as the base `realizeRhythmPhrase` output, apart from Stage 6 metadata such as the selected trajectory.

The remaining BarFunction semantics are exercised by the Stage 6 synthetic catalog fixture under GCC, Clang and ASan+UBSan. Those tests validate the API/algorithm contracts; they do **not** mean those functions are currently reachable from the shipped reference vocabulary or normal firmware flow.

Catalog expansion has an explicit validation coupling: when an archetype adds a trajectory for a new phrase length, that length must be added to `allowedPhraseBars`, and `MissingTrajectoryCoverage` requires trajectory coverage for every allowed realization level at that length. This is a data-contract gate, not an implicit production wiring change.

Stage 5 remains separately guarded by both of these conditions:

```text
request.phraseBars = 1
no include/call of bar_evolution.h
```

Consequently, adding multi-bar catalog data alone does not make Stage 5 request multi-bar plans. A later production wiring change must be explicit and separately reviewed.

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

Repeat the previous bar and add bounded legal ghost ornamentation only when `maxAdds > 0` and the mutation budget permits additions through `AllowOptionalAdds` or `AllowGhostConversion`.

### Response

Keep the independently realized bar for that phrase coordinate. It therefore shares phrase identity but is not forced to byte-copy the statement. Stage 6 v1 treats Response as trajectory/form metadata rather than a second event-topology transform.

### Reduction

Remove ghost ornamentation and then deterministically remove only non-anchor structural/secondary events while lane minima, density minima and hard relationships remain legal.

### Build

Add bounded legal ghost cues without changing hard structural topology, and only when the mutation budget permits additions.

### Turnaround

Add bounded legal late-bar ghost cues in steps 12–15 when the lane grammar and mutation add budget permit them. `maxAdds == 0` means no cue is added; there is no implicit fallback addition.

### Break

Apply a stronger reduction budget: clear ghost ornamentation and remove legal non-anchor structural/secondary events while preserving all required/canonical anchors and hard invariants.

### Return

Restore the first Statement bar at the rhythm-plan level.

## Deliberate conservative limit

Stage 6 does not yet suppress or displace canonical anchors through `AnchorTransformRule`.

The existing rule types remain available for later refinement, but this stage first proves multi-bar ownership, deterministic trajectory selection and safe bounded reduction/build behavior. This avoids changing established archetype identity while Stage 5 listening acceptance is still recent.

This also means `Reduction` and `Break` can be intentionally subtle on archetypes whose structural identity is dominated by immutable/canonical anchors. A stronger musical break requires a later explicit anchor-transform contract; Stage 6 v1 must not silently remove those anchors.

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

`EvolutionInvalid` is also transactional at the returned value-object boundary: the selected trajectory, evolved plan and phrase identity are committed to `BarEvolutionResult` only after the complete evolved plan passes validation. A failed evolution returns the default plan/identity/trajectory payload plus the failure/base-realization statuses; it never exposes a partially transformed plan.

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
- Response metadata semantics;
- Reduction semantics;
- Build semantics;
- Turnaround late-bar cue semantics;
- Break semantics;
- Return semantics;
- `maxAdds == 0` prevents RepeatWithGhosts/Turnaround additions;
- missing add flags prevent RepeatWithGhosts/Turnaround additions;
- `EvolutionInvalid` does not expose a partial plan/trajectory/identity;
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

The current firmware smoke check proves only that adding the dormant Stage 6 code does not regress existing one-bar Stage 5 behavior or memory stability. Because the shipped reference vocabulary exposes only the one-bar Statement path and normal firmware does not call BarEvolution, that smoke check is **not** semantic validation of multi-bar Stage 6 behavior.

## Stage 6.1 prerequisites before Stage 7

The following review findings are intentionally kept out of the blocker fix but must be resolved before Stage 7 starts:

1. remove the duplicate full catalog validation between `evolveRhythmPhrase` and `realizeRhythmPhrase` through an explicit prevalidated path/handle;
2. make secondary-event drop priority strict by reserving the rank high bit instead of OR-ing it onto an unrestricted 32-bit rank;
3. strengthen Reduction/Break tests to require actual drops when `maxDrops > 0` and to assert the drop count never exceeds budget;
4. add an explicit Statement/Response-vs-base realization equality regression so duplicated gate-policy logic cannot drift silently;
5. keep Response documented/tested as metadata-only until a real response topology transform is designed.

The Stage 6 review also identified stack/runtime follow-ups (stack high-water measurement and worst-case drop-candidate cost). Those are required before BarEvolution is wired into a production caller, even if Stage 7 remains data/runtime-role work rather than a BarEvolution caller.

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
