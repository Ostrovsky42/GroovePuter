# GroovePuter 0.9.11 Architecture Charter

Status: **RESEARCH / ARCHITECTURE ONLY**

Branch: `architecture/20260909-0.9.11`

Provisional base: `fcd0d77da5ed6ef38419547477ab26e77ec6ff26`

Source integration line: PR #451, `integration/20260909-0.9.10-pattern-phrase-runtime-ui-gf2`.

The base is provisional until the 0.9.10 integration candidate is accepted. Before any 0.9.11 implementation checkpoint starts, ancestry must be revalidated against the final accepted 0.9.10 SHA. If the accepted SHA differs, this architecture line is rebased/recreated from the accepted state before implementation work begins.

---

## 1. Purpose

0.9.11 starts with an architecture census, not a feature sprint.

The goal is to classify the current system into four buckets:

- **FIX** — a reachable correctness or invariant failure.
- **REFACTOR** — behavior is currently valid, but ownership/coupling makes the next change unsafe or ambiguous.
- **DEPRECATE / MIGRATE** — legacy state or vocabulary retained only for compatibility.
- **ACCEPT / KEEP** — an intentional embedded compromise or already-healthy boundary that must not be redesigned without evidence.

No item becomes `FIX` because code is ugly. A `FIX` requires a concrete failure path.

No item becomes `REFACTOR` because a desktop architecture would use more abstraction. Embedded constraints are first-class.

---

## 2. Hard branch rule

This branch owns architectural evidence and planning only.

Until a separate implementation checkpoint is explicitly approved:

- no production behavior changes;
- no storage format migration;
- no runtime source/lifetime redesign;
- no UI redesign;
- no GF2 semantic changes;
- no Song rewrite;
- no generic service/repository/command-bus framework;
- no cleanup mixed into research commits.

Allowed changes:

- architecture specs;
- evidence ledgers;
- dependency maps;
- research-only scripts/tests when clearly separated and non-production;
- checkpoint plans after the census is accepted.

---

## 3. Architectural target model

The working target is:

```text
GENERATION / GF2
        |
        v
     MATERIAL
   /          \
PATTERN      MELODY
   \          /
    MaterialRef
        |
        v
PHRASE / SONG arrangement
        |
        v
CONTROL-SIDE RESOLUTION
        |
   ACTIVE / NEXT
        |
        v
BOUNDED RUNTIME MATERIAL
        |
        v
ONE PLAYBACK LIFETIME OWNER
        |
   logical NoteOn/Off
      /        \
     v          v
 internal     MIDI
  synth      backend
```

Persistence is orthogonal to the audio path:

```text
Material identity
   |          |
   v          v
Pattern     Melody
page data   sidecar payload

Scene/project
   -> persistent descriptors/references

Audio path
   -> no filesystem
```

This is a hypothesis to validate against the code, not permission to rewrite the code toward the diagram.

---

## 4. Core invariants

### 4.1 Identity

Every musical material has one canonical identity.

Two distinct materials must never resolve to the same persistent payload by accident.

`residentSlot` is a residency coordinate, not automatically a global identity.

Required semantic distinction:

```text
UNKNOWN      != PATTERN
NOT RESIDENT != PATTERN
LOAD FAILED  != PATTERN
```

Legacy persisted data that historically implies Pattern may explicitly migrate/default to Pattern. Runtime uncertainty may not silently masquerade as Pattern.

### 4.2 Ownership

For each state value, the census must identify one semantic owner.

Especially:

- material identity;
- Pattern/Melody kind;
- active material;
- pending material;
- musical timing policy;
- genre constraints;
- feel interpretation;
- playback note lifetime;
- persistence descriptor;
- UI selection/request state.

Duplicated cached projections are allowed when authority remains singular and synchronization is explicit.

### 4.3 Audio lifetime

The accepted Pattern/Phrase lifetime work is presumed healthy until contradicted by evidence.

A source switch must not create:

- stuck notes;
- duplicate NoteOff;
- two lifetime owners;
- bar-boundary truncation of valid cross-bar notes.

The audio thread must not gain filesystem I/O, unbounded work, or per-activation allocation as part of 0.9.11 cleanup.

### 4.4 Persistence

The census must distinguish:

```text
working truth
persistent snapshot
runtime authority
```

A successful operation must have an explicit durability contract. In particular, `MAKE MELODY` must be classified as either:

- a session mutation whose durability occurs at project/page save; or
- a storage transaction whose reported success survives power loss.

The implementation must not accidentally promise both.

### 4.5 Musical semantics

The architecture must preserve musical decisions rather than engineering knobs.

Working semantic split to test:

```text
Genre     -> laws / possibility space
Feel      -> temporal interpretation
Generator -> realization
Runtime   -> execution only
```

A parameter exposed to the musician should pass the test:

> Would a musician plausibly make this decision while using the instrument?

Implementation terms must not become UI vocabulary merely because they exist in code.

### 4.6 Embedded constraints

The following are not bad patterns by themselves:

- fixed-size arrays;
- bounded buffers;
- preallocated scratch;
- startup-only heap allocation;
- POD state at audio boundaries;
- deliberate duplication used to avoid runtime allocation.

Every proposed refactor must record expected DRAM/heap/stack/code-size impact as:

`0 / lower / higher / unknown`.

---

## 5. Mandatory research hypotheses

Each hypothesis finishes as `CONFIRMED`, `REJECTED`, `PARTIAL`, or `NOT ENOUGH EVIDENCE`.

1. Melody persistent identity omits page/global slot and can collide across pages.
2. Empty-page initialization can retain stale `MaterialKind` descriptors.
3. Invalid/off-page/unresolved material lookup can fail open as Pattern.
4. `SongPosition.patterns[]` no longer matches the Material domain model.
5. `Phrase` names multiple distinct domain/runtime concepts.
6. `Scene` is becoming a universal aggregate with mixed authorities.
7. Genre / Feel / Generator contain overlapping semantic ownership.
8. UI page classes partially act as application transaction coordinators.
9. MORPH state is zombie/decode-only state.
10. `MAKE MELODY` durability semantics are incomplete or ambiguous.
11. Project copy/delete/paging may not fully include Melody sidecar payloads.
12. M3/M4 ACTIVE/NEXT and playback lifetime boundaries are already healthy and should be KEEP rather than redesign.

---

## 6. Required adversarial scenarios

The census must trace these through actual files/functions and, where practical, characterize them with focused tests:

1. same project + same voice + same local slot + different pages;
2. resident Melody -> initialize empty page;
3. Song references an off-page Melody;
4. Melody descriptor exists but payload is missing;
5. Melody payload CRC/format is corrupt;
6. power loss between payload publication and descriptor persistence;
7. page switch while Melody is ACTIVE;
8. Melody NEXT prepared, then page/project changes before boundary;
9. copy project containing promoted Melodies;
10. delete/clear project containing promoted Melodies.

---

## 7. Finding classification contract

Every finding uses this record:

```text
ID:
Title:
Class: FIX | REFACTOR | DEPRECATE | ACCEPT
Severity: P0 | P1 | P2 | P3
Status: CONFIRMED | PARTIAL | REJECTED | NOT ENOUGH EVIDENCE
Current owner:
Files/functions:
Current model:
Failure/repro path:
Expected invariant:
Actual behavior:
Musical/user consequence:
Why this is not merely style:
Minimal direction:
Dependencies:
Test needed:
Memory impact: 0 | lower | higher | unknown
0.9.11 action: MUST | SHOULD | MAY | DEFER
```

A `FIX` without a reachable failure path is invalid and must be downgraded to research/refactor evidence until proven.

---

## 8. Dependency ordering principle

The provisional order is:

```text
identity correctness
        |
persistence consistency
        |
MaterialRef semantics
        |
domain vocabulary
        |
semantic ownership
        |
application/UI boundaries
        |
future Song / GF2 / UX expansion
```

The final order must follow evidence, not this diagram.

A checkpoint may begin only after the preceding invariant it depends on is frozen.

---

## 9. Expected 0.9.11 checkpoint shape

The census should derive bounded checkpoints. Current provisional decomposition:

```text
A - Material Identity Correctness
B - Persistence / Promotion Durability
C - MaterialRef / Arrangement Semantics
D - Domain Vocabulary Migration
E - Genre / Feel / Generator Ownership
F - UI Application Boundary
G - Legacy State Retirement
```

This list is not authoritative until the census closes.

Each implementation checkpoint must later define:

- exact base SHA;
- one ownership boundary;
- invariants;
- non-goals;
- RED characterization;
- GREEN acceptance;
- memory impact;
- regression gates;
- exact-head provenance.

---

## 10. Research completion gate

The architecture census is complete only when it can answer:

1. What is factually incorrect today?
2. What works today but blocks or endangers the next feature layer?
3. What is intentionally correct for an embedded instrument?
4. What is the canonical Material identity?
5. Who owns Pattern/Melody selection?
6. Where exactly does persistence transaction success end?
7. What does `Phrase` mean after Melody exists?
8. What does Song actually reference?
9. Who owns Genre/Feel/Generator musical parameters?
10. Which UI operations are application transactions rather than presentation?
11. Which legacy states are decode-only/deprecated?
12. What is the dependency-ordered 0.9.11 implementation sequence?

Final research state:

```text
0.9.11 ARCHITECTURE CENSUS: COMPLETE
```

or

```text
0.9.11 ARCHITECTURE CENSUS: BLOCKED
reason:
missing evidence:
```
