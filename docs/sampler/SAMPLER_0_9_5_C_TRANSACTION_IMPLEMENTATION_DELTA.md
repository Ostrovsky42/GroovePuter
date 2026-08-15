# GroovePuter 0.9.5-C — Transactional Kit Load Implementation Delta

## Purpose

Freeze the minimal implementation strategy for transactional kit loading on top of the existing `RamSampleStore` / `SampleHandle` architecture.

The user-visible contract is:

```text
LOAD KIT
-> resolve all required assets
-> calculate memory requirement
-> prepare
   -> failure: old active kit/pads remain usable and Scene is unchanged
   -> success: publish the new kit/pads atomically
```

The transaction is about **active kit semantics**, not restoring the internal LRU warehouse byte-for-byte after every failed attempt.

Reference source head for this research:

```text
agent/20260814-0.9.3-sampler-final-acceptance
54a30847a7974655e54e3e472a8de0fbbb1042c2
```

## Hardware list

### Host research

- GroovePuter repository checkout;
- Python 3;
- normal CI environment.

### Future hardware acceptance

- M5Stack Cardputer ADV / ESP32-S3;
- microSD with canonical 0.9.5 kit fixtures;
- USB-C data cable;
- exact final 0.9.4-derived build.

No PORT.A wiring or external display is required.

## Wiring

No wiring changes. Use the normal Cardputer ADV USB-C and microSD setup.

---

## Existing primitives to reuse

The current sampler store already has the useful realtime-safe lifetime primitive:

```text
SampleHandle
SampleSlot::refCount
acquireHandle()
releaseHandle()
viewHandle()
```

LRU eviction only withdraws samples whose reference count is zero.

0.9.5-C should use this mechanism as a **temporary transaction pin** rather than inventing a second ownership system or fixed transaction buffer.

The implementation may clarify the API comments to state that handle acquire/release is safe for bounded control-thread pinning as well as audio voices. It must not add blocking I/O or locks to the audio handle path.

---

## Transaction phases

### C1. Resolve

From the selected canonical kit manifest:

- resolve stable kit identity;
- resolve every unique pad asset to its stable sample identity / runtime registration;
- reject malformed/missing required assets;
- deduplicate assets shared by multiple pads;
- preserve the current active kit and pad state untouched.

No pad publication occurs here.

### C2. Inspect

For each unique target asset not already resident:

- run the 0.9.5-A bounded WAV metadata inspection;
- obtain exact decoded mono bytes;
- reject unsupported/malformed/over-budget WAVs before staging.

No full PCM allocation occurs during inspect.

### C3. Pin current active kit

Acquire one transaction handle for each unique **resident** sample referenced by the current active user pads.

These temporary handles guarantee that prepare/LRU cannot evict PCM required by the unchanged old pad mapping if the transaction fails.

Deduplicate identical sample IDs before pinning.

If a current pad already references a non-resident sample, the transaction does not invent residency for it merely to improve rollback. The rollback contract preserves the pre-transaction user-visible state; it does not promise to repair an already non-resident old pad.

### C4. Pin already-resident target assets

A target asset that is already resident must also be pinned for the duration of prepare.

Otherwise this sequence would be possible:

```text
target A already resident
-> preload target B
-> LRU evicts unpinned target A
-> final pad publication points to target A that is no longer resident
```

Pinning target-resident assets prevents that race.

If one asset belongs to both old and target sets, hold only one transaction pin for that unique slot/sample.

### C5. Calculate safe transaction peak

Compute on unique assets:

```text
protectedResidentBytes
+
decodedBytesOfTargetAssetsNotResident
```

where protected resident bytes are the unique union of:

- resident old-kit assets pinned for rollback;
- already-resident target assets pinned for success.

Shared old/new assets count once.

If this safe peak exceeds configured sampler pool capacity, reject **before staging any new PCM**.

This is the intended reason a 32 KiB pool may safely reject:

