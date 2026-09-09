# GroovePuter 0.9.11 Architecture Census Ledger

Status: **OPEN / RESEARCH IN PROGRESS**

Charter: `docs/architecture/0.9.11/0_9_11_ARCHITECTURE_CHARTER.md`

This document is the evidence ledger for the 0.9.11 architecture line. It is deliberately stricter than a cleanup backlog: a correctness item is not `FIX` until a concrete failure path is established.

The entries below are **preliminary evidence from the current 0.9.10 integration candidate** and must be revalidated by the active deep research pass before the census is declared COMPLETE.

---

## 0. Current base

```text
branch: architecture/20260909-0.9.11
provisional parent: fcd0d77da5ed6ef38419547477ab26e77ec6ff26
source: PR #451 integration candidate
census state: OPEN
production changes: forbidden
```

---

# A. Preliminary findings

## A1 — Melody persistent identity can alias across pages

```text
CLASS: FIX
SEVERITY: P0
STATUS: PRELIMINARY CONFIRMED — deep research revalidation required
0.9.11: MUST
MEMORY IMPACT: 0 / negligible expected
```

### Current evidence

`src/state/melody_promotion.h` builds Melody sidecar identity from project + voice + resident/local slot. Page/global slot identity is not represented in the path.

The resident slot range is 0..15 while the material model supports paged/global addressing.

### Failure path to prove in census

```text
project P
page 0 / voice A / local slot 5 -> promote Melody X
page 1 / voice A / local slot 5 -> promote Melody Y

if both resolve to the same sidecar path:
Y overwrites/aliases X
```

### Expected invariant

Two distinct MaterialIds never share a persistent payload unless explicitly deduplicated by design.

### Minimal direction

Introduce one canonical Material identity used by persistence. Prefer a representation based on `voice + globalSlot`, with page/local coordinates derived from it, unless deeper repository evidence proves a different stable identity is already authoritative.

### Required test

Adversarial cross-page test with different payload bytes for the same local slot.

### Depends on

None. This is an identity primitive and should precede higher-level Song/UX changes.

---

## A2 — Empty page initialization can retain stale MaterialKind

```text
CLASS: FIX
SEVERITY: P0
STATUS: PRELIMINARY CONFIRMED — deep research revalidation required
0.9.11: MUST
MEMORY IMPACT: 0
```

### Current evidence

`PatternPagingService::initializeEmptyPage(Scene&)` resets resident Pattern banks but preliminary inspection found no corresponding reset of `scene.materialSlots`.

### Failure path to prove

```text
resident page contains:
slot X = MELODY

initialize a new/empty page

expected:
slot X = PATTERN

potential actual:
slot X descriptor remains MELODY while new page pattern payload is empty/default
```

### Expected invariant

Initializing a page initializes both payload and descriptor state belonging to that page.

### Minimal direction

Reset all resident Material descriptors to the canonical empty-page kind as part of the same page initialization operation.

### Required test

Seed all/some descriptors as Melody, call empty-page initialization, assert every descriptor matches the empty-page contract.

### Depends on

A1 only if tests use canonical global identity; the actual stale-reset fix is otherwise independent.

---

## A3 — Unresolved material state can masquerade as Pattern

```text
CLASS: FIX if a reachable wrong-source path is proven; otherwise REFACTOR
SEVERITY: P0/P1 pending caller trace
STATUS: PARTIAL
0.9.11: MUST if wrong-source path is reachable
MEMORY IMPACT: 0 / negligible
```

### Current evidence

Preliminary inspection of `src/state/material_slot_access.h` found APIs that return `Pattern` when the requested material is out of resident range/not locally resolvable.

This conflates semantic states:

```text
Pattern
Unknown
NotResident
Invalid
```

### Required proof

Trace every caller and determine whether any runtime/Song/application path can query an off-page Melody and silently receive Pattern.

### Expected invariant

```text
UNKNOWN      != PATTERN
NOT RESIDENT != PATTERN
LOAD FAILED  != PATTERN
```

### Minimal direction

Use an explicit result such as `optional<MaterialKind>`, `TryMaterialKind`, or a small result enum. Do not introduce a large error framework.

### Required tests

- off-page Melody lookup;
- invalid global slot;
- legacy persisted page without MaterialKind remains explicitly compatible as Pattern.

