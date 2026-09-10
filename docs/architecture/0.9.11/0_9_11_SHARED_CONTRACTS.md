# GroovePuter 0.9.11 — Shared Architecture Contracts

**Status:** AUTHORITATIVE / FROZEN FOR DOWNSTREAM WORK

**Scope:** 0.9.11 architecture and all implementation/research branches derived from it.

**Rule for agents:** read this file before changing Material, persistence, Song, Phrase Bank, GF2 integration, paging, application operations, ACTIVE/NEXT publication, or related CI evidence. If an older 0.9.11 document conflicts with this file, this file wins until an evidence-backed amendment is committed here.

This file defines semantic contracts, not a mandatory class hierarchy. Do not introduce a generic service framework, event bus, repository framework, or new runtime owner merely to mirror the names below.

---

## 0. Why this file exists

0.9.10 made Pattern/Phrase editing and playback substantially more truthful at runtime. 0.9.11 must connect musical generation, persistent material, arrangement, editing, and playback without creating several competing versions of truth.

The architecture is organized around three distinct truths:

```text
MUSICAL TRUTH
what musical idea / structural intent should exist?
        |
        v
MATERIAL TRUTH
what canonical musical object and revision exist?
        |
        v
AUDIBLE TRUTH
what exact resolved revision is ACTIVE/NEXT and actually sounds?
```

These truths are related, but they are not interchangeable.

The primary architectural failure to prevent is:

```text
UI says one thing
Scene descriptor says another
persistent payload names another
Song references another
NEXT contains stale work
ACTIVE plays something else
```

---

# 1. Canonical vocabulary

## 1.1 Material

A **Material** is the canonical editable musical object used by arrangement and playback.

For 0.9.11:

```text
MaterialKind = Pattern | Melody
```

`Phrase` is **not** a MaterialKind.

GF2 Phrase remains a real structural multi-bar concept. Legacy runtime names such as `currentPhrase_`, `PhraseSourceToggle`, and `MAKE PHRASE` do not define the final domain vocabulary.

---

## 1.2 MaterialAddress — coordinate, not identity

```text
MaterialAddress
    voice
    globalSlot
```

A MaterialAddress answers:

> Where should this project context look for a material binding?

It does **not** answer:

> Which musical object is this?

Properties:

- project-relative;
- compact/bounded;
- page/bank/local slot are projections of `globalSlot`;
- contains no project path;
- contains no MaterialKind;
- may be used by navigation and storage lookup;
- must never be treated as durable object identity by itself.

### CONTRACT ID-01 — ADDRESS IS NOT IDENTITY

```text
MaterialAddress != MaterialId
```

Deleting/reusing an address must not silently cause an old reference to point at a different musical object.

---

## 1.3 ProjectNamespace

`ProjectNamespace` identifies the persistence namespace in which an address/binding is resolved.

It belongs to persistence/control-side context only.

It must not enter the audio thread or become part of realtime playback ownership.

---

## 1.4 MaterialId — stable object identity

A **MaterialId** answers:

> Which canonical musical object is this?

Required semantics:

- stable across edits/revisions of the same object;
- different when `MAKE UNIQUE` creates a new independent object;
- not inferred solely from current slot/address;
- survives persistence/reload;
- old references cannot silently resolve to a different MaterialId after address reuse.

The exact persisted width/encoding is intentionally **not frozen here**. It must be bounded and embedded-appropriate.

### CONTRACT ID-02 — OBJECT IDENTITY IS STABLE

```text
same MaterialId
    => same logical material object
```

A new unrelated object requires a new MaterialId.

---

## 1.5 MaterialRevision — exact state identity

A **MaterialRevision** identifies a committed state of a MaterialId.

A revision changes when canonical musical content changes.

Conceptually:

```text
MaterialVersion = MaterialId + MaterialRevision
```

Runtime ACTIVE/NEXT must resolve an exact MaterialVersion, not merely an address or `latest` pointer.

### CONTRACT ID-03 — ACTIVE IS REVISION-EXACT

Once ACTIVE has accepted a MaterialVersion, later edits to the same MaterialId do not mutate the already sounding revision in place.

---

## 1.6 MaterialLineage / Idea provenance

Lineage answers:

> Where did this material come from?

Examples:

- GF2 intent / selection identity;
- source Pattern revision;
- generated variant parent;
- manual edit ancestry.