```text
old protected = 25 KiB
new not resident = 27 KiB
safe peak = 52 KiB
```

although either kit would fit alone.

### Pool capacity visibility

The current generic interface exposes `freePoolBytes()` but not configured total capacity.

Production C may add a small control/debug accessor such as:

```cpp
virtual std::size_t poolCapacityBytes() const = 0;
```

or keep equivalent transaction-capacity knowledge in the kit loader if it is already authoritative there.

Do not expose mutable allocator internals or slot arrays to UI code just to perform admission.

### C6. Prepare missing target assets

For each unique target asset not resident:

```text
preload(asset)
-> immediately acquire transaction handle
-> verify view is valid
-> continue
```

Immediate pinning is required. A staged target loaded early in the transaction must not become an LRU victim while later target assets are prepared.

All SD/WAV I/O and allocation stays outside `AudioGuard`.

### Dynamic runtime pressure

Peak calculation describes the transaction's own protected/new footprint. Runtime conditions can still make prepare fail, for example unrelated PCM held by active audio voice handles.

That failure is allowed and must remain safe:

```text
prepare failure
-> no pad/kit publication
-> no Scene dirty
-> release transaction pins
-> old pads still point to protected old samples
```

Do not hold `AudioGuard` while waiting for those conditions or performing eviction/load work.

### C7. Commit

Only after every target asset is resident and transaction-pinned:

- enter one short audio mutation boundary;
- publish the complete user pad mapping for pads 1..8;
- publish active stable kit identity/control-side state;
- apply the defined neutral v1 pad defaults or preserved manifest-owned defaults as one coherent operation;
- exit the mutation boundary;
- mark Scene dirty exactly once.

There must be no intermediate visible state such as:

```text
pad1 new
pad2 new
pad3 old
pad4 empty
```

### C8. Release transaction pins

After successful publication, release only handles acquired by the transaction.

Already-playing `SamplerVoice` instances retain their own handles and may finish using old PCM safely after the active pad mapping has switched to the new kit.

This gives the desired behavior:

```text
old voice already playing
-> kit commit
-> new triggers use new pad assets
-> old voice finishes naturally
-> its handle releases old PCM later
```

No global sampler panic is required for kit commit.

---

## Failure semantics and warehouse cache

### Normative rollback

On any failure before commit:

```text
active kit identity unchanged
pad mappings unchanged
pad musical parameters unchanged
Scene revision unchanged
old resident kit PCM protected during prepare
transaction handles released
```

### Non-normative warehouse cleanup

Successfully staged target PCM from a transaction that later fails **may remain resident** after its transaction pin is released.

That PCM is:

- not referenced by active pads;
- not user-visible as a partial kit;
- refCount zero after transaction cleanup;
- immediately eligible for normal LRU eviction.

This is acceptable and is preferred over adding a special unload/rollback API solely to restore warehouse cache contents.

Therefore do not require:

```text
failed transaction
-> warehouse resident set byte-for-byte identical to before
```

The warehouse is a cache. The active kit is the transaction boundary.

An implementation may opportunistically discard staged unreferenced PCM if a safe existing primitive makes that cheap, but user correctness must not depend on it.

---

## Slot pressure

The product kit surface is at most 8 user pads, while `RamSampleStore` currently has 64 slots. Still, active voice handles and unrelated resident samples can make a slot temporarily unavailable.

The prepare path must fail safely if `preload()` cannot obtain a slot while protected handles are held.

Do not solve this by evicting a protected old/target sample or by mutating pads early.

An exact preflight slot-capacity API is optional unless hardware evidence shows this dynamic failure is common. The safe fallback is prepare failure with unchanged active kit.

---

## Concurrency assumptions

### Audio thread

- continues rendering throughout resolve/inspect/prepare;
- may acquire/release normal voice handles;
- never performs SD I/O or kit parsing;
- never waits on the kit transaction.

### Control/UI thread

- owns kit manifest parsing and transaction orchestration;
- holds transaction handles only as bounded pins;
- enters `AudioGuard` only for final coherent pad/kit publication.

