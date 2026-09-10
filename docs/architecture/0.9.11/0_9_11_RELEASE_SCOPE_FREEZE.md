# GroovePuter 0.9.11 — Release Scope Freeze

**Status:** AUTHORITATIVE RELEASE BOUNDARY

**Branch:** `architecture/20260909-0.9.11`

**Scope:** Defines what belongs to 0.9.11, what explicitly does not, and the stop condition for the release.

**Rule for agents:** before proposing or implementing work for 0.9.11, read this file together with `docs/architecture/0.9.11/0_9_11_SHARED_CONTRACTS.md`. If a task is outside the release boundary below, move it to a later release/workstream instead of expanding 0.9.11.

---

## 1. Release thesis

0.9.11 is **not** the release that completes the final GroovePuter workflow.

0.9.11 has one product goal:

> **Make musical material a first-class user-facing entity.**

The user should be able to find a musical part, edit it, safely extend it, try another take, undo the decision, compare safely, and persist/recover it without needing to understand Pattern/Phrase as separate user objects.

The release ends when this vertical slice is trustworthy.

Do not expand 0.9.11 merely because Scene, Song, performance, or richer composition features become architecturally possible.

---

## 2. Product decomposition across releases

The broader product direction contains three different chapters:

```text
A. MATERIAL
   idea -> take -> part -> edit -> persistence -> Undo

B. PERFORMANCE
   scenes -> coherent launch -> QWERTYUI -> A/B/performance

C. COMPOSITION
   development -> Song -> capture performance -> shared/unique arrangement
```

They must not be implemented as one release.

### 0.9.11 — MATERIAL

Owns one complete material vertical slice.

### 0.9.12 — PERFORMANCE

Expected scope:

- Scene model and Scene bank;
- QWERTYUI Scene launch;
- coherent multi-track preparation/activation;
- `USED xN` / shared-reference presentation;
- `MAKE UNIQUE` in performance/Scene context;
- multi-track atomic switching.

These are **not** 0.9.11 blockers.

### 0.9.13 — COMPOSITION

Expected scope:

- Song built on stable Material/Scene contracts;
- recording successful Scene launches into Song;
- development of selected regions/endings;
- arrangement-level Undo.

These are **not** 0.9.11 blockers.

### Later

Candidates deliberately outside the 0.9.11 commitment:

- Scale Lock;
- transpose scope policy;
- richer take history;
- instant/mid-bar audition;
- richer performance gestures;
- broader Scene ownership such as tempo/instrument/FX.

A feature that looks small in UI but requires new ownership, persistence, multi-track causality, or musical-policy contracts is not automatically small in architecture.

---

## 3. 0.9.11 user-facing model

The normal user-facing musical object is:

```text
PART
```

Examples:

```text
BASS · Night 01 · 1 bar
BASS · Night 01 · 4 bars
LEAD · Glass 02 · 2 bars
```

The user should not need to choose between Pattern and Phrase in the ordinary workflow.

### RELEASE CONTRACT UX-01 — ONE USER OBJECT

```text
user-facing object = PART
Pattern/Phrase = internal representation detail
```

Pattern/Phrase may remain internally where required by storage/runtime compatibility.

Length does not determine storage representation.

A one-bar event-based material does not have to become a Pattern merely because its visible length is one bar.

---

## 4. Required 0.9.11 vertical slice

The release must support this complete scenario:

```text
obtain PART
-> edit PART
-> LENGTH 1/2/4/8
-> try another TAKE
-> compare safely
-> UNDO accepted change
-> save
-> reboot/reload
-> recover the same accepted material
```

If this scenario is not complete and trustworthy, 0.9.11 is not complete.

If it is complete, Scenes/Song are not required to keep the release open.

---

## 5. Required capability: PART + LENGTH

0.9.11 exposes `LENGTH` as a property/operation of the user-facing Part.

Supported lengths:

```text
1 / 2 / 4 / 8 bars
```

For 0.9.11, growth only needs the safe default:

```text
REPEAT
```

Example:

```text
1 -> 4
```

must:

- preserve the existing accepted material exactly in the original range;
- fill added bars by deterministic repetition;
- produce one accepted material mutation;
- remain Undo-safe;
- publish to playback only through the safe prepared transition path.

### RELEASE CONTRACT LEN-01 — NO DEVELOP REQUIREMENT

`DEVELOP` is not required for 0.9.11.

Do not block the release on phrase-development musical semantics.

### RELEASE CONTRACT LEN-02 — REPRESENTATION HIDDEN

Changing Part length must not require the user to invoke `MAKE PHRASE` or select a Pattern/Phrase source.

Internal projection/conversion may still occur as an application operation.

---

## 6. Required capability: Material architecture