Lineage is provenance and generation context. It is **not** playback authority and is **not** proof that two resulting materials remain musically equivalent.

### CONTRACT ID-04 — LINEAGE IS NOT CANONICAL CONTENT

```text
same generation selection / lineage
!= guaranteed same canonical events
```

After manual editing, canonical events are authoritative even if they no longer satisfy the original genre contract.

---

# 2. Binding and reference semantics

## 2.1 Address binding

A project resolves a MaterialAddress through a binding to a MaterialId/current committed revision.

Conceptually:

```text
(ProjectNamespace, MaterialAddress)
        -> binding
        -> MaterialId
        -> current MaterialRevision
```

The physical implementation may use descriptors, sidecars, a bounded index, page metadata, or another embedded representation. The semantic rule is mandatory even if the representation changes.

### CONTRACT REF-01 — NO SILENT REBIND

An occurrence/reference created for MaterialId X must never silently begin referencing unrelated MaterialId Y merely because Y later occupies the same MaterialAddress.

A legacy compact Song encoding may remain only if an additional persisted binding/generation mechanism proves this invariant.

---

## 2.2 Resolved reference

Control-side resolution must distinguish at least:

```text
Resolved
NotResident
MissingPayload
CorruptPayload
InvalidAddress
InvalidKind
StaleBinding
StaleRevision
```

Additional bounded statuses are allowed only when they imply a distinct recovery/action decision.

### CONTRACT REF-02 — UNKNOWN IS NOT PATTERN

```text
unknown / invalid / unavailable / not resident
!= Pattern
```

Only an explicitly resolved `MaterialKind::Pattern` means Pattern.

Older accessors that fail open to Pattern are transitional debt and must not define new 0.9.11 behavior.

---

# 3. Shared editing semantics

## 3.1 Editing a shared material

Default shared-edit rule:

> Editing material Q creates a new revision of the same MaterialId. Occurrences that reference Q see the new revision on their next lawful preparation/resolution. Already sounding ACTIVE remains stable until a permitted activation boundary.

This gives predictable shared-object behavior:

```text
Intro -> Q
Outro -> Q
edit Q

next preparations of Intro and Outro -> new revision of Q
current ACTIVE -> old exact revision until boundary
```

### CONTRACT EDIT-01 — SHARED EDIT CREATES REVISION

Normal edit:

```text
same MaterialId
new MaterialRevision
```

It does not create a new MaterialId by default.

---

## 3.2 MAKE UNIQUE

`MAKE UNIQUE` creates a new independent MaterialId and rebinds only the selected arrangement occurrence/target.

Therefore the operation requires an explicit occurrence target. An edit slot alone is insufficient to define which shared reference must be changed.

### CONTRACT EDIT-02 — UNIQUE REQUIRES OCCURRENCE TARGET

```text
MAKE UNIQUE
    source MaterialVersion
    selected occurrence
        -> new MaterialId
        -> occurrence rebound only there
```

The exact UI gesture/name may change; the semantic contract does not.

---

## 3.3 Draft vs canonical revision

Editable draft state is not automatically canonical persistent truth.

The implementation must make the transition explicit:

```text
editable draft
    -> validate
    -> commit revision
    -> canonical MaterialVersion
```

ACTIVE must never point into a mutable draft buffer.

---

# 4. Application operation contract

Cross-boundary musician gestures are owned by concrete **Application Operations**.

Examples:

```text
MakeMelody
CommitMaterialEdit
MakeUnique
SwitchPage
ApplySongOccurrence
ApplyGenreIntent
```

This is a semantic boundary, not a request for a generic command bus.

---

## 4.1 Request snapshot

Every asynchronous/slow preparation that can outlive the initiating UI gesture must carry a bounded request identity.

Conceptually:

```text
OperationRequest
    RequestToken
    ProjectSessionEpoch
    target address / MaterialId as required
    ExpectedSourceRevision
    activation target/group
```

Exact field widths are implementation details.

### CONTRACT OP-01 — REQUESTS ARE VERSIONED

A late result may publish only if its request token/session/target are still current.

Scenario that must be rejected:

```text
prepare Q starts
user selects W
W prepares and becomes current target
Q finishes late
Q MUST NOT overwrite NEXT
```

---

## 4.2 Optimistic source validation

An operation that derives new content from a source revision must capture the expected source revision before slow work.

