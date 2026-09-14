# GroovePuter 0.9.12 — FS2A CURRENT / NEXT CAUSALITY

## Status

Design freeze for the first session-only Material lifecycle slice.

Authoritative production base:

```text
release/0.9.11-rc1
860b10afd60eb1a90c1be62bb5baba529c79fdf5
```

Implementation branch:

```text
feature/20260914-fs2a-current-next-causality
```

FS1D remains a separate hardware checkpoint and is not promoted by FS2A.

---

## 1. Purpose

FS2A closes only the causal boundary between the material being worked on now and one prepared candidate for the same material.

It does **not** implement durable ACCEPT, DISCARD-to-canonical, cold-boot restoration, Material UI, Song integration, or a new persistence format.

The three truths are:

```text
CANONICAL / ACCEPTED
    durable musical truth
    MaterialId
    MaterialVersionToken
          |
          | resolve
          v
CURRENT / WORKING
    session mutable truth
    may be dirty
    is the material from which runtime sound is projected
          |
          | request candidate
          v
NEXT
    prepared candidate
    not canonical
    not audible
```

Transitions:

```text
NEXT --ACTIVATE--> CURRENT
NEXT --CANCEL----> disappears

CURRENT --ACCEPT--> CANONICAL   [FS2B, durable]
CURRENT --DISCARD-> CANONICAL   [FS2B, restore]

SAVE --> project/reference organization, not FS2A
```

`NEXT -> CURRENT` is never named ACCEPT.

---

## 2. Product invariants

### 2.1 Three-sentence model

1. **NEXT is not audible.**
2. **CURRENT is audible.**
3. **ACCEPTED survives reboot.**

AUDIBLE is not another owner:

```text
AUDIBLE = runtime projection of CURRENT
```

At a musical boundary:

```text
NEXT
  -> validate session causality
  -> CURRENT := NEXT
  -> runtime observes the new CURRENT
```

The audio path never reads NEXT directly.

### 2.2 ACCEPT remains durable

The release acceptance remains:

```text
edit
-> ACCEPT
-> hard power off
-> same accepted material restores
```

Therefore FS2A must not mutate canonical payload, `MaterialId`, canonical `MaterialVersionToken`, persistent descriptors, or Scene as an implementation shortcut.

### 2.3 NEXT is a realization of the same Material

For 0.9.12 NEXT/VARIANT/REWORK is not logical replacement.

```text
NEXT creation:          MaterialId unchanged
NEXT activation:        MaterialId unchanged
ordinary Working edit: MaterialId unchanged
```

A future explicit `NEW MATERIAL` / `REPLACE MATERIAL` / `MAKE UNIQUE` owns new identity creation.

---

## 3. Existing production owners at 860b10

### CANONICAL

- Material identity is resident in Scene through `MaterialReference` / `MaterialId`.
- `MaterialVersionToken` is the exact accepted-state fingerprint from `src/state/material_version.h`.
- `resolveMaterial()` proves identity before representation/payload and computes the canonical version only after successful resolution.

### CURRENT

- `MiniAcid::workingMaterial_[2]` is the per-voice session payload owner.
- `WorkingMaterialStorage` reuses one 1284-byte payload footprint per voice.
- Pattern Working stores a compact `MaterialReference` in unused payload tail bytes.
- Melody occupies the full payload and therefore has no embedded MaterialReference.
- `activeMaterial_[2]` is a small runtime publication (`slot + kind`), not canonical truth.

### NEXT

- `MiniAcid::pendingMaterial_[2]` already provides independent per-voice M2 storage.
- Each pending Melody buffer is fixed after startup and separately owned.
- `stagePendingMaterial()` already has replace-on-success behavior for Melody: invalid/null replacement fails before mutating the valid queued candidate.
- `activatePendingMaterial()` is only a lower primitive. At the production base it has no end-to-end NEXT producer and does not enforce dirty or identity causality.

---

## 4. Dirty policy

FS2A uses the strict first-release policy:

```text
if CURRENT is dirty:
    request NEXT = REJECT_CURRENT_DIRTY
```

Required consequences:

```text
CURRENT unchanged
CANONICAL unchanged
existing NEXT unchanged
other voice unchanged
```

There is no third musical payload owner for preserving an old dirty CURRENT after activation. Automatic snapshotting, branching and expanded Take history are out of scope.

The same condition is checked again at ACTIVATE. A candidate prepared while CURRENT was clean must not overwrite edits made before the boundary.

### 4.1 Existing Pattern dirty authority

At 860b10, Pattern dirty is already authority-based rather than a second flag:

```text
identity-bound Working Pattern
    compared with
Scene / ACCEPTED Pattern
```

`MiniAcid::hasModifiedWorking303Pattern()` performs that comparison and already blocks manual pattern/bank/page retarget when dirty.

FS2A must not add a competing Pattern dirty flag.

### 4.2 Melody dirty limitation at the production base

The current root has no equivalent in-RAM canonical-version owner for an accepted Melody. Computing its canonical version through `resolveMaterial()` may require filesystem I/O, which cannot be introduced at the musical activation boundary.

Therefore the first FS2A production slice is intentionally bounded to the path whose clean/dirty authority can be proven without inventing a second owner:

```text
clean ACCEPTED Pattern
    -> prepared Melody NEXT for the same Material
    -> ACTIVATE
    -> CURRENT becomes the Melody realization
    -> CURRENT is now dirty relative to the accepted Pattern
```

FS2A must fail closed rather than pretend that an accepted Melody is provably clean when no in-RAM canonical snapshot proves it.

This is a scope boundary, not a statement that Melody-to-Melody NEXT is undesirable. A later canonical session snapshot introduced with durable closure may generalize the same contract without changing the semantics below.

---

## 5. Candidate identity binding

Voice ownership alone is insufficient.

A NEXT candidate must be bound to the exact `MaterialReference` for which it was prepared:

```text
voice
MaterialAddress
MaterialId
```

This prevents the stale-target race:

```text
prepare NEXT for Material A
CURRENT cleanly retargets to Material B
boundary arrives
```

Required result:

```text
candidate A MUST NOT activate into B
CURRENT B unchanged
CANONICAL unchanged
candidate A remains pending
other voice unchanged
```

Activation precondition:

```text
pending.reference == current MaterialReference
```

The candidate preserves the same `MaterialId`; preparation and activation never allocate a new identity.

---

## 6. Replace-on-success

A valid existing NEXT is musical work and must not be destroyed by a failed replacement attempt.

```text
valid NEXT A exists
prepare replacement A2 fails

=> NEXT A remains intact
=> CURRENT unchanged
=> CANONICAL unchanged
=> voice B unchanged
```

`stagePendingMaterial()` already has the lower-level Melody property. FS2A must preserve it in the lifecycle API, including metadata/reference binding.

---

## 7. Cancel

Cancellation is per voice:

```text
cancel NEXT A
```

must:

- clear only A's queued candidate;
- leave CURRENT A untouched;
- leave CANONICAL A untouched;
- leave voice B CURRENT/NEXT untouched;
- perform no persistence and no audio mutation.

Cancellation is not DISCARD. `DISCARD` is FS2B and restores CURRENT from CANONICAL.

---

## 8. Activation boundary

FS2A adds a lifecycle gate above the existing M4 primitive.

For each voice independently at the musical boundary:

1. no pending candidate -> no-op;
2. verify the pending candidate is still bound to the current MaterialReference;
3. re-check CURRENT dirty;
4. if reference mismatches or CURRENT is dirty, leave that voice's NEXT queued and leave CURRENT untouched;
5. if valid, copy candidate into Working/CURRENT, publish the runtime source, then clear only that voice's pending flag;
6. failure/rejection of one voice must not block activation of the other clean voice.

No allocation, filesystem I/O, generation, or durable publication is allowed at the boundary.

---

