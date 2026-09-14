# FS2A CURRENT / NEXT CAUSALITY — Implementation Plan

Authoritative production base:

```text
860b10afd60eb1a90c1be62bb5baba529c79fdf5
```

Branch:

```text
feature/20260914-fs2a-current-next-causality
```

Design:

```text
docs/superpowers/specs/2026-09-14-fs2a-current-next-causality-design.md
```

## Scope rule

FS2A is session causality only.

It must not implement or redefine durable ACCEPT, DISCARD-to-canonical, persistence, cold boot restoration, new MaterialId creation, Take branching, Material UI, Song references, or FS1D hardware evidence.

The implementation must preserve:

```text
NEXT is not audible.
CURRENT is audible.
ACCEPTED survives reboot.
```

`NEXT -> CURRENT` is ACTIVATE, never ACCEPT.

## Causal binding tightening

A candidate is prepared against both:

```text
MaterialReference
accepted MaterialVersionToken
```

Reference equality prevents stale target/address/identity use. Version equality prevents a legacy or concurrent canonical mutation under the same MaterialId from silently activating a candidate prepared against older accepted musical bytes.

For the first production slice, accepted Pattern provides an in-memory canonical version. Accepted Melody remains unsupported because proving its canonical version may require filesystem I/O, which is forbidden at the musical boundary.

---

## Task 1 — RED: add FS2A host contract harness

Create:

- `tests/test_fs2a_current_next_causality.cpp`
- `tests/run_fs2a_current_next_causality_tests.sh`

The runner mirrors the existing M4 host build and uses the same SDL host dependencies and source set.

The RED test must cover, per voice where applicable:

1. **Clean prepare**
   - prepare Melody NEXT against a clean accepted Pattern;
   - CURRENT remains unchanged;
   - canonical Scene Pattern remains unchanged;
   - candidate is pending and lifecycle-bound to the current MaterialReference/version;
   - voice B remains unchanged.

2. **Replace-on-success**
   - prepare A1, then valid A2;
   - result distinguishes replacement;
   - pending payload becomes A2;
   - CURRENT/canonical/B unchanged.

3. **Replacement failure preserves prior NEXT**
   - prepare valid A1;
   - attempt invalid replacement;
   - result is failure;
   - A1 payload and reference/version binding remain intact;
   - CURRENT/canonical/B unchanged.

4. **Dirty CURRENT rejects prepare**
   - create identity-bound Working Pattern edit using the existing Working path;
   - assert dirty;
   - prepare NEXT returns `RejectedCurrentDirty`;
   - edit, canonical, existing NEXT and B are preserved.

5. **Cancel is per voice**
   - prepare A and B;
   - cancel A;
   - A has no lifecycle pending candidate;
   - CURRENT/canonical untouched;
   - B pending unchanged.

6. **Activation**
   - prepare on clean Pattern;
   - activate A at boundary;
   - Working/CURRENT becomes candidate Melody;
   - runtime publication becomes Melody;
   - canonical Scene Pattern, MaterialId and canonical Pattern VersionToken stay unchanged;
   - B unchanged.

7. **Dirty-after-prepare race**
   - prepare A while clean;
   - edit Working A before boundary;
   - activation returns `RejectedCurrentDirty`;
   - edited CURRENT and NEXT are both preserved;
   - canonical unchanged;
   - independently clean B may still activate.

8. **Reference-retarget race**
   - prepare for Material A;
   - cleanly retarget current UI/Scene reference to Material B without activating A;
   - activation returns `RejectedReferenceMismatch`;
   - CURRENT B/canonical B unchanged;
   - candidate A remains pending.

9. **Canonical-version race under same MaterialId**
   - prepare against accepted Pattern version V1;
   - test-only mutate accepted Pattern bytes in Scene while preserving the same MaterialId/reference;
   - activation returns `RejectedCanonicalChanged`;
   - candidate remains pending;
   - CURRENT is not replaced.

10. **Unsupported accepted Melody fails closed**
    - put the current accepted representation into a state for which FS2A cannot prove an in-memory canonical Pattern snapshot;
    - prepare returns `UnsupportedCurrentState`;
    - no existing candidate/current/canonical state is destroyed.

Run RED before production API exists and record the expected compile/contract failure. Do not weaken the test to make it compile against the lower M4 primitive.

---

## Task 2 — Add lifecycle API and candidate binding metadata

Modify:

- `src/dsp/miniacid_engine.h`

Add narrow session-only result types, for example:

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
  RejectedCanonicalChanged,
  RejectedUnboundCandidate,
  UnsupportedCurrentState,
  InvalidVoice,
};
```

Add upper lifecycle methods with no persistence semantics:

```cpp
NextPrepareResult prepareNextMelody(
    int voiceIndex,
    const PhraseRuntime::RuntimeSynthEventBuffer& candidate);

bool cancelNextMaterial(int voiceIndex);

