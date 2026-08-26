# 0.9.9-M3-A1 — Harmonic Crossing Contract Audit

Status: **RESEARCH / TESTS ONLY**  
Frozen M1 base: `5ad44bb9400ea38d349b7f815f84f833fb18ce6a`  
M1 hardware status: **ACCEPTED / CLOSED**  
Decision: **B — HARMONIC TIMELINE REPRESENTATION GAP**

## Scope

This audit asks what harmonic source a melodic phrase must see when its logical identity spans multiple physical bars. It does not change harmonic clocks, profile policy, Genre/BPM ownership, melodic phrase policy, or note lifetime.

The ownership boundary remains:

- `ChordRhythm` = physical chord articulation.
- `HarmonicRhythm` = WHEN harmony advances.
- `ChordProgression` = WHAT harmony advances to.
- melodic phrase identity = logical identity that must not depend on physical pattern addresses.

## Exact ancestry

The audit branch is based directly on the hardware-accepted M1 head:

`agent/20260826-06-0.9.9-m1l-multibar-melodic-listening @ 5ad44bb9400ea38d349b7f815f84f833fb18ce6a`

Accepted F08 and F08.1 are parallel contract evidence, not ancestors of this M1 line:

- accepted F08: `cfb1f9a8e214cfcb823a5e75445f26356b55bed6`
- F08.1 vocabulary: `ee0fa06e6db0c78f84e85e6d2736db21268d590d`
- M1/F08 lineage merge-base: `78bc8394ede5e6d81464cff5878c29bbf754c555`

Therefore this audit must distinguish the exact M1 executable flow from the already accepted F08/F08.1 ownership contract.

## Current M1 owner graph

Exact M1 flow at the frozen base:

```text
StrongRhythmFrozenSelection
        |
        +-- stable realization GenerationContext
        +-- phraseBarOrdinal
        |
        v
semanticBarOrdinal(...)
        |
        +--------------------------+
        |                          |
        v                          v
ChordRhythmRequest             MelodicMotifRequest / MelodicPitchIntentRequest
barOrdinal                     barOrdinal
        |                          |
        v                          v
ChordRhythmPlan                melodic onsets / continuations / degree offsets
physical chord attacks
        |
        +--> onsetCount(chord.plan.onsets)
        |       |
        |       v
        |   ChordProgressionRequest
        |   harmonicEventCount = local chord-onset count
        |   phraseBars = 1
        |       |
        |       v
        |   one-bar ChordProgressionPlan
        |
        +------------------------------------+
                                             |
                                             v
                                  TonalMaterializationRequest
                                  harmonicEventOnsets = chord.plan.onsets
                                  progression = one-bar progression
                                             |
                                             v
                                  local harmonic event index for step
                                             |
                                             v
                                  melodic pitch projection
```

So the frozen M1 executable line still derives harmonic timing from `ChordRhythm`. This is the pre-F08 ownership shape. It is evidence about current wiring only; M3-A1 does not reopen or alter M1 semantics.

## Accepted F08 / F08.1 contract evidence

F08 freezes the corrected ownership: `HarmonicRhythm` owns WHEN progression advances and deliberately has no `ChordRhythm` input. F08 carries `phraseBarOrdinal` and `phraseHarmonicPosition` only as coordinates and explicitly does **not** create a cross-bar temporal carrier or progression state.

F08.1 expands only the bounded one-bar harmonic-clock vocabulary. It still states that phrase coordinates are carried and that a later temporal owner may use them. No cross-bar scheduler/carrier is introduced.

Therefore F08/F08.1 solve the harmonic-policy ownership problem but do not represent a phrase-wide active harmonic source.

## Exact current answers

| Question | Frozen M1 answer |
|---|---|
| phrase-wide harmonic position? | **NO** |
| one-bar progression? | **YES** — `phraseBars = 1` per materialization |
| current bar ordinal? | **YES** — melodic/chord rhythm owners receive semantic `barOrdinal` |
| current harmonic event ordinal? | **LOCAL ONLY** — `tonal_materializer` derives it from the current bar mask |
| previous harmonic source? | **NO** |
| recomputed independently per physical pattern? | **YES** for harmonic progression/context |
| can M1 address-independence hide harmonic resets? | **YES** |

`TonalMaterializationRequest` contains only a local `harmonicEventOnsets` mask plus a bounded `ChordProgressionPlan`; it has no `phraseBarOrdinal`, phrase-wide event base ordinal, previous source, or phrase harmonic timeline.

`ChordProgressionPlan` is bounded to 8 events. F08.1 can legitimately expose four harmonic events in one bar, so a four-bar phrase can require more than one current plan can represent. A single enlarged one-bar plan is therefore not the missing contract.

## Required cases

### A. Static harmony across four bars

**Characterization: sufficient today.**

Static progression materializes one harmonic source (`event[0]`). Recomputing the one-bar plan does not change the source, so all four bars see the same harmonic identity. This is a useful control but does not prove moving-harmony crossing.

### B. Harmony changes once per bar

**Characterization: reset gap.**

