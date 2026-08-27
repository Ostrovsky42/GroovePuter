# 0.9.9 PHRASE-C2 — Minimal A-Onset Cross-Bar Lifetime Producer

## Purpose

Populate the existing `MelodicCrossBarLifetime` semantic carrier for the one topology class frozen by PHRASE-C2-C0: `A_ONSET`.

C2 is observational. It classifies already-realized phrase material and does not create, move, extend, retune, retrigger, or otherwise modify musical events. Physical sustain remains a future PHRASE-R1 concern.

## Frozen predecessor

Authoritative predecessor:

```text
PHRASE-C2-C0
cd0e77a8acdf62e449792964f14899cfa118120b

DECISION A —
NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE

A_ONSET         REACHABLE
A_CONTINUATION  UNREACHABLE
A_OVERLAP       ZERO
```

This branch is a direct descendant of that exact SHA. It is not rebased onto `dev`.

## Exact production owner

Only the existing phrase execution owner changes:

```text
src/generation/migration/phrase_execution.h
src/generation/migration/phrase_execution.cpp
```

`preparePhraseExecution(...)` already performs one production semantic probe per phrase bar into caller-owned one-bar scratch. C2 observes that existing discarded probe, classifies ordinary adjacent phrase boundaries, and populates `PhraseSemanticResult::bars[].melodicLifetime`.

No second phrase engine, second materializer, physical N-bar buffer, retained cursor, heap allocation, Song/Bank owner, transport owner, MIDI owner, or synth-runtime owner is added.

## Semantic observation

C2 uses transient `PhraseMelodicBoundaryObservation` values containing:

```text
phraseGenerationIdentity
phraseBarOrdinal
SemanticSynthBRole
MelodicMotifStatus
admitted melodic onset mask
admitted melodic continuation mask
```

The existing P1R preparation scratch seeds source Synth steps with `slide=false`. In the existing pure-melodic projection paths, semantic continuation cells in this discarded probe are represented by `slide=true`; pure-melodic onsets remain `slide=false` under the controlled probe. C2 decodes only this preparation-local result. It does not inspect retained Song/pattern storage or runtime voice state.

The observation is fail-closed: inconsistent continuation topology or status/material disagreement becomes ineligible.

## Same logical melodic voice

Logical continuity is not inferred merely from physical Synth B ownership.

Both sides must belong to the same `PreparedPhraseExecution`, which owns one frozen phrase selection and one `phraseGenerationIdentity`. The phrase-global composition owner must have:

```text
CompositionSecondaryRole::Melodic
```

The classifier additionally requires equal non-sentinel phrase identity and sequential phrase-bar ordinals.

## A-onset law

For an ordinary boundary `N -> N+1`, C2 sets:

```text
bar[N].melodicLifetime.continuesIntoNextBar = true
bar[N+1].melodicLifetime.entersFromPreviousBar = true
```

only when all of the following are true:

```text
same prepared phrase / same logical melodic voice
same non-sentinel phraseGenerationIdentity
supported effective phrase length >= 2
N+1 is exactly sequential and both ordinals are inside the phrase
outgoing role == Melodic
incoming role == Melodic
outgoing melodic status == Ok
incoming melodic status == Ok
onset/continuation masks are internally non-overlapping
outgoing admitted onset contains logical step15
outgoing admitted continuation does NOT contain logical step15
incoming admitted onset does NOT contain logical step0
incoming admitted onset contains at least one later step
```

StepMask convention remains:

```text
logical step0  = bit15
logical step15 = bit0
```

The law is topology-based. It contains no `PickupPhrase`, `Pivot`, genre, recipe, or archetype special case.

## Explicit excluded classes

All non-proven cases remain false, including:

- `A_CONTINUATION`;
- `A_OVERLAP`;
- outgoing last occupancy at step14 or earlier;
- incoming `ValidButEmpty`;
- incoming onset at logical step0;
- incoming without a later onset;
- `ChordWithMelodicFill`;
- `Chord`;
- different phrase identity;
- non-sequential bars;
- phrase end;
- loop wrap `N-1 -> 0`;
- invalid/unsupported semantic observations.

C2 deliberately defines no onset-vs-continuation precedence and no hybrid arbitration.

## Positive production witnesses

Frozen C2-C0 witnesses are reused; no synthetic positive is required for acceptance.

Minimal / production-default witness:

```text
GenreSettings{}
mode=Acid(0)
recipe=0
AUTO rhythm selection
bars=2
identity=2
boundary=0->1
archetype=405
progression=1
rhythm=PICKUP PHRASE
motif=PIVOT
out_on=0x0009
out_cont=0x0000
in_on=0x0009
in_cont=0x0000
```

`0x0009` is logical step12 + step15. C2 produces the paired lifetime flags while material remains unchanged.