0.9.11 must close the currently identified middle-layer architecture gap between musical intent and audible runtime state.

Required concepts are defined by `0_9_11_SHARED_CONTRACTS.md`, including:

- `MaterialAddress` as coordinate, not identity;
- stable `MaterialId` semantics;
- exact `MaterialRevision` / MaterialVersion semantics;
- explicit Material resolution states;
- request/version freshness;
- durable publication distinct from audible activation;
- exact prepared NEXT -> ACTIVE publication;
- no filesystem work in the audio path.

### RELEASE CONTRACT MAT-01 — ADDRESS IS NOT OBJECT IDENTITY

0.9.11 must not establish a new public/domain contract where slot/address reuse can silently turn an old reference into a different material object.

### RELEASE CONTRACT MAT-02 — UNKNOWN IS NOT PATTERN

Unavailable, invalid, stale, corrupt, or not-resident material must not silently resolve as Pattern.

### RELEASE CONTRACT MAT-03 — APPLICATION OPERATION BOUNDARY

User intent must not directly become runtime ownership mutation where persistence/resolution is required.

At least the core material operation must follow the bounded pattern:

```text
capture request
-> validate source/target
-> prepare candidate
-> reserve bounded runtime capacity
-> durable publication
-> resolve exact revision
-> stage with request token
-> activate at lawful boundary
-> confirm activation
```

Do not introduce a generic event bus/command framework merely to express this flow.

---

## 7. Required capability: TAKE semantics

0.9.11 must expose the minimum user distinction justified by the generation work:

```text
ANOTHER TAKE
NEW IDEA
```

### ANOTHER TAKE

Means:

> another realization of the currently accepted musical intent.

### NEW IDEA

Means:

> establish a new musical intent/identity before realization.

Do not expose implementation labels such as retry counters, pattern addresses, P1/P2/P3, trajectory numbers, or raw generation attempts as the primary user model.

### RELEASE CONTRACT TAKE-01 — ACCEPTED CONTENT IS CANONICAL

GF2/generation may propose a candidate, but after acceptance the canonical material events belong to Material.

Manual edits are not automatically corrected back toward the original generation contract.

### RELEASE CONTRACT TAKE-02 — G4 MAY CONTINUE IN PARALLEL

Full proof/calibration of every genre/profile is **not** a prerequisite for Material UX.

G4 continues in parallel.

Only the specific semantics required by `ANOTHER TAKE` / `NEW IDEA` must be sufficiently proven for the release slice.

Research UNKNOWN/REVIEW_REQUIRED areas must remain honestly classified; they do not automatically block unrelated material checkpoints.

---

## 8. Required capability: one material Undo

0.9.11 requires one user Undo stream only for accepted **material mutations** in this release.

Required mutations:

- note edits;
- length change;
- accepted take.

### RELEASE CONTRACT UNDO-01 — ONE ACTION, ONE UNDO

One musician-visible accepted operation must require one Undo action.

If generation includes candidate acceptance and material assignment as one user action, Undo returns the complete previous accepted state in one step.

### RELEASE CONTRACT UNDO-02 — RESTORE INTENT WITH MATERIAL

Undo must restore the accepted musical intent/settings needed to make the next `ANOTHER TAKE` semantically consistent with the restored material state.

Do not expand 0.9.11 Undo scope to Scene composition, `MAKE UNIQUE`, or Song arrangement merely for conceptual completeness.

Navigation, playback selection and temporary audition/performance state are not material Undo entries.

Keep the fixed/bounded receipt discipline. Do not raise global Undo memory limits without evidence.

---

## 9. Required capability: safe comparison/publication

0.9.11 must allow the accepted Part and a prepared Take to be compared without reintroducing unsafe source switching.

For the first release:

```text
COMPARE switch
-> lawful bar boundary
```

Transport may continue running.

No mid-bar substitution guarantee is required.

### RELEASE CONTRACT RT-01 — NO MID-BAR PROMISE

Equal length or phase is not sufficient proof that a source swap is musically/runtime safe.

Held notes, continuations, slide and MIDI ownership remain governed by the proven runtime lifetime architecture.

Use existing M3/M4 ACTIVE/NEXT and lifetime ownership rather than bypassing them.

### RELEASE CONTRACT RT-02 — STALE PREPARATION CANNOT WIN

A late result from an older project/session/request/revision must not replace newer user intent in NEXT.

---

## 10. Required capability: persistence for the material slice

Persistence is part of 0.9.11 completeness, not a later polish task.

The release must prove the accepted Part can survive the relevant lifecycle:

```text
create/edit/extend/take
-> durable publish/save
-> reboot/reload
-> same canonical accepted material recovered
```

Required failure cases include at least:

