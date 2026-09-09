# GroovePuter 0.9.11 — P0 Architecture Evidence Pass 2

Status: **RESEARCH EVIDENCE / NO PRODUCTION CHANGES**

Branch: `architecture/20260909-0.9.11`

This pass investigates the boundary between the runtime PATTERN/PHRASE source model and the persistent MaterialSlot/Melody model, then traces page switching through the actual control-side loader.

---

## EVID-005 — Runtime MAKE PHRASE and persistent Material promotion are separate authorities

```text
CLASS: FIX / ARCHITECTURAL CORRECTNESS
SEVERITY: P0
STATUS: CONFIRMED
0.9.11: MUST
MEMORY IMPACT: 0 expected for authority unification; persistence cost separately measured
```

### Files / functions

- `src/state/material_slot.h`
- `src/ui/phrase_source_toggle.h`
- `src/dsp/miniacid_engine.cpp`
- `MiniAcid::makePhrase()`
- `MiniAcid::setSequencedSource()`
- `src/state/melody_promotion.h`

### Contract already stated by the codebase

`material_slot.h` explicitly states:

```text
One canonical representation per slot, and promotion is one-way.
Holding a Pattern and a Melody for the same slot would immediately ask
which one Song plays, which one the generator rewrites, and what Undo means —
two owners again.
```

### Actual runtime operation

`PhraseSourceToggle` is the UI-level single owner of PATTERN/PHRASE switching. It operates through:

```text
engine.currentSequencedSource()
engine.currentPhraseBuffer()
engine.setSequencedSource()
engine.makePhrase()
```

It does not mutate the persistent `Scene.materialSlots` descriptor and does not call MelodyPromotion/storage.

`MiniAcid::makePhrase()`:

1. projects the active Pattern into a local `RuntimeSynthEventBuffer candidate`;
2. assigns `currentPhrase_[voice] = candidate`;
3. changes `activeMaterial_[voice].kind` to Melody through `setSequencedSource()`.

It does not:

- update the slot's persistent MaterialKind;
- write or verify a GPML payload;
- make project/page persistence aware of the new runtime Melody.

### Split authority state

A successful user-visible MAKE PHRASE can therefore produce:

```text
runtime:
ActiveMaterial.kind = MELODY
currentPhrase_ = edited/event material

persistent Scene/page descriptor:
MaterialSlot.kind = PATTERN

persistent sidecar:
no GPML required / possibly absent
```

The runtime and persistence layers can disagree about the representation of the same logical slot.

### Invariant violated

```text
A slot has one canonical musical representation.
Runtime authority may be a projection/cache of that representation,
but it must not independently decide Pattern vs Melody.
```

### Why this is not merely naming debt

This disagreement changes behavior across page/project/persistence boundaries. It creates a musical source that can sound and be edited while the persistent object graph still says the slot is Pattern.

### Minimal direction

Do not merge runtime and storage into one large owner. Introduce one bounded application operation for one-way conversion:

```text
Pattern material
  -> prepare Melody candidate
  -> validate/persist according to chosen durability contract
  -> publish canonical Material descriptor
  -> publish/activate runtime projection
  -> explicit receipt
```

The runtime ACTIVE/NEXT boundary remains the consumer and should not become a filesystem owner.

### Required implementation RED

A user-level MAKE operation must be tested across:

- immediate playback;
- page switch away/back;
- project save/reload;
- failed storage/persistence preparation.

One operation cannot report success while those layers disagree about Pattern vs Melody.

---

## EVID-006 — Page switch can leave the old page's Phrase sounding on the new page

```text
CLASS: FIX
SEVERITY: P0
STATUS: CONFIRMED
0.9.11: MUST
MEMORY IMPACT: 0 / uses existing bounded material preparation path
```

### Files / functions

- `src/ui/miniacid_display.cpp`
- `MiniAcidDisplay::handlePaging_()`
- `src/dsp/miniacid_engine.cpp`
- `MiniAcid::setCurrentPage()`
- `MiniAcid::currentPhrase_`
- `MiniAcid::activeMaterial_`

### Exact control-side page switch

`handlePaging_()` performs, under AudioGuard:

```text
savePage(current, scene)
loadPage(target, scene)        OR initializeEmptyPage(scene)
rebuildPatternRuntimeEventBank()
setCurrentPage(target)
```

After success, target page Pattern payload/descriptors are resident.

### What `setCurrentPage()` does

`MiniAcid::setCurrentPage()` currently:

```text
hardBarrierPatternPlayback_()
currentPage_.store(page)
```

It does not:

- re-resolve the selected Material slot on the new page;
- load the new page's Melody sidecar;
- stage/activate a new Melody through M4;
- reset or replace `currentPhrase_`;
- change `activeMaterial_[voice].kind`;
- change `activeMaterial_[voice].slot`.

### Concrete audible failure path