### Depends on

A1 canonical identity and C1 arrangement reference semantics may affect the final API.

---

# B. Domain/refactor findings

## B1 — Domain vocabulary is split between Pattern / Phrase / Melody / Material

```text
CLASS: REFACTOR + MIGRATE
SEVERITY: P1
STATUS: PRELIMINARY CONFIRMED
0.9.11: SHOULD, after correctness foundations
MEMORY IMPACT: 0 expected
```

### Current evidence

The current codebase contains overlapping meanings around:

- `MaterialKind::{Pattern, Melody}`;
- Pattern/Phrase runtime source selection;
- `PhraseCore::PhraseBank`;
- `RuntimeSynthEventBuffer` used as an event representation;
- UI action `MAKE PHRASE`;
- arrangement-level Phrase concepts;
- `SongPosition.patterns[]`.

### Architectural risk

The word `Phrase` can describe both a musical arrangement-level object and a runtime/event-source representation. `Pattern` can be used as a reference noun even when the referenced slot may hold Melody.

### Working target vocabulary

```text
MATERIAL
  PATTERN = step/grid material
  MELODY  = event material

PHRASE = arrangement-level section/reference structure
SONG   = arrangement sequence

RuntimeSynthEventBuffer = representation, never a domain noun
```

This target must be validated against persisted compatibility requirements before implementation.

### Minimal direction

Freeze canonical vocabulary first; then migrate names at boundaries without rewriting healthy runtime behavior.

### Depends on

A1/A3 and C1.

---

## B2 — Scene is accumulating mixed responsibilities

```text
CLASS: REFACTOR
SEVERITY: P1/P2
STATUS: PARTIAL
0.9.11: SHOULD only where ownership is proven ambiguous
MEMORY IMPACT: unknown
```

### Current evidence

`Scene` currently aggregates persistent musical data plus settings and state across Pattern banks, material descriptors, Phrase/Song state, generator settings, Feel, Genre, FX and other subsystems.

Large size alone is not a defect.

### Research question

Determine which fields are:

```text
persistent project truth
runtime authority
UI state
generation policy
compatibility schema
derived/cache
```

### Refactor threshold

Only split/move a field if multiple components can independently treat it as authority or if a future bounded change requires synchronized mutation across unrelated owners.

Do not decompose Scene into many classes for aesthetics.

### Depends on

Ownership census E1 and persistence census D1.

---

## B3 — Genre / Feel / Generator likely overlap in semantic ownership

```text
CLASS: REFACTOR
SEVERITY: P1
STATUS: PARTIAL
0.9.11: SHOULD
MEMORY IMPACT: 0 / unknown depending on representation
```

### Current evidence

Preliminary inspection found timing/generation-related values distributed among `GeneratorParams`, `FeelSettings`, `GenreSettings`, generation profiles and UI application logic.

Candidate overlap includes swing/microtiming/grid/bar-level generation decisions.

### Target authority hypothesis

```text
Genre     -> laws / possibility space
Feel      -> temporal interpretation
Generator -> realization
Runtime   -> execution only
```

### Required output

Build an ownership matrix for:

- BPM;
- tempo corridor;
- swing;
- microtiming;
- grid;
- density;
- patternBars;
- harmonic rhythm;
- rhythm archetype;
- scale/root;
- velocity/ghost variation.

Every row should identify one semantic owner and any projections/consumers.

### Depends on

GF2 multi-time-scale census and UI application boundary census.

---

## B4 — UI pages contain application orchestration

```text
CLASS: REFACTOR
SEVERITY: P1
STATUS: PRELIMINARY CONFIRMED
0.9.11: SHOULD
MEMORY IMPACT: 0 expected
```

### Current evidence

`src/ui/pages/genre_page.cpp` performs more than presentation/input mapping. Preliminary inspection shows it participates in normalization, apply-mode selection, generation decision, tempo application, runtime/Scene mutation, generation commit selection and user feedback.

### Why this matters

A UI Page becomes a hidden application service. Song automation, MIDI control or another UI surface would need to duplicate or call UI-owned semantics.

### Minimal target

```text
UI gesture
  -> musician-level intent
  -> bounded application/domain operation
  -> explicit result/receipt
  -> UI presentation
```

Do not introduce a generic command bus.

### Depends on