Before durable publication it must verify that the source/project context still matches the request contract, or explicitly apply a product-defined conflict policy.

### CONTRACT OP-02 — NO COMMIT FROM SILENTLY STALE SOURCE

A Pattern edited while MAKE MELODY is writing to SD cannot silently yield a new Melody presented as if it came from the newer Pattern.

---

## 4.3 MAKE MELODY vertical checkpoint

The first complete 0.9.11 vertical operation is:

```text
Pattern MaterialVersion
    -> MAKE MELODY
    -> durable Melody MaterialVersion
    -> prepared playback
    -> lawful activation
    -> activation confirmation
    -> reload/reboot recovery
```

Required phase model:

```text
1. CAPTURE REQUEST SNAPSHOT
2. VALIDATE SOURCE + TARGET
3. PREPARE CANDIDATE
4. RESERVE / VERIFY BOUNDED RUNTIME CAPACITY
5. DURABLE PUBLICATION
6. RESOLVE EXACT PUBLISHED REVISION
7. PREPARE / STAGE ACTIVATION WITH REQUEST TOKEN
8. ACTIVATE AT LAWFUL BOUNDARY
9. CONFIRM ACTIVATION
```

The ordering may be optimized only if all contracts below remain true.

---

## 4.4 Two different success boundaries

Persistence success and audible activation success are different facts.

### CONTRACT OP-03 — DURABILITY != ACTIVATION

After durable publication, failure to activate does **not** mean that no material was created.

Receipts must represent both axes independently.

Conceptual receipt:

```text
MaterialOperationReceipt
    publication:
        NotPublished | Published
    activation:
        NotRequested | Prepared | Staged | Activated |
        Superseded | Failed
    resulting MaterialVersion, when published
```

A valid result may therefore be:

```text
Published + Failed
= saved successfully, not activated
```

UI must not report this as a full rollback.

---

## 4.5 Idempotency

### CONTRACT OP-04 — RETRY IS SAFE

Re-executing the same logical operation/request must not create an unintended duplicate MaterialId or duplicate revision.

The implementation may use request token, transaction marker, deterministic publication key, or another bounded mechanism.

---

## 4.6 Stale NEXT rejection

### CONTRACT OP-05 — STALE PREPARATION NEVER BECOMES NEXT

Prepared work is eligible for NEXT only if the request/session/target/revision constraints that produced it are still current.

Atomic pointer replacement by itself is insufficient.

---

## 4.7 Multi-voice activation unit

For a Song/page transition requiring coordinated Synth A/B change, the activation unit is a group.

### CONTRACT OP-06 — GROUP ACTIVATION IS ATOMIC

If the transition semantically requires A and B together:

```text
prepare A
prepare B
validate same activation group/token
then activate A+B at one allowed boundary
```

Do not expose a state where A has moved to the new arrangement position while B is still intentionally on the previous one, unless that split is itself an explicit musical operation.

Durable publication of an independently created material may already have succeeded; group activation still remains atomic as an audible transition.

---

# 5. Persistence and durability

Persistence owns:

- project namespace;
- address bindings;
- MaterialId persistence;
- revision persistence;
- descriptors and Melody payloads;
- GPML/path derivation;
- CRC/readback validation;
- Save As / copy / rename / delete;
- orphan/GC policy;
- crash/power-loss recovery.

Persistence does not directly own ACTIVE/NEXT.

---

## 5.1 Durable publication invariant

### CONTRACT PERSIST-01 — OLD VALID OR NEW VALID

A completed/power-interrupted publication must recover to one of:

```text
old valid canonical state
or
new fully valid canonical state
```

Forbidden state after reported publication success:

```text
descriptor/binding says Melody revision R
but payload R is missing/corrupt/unaddressable
```

Temporary orphan files may be acceptable if nothing canonical references them and cleanup is defined.

---

## 5.2 Save As / project lifecycle

### CONTRACT PERSIST-02 — PROJECT OPERATIONS MOVE COMPLETE MATERIAL TRUTH

Save As/copy/rename/delete must operate on the complete project material namespace, not only `.gpp` page descriptors.

A copied project must remain independently loadable after the source project is changed or deleted.

---

## 5.3 Address reuse

### CONTRACT PERSIST-03 — REUSE CHANGES BINDING, NOT HISTORY

When an address is reused for a new MaterialId, old references must either:

