# 0.9.9-M3-A1 — Harmonic Crossing Contract Audit

Status: **CLOSED**  
Scope: **RESEARCH / TESTS ONLY**  
Frozen M1 base: `5ad44bb9400ea38d349b7f815f84f833fb18ce6a`  
Executable pre-closure head: `9ddc0389a9454945ac66868beb6d50c7f83b7ea0`  
M1 hardware status: **ACCEPTED / CLOSED**  
Decision: **B — HARMONIC TIMELINE REPRESENTATION GAP**  
Evidence level: **EXECUTABLE CHARACTERIZATION COMPLETE**

## Closure evidence

M3-A1 is closed by executable evidence, not source inspection alone.

Focused workflow:

- workflow: `0.9.9 M3-A1 harmonic crossing audit`;
- run: `32993080660`;
- exact head: `9ddc0389a9454945ac66868beb6d50c7f83b7ea0`;
- conclusion: **SUCCESS**;
- `focused-harmonic-crossing`: **SUCCESS**;
- `core-host-regression`: **SUCCESS**.

Normal Core regressions:

- workflow: `Core regressions`;
- run: `32993080867`;
- exact head: `9ddc0389a9454945ac66868beb6d50c7f83b7ea0`;
- conclusion: **SUCCESS**.

The focused executable log physically printed:

```text
A static_4bar=PASS
B one_event_per_bar=RESET_TO_LOCAL_EVENT_0
C inside_bar=PASS
D exact_change_onset=NEW_SOURCE
E continuation_crossing=ONSET_SOURCE_STABLE
F empty_melodic_bar=VALID_BUT_NO_CROSSBAR_SOURCE_STATE
M3-A1 focused harmonic crossing characterization: OK
G physical_address_invariance=M1_EXECUTED_PASS
Stage 15 Tonal Materializer gate: OK
0.9.9-M3-A1 harmonic crossing audit gate: OK
```

Therefore the former `EXECUTION PENDING` and `PROVISIONAL / CODE-REVIEW ONLY` qualifiers are retired.

## Source-guard correction

The first executable source guard contained a test-only incorrect assumption:

```cpp
uint32_t phraseGenerationIdentity = 0;
```

That is not the frozen M1 production contract.

The exact frozen M1 production fact is:

```cpp
constexpr uint16_t kUnspecifiedPhraseGenerationIdentity = 0xFFFFu;
...
uint16_t phraseGenerationIdentity = kUnspecifiedPhraseGenerationIdentity;
```

Therefore `phraseGenerationIdentity` is a **bounded `uint16_t` logical phrase identity with explicit unspecified sentinel semantics**.

The correction was test-only. M3-A1 does not reinterpret this identity, broaden its ownership, or change production semantics.

## Scope and ownership

This audit asks which harmonic source a melodic phrase sees when its logical identity spans multiple physical bars. It does not implement a phrase harmonic timeline, alter harmonic policy, or change note lifetime.

Ownership remains:

- `ChordRhythm` = physical chord articulation;
- `HarmonicRhythm` / `HarmonicClock` = WHEN harmony advances;
- `ChordProgression` = WHAT harmony advances to;
- melodic phrase identity = logical identity independent of physical pattern addresses.

M3-A1 freezes the representation gap between those owners. It does not create a new musical owner.

## Exact ancestry

Audit branch:

`research/20260826-08-0.9.9-m3-a1-harmonic-crossing`

Direct frozen base:

`agent/20260826-06-0.9.9-m1l-multibar-melodic-listening @ 5ad44bb9400ea38d349b7f815f84f833fb18ce6a`

Accepted F08/F08.1 remain parallel contract evidence rather than ancestors of this frozen M1 line:

- accepted F08: `cfb1f9a8e214cfcb823a5e75445f26356b55bed6`;
- F08.1 vocabulary evidence: `ee0fa06e6db0c78f84e85e6d2736db21268d590d`;
- M1/F08 lineage merge-base: `78bc8394ede5e6d81464cff5878c29bbf754c555`.

## Frozen M1 owner graph

Exact frozen-M1 source flow remains:

```text
StrongRhythmFrozenSelection
        |
        +-- stable realization GenerationContext
        +-- bounded uint16_t phraseGenerationIdentity
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

F08/F08.1 separate harmonic advancement from physical chord articulation, but they do not by themselves provide the phrase-wide active harmonic-source carrier required by a multi-bar melodic phrase.

## Frozen source answers

| Question | Frozen M1 answer |
|---|---|
| phrase-wide harmonic position? | **NO** |
| one-bar progression in current materialization? | **YES** — `phraseBars = 1` |
| current semantic bar ordinal? | **YES** |
| harmonic event ordinal? | **LOCAL ONLY** |
| previous/active harmonic source at later bar start? | **NO** |
| current harmonic context rebuilt per bar call? | **YES — executable B characterizes the reset** |
| physical address required for harmonic identity? | **NO — executable G passes** |

`TonalMaterializationRequest` carries local `harmonicEventOnsets` plus a bounded `ChordProgressionPlan`; it carries no phrase-wide event base, active prior source, or phrase harmonic timeline.

## Executable characterization A–G

### A. Static harmony across four bars

**PASS** — `A static_4bar=PASS`.

Repeated static source remains stable across four independent materializations. This is a control case and does not imply moving-harmony continuity.

### B. One harmonic event per bar

**CHARACTERIZED** — `B one_event_per_bar=RESET_TO_LOCAL_EVENT_0`.

Four independent one-event progression realizations currently restart at local progression event 0. This is the executable evidence for the cross-bar representation gap.

### C. Inside-bar harmonic mapping

**PASS** — `C inside_bar=PASS`.

The local tonal materializer can select multiple harmonic sources within one 16-step bar.

### D. New melodic onset exactly at harmonic change

**NEW_SOURCE** — `D exact_change_onset=NEW_SOURCE`.

A new onset exactly on the harmonic-change step uses the new local harmonic source.

### E. Continuation crossing harmonic change

**ONSET_SOURCE_STABLE** — `E continuation_crossing=ONSET_SOURCE_STABLE`.

A continuation is not independently re-pitched merely because harmony changes underneath it.

This freezes an ownership boundary rather than a new note-lifetime rule:

- M2 owns whether an already-started note remains alive;
- M3 owns which harmonic source is active at phrase time;
- M3 does **not** automatically retrigger, retune, or terminate an already-held note because harmony advanced;
- a future new onset uses the harmonic source active at that onset.

### F. Empty melodic bar while harmony advances

**VALID, NO AUTHORITATIVE CROSS-BAR SOURCE STATE** — `F empty_melodic_bar=VALID_BUT_NO_CROSSBAR_SOURCE_STATE`.

The empty melodic materialization is valid, but current representation does not carry authoritative cross-bar harmonic source state through it.

Therefore melodic emptiness must never become the owner of harmonic time.

Future representation invariant:

**harmonic timeline advancement must be independent of melodic event presence.**

An empty melodic bar must not pause, collapse, renumber, or suppress phrase harmonic event coordinates.

This is a representation requirement, not a new musical policy.

### G. Same melodic identity in another physical address range

**M1_EXECUTED_PASS** — `G physical_address_invariance=M1_EXECUTED_PASS`.

The frozen M1 regression materializes the same logical phrase into different physical address ranges and preserves musical material for equal phrase coordinates.

Physical-address invariance and phrase-wide harmonic continuity are separate invariants: address independence does not by itself repair the local harmonic reset characterized by B.

## Frozen decision

**DECISION B — HARMONIC TIMELINE REPRESENTATION GAP.**

The current pieces are individually sufficient for their existing jobs:

- M1 provides stable logical phrase identity and phrase-bar coordinates;
- F08/F08.1 own bounded harmonic timing independently of chord articulation;
- `ChordProgression` owns harmonic values;
- `tonal_materializer` maps local melodic onset time to local harmonic events.

They are not sufficient as a phrase-wide carrier because a later bar has no authoritative phrase-relative harmonic event/source coordinate that tells it which source is active across the physical boundary.

No new harmonic policy gap is required to reach this decision. If later source resolution exceeds the existing `ChordProgression` contract, that remains a separate explicit gap rather than permission for M3 to invent a policy.

## M3-T1 forward contract — representation only

M3-T1 is **NOT STARTED** by this audit closure.

Its first representation contract must support:

- `phraseBars ∈ {1,2,4,8}`;
- global `phraseBarOrdinal ∈ 0..7`;
- HarmonicClock local capacity up to 4 harmonic event positions per bar;
- worst-case F08.1 phrase harmonic TIME capacity of `8 × 4 = 32` **phrase harmonic event positions**.

Use **event positions** deliberately. Thirty-two phrase harmonic event positions do not necessarily mean thirty-two actual harmonic value transitions; adjacent positions may resolve to the same value according to the existing harmonic owners.

A compact representation may map local bar clock positions into phrase-relative coordinates, but M3-A1 freezes no C++ storage type.

## Three independent capacity domains

### 1. Phrase coordinate capacity

Forward M3-T1 bound:

```text
phraseBars <= 8
phraseBarOrdinal <= 7
phrase harmonic event positions <= 32
```

This is phrase harmonic TIME capacity.

### 2. Chord progression value capacity

Current production contract:

```text
ChordProgressionPlan::kMaxHarmonicEvents = 8
```

This is harmonic VALUE/vocabulary-plan capacity.

**Thirty-two phrase harmonic event positions do not imply that `ChordProgressionPlan` must grow to 32 entries.**

M3-T1 must keep time representation and progression-value storage conceptually separate.

### 3. Physical pattern storage

M4/PhraseCore evidence establishes physical phrase placement capacity:

```text
PhraseCore::kMaxBars = 8
patternRefs[8][3]
```

This proves space for eight physical phrase rows across Drums/SynthA/SynthB references. It does **not** prove capacity for thirty-two phrase harmonic event positions.

Frozen distinction:

```text
PHYSICAL PHRASE STORAGE / PhraseCore / Bank / Song = SUFFICIENT
PHRASE HARMONIC TIMELINE / SEPARATE REPRESENTATION = MISSING
```

## No-new-harmonic-policy firewall

M3-A1 and future M3-T1 must not invent progression extension semantics such as:

- `eventOrdinal % progression.eventCount`;
- implicit cycling;
- nearest-source selection;
- repeat-last;
- clamping;
- implicit progression regeneration;
- any other unowned rule for extending progression values through phrase time.

Ownership remains:

```text
Phrase harmonic timeline = WHEN
ChordProgression = WHAT
```

If phrase harmonic event positions beyond the currently resolvable progression-source contract cannot be mapped without a new rule, M3-T1 must report that as a separate source-resolution gap. It must not hide the gap behind modulo, wrap-around, clamping, regeneration, or another new policy.

## M2 × M3 ownership firewall

M2 owns note lifetime: whether an already-started note remains alive across a boundary.

M3 owns harmonic source/time: which harmonic source is active at a phrase-relative instant.

Therefore harmonic advancement alone does not imply automatic note retrigger, retune, or termination. E execution evidence explicitly characterizes the current onset-driven behavior, while D establishes that a future new onset uses the source active at that onset.

No M2 production semantics are changed here.

## Production boundary

Comparison against the frozen M1 base contains only research workflow, docs, runner wiring, and tests. No `src/` path is changed.

Equivalent required check:

```bash
git diff \
  5ad44bb9400ea38d349b7f815f84f833fb18ce6a...HEAD \
  -- src/
```

Expected and frozen result:

```text
EMPTY
```

Production `src/` semantic delta: **ZERO**.

No production changes were made to:

- `ChordProgressionPlan`;
- `HarmonicRhythm` / `HarmonicClock`;
- `TonalMaterializer`;
- `MelodicPitchIntent`;
- StrongRhythm production semantics.

## Closure

`0.9.9-M3-A1` is **CLOSED**.

Frozen result:

**DECISION B — HARMONIC TIMELINE REPRESENTATION GAP.**

Execution evidence is complete on pre-closure head `9ddc0389a9454945ac66868beb6d50c7f83b7ea0` via focused run `32993080660` and normal Core run `32993080867`.

Next possible checkpoint is `0.9.9-M3-T1 — BOUNDED PHRASE HARMONIC TIMELINE CONTRACT`, but it is not started by this closure.

M3-T1: **NOT STARTED**.  
M3-P1: **NOT STARTED**.
