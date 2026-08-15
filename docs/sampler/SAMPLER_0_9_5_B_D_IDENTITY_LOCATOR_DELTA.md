# GroovePuter 0.9.5-B/D — Logical Asset Identity and Missing Locator Delta

## Purpose

Resolve two coupled productization problems before production implementation:

1. canonical kit identity must survive a physical kit-directory rename;
2. missing-sample UX needs a reversible locator without adding large fixed state to the realtime Scene or Cardputer ADV BSS.

Reference production behavior is the final 0.9.3 sampler recovery at:

```text
54a30847a7974655e54e3e472a8de0fbbb1042c2
```

That line already has:

- compact 64-bit `SampleRef` persistence;
- compact 32-bit runtime `SampleId` / `SampleHandle` audio ownership;
- stable loose-sample identity based on canonical `/samples` paths;
- a bounded unresolved-ref sidecar that survives Save/reboot while an external asset is absent.

0.9.5 extends this ownership model. It does not replace it.

## Hardware list

### Research

- GroovePuter repository checkout;
- Python 3.

### Future Cardputer validation

- M5Stack Cardputer ADV / ESP32-S3;
- microSD with canonical `/samples` and `/kits` fixtures;
- USB-C data cable;
- exact final 0.9.4-derived 0.9.5 candidate.

No PORT.A wiring or external display is required.

## Wiring

No wiring changes. Use normal Cardputer ADV USB-C and microSD setup.

---

## B — identity namespaces

### Loose samples

Keep current compatibility semantics for loose samples.

Logical key:

```text
samples/<relative-path>
```

The current mount aliases remain equivalent:

```text
/samples/kick.wav
/sd/samples/kick.wav
samples/kick.wav
```

They resolve to the same canonical loose key and therefore the same `SampleRef`.

This preserves existing 0.9.3 Scene ownership.

### Kit assets

A kit asset must **not** derive stable identity from its physical directory path.

Wrong:

```text
/kits/SP12/snare.wav
-> hash physical path
```

because:

```text
/kits/SP12/
rename to
/kits/MyRenamedSP12/
```

would change every persisted sample identity even though manifest kit identity did not change.

Canonical kit asset locator:

```text
kit:<stable-kit-id>:<relative-asset-path>
```

Example:

```text
kit:sp12.factory.v1:snare.wav
```

Kit asset `SampleRef` is derived from this **logical locator**, not from the physical `/kits/<directory>/...` path.

### Namespace separation

These must remain distinct:

```text
samples/sp12.factory.v1/snare.wav
kit:sp12.factory.v1:snare.wav
```

Even if the filenames happen to describe the same PCM, loose-sample ownership and kit-asset ownership are separate logical namespaces.

### Physical resolution

`KitCatalog` owns:

```text
stable kit id -> current physical kit directory
```

The selected manifest owns:

```text
relative asset path
```

Resolution becomes:

```text
kit asset SampleRef
-> logical locator / manifest ownership
-> stable kit id
-> current KitCatalog directory
-> validated relative asset path
-> physical WAV path
```

The physical path may change between boots while the logical `SampleRef` remains stable.

### Duplicate kit identity

If two physical directories expose manifests with the same stable kit id:

```text
/kits/SP12/kit.json          id=sp12.factory.v1
/kits/OtherSP12/kit.json     id=sp12.factory.v1
```

the stable kit identity is ambiguous and must fail closed.

Never select first/last directory by enumeration order.

### SampleRef collision ownership

If one logical `SampleRef` is observed bound to two different logical asset locators, fail that binding closed just as current stable/legacy collision ownership does.

Do not silently rebind a runtime ID to another physical file.

---

## D — why the current unresolved-ref sidecar is not enough for UX

Final 0.9.3 stores missing ownership as a compact `SampleRef`. That is enough to avoid wrong substitution and restore the sample when the same ref is resolvable again.

It is not enough to render:

```text
PAD 3
MISSING
kit:sp12.factory.v1:snare.wav
```

because a 64-bit hash is intentionally not reversible.

Therefore 0.9.5-D needs a bounded **missing-locator sidecar** in addition to the existing unresolved-ref sidecar.