- failure before durable publication;
- failure after payload write but before completed publication;
- failed promotion/validation;
- unavailable/removed SD during a storage operation;
- reload of the accepted durable material.

### RELEASE CONTRACT PERSIST-01 — DURABLE COMMIT != AUDIBLE COMMIT

If durable publication succeeded but activation did not, the system must not claim the material was rolled back.

The result must distinguish at least the semantic difference between:

```text
not created
saved but not activated
queued/prepared
activated
```

Exact UI vocabulary may be refined, but state truth may not be collapsed.

### RELEASE CONTRACT PERSIST-02 — AUDIO HAS NO STORAGE WORK

Filesystem operations, SD waits, decode/CRC work and persistence lookup remain outside the audio callback/path.

---

## 11. Explicit 0.9.11 non-goals

The following are not required for release acceptance:

```text
SCENES
QWERTYUI Scene launch
multi-track Scene activation
USED xN
MAKE UNIQUE
Song arrangement
record Scene launches
DEVELOP / DEVELOP ENDING
arrangement Undo
Scale Lock
scene/part transpose policy
instant/mid-bar audition
large take browser/history UI
8 full takes resident in RAM
```

Research or design work may continue in parallel, but these features must not expand the critical path for 0.9.11.

The existing Hybrid Song/O1 branch may finish already-started bounded characterization/migration work, but its broader Scene/Song product work belongs to later release scope unless a correctness prerequisite for Material is discovered and evidenced.

---

## 12. Memory and CPU contract

Do not promise that resident DRAM is literally identical for projects containing 10 versus 500 persisted materials.

The release invariant is:

> The number of saved materials must not create unbounded resident memory growth; all caches, indexes, queues and working buffers have explicit bounds.

Relevant bounded categories include:

- ACTIVE runtime material;
- NEXT runtime material;
- editable draft;
- generation candidate;
- Undo receipt/state;
- SD I/O staging;
- metadata/index cache.

Persistence may hold more material than the resident cache.

CPU/deadline behavior is also part of correctness.

If NEXT is not ready at the requested lawful boundary:

```text
old ACTIVE remains valid
no partial transition occurs
```

The system may retry at a later lawful boundary only under an explicit current request/token.

---

## 13. Expected implementation checkpoints

Exact checkpoint names may change after code census, but 0.9.11 should remain approximately 4–6 serious checkpoints rather than becoming a broad product epic.

Recommended decomposition:

```text
M1 — Material Identity + Resolution
     address/identity/revision semantics
     explicit resolution

M2 — Canonical Material Operation
     one vertical Pattern/internal source -> user Part mutation path
     durable publication + exact NEXT staging

M3 — PART + LENGTH REPEAT
     user-facing Part
     Pattern/Phrase hidden
     1/2/4/8 safe repeat growth

M4 — TAKE + Material Undo
     ANOTHER TAKE / NEW IDEA
     accepted take
     note/length/take one Undo stream

M5 — Persistence + Failure Recovery
     save/load/reboot
     dirty/published/activated truth
     SD failure paths

M6 — Release Integration Gate
     compare at lawful boundary
     exact SHA
     host + Cardputer + memory + CPU/lifetime regressions
```

Some may combine if evidence shows they are genuinely one bounded change. Do not combine merely to reduce checkpoint count.

---

## 14. Release acceptance scenario

A beginner should be able to perform this scenario without learning Pattern/Phrase:

```text
1. Obtain a musical Part.
2. Hear it playing.
3. Edit a note.
4. Change LENGTH from 1 to 4 using safe repeat growth.
5. Ask for ANOTHER TAKE.
6. Compare the candidate with the accepted Part at a safe boundary.
7. Accept or reject it.
8. Undo the accepted change in one action.
9. Save/persist the Part.
10. Reboot/reload and recover the accepted state.
```

At each step the system must maintain truthful distinctions between:

```text
accepted material
candidate/prepared material
NEXT
ACTIVE
durable state
```

---

## 15. Stop condition

0.9.11 is done when:

> **One Part has stable identity, can be safely edited and grown, can receive another take, can be undone and durably recovered, and can reach ACTIVE through the proven safe runtime path — without the user needing to understand Pattern/Phrase.**

This is the release boundary.

Do not keep 0.9.11 open to add Scenes, Song, Scale Lock, richer history, or performance tricks after this condition is met.

The architecture may become richer internally.

The product model must become smaller externally:

```text
BEFORE
PATTERN
PHRASE
MAKE PHRASE
SOURCE
PLAY
EDIT
Phrase Bank
Song slot
Pattern slot

0.9.11 TARGET
PART
LENGTH
TAKE
UNDO
```

If a beginner still needs a Pattern-vs-Phrase explanation to complete the acceptance scenario, the release has not reached its product goal.