- continue to resolve their original MaterialId; or
- fail explicitly as stale/unresolved.

They must not silently bind to the new object.

---

# 6. Runtime and audible truth

Keep existing 0.9.10 strengths unless adversarial evidence disproves them:

- one active-note lifetime owner;
- M3 explicit ACTIVE authority;
- M4 control-side prepare + boundary activation;
- fixed/bounded runtime buffers;
- no filesystem in audio;
- no blocking SD waits in audio;
- hard barriers on ownership/source transfer, not every bar;
- valid cross-bar notes survive bar boundaries.

---

## 6.1 ACTIVE/NEXT version identity

### CONTRACT RT-01 — ACTIVE/NEXT ARE EXACT MATERIAL VERSIONS

Runtime publication must carry enough control-side identity to prove which exact MaterialId/revision was prepared.

Project path, filesystem handles, and mutable persistence objects do not belong in audio.

---

## 6.2 Missed deadline behavior

### CONTRACT RT-02 — LATE IS NOT BETTER THAN WRONG

If NEXT is not ready by the permitted activation boundary:

- keep the current valid ACTIVE;
- report/retain a bounded pending/failure state according to operation contract;
- do not partially activate an unready group;
- do not perform blocking recovery in audio;
- do not later activate stale work merely because it finally completed.

CPU/latency is therefore a 0.9.11 correctness concern, not only a performance concern.

---

## 6.3 Lifetime invariant

### CONTRACT RT-03 — REVISION CHANGE DOES NOT BREAK NOTE LIFETIME RULES

A Material revision/owner transition may use the existing ownership barrier semantics.

A normal bar boundary inside the same owner/revision remains non-destructive for valid cross-bar notes.

---

# 7. Arrangement / Song contract

Song owns arrangement occurrence, not playback.

Conceptually:

```text
SongOccurrence
    EMPTY
    REST
    MATERIAL(reference)
```

A compact persisted `SongCell` remains desirable, but its representation must satisfy `REF-01`.

### CONTRACT SONG-01 — SONG REFERENCES OBJECTS, NOT PLAYBACK IMPLEMENTATION

Song must not decide Pattern vs Melody by itself.

It supplies an occurrence/reference to the common Material resolver/application path.

### CONTRACT SONG-02 — SONG DOES NOT PUBLISH ACTIVE

Song selection/playhead advancement may request a material transition, but only the application/resolution/runtime path may publish ACTIVE/NEXT.

---

# 8. Phrase Bank / edit-target contract

Phrase Bank owns edit targeting only.

### CONTRACT EDITTARGET-01 — EDIT != PLAY

Navigation/edit selection must not silently change audible ownership.

Phrase Bank may select an address/material target for editing, but it does not own ACTIVE/NEXT.

The existing bounded per-voice edit-state direction is compatible with this contract.

---

# 9. GF2 / musical truth contract

GF2 owns musical intent, constraints, generation choices, structural Phrase semantics, and candidate creation.

GF2 does not own persistence identity, canonical manual edits, Song playback authority, or ACTIVE/NEXT.

---

## 9.1 Manual edits are canonical

### CONTRACT GF2-01 — GENERATOR DOES NOT REPAIR USER EDITS AUTOMATICALLY

After a user edits a Melody, the canonical events may no longer satisfy the original GF2 genre contract.

This is allowed.

GF2 lineage/provenance may describe origin, but it must not silently rewrite the material back toward its original contract.

---

## 9.2 Variant basis must be explicit

A future `VARIANT`/new-take operation must state what it inherits.

Allowed conceptual bases include:

```text
OriginalIntent
CurrentMaterialRevision
Hybrid(intent + edited material with explicit precedence)
```

The exact UI wording is not frozen here.

### CONTRACT GF2-02 — VARIANT BASIS IS NOT IMPLICIT

Two operations with different inheritance semantics must not share one ambiguous command path.

---

## 9.3 Genre identity is structural, not a universal slogan

Genre constraints must be scoped to concrete genre/profile/archetype/recipe contracts.

User-facing musical review filters remain useful:

1. remove timbre and ask what structure remains;
2. define identity partly by prohibitions;
3. inspect structurally recognisable locations;
4. inspect activity across step/beat/bar/phrase/section time levels;
5. expose only decisions a musician could plausibly make.

