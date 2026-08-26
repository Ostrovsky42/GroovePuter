# 0.9.9-M3-A1 — Harmonic Crossing Contract Audit

Status: **REASONING COMPLETE / EXECUTION PENDING**  
Scope: **RESEARCH / TESTS ONLY**  
Frozen M1 base: `5ad44bb9400ea38d349b7f815f84f833fb18ce6a`  
M1 hardware status: **ACCEPTED / CLOSED**  
Provisional decision: **B — HARMONIC TIMELINE REPRESENTATION GAP**  
Evidence level: **code-review hypothesis until executable characterization passes**

## Verification rule

M3-A1 is not closed by source inspection alone.

The source audit may establish what data is present or absent in the current requests and plans, but behavioral claims A-G become audit-verified only after `tests/run_0_9_9_m3_a1_tests.sh` physically compiles and runs the focused C++ characterization and the frozen M1 regressions.

Until that happens:

- Decision B is provisional;
- A-G below are code-review hypotheses, not runtime observations;
- `0.9.9-M3-T1` must not start as an implementation checkpoint;
- topology/reasoning must not be reported as executable truth.

The focused runner is wired into the pre-existing `Core regressions` pull-request path through `tests/run_generation_stage13_tests.sh`, so the audit no longer relies on a workflow file that exists only on the PR head.

## Scope

This audit asks what harmonic source a melodic phrase should see when its logical identity spans multiple physical bars. It does not change harmonic clocks, profile policy, Genre/BPM ownership, melodic phrase policy, progression policy, or note lifetime.

Ownership remains:

- `ChordRhythm` = physical chord articulation;
- `HarmonicRhythm` = WHEN harmony advances;
- `ChordProgression` = WHAT harmony advances to;
- melodic phrase identity = logical identity independent of physical pattern addresses.

## Exact ancestry

Audit branch:

`research/20260826-08-0.9.9-m3-a1-harmonic-crossing`

Direct frozen base:

`agent/20260826-06-0.9.9-m1l-multibar-melodic-listening @ 5ad44bb9400ea38d349b7f815f84f833fb18ce6a`

Accepted F08/F08.1 are parallel contract evidence, not ancestors of this M1 line:

- accepted F08: `cfb1f9a8e214cfcb823a5e75445f26356b55bed6`;
- F08.1 vocabulary: `ee0fa06e6db0c78f84e85e6d2736db21268d590d`;
- M1/F08 lineage merge-base: `78bc8394ede5e6d81464cff5878c29bbf754c555`.

Therefore the audit distinguishes exact frozen-M1 executable wiring from accepted F08/F08.1 ownership semantics.

## Current owner graph — source evidence

Exact frozen-M1 source flow:

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
ChordRhythmRequest             MelodicMotif / MelodicPitchIntent
barOrdinal                     barOrdinal
        |                          |
        v                          v
ChordRhythmPlan                melodic onsets / continuations / offsets
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

This is the pre-F08 ownership shape present on the frozen M1 code line. It is source evidence only; M3-A1 does not reopen M1 semantics.

## Accepted F08 / F08.1 contract evidence

F08 separates harmonic advancement from physical chord articulation: `HarmonicRhythm` owns WHEN progression advances and does not take `ChordRhythm` as its owner.

F08/F08.1 still do not supply a phrase-wide active harmonic-source carrier. Their phrase coordinates do not by themselves state which progression source is active at the beginning of a later bar.

Therefore accepted policy ownership and missing cross-bar representation are separate questions.

## Exact current source answers

| Question | Frozen M1 source answer |
|---|---|
| phrase-wide harmonic position? | **NO** |
| one-bar progression? | **YES** — `phraseBars = 1` per current materialization |
| current bar ordinal? | **YES** — semantic `barOrdinal` reaches chord/melodic owners |
| current harmonic event ordinal? | **LOCAL ONLY** — derived from the current-bar mask |
| previous/active harmonic source at bar start? | **NO** |
| harmonic context rebuilt per bar call? | **SOURCE SAYS YES** |
| can address independence coexist with a harmonic reset? | **YES, logically** |

`TonalMaterializationRequest` has local `harmonicEventOnsets` plus one bounded `ChordProgressionPlan`; it has no phrase-wide event base, previous source, or phrase harmonic timeline.

`ChordProgressionPlan` is bounded to 8 events.

## Required executable cases

The labels below remain **hypotheses until the focused runner passes**.

### A. Static harmony across four bars

Expected runtime result: repeated static source remains stable across four independent materializations.

Purpose: control case. A pass does not prove moving-harmony continuity.

### B. Harmony changes once per bar

Expected runtime result: four independent `phraseBars = 1`, one-event progression realizations restart at local progression event 0 even when the same progression grammar has later events.

This is the key executable test for the proposed representation gap.

### C. Harmony changes inside a bar

Expected runtime result: local `tonal_materializer` can select multiple harmonic sources within one 16-step bar.

### D. Melodic onset exactly on harmonic change

Source review indicates the boundary scan includes a same-step harmonic onset. Expected runtime result: melodic onset at the change sees the new harmonic source.

### E. Melodic continuation across harmonic change

Source review indicates pitch projection is onset-driven and continuation bits preserve topology. Expected runtime result: a continuation is not independently re-pitched when harmony changes underneath it.

This does not create a cross-bar note-lifetime contract.

### F. Empty melodic bar while harmony advances

Expected runtime result: tonal materialization can be `ValidButEmpty` while harmonic events exist, and no cross-bar source state is produced by the empty melodic result.

This case freezes an important future requirement:

**harmonic advancement must be completely independent of melodic presence or absence.**

It is not enough for a future timeline to remain continuous only between non-empty melodic bars.

