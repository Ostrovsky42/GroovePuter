# 0.9.11 Top-Down Architecture Convergence Report

Date: 2026-09-10

Status: architecture/report only. This document describes current convergence and is intended as the source for a visual architecture diagram. It does not authorize production changes.

## 1. Executive view

The architecture is no longer best understood as Pattern/Phrase plus a generator. It is converging into three explicit truths connected by bounded transitions:

```text
MUSICAL TRUTH
What musical idea should exist?
        |
        v
MATERIAL TRUTH
What canonical material is addressed, stored and editable?
        |
        v
AUDIBLE TRUTH
What fully resolved material is ACTIVE/NEXT and actually sounds?
```

These truths have different owners and must not be collapsed back into `Scene`, UI pages, Song, or `MiniAcid`.

The strongest architectural direction is therefore a vertical causal spine with side planes:

```text
                           USER / UI
                              |
                         musician intent
                              |
                              v
                    APPLICATION OPERATIONS
                              |
             +----------------+----------------+
             |                                 |
             v                                 v
      MUSICAL SEMANTICS                  MATERIAL CONTROL
           GF2                           MaterialAddress
             |                                 |
      generated candidate                       v
             +-------------------------> RESOLUTION
                                                |
                                      prepared ACTIVE/NEXT
                                                |
                                                v
                                         RUNTIME M3/M4
                                                |
                                                v
                                             AUDIO

ARRANGEMENT (Song/Phrase) ---- references ----> MaterialAddress
EDIT TARGET (Phrase Bank) ----- selects ------> MaterialAddress
PERSISTENCE -------------------- stores ------> PersistentMaterialId
```

The key design property is that side planes reference the causal spine but do not become playback authority.

## 2. Foundation state: 0.9.10 runtime

The current release closure remains open and Draft. Therefore 0.9.11 is still developed on a provisional product foundation.

What 0.9.10 already gives 0.9.11 and should be treated as lower-layer infrastructure rather than redesigned:

- one runtime active-note lifetime owner;
- Pattern/Phrase ownership transfer barriers;
- cross-bar note lifetime;
- bounded Phrase buffers;
- M3 ACTIVE authority;
- M4 NEXT prepare/activate discipline;
- no filesystem work in audio;
- AudioGuard-protected mutations;
- fixed-DRAM discipline.

The current 0.9.10 integration head has moved beyond the original `fcd0d77...` base used to start the architecture branch. This is a branch-integration concern, not a reason to reopen the top-down ownership model.

Important legacy seam still visible in the current release line:

```text
PhraseSourceToggle
    -> MiniAcid::setSequencedSource()
    -> MiniAcid::makePhrase()
```

This remains a direct UI-to-runtime operation. It is acceptable as inherited 0.9.10 behavior, but it is not the final 0.9.11 application boundary.

## 3. Layer A: User intent and UI

### Target owner

UI owns:

- musician gesture;
- current presentation;
- navigation/edit selection;
- rendering an operation receipt.

UI does not own:

- generation transaction;
- persistence transaction;
- material resolution;
- ACTIVE/NEXT publication.

### Current state

Partially legacy.

The current Pattern/Phrase source UI still directly changes runtime source and invokes runtime `makePhrase()`. This is the principal top-layer architectural debt that should remain untouched until canonical Material operations exist underneath it.

### Diagram label

```text
UI
intent only
STATUS: MIGRATION LATER
```

## 4. Layer B: Application operations

### Target owner

Concrete musician-level operations crossing subsystem boundaries, for example:

```text
MakeMelody
SelectMaterial
SwitchPage
ApplyGenreIntent
ApplySongOccurrence
```

They perform bounded orchestration:

```text
PREPARE
-> VALIDATE
-> PERSIST where required
-> RESOLVE
-> PREPARE RUNTIME
-> COMMIT / STAGE NEXT
-> RECEIPT
```

This is intentionally not a generic command bus.

### Current state

This is now the largest missing architectural layer.

A2 `canonical-promotion` has been created, but currently points at the same head as completed A1. Therefore the contract exists but the first canonical cross-boundary operation has not yet begun production implementation.

### Diagram label