## 9. FS2A lifecycle API boundary

The lower FS1 primitive remains available for low-level tests but is not the product lifecycle API.

FS2A should introduce a narrow upper API with explicit results rather than overloading `bool`:

```cpp
enum class NextPrepareResult : uint8_t {
  Prepared,
  Replaced,
  RejectedCurrentDirty,
  UnsupportedCurrentState,
  InvalidVoice,
  InvalidCandidate,
  PendingUnavailable,
};

enum class NextActivationResult : uint8_t {
  NoPending,
  Activated,
  RejectedCurrentDirty,
  RejectedReferenceMismatch,
};
```

Exact names may vary only if tests and implementation retain the semantic distinctions above.

The prepare API must not accept an arbitrary new MaterialId or arbitrary target MaterialReference. It derives/binds the current reference because NEXT is a realization of the current Material.

---

## 10. RED contract

### Clean CURRENT

```text
prepare NEXT A
ASSERT CURRENT unchanged
ASSERT CANONICAL unchanged
ASSERT B unchanged
ASSERT candidate bound to current MaterialReference

replace NEXT A
ASSERT CURRENT unchanged
ASSERT CANONICAL unchanged
ASSERT B unchanged
```

### Dirty CURRENT

```text
edit CURRENT A
ASSERT dirty == true

request NEXT A
ASSERT REJECT_CURRENT_DIRTY
ASSERT CURRENT edit preserved
ASSERT CANONICAL unchanged
ASSERT existing NEXT unchanged
ASSERT B unchanged
```

### Cancel

```text
prepare NEXT A
cancel NEXT A

ASSERT CURRENT unchanged
ASSERT CANONICAL unchanged
ASSERT no pending A
ASSERT B unchanged
```

### Activation

```text
prepare NEXT A
activate at boundary

ASSERT CURRENT becomes candidate
ASSERT CANONICAL unchanged
ASSERT MaterialId unchanged
ASSERT canonical MaterialVersionToken unchanged
ASSERT B unchanged
```

### Dirty-after-prepare race

```text
prepare NEXT A while CURRENT clean
edit CURRENT A
activate at boundary

ASSERT activation rejected CURRENT_DIRTY
ASSERT edited CURRENT preserved
ASSERT NEXT preserved
ASSERT CANONICAL unchanged
ASSERT clean pending B may still activate independently
```

### Retarget-after-prepare race

```text
prepare NEXT for Material A
retarget clean CURRENT to Material B
activate at boundary

ASSERT activation rejected REFERENCE_MISMATCH
ASSERT CURRENT B unchanged
ASSERT NEXT A preserved
ASSERT CANONICAL unchanged
```

### Replacement failure

```text
valid NEXT A exists
prepare replacement fails

ASSERT previous valid NEXT A preserved
ASSERT previous NEXT metadata/reference preserved
ASSERT CURRENT unchanged
ASSERT CANONICAL unchanged
ASSERT B unchanged
```

---

## 11. Out of scope

FS2A does not implement or redefine:

- durable `ACCEPT`;
- `DISCARD` restore from canonical;
- cold-boot restoration;
- project `SAVE` semantics;
- new disk format;
- new `MaterialId` allocation;
- lineage / Take branching;
- Song MaterialRef migration;
- Material UI verbs or shortcuts;
- QWERTY/performance/nanoKEY2/USB Host work;
- Pattern NEXT payload storage that does not already exist;
- synthetic FS1D hardware evidence.

---

## 12. Acceptance

FS2A is GREEN only when host tests prove all RED contracts above and the production change remains session-only.

The canonical release statement after FS2A is:

> CURRENT/NEXT session causality is fail-closed: a prepared candidate is inaudible, identity-bound, replace-on-success, independently owned per voice, cannot overwrite dirty Working, and becomes CURRENT only at a musical activation boundary. ACCEPTED/canonical truth is untouched.

This statement does **not** claim durable ACCEPT, DISCARD, cold-boot closure, or full Material UI.