```text
page 0
voice A
MAKE PHRASE
ActiveMaterial.kind = MELODY
currentPhrase_ = Melody X

Alt+] -> switch to page 1

page 1 Scene and Pattern runtime bank are loaded
setCurrentPage(1) executes

but:
ActiveMaterial.kind remains MELODY
currentPhrase_ remains Melody X
```

The sequencer selects its source from `ActiveMaterial.kind`, so it continues taking events from the old `currentPhrase_` instead of the newly resident page's material.

### Invariant violated

```text
After an atomic page transition, every active material authority must belong
to the newly visible/resident page (or the transition must explicitly preserve
a globally identified material by design).
```

The current model does neither: runtime material has only a local slot/kind and is not rebound to the new page.

### User / musical consequence

The display/page context can say page 2 while Synth A is audibly playing/editing a Melody created on page 1. This violates the UI constitution principle that shown/edited/sounding source must be the same owner.

### Why the existing lifetime barrier is not enough

`hardBarrierPatternPlayback_()` correctly terminates the old active note. It solves note lifetime, not material identity. After the barrier, the next scheduled onset still comes from the stale old-page Melody buffer.

Therefore the P3 lifetime architecture remains KEEP; the missing piece is control-side material re-resolution/activation.

### Minimal direction

Page switch must be a material transition, not only a Scene/Pattern-bank transition:

```text
prepare target page
resolve target MaterialId(s)
prepare Melody payload(s) if required
validate
commit resident page
publish/stage target material authority
activate at the defined safe boundary
```

If a target Melody cannot be prepared, fail closed and keep the old page/context atomically rather than displaying a new page with an old material owner.

Do not put filesystem I/O on the audio boundary.

### Required RED characterization

For Synth A and B independently:

1. page 0: create/distinguish Melody X;
2. page 1: Pattern or distinct Melody Y;
3. switch page while X is active;
4. prove the next sounding/edited material belongs to page 1;
5. prove no stale X onset occurs after transition;
6. prove no stuck NoteOn/NoteOff regression.

Also test a failed target Melody load and require atomic rollback.

---

## EVID-007 — A3 fail-open API is not the root cause of the current page-switch bug

```text
CLASS: REFACTOR / latent correctness risk
SEVERITY: P1 for now
STATUS: CONFIRMED DISTINCTION
```

The current page-switch failure does not require `materialKind()` returning Pattern for NotResident. The page loader does not perform canonical material resolution at all before calling `setCurrentPage()`.

This matters for ordering:

```text
wrong fix:
change NotResident -> error and assume paging is repaired

required fix:
establish a real control-side Material resolution/transition step,
then make unresolved states explicit within that step
```

The fail-open API should still be corrected, but it is subordinate to the missing material transition owner.

---

# Revised architecture diagnosis after Pass 2

The central 0.9.11 problem is now clearer:

```text
PERSISTENT MATERIAL MODEL
Scene.materialSlots + GPML sidecars
          
          X   <- no single conversion/transition owner
          
RUNTIME PHRASE MODEL
activeMaterial.kind + currentPhrase_
```

M3/M4 solved the *runtime publication boundary* correctly, but the application layer that should feed that boundary from the canonical persistent Material graph is missing/incomplete.

Therefore 0.9.11 must not respond by redesigning the audio engine. It must establish the control-side/application owner connecting:

```text
MaterialId
Persistent descriptor/payload
Page/project lifecycle
ACTIVE/NEXT runtime publication
```

---

# Updated P0 order

```text
A0  Freeze M3/M4 + lifetime owner as KEEP

A1  Canonical global MaterialId
      - page-aware persistent identity
      - stable project namespace

A2  Canonical material conversion operation
      - replace split MAKE PHRASE / MelodyPromotion authority
      - define durability success contract

A3  Atomic page material transition
      - target page + descriptors + payload preparation
      - runtime rebind/ACTIVE-NEXT publication
      - rollback on failure

A4  Empty-page descriptor initialization

A5  Complete project lifecycle
      - Save As copies full reachable material graph
      - then characterize clear/delete/orphans

A6  Explicit unresolved result
      - Pattern != NotResident/Invalid/LoadFailed

A7  Song -> MaterialRef semantics
```

Vocabulary renames and Scene/Genre/UI refactors come only after A1-A7 contracts are stable.

---

# Research state after Pass 2

```text
CONFIRMED P0 FIX:
1. cross-page GPML identity collision
2. stale MaterialKind in initializeEmptyPage
3. Save As copies descriptors/pages without Melody payloads
4. MAKE PHRASE runtime authority diverges from persistent Material authority
5. page switch can leave old-page Phrase sounding on new page

P1 / LATENT:
6. NotResident/Invalid -> Pattern fail-open API
7. Song Pattern-centric reference semantics

KEEP:
- M3 ActiveMaterial publication boundary
- M4 bounded NEXT preparation/activation
- single runtime note lifetime owner
- MelodyStore explicit ABI

NEXT RESEARCH:
- project switch/load with active runtime Melody
- power-loss/durability contract after authority unification
- Song resolver path
- Genre/Feel/Generator ownership matrix
- Scene authority census
```