### LRU

- remains warehouse policy;
- may evict unrelated unpinned samples during prepare;
- must not evict transaction-pinned old/target assets.

---

## Build / flash steps

### Research

```bash
bash tests/run_sampler_0_9_5_research_tests.sh
```

The transaction pin reference model is:

```text
tests/test_sampler_0_9_5_transaction_pin_contract.py
```

### Future production C

After A/B are implemented on the final 0.9.4-derived line:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash normally:

```bash
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

---

## Expected behavior

### Screen

Successful `LOAD KIT` changes the visible kit and all assigned pads together.

Failed load leaves the previous kit/pad presentation unchanged and reports a concise failure reason.

No temporary partial kit should be rendered.

### Serial

Useful diagnostic classes include:

```text
resolve failure
WAV inspect failure
transaction peak rejection
prepare/preload failure
slot/runtime pressure
commit success
```

Exact strings are implementation-owned.

### Audio

- audio continues during resolve/inspect/prepare;
- old kit remains playable while preparation runs;
- no long `AudioGuard` pause for SD/WAV work;
- voices already playing at commit may finish via their existing handles;
- new triggers after commit use the new pad mapping.

---

## Troubleshooting

### Old kick disappears before a failed kit load returns

The old resident kit was not transaction-pinned before staging. Protect unique old resident sample handles before any LRU-producing preload.

### A target asset was resident at preflight but missing at commit

Already-resident target assets were not pinned. Pin the complete resident target union during prepare.

### First staged target disappears while loading later pads

Acquire its transaction handle immediately after successful preload.

### Failure leaves `a-new-kick` in RAM

That alone is not a correctness bug. If pads/active kit were never published and the staged sample has no transaction pin afterward, it is normal LRU cache.

### Scene becomes dirty after failed load

Commit ownership is wrong. Scene mutation occurs once, only after all target assets are prepared and pinned.

### Audio stalls during kit load

Check for SD scan, manifest parse, WAV inspect/decode, malloc or LRU loops inside `AudioGuard`. Only final state publication belongs inside the guard.

### Old playing sound cuts off exactly at commit

Transaction cleanup or commit is releasing voice-owned handles/global-stopping sampler voices. Release only transaction handles; existing voice lifetime must remain independent.

---

## Acceptance checklist

### Research

- [ ] existing SampleHandle/refCount lifetime is reused as transaction pinning;
- [ ] unique old resident kit assets are pinned before staging;
- [ ] already-resident target assets are pinned before staging;
- [ ] newly staged target assets are pinned immediately after preload;
- [ ] safe transaction peak counts protected resident union + missing target decoded bytes;
- [ ] overlapping old/new/shared pad assets count once;
- [ ] peak rejection happens before new staging;
- [ ] dynamic active-voice pressure may fail prepare safely;
- [ ] failed prepare leaves active kit/pads and Scene revision unchanged;
- [ ] failed staged PCM may remain as unpinned LRU cache;
- [ ] no dedicated cache rollback API is required for user semantics;
- [ ] successful commit is one short coherent publication;
- [ ] Scene dirty increments exactly once on success;
- [ ] transaction pin cleanup does not release unrelated voice handles;
- [ ] old voices may finish naturally after successful commit.

### Future Cardputer ADV

- [ ] kit A is audible before switching;
- [ ] intentionally malformed kit B fails and kit A remains fully usable;
- [ ] intentionally over-budget kit fails before partial pad publication;
- [ ] target assets already resident survive multi-asset prepare;
- [ ] unrelated unpinned warehouse samples may be evicted without changing active kit semantics;
- [ ] successful switch publishes all pads together;
- [ ] hold/play an old sample across commit and confirm it finishes cleanly;
- [ ] no WDT/reset/UAF/stuck audio;
- [ ] no systematic sampler underruns during kit preparation;
- [ ] Save after successful kit commit persists only the committed kit/pad identities.