With a moving progression and one local harmonic event, four independent `phraseBars = 1` realizations each expose the first progression event. The progression grammar contains later events, but the next physical bar has no source ordinal telling it to begin at event 1, 2, or 3.

Required phrase behavior is not representable by the current request/plan pair.

### C. Harmony changes inside a bar

**Characterization: sufficient locally.**

`tonal_materializer` walks `harmonicEventOnsets` from step 0 through the melodic onset step and chooses the corresponding local progression event. Multiple harmonic sources within one bar are already supported.

### D. Melodic onset exactly on harmonic change

**Characterization: exact boundary already defined.**

A harmonic onset at the same step as the melodic onset is included in the scan (`candidateStep <= melodicStep`), therefore the melodic onset sees the **new** harmonic source.

### E. Melodic continuation across harmonic change

**Characterization: onset source remains authoritative.**

Pitch projection occurs for melodic onsets. Continuation bits preserve topology and are not re-projected merely because a harmonic change occurs underneath them. M3-A1 therefore freezes no retune/retrigger rule. Cross-bar note lifetime remains explicitly out of scope.

### F. Empty melodic bar while harmony advances

**Characterization: exposes the representation gap.**

An empty melodic role can return `ValidButEmpty` while a harmonic timeline exists. The next bar cannot infer the correct harmonic source from melodic events because there were none. Harmonic advancement must therefore be represented independently of melody/materialized note presence.

### G. Same melodic identity in another physical address range

**Characterization: M1 address invariance PASS, harmonic continuity still unproven.**

The frozen M1 test materializes one `StrongRhythmFrozenSelection` into address ranges `40..43` and `120..123` and verifies identical musical material for equal `phraseBarOrdinal` values. This proves physical address is not melodic phrase identity.

It does **not** prove harmonic crossing: both ranges can identically reset their one-bar progression at every bar. Address independence and phrase-wide harmonic-source continuity are separate invariants.

## Frozen crossing contract

M3 needs a logical, bounded phrase-harmonic mapping before tonal projection:

```text
logical phrase identity
+
phraseBarOrdinal
+
local melodic onset step
+
bounded phrase harmonic timeline
        |
        v
phrase-wide harmonic source ordinal
        |
        v
ChordProgression source event
        |
        v
melodic tonal projection
```

Semantic invariants:

1. The harmonic source is keyed by logical phrase coordinates, never by physical `patternAddress`.
2. Every local harmonic event has a phrase-wide source ordinal (or an equivalent explicit source identity).
3. Bar start inherits the source active after prior phrase harmonic events; a physical bar boundary must not implicitly reset to progression event 0.
4. A melodic onset exactly on a harmonic change uses the new source.
5. A melodic continuation is not re-pitched merely because the harmonic source changes underneath it; note lifetime is a separate future contract.
6. Harmonic time advances even through an empty melodic bar.
7. Static harmony may map every bar to the same source.
8. Moving harmony may advance through progression sources without depending on chord retriggers.
9. Re-materializing the same logical phrase into another physical address range yields the same harmonic-source mapping.

M3-A1 deliberately does not freeze a C++ storage type. A later checkpoint may choose a fixed-capacity event timeline, per-bar source base, or another equivalent representation, provided the invariants above are testable and no new harmonic policy is introduced.

## Representation sufficiency

**INSUFFICIENT.**

Existing ingredients are individually useful:

- M1 has stable `phraseBarOrdinal` and phrase identity.
- F08/F08.1 have independent one-bar harmonic clocks.
- `ChordProgression` has deterministic source vocabulary.
- `tonal_materializer` maps local onset step to local harmonic event.

What is missing is the phrase-wide carrier that composes those pieces and tells a later bar which progression source is currently active.

This is not fixed by passing `phraseBarOrdinal` alone: F08/F08.1 explicitly carry that coordinate without assigning cross-bar source state.

## Policy sufficiency

**SUFFICIENT FOR THIS CHECKPOINT.**

No evidence requires new harmonic clocks, `kProfiles` edits, Genre/BPM ownership, or a new progression vocabulary. F08/F08.1 already define the relevant ownership and bounded within-bar clock policy. The unresolved question is how accepted per-bar harmonic events are located on one logical phrase timeline.

That is representation/wiring, not a demonstrated musical-policy deficiency.

## Decision

**B — HARMONIC TIMELINE REPRESENTATION GAP.**

Not A: wiring alone cannot supply the missing prior/source ordinal because no current request carries it.

Not C: no new harmonic choice is required to state or test cross-bar continuity; accepted F08/F08.1 policy can remain unchanged.

## Next checkpoint

Recommended next checkpoint:

`0.9.9-M3-T1 — BOUNDED PHRASE HARMONIC TIMELINE CONTRACT`

It should:

- start from the accepted M1 line plus the accepted F08/F08.1 ownership semantics without changing their policy;
- freeze a fixed-capacity phrase harmonic timeline/source-ordinal representation;
- prove cases A-G, especially empty-bar advancement and physical-address invariance;
- wire that representation to tonal materialization only after the representation contract is frozen;
- keep cross-bar note lifetime out of scope.

No harmonic policy change is justified by M3-A1.