---

## D — locator persistence boundary

### Logical locator forms

Loose:

```text
samples/<relative-path>
```

Kit:

```text
kit:<stable-kit-id>:<relative-asset-path>
```

The research contract caps one encoded locator at:

```text
128 bytes
```

No absolute path, path traversal, backslash alias or invalid kit id is accepted.

### Scene file

The serialized project may carry a locator field beside the existing stable ref at the storage boundary.

Conceptually:

```json
{
  "id": 0,
  "ref": "0123456789abcdef",
  "loc": "kit:sp12.factory.v1:snare.wav"
}
```

Exact short field spelling is implementation-owned, but the persisted representation must remain inside the existing bounded per-pad streaming filter model.

The locator is not added to `SamplerPadState`, `SampleSlot`, `SampleHandle`, `SamplerVoice`, or realtime audio structs.

### Existing filter buffers

Final 0.9.3 currently uses:

```text
kMaxPadObjectBytes = 384
kMaxOutputBytes    = 448
```

A 128-byte locator plus the existing scalar pad fields/ref is expected to fit this budget, but the production D implementation must prove it with an executable maximum-size persistence regression before changing these constants.

Do not casually increase these buffers, because each filter instance already owns bounded scratch state.

---

## D — conditional runtime locator sidecar

### Do not add a fixed 1–2 KiB BSS array

Cardputer fixed DRAM has historically been a tight gate. A design such as:

```cpp
char missingLocators[8][129];
```

as a permanent global/static object spends approximately 1 KiB even when no sample is missing.

That is the wrong default.

### Research target

Use **one conditional control-side allocation** only when at least one user-facing sampler pad needs a missing locator.

Reference bounded layout for pads 1..8:

```text
8 x 129-byte locator slots = 1032 B
8 x 1-byte lengths         =    8 B
1-byte occupancy mask      =    1 B
-------------------------------------
raw payload                = 1041 B
```

Allowing alignment/header overhead, the research contract caps the one conditional allocation at:

```text
<= 1088 B
```

When there are no missing user-facing locators:

```text
locator sidecar allocation = 0 B
```

This is control/UI persistence state, not sampler PCM pool capacity.

The final C++ layout may be more compact, but it must not become unbounded or allocate one independent heap string per pad by default.

### Pads 9..16

The recovered product surface exposes pads 1..8. Internal/reserved pads 9..16 retain current stable-ref persistence behavior but do not require product locator strings in 0.9.5-D.

This keeps the UX sidecar bounded to eight locators.

---

## D — transactional Scene load publication

The existing final 0.9.3 filter already avoids publishing partial unresolved refs from a failed Scene load.

Locator state must match that behavior.

During Scene load:

```text
parse pad object
-> collect pending unresolved ref + locator
-> continue streaming Scene
-> finish succeeds
   -> publish complete locator sidecar
-> finish/parser fails
   -> publish no partial locator sidecar
```

Do not write global missing-locator state pad-by-pad before the whole streaming filter finishes successfully.

A temporary pending locator sidecar may itself be a bounded heap allocation owned by the filter/load transaction rather than a ~1 KiB stack member.

This avoids expanding `SamplerSceneFilter` stack footprint substantially.

---

## D — clearing and recovery

When a formerly missing asset resolves normally:

- runtime `SampleId` becomes valid;
- missing locator entry for that user pad is cleared;
- if no locator entries remain, release the conditional locator sidecar allocation.

For a resident sample, persistence can derive logical locator from the active registry/catalog ownership at Save time. The missing-only sidecar is not the primary identity database for resident samples.

Successful `RELINK` similarly commits the new identity/locator, then clears stale missing-sidecar state for that pad.

Failed `RELINK` leaves it unchanged.

---

## B/D interaction with rename scenarios

### Physical kit directory renamed, manifest id unchanged

Before:

```text
/kits/SP12/
kit id = sp12.factory.v1
asset = snare.wav
logical locator = kit:sp12.factory.v1:snare.wav
```

After:

```text
/kits/MyRenamedSP12/
kit id = sp12.factory.v1
asset = snare.wav
logical locator = kit:sp12.factory.v1:snare.wav
```