Do not encode oversimplified universal laws such as one rhythmic prohibition applying to every archetype of a broad genre.

---

# 10. Memory and CPU contract

0.9.11 must remain embedded-bounded.

Working memory includes more than ACTIVE/NEXT:

```text
ACTIVE
NEXT / staged
editable draft
generation candidate
undo payload
SD I/O scratch
resolver/index cache
operation/request metadata
```

### CONTRACT MEM-01 — STORED MATERIAL COUNT DOES NOT CREATE UNBOUNDED DRAM

The number of persisted materials may grow without linearly growing unbounded resident DRAM.

Use bounded/paged/index-cache strategies. Do not require a permanently resident metadata object for every saved material unless a measured bound proves it fits the product envelope.

### CONTRACT MEM-02 — EVERY WORKING SET HAS A LIMIT

All queues, caches, drafts, candidates, undo buffers, I/O buffers, and pending operations must have explicit finite capacity and defined overflow/refusal behavior.

### CONTRACT CPU-01 — PREPARATION DEADLINES HAVE DEFINED SEMANTICS

A missed preparation deadline preserves current valid audio and yields an explicit operation/activation outcome. It must not cause blocking audio work or stale late activation.

---

# 11. Research and CI evidence contracts

Research tooling may inform architecture only when its observations are truthful about what was actually measured.

These rules apply especially to GF2/G4 corpus, topology, diversity, collision, and coordinate claims.

---

## 11.1 Observation validity

### CONTRACT EVID-01 — NOT OBSERVED IS NOT A MUSICAL VALUE

Observation status must be separate from migration/generation status.

If migration is `APPLIED` but a required probe/topology was not observed:

```text
observation = NOT_OBSERVED
```

That row must not participate as if `NOT_OBSERVED` were a legitimate topology/signature value.

For a mandatory census, missing required observation is a gate failure.

Analyzers must validate topology field format/range before diversity/collision statistics.

---

## 11.2 Exact-SHA source firewall

### CONTRACT EVID-02 — EVIDENCE EXECUTES AND COMPARES THE SAME SHA

A source firewall/preflight must compare the actual checked-out `HEAD` used by tests, and assert it equals the expected event/head SHA when applicable.

Do not substitute a mutable remote branch ref such as `origin/${GITHUB_HEAD_REF}` for the executed SHA.

Historical baselines used to prove byte/source invariance should be pinned to immutable commit SHAs when repeatability matters.

---

## 11.3 Claim strength must match measured output

### CONTRACT EVID-03 — ADDRESS-INDEPENDENCE OF SELECTION IS NOT MUSIC-INVARIANCE

If a coordinate test proves only that selection fields and realization seed do not change with storage address, its conclusion is limited to those fields.

To claim:

> moving/readdressing material preserves the music

at minimum compare the effective material fingerprint; for a strict contract compare canonical events/normalized musical content.

---

## 11.4 Golden drift

### CONTRACT EVID-04 — GOLDEN FAILURE IS NOT AUTOMATICALLY A PRODUCTION REGRESSION

A historical fingerprint/golden mismatch requires causal comparison against the relevant base before production changes or golden updates are justified.

Report separately:

```text
execution/invariant failure
vs
historical golden drift
```

---

# 12. Ownership matrix

| Owner | Owns | Must not own |
|---|---|---|
| UI / Presentation | gesture, navigation, presentation, receipts | persistence transaction, resolver truth, ACTIVE/NEXT |
| Application Operations | cross-boundary musician transaction, request freshness, commit/activation receipts | audio DSP, generic global framework |
| GF2 | musical intent, constraints, structural generation, candidates, lineage | canonical manual edit truth, storage identity, playback authority |
| Material control / resolver | binding resolution, explicit status, exact MaterialVersion preparation | UI navigation, audio filesystem work |
| Persistence | namespace, IDs/revisions on disk, descriptor/payload atomicity, project lifecycle | ACTIVE/NEXT publication |
| Song | arrangement occurrences/references | Pattern-vs-Melody decision, direct playback authority |
| Phrase Bank | edit target | PLAY authority |
| Runtime M3/M4 | exact ACTIVE/NEXT, boundary activation | project paths, SD, mutable drafts |
| Runtime note owner | note lifetime / targeted barriers | material persistence semantics |

---

# 13. First vertical implementation checkpoint