Length coverage reuses:

```text
2 bars: Acid/base, identity=2
4 bars: Acid/base, identity=2
8 bars: Broken/recipe9, identity=0
```

The frozen 8-bar Broken/recipe9 witness also crosses `3->4`; the same law accepts the evolution seam with no seam-specific policy.

Length 1 can never produce a cross-bar lifetime.

## Negative controls

Classifier-level negatives cover step0 replacement, `ValidButEmpty`, step14-or-earlier, hybrid, chord role, different logical voice, different phrase identity, non-sequential bars, loop wrap, A-continuation, and A-overlap.

Frozen M1L controls remain non-crossing:

```text
SPARSE:     N1 / N3 / N1
CALL-style: B / B / B
```

The unchanged P1R LoFi/hybrid lifetime-inert fixture remains all false.

## Random-access behavior

Lifetime is fully prepared before physical random-access materialization and stored only in the existing fixed phrase semantic carrier.

The focused test materializes in scrambled order:

```text
7 -> 0 -> 4 -> 7
```

and requires identical repeated bar-7 material plus unchanged lifetime flags. No mutable previous-bar runtime cursor exists.

## patternAddress firewall

`patternAddress` remains physical storage only.

The focused test materializes the same prepared semantic bar at distinct physical destinations and requires identical musical material. The prepared lifetime carrier is independent of destination address.

## Material invariance proof

The test clones one prepared phrase, clears only `MelodicCrossBarLifetime` in the clone, and materializes every bar from both versions.

It compares DrumPatternSet field-by-field, including automation and groove, and SynthPattern field-by-field, including note, slide, accent, ghost, velocity, timing, FX, FX parameter, and probability.

Required result:

```text
Material(with C2 lifetime) == Material(with lifetime carrier cleared)
```

Progression source identity/period and phrase harmonic timeline cardinality also remain identical.

`materializePreparedPhraseBar(...)` does not execute or consume the lifetime carrier in C2.

## Harmony

C2 does not change:

```text
ChordRhythm
HarmonicRhythm
ChordProgressionSource
PhraseHarmonicClockProjection
PhraseHarmonicTimeline
H2 global harmonic ordinals
```

No note is retuned or retriggered because of the lifetime flag.

## Physical gate and runtime non-goals

Frozen C2-C0 evidence for the production-default Acid witness remains:

```text
preset gateLengthMultiplier = 0.8
runtime Synth-B multiplier  = 1.05
effective gate              = 0.84 step
```

Therefore C2 may say semantic `Continue` while the current runtime still releases before the physical bar boundary. That mismatch is expected until PHRASE-R1.

C2 does not modify gate countdown, NoteOff, AllNotesOff, pattern transition release, Song transport, MIDI lifecycle, internal synth lifecycle, held-note harmony behavior, or runtime continuation/release decisions.

## Memory impact

Retained P1R sizes remain byte-for-byte unchanged:

```text
sizeof(PreparedPhraseExecution)      = 324 B
sizeof(PhraseSemanticResult)          = 82 B
sizeof(StrongRhythmFrozenSelection)   = 48 B
sizeof(ChordProgressionSource)        = 14 B
sizeof(SynthPattern)                 = 112 B
sizeof(DrumPatternSet)              = 1192 B
```

New transient observation:

```text
sizeof(PhraseMelodicBoundaryObservation) = 10 B
```

Preparation uses a fixed array for at most 8 semantic bars: at most 80 B additional transient stack observation storage. It is not retained in `PreparedPhraseExecution`, not persisted, and not heap allocated.

The previous I2 estimate remains informational only and is not C2 policy.

## C2-C0 compatibility

The focused runner recompiles the frozen C2-C0 observation/corpus tests against current production sources.

It requires the complete frozen attempt-0 corpus to remain exactly:

```text
request tuples             75,496,320
admitted phrases           41,418,120
adjacent boundaries        96,729,660
unique signatures             294,725

A_ONSET          17,530,610 / 30,408 unique
A_CONTINUATION            0 / 0
A_OVERLAP                 0 / 0
B                 22,115,006 / 33,632
H                  9,276,932 / 80,113
N0                 1,644,348 / 1,408
N1                21,660,687 / 87,948
N3                   909,477 / 6,091
OTHER             23,592,600 / 55,125
```

It also requires the frozen default-path A-onset count (`59,684`) and frozen intra/seam distribution to remain unchanged.

Thus C2 consumes existing topology rather than changing the topology corpus.

## Hardware

Host semantic validation requires no hardware.

Repository Core CI additionally compiles Cardputer ADV, checks fixed DRAM, builds SDL, and builds the Cardputer ADV SEQTRAK MIDI-only target. C2 adds no hardware-specific path.

## Wiring

None. No Cardputer, SEQTRAK, MIDI cable, external display, or audio connection is required for the C2 semantic test.