B3 semantic ownership. The application boundary cannot be cleanly extracted before musical authority is known.

---

# C. Arrangement/material-reference findings

## C1 — Song naming/reference model is Pattern-centric after Material introduction

```text
CLASS: REFACTOR / possible FIX after caller trace
SEVERITY: P1
STATUS: PARTIAL
0.9.11: SHOULD
MEMORY IMPACT: 0 / negligible expected
```

### Current evidence

`SongPosition` still exposes `patterns[]` while a resident slot can now contain Pattern or Melody.

### Research questions

1. Does Song reference a physical Pattern, a slot, or a Material?
2. Can a Song position lawfully resolve Melody?
3. Is page residency part of the reference or only of resolution?
4. Can off-page Melody resolve as Pattern because of A3?
5. Who prepares/loads the material before activation?

### Target hypothesis

Conceptually migrate Song from `PatternRef` semantics to `MaterialRef`, even if persisted `int16_t` layout is retained temporarily for compatibility.

### Depends on

A1 and A3.

---

# D. Persistence findings

## D1 — MAKE MELODY durability contract is not yet explicit

```text
CLASS: ARCHITECTURAL CONTRACT / possible FIX after product contract is chosen
SEVERITY: P1
STATUS: PARTIAL
0.9.11: MUST decide, SHOULD implement according to decision
MEMORY IMPACT: unknown
```

### Current evidence

Promotion preliminarily follows a defensive payload sequence:

```text
encode
-> temp write
-> readback/decode verify
-> rename final
-> update in-memory descriptor last
```

This protects against publishing an invalid sidecar payload.

It does not by itself prove that a successful UI operation has durably persisted the descriptor/page reference.

### Power-loss window to characterize

```text
new Melody payload durable
RAM descriptor = Melody
page/project descriptor on storage = old Pattern
power loss
```

### Product contract to choose

A. `MAKE MELODY` is a session mutation; durability occurs at explicit project/page save.

B. `MAKE MELODY` success is a durable storage transaction.

The implementation must expose one contract consistently.

### Depends on

A1 identity and project/page storage lifecycle census.

---

## D2 — Melody sidecar lifecycle may be incomplete for project operations

```text
CLASS: FIX if copy/delete loses or aliases user material; otherwise REFACTOR
SEVERITY: P0/P1 pending evidence
STATUS: NOT ENOUGH EVIDENCE
0.9.11: MUST investigate
MEMORY IMPACT: 0 expected for lifecycle fixes
```

### Required traces

- project copy with promoted Melodies;
- project rename if supported;
- project delete/clear;
- page copy/delete if supported;
- SD removal/reinsert;
- legacy project load.

### Failure criteria

- copied project references source project's sidecar files;
- copied project loses Melody payloads;
- deleting one project removes another project's material;
- stale orphan payload can be mistaken for live material;
- namespace normalization differs between Pattern page storage and Melody storage in a way that aliases names.

### Depends on

A1 canonical identity/namespace.

---

# E. Legacy state findings

## E1 — MORPH appears to be zombie state

```text
CLASS: DEPRECATE / MIGRATE
SEVERITY: P2
STATUS: PRELIMINARY CONFIRMED
0.9.11: MAY/SHOULD depending on persistence compatibility
MEMORY IMPACT: 0 / small lower if eventually removed
```

### Current evidence

`morphTarget` / `morphAmount` remain in settings/schema while current application paths preliminarily force neutral/default values rather than exposing active musical behavior.

### Required census

For each field determine:

```text
read?
write?
persisted?
runtime consumer?
UI exposure?
migration-only?
```

### Minimal direction

If no modern consumer exists, stop treating MORPH as current product architecture. Preserve decode compatibility if required and mark decode-only/deprecated until a safe format boundary permits removal.

---

# F. KEEP candidates

## F1 — ACTIVE/NEXT material authority

```text
CLASS: ACCEPT / KEEP
SEVERITY: n/a
STATUS: PRELIMINARY CONFIRMED HEALTHY
0.9.11: DO NOT REDESIGN without contradictory evidence
MEMORY IMPACT: preserve current bounded model
```

### Current evidence

The M3/M4 architecture preliminarily demonstrates:

- control-side material resolution/preparation;
- explicit ACTIVE authority;
- NEXT staging without immediate activation;
- boundary activation;
- no filesystem I/O at activation;
- bounded resident buffers;
- stable allocation lifetime;
- per-voice separation.