The first implementation line after these contracts should prove one complete path rather than several horizontal abstractions:

```text
Pattern revision
    -> MAKE MELODY request
    -> candidate
    -> durable Melody revision
    -> exact resolve
    -> bounded prepare
    -> staged NEXT
    -> confirmed activation
    -> reboot/reload recovery
```

Acceptance criteria:

1. failure before durable publication preserves the original canonical state;
2. failure after durable publication reports `published but not activated` rather than pretending rollback;
3. retry cannot create unintended duplicate object/revision;
4. stale preparation cannot activate after target/project/session changes;
5. source revision is validated across slow SD work;
6. ACTIVE note lifetime remains correct;
7. no SD/blocking wait enters audio;
8. all working buffers are bounded;
9. host, Cardputer, memory and relevant runtime evidence refer to one exact SHA.

Only after this vertical path is proven should shared reuse/edit, `MAKE UNIQUE`, and Song occurrence integration depend on it.

---

# 14. Relationship to existing 0.9.11 work

## Architecture A0

The earlier A0 document remains useful for ownership decomposition, but this file supersedes one important simplification:

```text
OLD conceptual wording:
PersistentMaterialId = ProjectNamespace + MaterialAddress

CURRENT authoritative wording:
(ProjectNamespace + MaterialAddress) = persistent lookup/binding coordinate
MaterialId = logical object identity
MaterialRevision = committed object state
MaterialLineage = provenance / musical ancestry
```

## A1 Persistent Identity branch

A1's global-slot Melody addressing solves the proven cross-page alias defect and establishes a correct address coordinate.

It must **not** be interpreted as final proof that `MaterialAddress` alone is full durable object identity.

Existing fail-open `NotResident/invalid -> Pattern` behavior is transitional and remains outside the final resolver contract.

## A2 Canonical Promotion branch

A2 must implement/characterize the first vertical `MAKE MELODY` operation against the operation, revision, durability, freshness, and activation contracts in this file.

Do not let `MiniAcid::makePhrase()` remain the final cross-layer authority.

## Hybrid Song branch

Keep bounded `SongCell` / `EMPTY | REST | MATERIAL` semantics where useful.

Before final integration, prove `REF-01` so compact address storage cannot silently rebind an old occurrence to a new MaterialId after reuse.

## Phrase Bank branch

Keep `EDIT != PLAY` and bounded edit-state ownership.

## G4 / PR #453 and descendants

Treat research measurements according to `EVID-01..04`.

Musical identity, generation attempt, storage address, MaterialId, MaterialRevision, and lineage are distinct coordinates/concepts. Do not collapse them merely because a current test fixture can represent several with integers.

---

# 15. Agent checklist

Before changing production or research behavior, every 0.9.11 agent should answer:

```text
1. Which contract IDs does this checkpoint implement or test?
2. Which owner does it modify?
3. Does it create a second source of truth?
4. Is MaterialAddress being mistaken for MaterialId?
5. Which exact MaterialRevision is the operation based on?
6. Where is the durable publication boundary?
7. Where is the audible activation boundary?
8. How are stale requests rejected?
9. What happens if preparation misses its deadline?
10. Are shared references edited-in-place-by-revision or made unique explicitly?
11. What is the bounded RAM/CPU working set?
12. Does CI evidence refer to the exact executed SHA?
13. Does the strength of the claim exceed what was actually observed?
```

If any answer is unclear, the checkpoint is not ready for production implementation.

---

# 16. Freeze summary

The following are frozen until evidence disproves them:

```text
ADDRESS != IDENTITY
IDENTITY != REVISION
REVISION != LINEAGE
MUSICAL INTENT != CANONICAL EVENTS
DURABLE PUBLICATION != AUDIBLE ACTIVATION
EDIT TARGET != PLAY TARGET
SONG REFERENCE != PLAYBACK AUTHORITY
NOT_OBSERVED != OBSERVED MUSICAL VALUE
LATE PREPARATION != VALID NEXT
```

And the top-level trusted path is:

```text
musician intent
    -> versioned application request
    -> musical candidate / existing source revision
    -> canonical MaterialId + MaterialRevision
    -> durable publication
    -> explicit resolution
    -> bounded prepared revision
    -> freshness-checked NEXT
    -> boundary activation
    -> confirmed ACTIVE
    -> audio
```

This is the common contract surface for parallel 0.9.11 development.