```text
APPLICATION OPERATIONS
STATUS: PRIMARY GAP
NEXT: A2 CANONICAL PROMOTION
```

## 5. Layer C: Musical truth — GF2

### Owner

GF2 owns decisions such as:

- genre structural possibility space;
- rhythm family/archetype compatibility;
- role identity selection;
- FEEL/profile interpretation;
- bass/chord/melodic role relationships;
- phrase law and multi-bar development;
- harmonic clock/progression planning;
- generated musical candidate.

GF2 does not own material storage address, project path, paging, ACTIVE/NEXT, or UI navigation.

### Important ownership progress

The G4 research line has explicitly separated:

```text
MUSICAL IDENTITY
!= GENERATION ATTEMPT
!= STORAGE ADDRESS
```

This is a major architectural improvement because a new take no longer has to imply a new musical identity, and a destination address does not define the idea being generated.

The later G4 line has progressed through structural ownership work for Dub Techno and House, a global ownership census, Dub bass compatibility, and an explicit bass-family compatibility binding.

The current profile model also reflects stronger semantic ownership: for example bass selection can be explicitly `Independent` or `FamilyNative`, rather than assuming RhythmFamily automatically owns every bass decision.

### Current verification state

At the latest I8 head:

- generation host matrix: PASS;
- bass-family binding focused job: PASS;
- C0R6 observer replay: PASS;
- final target-matrix workflow: RED at mandatory target-status enforcement.

Therefore the musical semantics work is advanced and causally tested, but I8 is not globally GREEN on its current exact SHA.

### Diagram label

```text
GF2 / MUSICAL SEMANTICS
musical truth owner
STATUS: ADVANCED, I8 TARGET GATE RED
```

## 6. Layer D: Material contract — canonical address and kind

### Frozen cross-workstream contract

```text
MaterialAddress
    voice
    globalSlot
```

It deliberately excludes:

- project namespace;
- page;
- bank;
- resident/local slot;
- Pattern/Melody kind.

This is the common address vocabulary for Song, edit targeting, resolution and persistence projection.

### A1 implementation progress

A1 has moved from characterization into production implementation.

Current production direction:

- `MaterialAddress` is a compact two-byte value;
- `globalSlot` covers the global pattern/material address space;
- page/bank/local slot are projections;
- Melody sidecar path derives from voice + global slot instead of page-local slot;
- persistence path now has the form conceptually:

```text
ProjectNamespace + MaterialAddress
-> /projects/<project>/melody/v<voice>_g<globalSlot>.gpml
```

This closes the original cross-page alias root cause.

A1 exact-head workflow is GREEN on its current head, including its host/Cardputer/DRAM gate.

### Remaining intentional debt

A1 did not yet implement explicit resolution semantics. Existing accessors can still answer `Pattern` for non-resident/out-of-range state.

That means:

```text
MaterialAddress identity: improved
MaterialResolution truth: not yet complete
```

This must not be mistaken for the final A0 contract. A later resolution checkpoint must enforce:

```text
NotResident != Pattern
Invalid != Pattern
MissingPayload != Pattern
```

### Diagram label

```text
MATERIAL ADDRESS / KIND
STATUS: A1 GREEN
GAP: explicit resolution
```

## 7. Layer E: Persistence plane

### Owner

Persistence owns:

```text
ProjectNamespace
+ MaterialAddress
= PersistentMaterialId
```

and:

- page descriptors;
- GPML payloads;
- CRC/readback;
- path derivation;
- project copy/save-as/rename/delete;
- orphan policy;
- durability/power-loss semantics.

### Current state

Identity is materially improved by A1.

Still unresolved:

- descriptor + payload durability as one user-visible transaction;
- Save As copying Melody sidecars;
- project delete/orphan semantics;
- recovery from partial promotion;
- final empty-page descriptor reset contract.

### Diagram label

```text
PERSISTENCE
identity partly GREEN
transaction/lifecycle TODO
```

## 8. Layer F: Material resolution

### Target role

Resolution is the boundary between stored/editable material truth and runtime truth.

Conceptually:

```text
resolve(ProjectContext, MaterialAddress)
    -> Resolved Pattern
    -> Resolved Melody
    -> NotResident
    -> MissingPayload
    -> CorruptPayload
    -> InvalidAddress
    -> InvalidKind
```