### Research goal

Attempt to break these invariants with paging/project churn and stale pending state. If they survive, freeze the boundary as accepted architecture.

---

## F2 — Single playback lifetime owner

```text
CLASS: ACCEPT / KEEP
SEVERITY: n/a
STATUS: PRELIMINARY CONFIRMED HEALTHY
0.9.11: DO NOT REDESIGN without regression evidence
MEMORY IMPACT: preserve
```

### Current evidence

The 0.9.10 Pattern/Phrase lifetime work established a dedicated runtime playback owner and targeted source-transfer barriers while preserving legitimate cross-bar note lifetime.

### Research goal

Verify that Melody/Material refactors do not introduce a second note lifetime owner or move persistence/control logic into audio execution.

---

## F3 — Explicit MelodyStore file ABI

```text
CLASS: ACCEPT / KEEP
SEVERITY: n/a
STATUS: PRELIMINARY CONFIRMED HEALTHY
0.9.11: KEEP
MEMORY IMPACT: preserve bounded decode model
```

### Current evidence

Preliminary inspection of `src/state/melody_store.h` shows an explicit versioned file format, field-wise event serialization, CRC validation and decode-to-scratch-before-publication rather than dumping runtime structs directly.

### Constraint

Do not replace this with raw runtime-struct persistence for convenience.

---

# G. Persistence-format debt

## G1 — Material descriptor page persistence uses representation-coupled bytes

```text
CLASS: REFACTOR / FORMAT DEBT
SEVERITY: P2
STATUS: PARTIAL
0.9.11: MAY/DEFER unless descriptor evolves
MEMORY IMPACT: 0
```

### Current evidence

Pattern page persistence preliminarily writes `materialSlots` representation bytes directly, while MelodyStore uses an explicit field-level ABI.

The current descriptor is small/simple, so this is not automatically a correctness bug.

### Trigger for action

Fix before expanding `MaterialSlotDescriptor` with fields whose padding/layout/endian representation is not intentionally frozen.

---

# H. Research dependency graph

Current provisional ordering:

```text
A1 Material identity
 |\
 | +--> D2 project/sidecar lifecycle
 |
 +--> A3 unresolved/off-page semantics
 |      |
 |      +--> C1 Song -> MaterialRef semantics
 |
 +--> D1 promotion durability

A2 empty-page descriptor reset

C1 + A1/A3
  -> B1 vocabulary freeze

B3 Genre/Feel/Generator authority
  -> B4 UI application boundary

B1/B3/B4
  -> later UX/Song/GF2 expansion

F1/F2/F3
  -> KEEP unless adversarial evidence contradicts
```

---

# I. Provisional 0.9.11 implementation checkpoints

These are not approved implementation work yet.

## 0.9.11-A — Material Identity Correctness

Freeze canonical MaterialId and eliminate cross-page aliasing/semantic fail-open paths.

Candidate scope: A1, A2, A3.

## 0.9.11-B — Persistence / Promotion Durability

Choose and implement the `MAKE MELODY` durability contract; align sidecar lifecycle with project/page operations.

Candidate scope: D1, D2.

## 0.9.11-C — MaterialRef / Arrangement Semantics

Define what Song/Phrase arrangement references and separate residency from identity.

Candidate scope: C1.

## 0.9.11-D — Domain Vocabulary Migration

Freeze `Material / Pattern / Melody / Phrase / Song` vocabulary and stop propagation of runtime legacy naming.

Candidate scope: B1 plus compatibility aliases.

## 0.9.11-E — Musical Semantic Ownership

Establish single semantic owners across Genre / Feel / Generator.

Candidate scope: B3.

## 0.9.11-F — UI Application Boundary

Move domain/application transactions out of Page classes while keeping UI musician-facing and small.

Candidate scope: B4.

## 0.9.11-G — Legacy State Retirement

Mark or remove zombie state only after compatibility census.

Candidate scope: E1 and related decode-only fields.

---

# J. Completion state

Current state:

```text
0.9.11 ARCHITECTURE CENSUS: OPEN
```

The ledger must not move to COMPLETE until every mandatory hypothesis in the charter has a final evidence-backed classification and the dependency order has been revalidated against the accepted 0.9.10 base.