The M1 REST HEAVY listening case is the production-proven anchor for this requirement: empty melodic physical slots must not stall, skip, or reset harmonic time. Current pre-F08 wiring also recreates local `ChordRhythm`/`chord.plan.onsets` and a `phraseBars = 1` progression per bar, so melodic emptiness cannot be used as an implicit harmonic clock or state carrier.

M3-T1 must contain an explicit REST-HEAVY-style test where harmonic advancement continues across empty melodic bars.

### G. Same melodic identity in another physical address range

The frozen M1 executable regression materializes the same frozen phrase selection into physical ranges `40..43` and `120..123` and compares musical material for equal `phraseBarOrdinal` values.

Expected M3-A1 runner evidence: that frozen M1 regression physically executes after A-F and passes.

Even a G pass does not by itself prove harmonic continuity: two address ranges could reproduce the same local reset. Physical-address invariance and phrase-wide harmonic-source continuity are separate invariants.

## Provisional crossing contract

If execution confirms A-G, the representation contract to freeze next is:

```text
logical phrase identity
+
phraseBarOrdinal
+
local harmonic/melodic step
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

Required semantic invariants:

1. Harmonic source is keyed by logical phrase coordinates, never `patternAddress`.
2. Every local harmonic event resolves to a phrase-wide source identity/ordinal or an equivalent explicit representation.
3. Physical bar boundaries do not implicitly reset progression source to event 0.
4. Bar start can identify the source left active by prior phrase harmonic events.
5. A melodic onset exactly on a harmonic change uses the new source if D is execution-confirmed.
6. A continuation is not re-pitched merely because harmony changes underneath it if E is execution-confirmed; note lifetime remains separate.
7. Harmonic time advances through empty melodic bars.
8. Static harmony may map all phrase positions to one source.
9. Moving harmony advances independently of physical chord retriggers.
10. Re-materializing one logical phrase at another physical address range produces the same harmonic-source mapping.

M3-A1 deliberately does not freeze a C++ storage type.

## Capacity requirement for M3-T1

Current production rhythm vocabulary is bounded by `kMaxPhraseBars = 4`. M3-A1 must not pretend that current code already supports eight-bar phrase identity.

However, the planned M4 direction is an 8-bar structured phrase (4+4), and accepted F08.1 policy can expose up to four harmonic advances inside one bar. The future representation must therefore be evaluated against this design worst case:

```text
8 bars * 4 harmonic events per bar = 32 phrase harmonic events
```

A representation that is inherently capped at the current `ChordProgressionPlan` capacity of 8, or that is sized only for a four-bar happy path, is not an acceptable M3-T1 design unless it can prove an equivalent bounded encoding that covers the 8-bar / 32-event requirement without later structural replacement.

This is a forward-capacity requirement, not permission to change current `kMaxPhraseBars`, F08.1 policy, or M4 semantics in M3-T1.

## Representation sufficiency — provisional

**CODE-REVIEW RESULT: INSUFFICIENT.**

Available pieces are individually useful:

- M1 has stable phrase identity and `phraseBarOrdinal`;
- F08/F08.1 own bounded harmonic timing independently of chord articulation;
- `ChordProgression` has deterministic harmonic source vocabulary;
- `tonal_materializer` maps local onset step to a local harmonic event.

Source review finds no carrier that tells a later bar which phrase-wide progression source is active.

This becomes audit-verified only if executable B/F and the surrounding characterization pass.

## Policy sufficiency — provisional

**CODE-REVIEW RESULT: NO NEW POLICY GAP DEMONSTRATED.**

Nothing in the audit currently requires:

- new harmonic clocks;
- `kProfiles` changes;
- raw Genre -> clock mapping;
- BPM ownership;
- new progression vocabulary;
- 1/2/4/8 musical policy;
- cross-bar note lifetime.

If execution contradicts the assumed behavior, this section must be revisited before Decision B is frozen.

## Decision state

Current status:

**B — HARMONIC TIMELINE REPRESENTATION GAP — PROVISIONAL / CODE-REVIEW ONLY.**

Why not A, provisionally: no current request carries the prior/source ordinal needed for bar-start harmonic continuity.

Why not C, provisionally: accepted F08/F08.1 ownership/policy is enough to formulate the continuity problem without inventing a new musical choice.

Neither statement is checkpoint-closed until executable characterization confirms current behavior.

## Execution gate

Required command:

```bash
bash tests/run_0_9_9_m3_a1_tests.sh
```

A closing run must physically compile and execute the focused C++ test and show A-G evidence, including at minimum:

```text
A static_4bar=PASS
B one_event_per_bar=RESET_TO_LOCAL_EVENT_0
C inside_bar=PASS
D exact_change_onset=NEW_SOURCE
E continuation_crossing=ONSET_SOURCE_STABLE
F empty_melodic_bar=VALID_BUT_NO_CROSSBAR_SOURCE_STATE
G physical_address_invariance=M1_EXECUTED_PASS
0.9.9-M3-A1 harmonic crossing audit gate: OK
```

Compiler/runtime failure, sanitizer failure, or disagreement with these observations reopens the corresponding claim and may change Decision A/B/C.

## Next checkpoint

Candidate next checkpoint after the execution gate is green:

`0.9.9-M3-T1 — BOUNDED PHRASE HARMONIC TIMELINE CONTRACT`

M3-T1 must:

- freeze representation before production wiring;
- preserve F08/F08.1 policy ownership;
- cover the future 8-bar / 32-event capacity requirement;
- explicitly prove harmonic advancement across REST-HEAVY / empty melodic bars;
- prove physical-address invariance of harmonic-source mapping;
- keep note lifetime and new harmonic policy out of scope.

Until the executable M3-A1 gate is observed green, M3-T1 remains blocked.