NextActivationResult activateNextMaterialAtBoundary(int voiceIndex);
```

Do not accept arbitrary target slot/MaterialId in the prepare API. Derive the current `MaterialReference`; NEXT is a realization of that current Material.

Extend each `PendingMaterial` only with small causal metadata:

```text
MaterialReference preparedFor
MaterialVersionToken acceptedVersion
bool lifecycleBound
```

This metadata is not a third musical payload owner. Do not allocate another `RuntimeSynthEventBuffer`.

Keep the lower M4 primitive API for primitive regression tests.

---

## Task 3 — Implement fail-closed readiness and activation

Modify:

- `src/dsp/miniacid_engine.cpp`
- `src/dsp/miniacid_engine.h` only where existing inline Working helpers make that unavoidable.

Implement an internal classifier that can prove one of:

```text
CleanAcceptedPattern
DirtyCurrent
UnsupportedCurrentState
```

For the first slice:

- derive current `MaterialReference` with the existing identity path;
- require current accepted resident representation to be Pattern;
- compute accepted canonical version in RAM with `versionForPattern`;
- if Working is empty, the accepted Pattern is clean;
- if Working holds Pattern bound to the same reference, compare it with accepted Pattern to determine clean/dirty;
- if Working holds Melody while canonical remains Pattern, CURRENT is dirty;
- mismatched/unprovable retained Working is fail-closed unsupported/dirty as appropriate;
- accepted Melody is unsupported in FS2A rather than performing filesystem I/O.

### Prepare

Order is important:

1. validate voice/candidate and pending buffer availability;
2. classify CURRENT;
3. reject dirty/unsupported without changing an existing NEXT;
4. capture current `MaterialReference` and accepted Pattern VersionToken;
5. remember whether a valid lifecycle candidate already existed;
6. call the existing lower `stagePendingMaterial()`;
7. only after staging succeeds, atomically replace causal metadata (`reference`, `version`, `lifecycleBound=true`);
8. return `Prepared` or `Replaced`.

A failed replacement must preserve both payload and causal metadata of the previous valid NEXT.

### Cancel

Per voice only:

- clear queued/lifecycle-bound state for that voice;
- do not mutate buffer bytes, CURRENT, canonical, runtime source, or the other voice.

### Activation

Activation performs no allocation, filesystem I/O, generation or persistence.

For one voice:

1. no queued candidate -> `NoPending`;
2. candidate must be lifecycle-bound;
3. derive current reference and require exact equality with candidate reference;
4. require accepted current representation is still Pattern;
5. recompute accepted Pattern VersionToken and compare with captured version;
6. re-check CURRENT dirty now, not only at prepare time;
7. on any rejection, preserve both CURRENT and NEXT;
8. on success, copy pending Melody into Working/CURRENT, publish runtime active material, then clear only that voice's queued/binding state.

Because one voice can reject while the other is clean, factor the existing global lower activation into a private per-voice primitive if needed. Keep `activatePendingMaterial()` behavior compatible with existing M4 tests.

---

## Task 4 — GREEN FS2A and M4 primitive regression

Run:

```text
bash tests/run_fs2a_current_next_causality_tests.sh
bash tests/run_m4_next_material_tests.sh
```

Required:

```text
FS2A current-next causality: PASS
M4 next material: PASS
```

Do not call FS2A GREEN if M4 semantics regress.

---

## Task 5 — CI verification checkpoint

Create:

- `.github/workflows/0-9-12-fs2a-current-next-causality.yml`

The workflow should:

1. checkout exact branch SHA;
2. assert authoritative production root `860b10...` is an ancestor;
3. assert no FS1 verification branch is used as base;
4. install `build-essential libsdl2-dev libsdl2-gfx-dev`;
5. run FS2A host contract;
6. run M4 primitive regression;
7. run directly relevant Working/Material identity/version host/source contracts already present in the repository;
8. prove changed production paths do not include persistence implementation, Melody storage format, Song routing, or UI semantics;
9. optionally build the normal Cardputer image as a compile/memory regression, but do not label that FS1D hardware evidence;
10. require clean tracked source at end.

CI must not add synthetic NEXT production behavior merely to make a test pass.

---

## Task 6 — Scope and durability audit

Compare exact base to final FS2A SHA.

Required final production scope should be narrowly limited to the session lifecycle gate around existing Working/Pending owners. In particular, no behavior changes are allowed in:

```text
src/state/melody_storage*
src/state/melody_promotion*
project save/load semantics
Song MaterialRef semantics
MaterialId allocation/history
```

Search final diff for `ACCEPT`, persistence writes, `commitSelectedScene`, filesystem APIs and new `RuntimeSynthEventBuffer` allocations. Any new durable mutation or third musical payload owner is a STOP condition.

---

## Task 7 — Final verification statement

Before claiming completion, verify exact remote HEAD and CI result on that SHA.

FS2A may be called GREEN only with evidence for:

```text
prepared NEXT is inaudible
candidate is bound to reference + accepted canonical version
replacement is replace-on-success
CURRENT dirty rejects prepare
CURRENT dirty at boundary rejects activation
stale reference rejects activation
stale canonical version rejects activation
cancel is per voice
activation mutates CURRENT/runtime only
canonical payload/MaterialId/VersionToken remain unchanged
voice ownership remains independent
M4 lower primitive remains green
```

Allowed statement after GREEN:

> CURRENT/NEXT session causality is fail-closed: a prepared candidate is inaudible, reference-and-version-bound, replace-on-success, independently owned per voice, cannot overwrite dirty Working or a changed canonical base, and becomes CURRENT only at a musical activation boundary. ACCEPTED/canonical truth is untouched.

Do **not** claim durable ACCEPT, DISCARD, cold-boot closure, full NEXT generation, Material UI, or FS1D hardware ratification.