Expected:

```text
same logical locator
same SampleRef
new physical path
project still resolves
```

### Kit manifest missing entirely

The logical ref/locator can remain persisted, but the asset cannot resolve physically.

Expected UX:

```text
MISSING
kit:sp12.factory.v1:snare.wav
```

No fallback to another kit with a similar directory name or filename.

### Asset renamed inside same kit

This is a true logical asset identity change unless the manifest explicitly keeps the same relative locator through another future alias mechanism.

0.9.5-D `RELINK` is the correct product mechanism to choose the replacement.

Do not guess by basename.

---

## Build / flash steps

### Research

From repository root:

```bash
bash tests/run_sampler_0_9_5_research_tests.sh
```

Relevant contracts:

```text
tests/test_sampler_0_9_5_logical_identity_contract.py
tests/test_sampler_0_9_5_locator_sidecar_contract.py
```

### Future production B/D

Only on the final 0.9.4-derived implementation line:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Then flash normally:

```bash
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

---

## Expected behavior

### Screen

A valid kit keeps the same logical identity if its physical folder is renamed.

A missing user pad shows a meaningful logical locator rather than only `(empty)` or an opaque hash.

### Serial

Diagnostics should distinguish:

- duplicate stable kit id;
- invalid logical locator;
- logical ref collision;
- physical kit/asset missing;
- successful logical->physical resolution.

Exact wording is implementation-owned.

### Memory

No missing assets:

```text
missing locator sidecar = 0 B
```

One or more missing user pads:

```text
one bounded conditional allocation <= 1088 B
```

The implementation must not spend ~1 KiB permanent fixed BSS for a rare error/UX state.

---

## Troubleshooting

### Renaming `/kits/SP12` breaks Scene Load

Check whether kit asset `SampleRef` was derived from the physical path. It must be derived from `kit:<stable-id>:<asset>`.

### Two different kits with `snare.wav` collide

The stable kit id is missing from the logical sample key. Kit identity must namespace the asset.

### Missing path UX shows only hex ref

The locator storage boundary was not persisted/published. `SampleRef` alone is not reversible.

### Fixed DRAM budget regresses by ~1 KiB

Look for a permanent static locator array. Use the conditional bounded control-side sidecar instead.

### Failed Scene load leaves some missing locators visible

The load path published the locator sidecar incrementally. Publish only after successful streaming-filter `finish()`.

### Locator survives after sample resolves

Clear the missing-only entry on normal resolution/relink commit and release the sidecar block when empty.

---

## Acceptance checklist

### Research

- [ ] loose `/samples` refs retain current mount-alias identity semantics;
- [ ] kit asset ref derives from logical kit id + relative asset locator;
- [ ] physical kit-directory rename does not change kit asset SampleRef;
- [ ] same filename in different kit ids yields different refs;
- [ ] loose and kit namespaces do not alias;
- [ ] duplicate stable kit ids fail closed;
- [ ] logical->physical binding never silently rebinds one ref;
- [ ] missing locator is max 128 bytes and traversal-safe;
- [ ] locator is storage/control-side only, not realtime Scene/audio ABI;
- [ ] current 384/448 persistence buffers are proven before any size increase;
- [ ] missing locator storage costs 0 B when unused;
- [ ] populated locator sidecar uses one bounded allocation <=1088 B;
- [ ] locator UX covers user pads 1..8 only in 0.9.5-D;
- [ ] failed Scene load publishes no partial locator sidecar;
- [ ] resolving/relinking the last missing pad releases locator sidecar storage.

### Future Cardputer ADV

- [ ] Save project using kit asset, rename kit directory, reboot/load, same pad resolves;
- [ ] duplicate kit-id fixture fails closed without wrong substitution;
- [ ] remove one kit WAV and verify logical locator appears on its pad;
- [ ] Save/reboot while missing preserves locator;
- [ ] restore asset/manifest and verify automatic resolution;
- [ ] successful RELINK clears stale missing state;
- [ ] fixed DRAM gate does not regress by a permanent locator array;
- [ ] conditional sidecar allocation remains bounded;
- [ ] no WDT/reset/UAF/audio underrun regression.