Only a fully resolved result may be prepared for M3/M4.

### Current state

Missing as a complete explicit layer.

This is where several proven defects converge:

- `NotResident -> Pattern` fallback;
- stale material after page switch;
- descriptor/payload disagreement;
- Song off-page references;
- project/page context changing while NEXT is pending.

### Diagram label

```text
MATERIAL RESOLUTION
STATUS: MISSING / CRITICAL
```

## 9. Layer G: Arrangement — structural Phrase and Song

Two different concepts must be drawn separately.

### GF2 Phrase

A real multi-bar musical structure. It owns temporal development, phrase law, harmonic clock and multi-bar semantic organization.

This is not the legacy runtime `currentPhrase_` buffer.

### Song

Song owns occurrences and ordering, not playback authority.

Target model:

```text
SongCell
    EMPTY
    REST
    MATERIAL(globalSlot)

lane/track voice + globalSlot
    -> MaterialAddress
```

### Current state

The Hybrid Song research line has a good bounded `SongCell` direction, but remains at a RED/research stage and still carries transitional Pattern-centric naming such as `patternRef`.

Therefore Song should be drawn as an arrangement owner whose material-resolution edge is not yet integrated.

### Diagram label

```text
STRUCTURAL PHRASE: GF2, REAL MULTI-BAR OWNER
SONG: occurrence owner, integration pending
```

## 10. Layer H: Edit target — Phrase Bank

### Target invariant

```text
EDIT != PLAY
```

Phrase Bank may choose what the musician edits but must never become audible authority.

The existing Phrase Bank state is already shaped correctly: it stores only a compact per-voice edit slot and intentionally contains no PLAY field.

### Current state

Architecturally healthy but parked on an older branch/base. It has not yet been rebound to the new `MaterialAddress` contract.

### Diagram label

```text
PHRASE BANK / EDIT TARGET
STATUS: GOOD BOUNDARY, NEEDS REBASE + ADDRESS INTEGRATION
```

## 11. Layer I: Runtime publication — audible truth

### Owner

M3/M4 remain the lower execution boundary:

```text
fully resolved prepared material
        |
        +-> ACTIVE
        +-> NEXT
                 |
          boundary activation
                 |
                 v
             PLAYBACK
```

Runtime owns:

- what actually sounds;
- NEXT staging;
- activation boundary;
- active-note lifetime.

Runtime does not own:

- project identity;
- storage path;
- Song meaning;
- genre selection;
- UI state.

### Current state

Strong foundation inherited from 0.9.10. Keep/freeze unless new adversarial evidence disproves it.

### Diagram label

```text
M3/M4 ACTIVE/NEXT
STATUS: KEEP / LOWER-LAYER AUTHORITY
```

## 12. Layer J: Audio/backends

Audio consumes logical note events from the one runtime playback owner and sends them to internal synth/MIDI paths.

No 0.9.11 domain identity should cross this boundary.

### Diagram label

```text
AUDIO / SYNTH / MIDI
execution only
STATUS: KEEP
```

## 13. Top-down architecture as it stands now

```text
[ USER ]
    |
    v
[ UI ] ------------------------------ EDIT ----> [ Phrase Bank ]
    |
    | intent
    v
[ APPLICATION OPERATIONS ]  <---- PRIMARY MISSING LAYER
    |
    +--------------------+
    |                    |
    v                    v
[ GF2 ]              [ MATERIAL ]
 musical truth       address + kind
    |                    |
    | candidate          +<---------------- [ Song occurrences ]
    |                    |
    +------------------->|
                         v
                  [ RESOLUTION ]  <--------- [ Persistence ]
                         |
                  prepared material
                         |
                         v
                  [ ACTIVE / NEXT ]
                         |
                         v
                    [ PLAYBACK ]
                         |
                         v
                 [ SYNTH / MIDI ]
```

## 14. Status legend for the future diagram

Recommended visual statuses:

```text
SOLID / GREEN
    proven owner or accepted lower boundary

BLUE / ACTIVE
    current implementation/research line

AMBER / GAP
    contract known, implementation incomplete

RED / CONFLICT
    two authorities or fail-open path still reachable

DASHED / LEGACY
    current compatibility path that must not define new architecture
```

Suggested node statuses:

```text
GF2                    BLUE     advanced, I8 global target gate RED
MaterialAddress A1     GREEN    exact-head workflow successful
Persistence identity   GREEN*   identity only; lifecycle not closed
Application ops        AMBER    main missing layer
Resolution             RED      fail-open semantics still present
Song                    AMBER    bounded model exists, not Material-integrated
Phrase Bank             BLUE     healthy ownership, older base
M3/M4 runtime           GREEN    keep
PhraseSourceToggle      DASHED   inherited runtime-oriented orchestration
0.9.10 release base     AMBER    PR #451 still Draft/open
```

## 15. Branch topology versus logical architecture

Do not draw Git branches as architecture components.

Current branch history has diverged intentionally:

- architecture/A0 and A1 are based on the earlier 0.9.10 integration point;
- current 0.9.10 release closure has moved several commits forward;
- G4 has advanced far on its own semantic line;
- Song and Phrase Bank are older parallel branches.

This is acceptable while contracts remain independent, but before integration every live 0.9.11 line must be replayed/rebased onto the accepted 0.9.10 release SHA.

A branch conflict is not necessarily an ownership conflict. Conversely, a clean git merge does not prove architectural correctness.

## 16. The two most important convergence points

### Convergence point 1 — A2 canonical promotion

This is the first place where the architecture must prove:

```text
musician MAKE MELODY intent
-> canonical material candidate
-> persistence
-> descriptor truth
-> resolution
-> ACTIVE/NEXT
-> receipt
```

A2 is therefore more important than a simple refactor of `makePhrase()`.

### Convergence point 2 — I0 Material Causality

Final end-to-end invariant:

```text
one musician intent
-> one MaterialAddress
-> one persistent truth
-> one resolved truth
-> one ACTIVE/NEXT truth
-> one audible truth
```

This is where Material, Song, Phrase Bank, persistence and runtime finally meet.

GF2 should feed this system as a candidate producer without becoming an identity/storage owner.

## 17. What the visual scheme should explain to a reader

A successful diagram should make five ideas obvious without reading code:

1. **Music and storage are different coordinates.** A musical identity, generation attempt and material address are not the same thing.
2. **Material is the bridge.** Pattern/Melody are canonical representations addressed by `MaterialAddress`.
3. **Arrangement references; it does not play.** Song and structural Phrase organize material but do not become runtime authority.
4. **Resolution is the safety boundary.** Nothing unresolved may reach ACTIVE/NEXT.
5. **Runtime is intentionally boring.** Once a material is resolved, M3/M4 and the lifetime owner execute it without knowing genre, project or filesystem semantics.

## 18. Immediate architectural next moves

In dependency order, not necessarily branch order:

```text
1. Preserve A1 GREEN as the address/persistence-identity base.
2. Start A2 canonical promotion as the first real application operation.
3. Specify/implement explicit MaterialResolution before paging/Song integration.
4. Rebind paging to resolution so old-page Melody cannot survive by accident.
5. Adapt SongCell from Pattern reference to Material occurrence.
6. Rebase Phrase Bank and bind EDIT target to MaterialAddress.
7. Close project lifecycle/durability.
8. Converge at I0 Material Causality.
9. Only then move UI vocabulary/application cleanup off legacy PhraseSourceToggle.
```

G4 may continue in parallel, but its current exact head is not globally GREEN and its branch must eventually be replayed onto the accepted 0.9.10 base.

## 19. Current architecture verdict

The project is moving in the intended direction.

The most important improvement is not a new class: it is the separation of authorities.

```text
GF2               = musical truth
Material/Persist  = canonical material truth
Resolution        = control-side truth boundary
M3/M4             = audible truth
Song/Phrase       = arrangement
Phrase Bank       = edit target
UI                = musician intent/presentation
```

The dominant remaining risk has also become much clearer: there is still a hole between canonical material truth and runtime truth. That hole is `Application Operations + Material Resolution`.

Closing that boundary is the architectural center of the next phase of 0.9.11.