## Build / test

Focused host validation:

```sh
bash tests/run_0_9_9_phrase_c2_tests.sh
```

The runner checks exact predecessor/source scope, P1R source guards, GCC, deterministic repeat, Clang, ASan, UBSan, frozen C2-C0 witness and full corpus compatibility, full focused P1R compatibility, and final source scope.

No firmware flash is required for this checkpoint.

## Expected behavior

Focused output must contain at least:

```text
T1 exact predecessor/source scope: OK
T2 known A-onset positive: OK
T3 production-default positive: OK
T4 exact length coverage 1/2/4/8: OK
T5 8-bar 3->4 evolution seam: OK
T10 A-continuation rejected: OK
T11 A-overlap rejected: OK
T14 paired carrier invariant: OK
T15 random access 7->0->4->7 deterministic: OK
T16 patternAddress firewall: OK
T17 lifetime-only material invariance: OK
T18 P1R compatibility: OK
T19 C2-C0 exhaustive corpus compatibility: OK
T20 memory: OK
PHRASE-C2 focused production gate: OK
```

## Troubleshooting

- If `0x0009` is read as steps 0 and 3, the StepMask convention is being interpreted backwards; it is logical steps 12 and 15.
- A rhythm or motif name is never sufficient evidence. Inspect post-admission observed masks.
- A step15 continuation is intentionally rejected even if future vocabulary makes it reachable; that requires a new semantic checkpoint.
- Hybrid Synth B is intentionally rejected; physical voice ownership does not prove logical melodic lifetime ownership.
- A true semantic lifetime flag with an audible early release is expected in C2 because runtime gate execution belongs to R1.
- Any new `src/` file in the C2 delta is a source-scope failure.

## Validation evidence before freeze document

Implementation/content candidate parent:

```text
fc9d62e70cc2a073bce100098c860f9f14708c2c
```

An earlier focused implementation run before the full-corpus T19 reinforcement was terminal GREEN:

```text
33108987350
```

It proved T1-T20 focused behavior, GCC/repeat/Clang/ASan/UBSan, C2-C0 minimal witness compatibility, full focused P1R compatibility, and the retained memory sizes above.

The authoritative full-corpus and broad exact-document-SHA run IDs are generated only after this immutable document commit exists. They are recorded in Draft PR #396 and in the final checkpoint report without modifying this document again.

## Acceptance checklist

- [x] exact predecessor `cd0e77a8acdf62e449792964f14899cfa118120b`
- [x] separate C2 production lineage
- [x] production source scope limited to `phrase_execution.h/.cpp`
- [x] topology-based rule; no rhythm/motif/genre-name rule
- [x] A-onset only
- [x] frozen Acid/base 2-bar identity=2 production witness
- [x] production-default `GenreSettings{}` witness
- [x] positive lengths 2/4/8
- [x] length 1 cannot cross
- [x] natural 8-bar `3->4` seam uses the same law
- [x] A-continuation excluded
- [x] A-overlap excluded
- [x] hybrid/chord excluded
- [x] ValidButEmpty excluded
- [x] incoming step0 excluded
- [x] step14-or-earlier excluded
- [x] phrase end and loop wrap excluded
- [x] paired outgoing/incoming lifetime invariant
- [x] deterministic random access
- [x] patternAddress irrelevant
- [x] musical material invariant
- [x] harmony invariant
- [x] physical gate/runtime untouched
- [x] transport/MIDI untouched
- [x] no physical N-bar retention
- [x] no retained mutable cursor
- [x] no heap
- [x] retained P1R sizes unchanged
- [x] focused GCC/Clang/ASan/UBSan proven on implementation candidate
- [ ] exact document SHA focused/full-corpus gate terminal GREEN
- [ ] exact document SHA Core/broad required gates terminal GREEN
- [x] Draft PR #396 against exact frozen C2-C0 predecessor
- [ ] exact final SHA recorded in PR #396/final checkpoint report
- [x] R1 not started

## Final decision

The semantic decision frozen by this document is intentionally narrow:

```text
DECISION A —
MINIMAL A-ONSET CROSS-BAR LIFETIME PRODUCER READY

scope:
A_ONSET only

next separate checkpoint after terminal exact-document-SHA validation:
0.9.9-PHRASE-R1 — CROSS-BAR LIFETIME RUNTIME EXECUTOR
```

This decision becomes authoritative only when the exact commit containing this document is terminal GREEN on the required focused/full-corpus and broad regression gates. Until then the PR remains Draft and R1 is not authorized.

Because a Git commit hash includes the document contents, the document cannot literally contain its own final SHA without a self-referential hash. The authoritative exact freeze SHA is therefore recorded in PR #396 and the final checkpoint report after terminal validation, without another commit that would move the SHA.
