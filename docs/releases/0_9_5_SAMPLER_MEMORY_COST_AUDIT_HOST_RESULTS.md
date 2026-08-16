# 0.9.5 Sampler Memory Cost Audit — Host Results

## Exact measured SHA

```text
ad5ac1859f80cded81967f0e911aaf466a01033e
```

Workflow:

```text
Sampler memory audit / run #3
result: SUCCESS
runner: Ubuntu 24.04 x86_64
host pointer: 8 bytes
host size_t: 8 bytes
```

These values are host ABI measurements. They are not Cardputer ADV byte-exact sizes and must not be substituted for ESP32-S3 measurements.

## Host layout table

| Object | Host `sizeof` | Category | S1 interpretation |
|---|---:|---|---|
| `SampleSlot` | 40 B | fixed idle | descriptor cost is measurable; do not shrink yet |
| `SampleSlot[64]` | 2560 B | fixed idle | dominates host `RamSampleStore` object layout |
| `RamSampleStore` | 2680 B | fixed + dynamic owners | only 120 B beyond the 64-slot array on this ABI; dynamic map nodes are additional |
| `SamplerVoice` | 56 B | fixed idle | one logical playback voice |
| `SamplerPool` | 448 B | fixed idle | exactly 8 host voices; small relative to catalog evidence |
| `SamplerPad` | 24 B | fixed idle | per-pad runtime controls |
| `DrumSamplerTrack` | 840 B | fixed idle | 16 pads + 8-voice pool + small overhead |
| `SampleIndex` | 72 B | dynamic catalog owner | small object shell; real cost is vector/map/string allocations |
| `SampleFileInfo` | 72 B | catalog | inline element cost before string payload allocations |
| `SamplerPadState` | 24 B | persistence | persisted pad state |
| `SamplerPadState[16]` | 384 B | persistence | too small to justify persistence migration on host evidence |
| `SamplerPage` | 312 B | UI shell | does not include heap allocations owned by shared components |

## Immediate calculations

### 64 sample descriptors

On the host ABI:

```text
SampleSlot[64] / RamSampleStore
= 2560 / 2680
~= 95.5%
```

So the fixed store object is mostly the 64 descriptor table. That makes descriptor-count reduction a legitimate later candidate, but not an S1 change. Cardputer byte cost is still required before deciding whether it is worth touching.

### Eight logical voices

On the host ABI:

```text
SamplerPool = 448 B
```

Reducing logical polyphony would therefore target hundreds of bytes on host, while hardware catalog indexing has already demonstrated a loss measured in tens of KiB plus severe largest-block collapse. There is no evidence supporting a voice-count reduction.

### Persisted sampler pad state

On the host ABI:

```text
SamplerPadState[16] = 384 B
```

This reinforces the existing protection rule: persistence migration would carry disproportionate compatibility risk for a small fixed saving.

### Full catalog inline payload

The production-base `SampleIndex` owns one `SampleFileInfo` for every indexed WAV. On the host ABI:

```text
172 * 72 B = 12384 B
```

That is about 12.1 KiB of vector element payload before counting:

- each `filename` string allocation when it exceeds small-string storage;
- each `fullPath` string allocation;
- `nameToId_` tree/map nodes and their owned strings;
- vector capacity slack;
- allocator headers/alignment;
- the production-base `RamSampleStore::filePaths_` map and duplicated paths.

This host result is directionally consistent with the real ADV transition:

```text
free internal heap:  ~50.2 KiB -> ~33.5 KiB
largest block:       ~21.5 KiB ->  ~8.7 KiB
```

The host number does not prove the ESP32 byte breakdown, but it strongly supports catalog ownership as the first production target after S1.

## Current rank — evidence, not implementation order mutation

### Rank 1 — full persistent catalog ownership

Reason:

- scales with library size;
- contains multiple long-lived dynamic owners;
- real hardware already shows the largest topology collapse at indexing time;
- host inline payload alone is substantial at 172 entries;
- #283 independently demonstrated that duplicate path ownership can be removed without changing sampler semantics.

Expected later production direction:

```text
full-library resident catalog
-> bounded directory browser window
```

No S2 code belongs in S1.

### Rank 2 — duplicate runtime path ownership

Production base owns full paths in both catalog-side structures and `RamSampleStore::filePaths_`. #283 already provides evidence that the store can borrow the session-stable index instead of copying every path.

This should remain part of the ownership cleanup lineage and must not be accidentally reintroduced by S2.

### Rank 3 — `SampleSlot[64]`

The host table shows a real fixed cost, but Cardputer exact size and the value of reducing descriptors versus future resident-cache needs are not yet known. Measure first.

### Rank 4 — `DrumSamplerTrack` lazy allocation

`840 B` on x86_64 is not zero, but it is far below demonstrated catalog loss. Cardputer exact size and lifetime must be measured before introducing ownership complexity.

### Protected — logical voices and Scene sampler state

Host evidence gives no justification for reducing eight logical voices or migrating `SamplerPadState[16]`.

## Still not measured

S1 is not complete. Required Cardputer ADV evidence remains:

- exact ESP32-S3 object/layout values where practical;
- dynamic idle cost of `RamSampleStore`;
- catalog delta before/after scan and registry binding;
- comparison against #283 `13e75489...` duplicate-ownership cleanup;
- `SamplerPage` open/leave runtime heap delta;
- preload/eviction `free8` and `largest8` recovery;
- repeated browse/select fragmentation trend;
- underruns and reset/WDT state during the measurement run.

Do not mark S1 production-ready until those hardware rows exist on one exact firmware SHA.
