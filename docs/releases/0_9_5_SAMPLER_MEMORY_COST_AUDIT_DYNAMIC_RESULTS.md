# 0.9.5 Sampler Memory Cost Audit — Dynamic Host Results

## Exact measured SHA

```text
768532a6d30b85214f774661075f2a596da2fdb3
```

Focused workflow:

```text
Sampler memory audit / run #7
result: SUCCESS
runner: Ubuntu 24.04 x86_64 / glibc allocator
```

This probe measures live host heap after two production-base operations:

```text
SampleIndex::scanDirectory()
        |
        v
SampleIndex full catalog ownership
        |
        v
SampleIndex::bindToStore(RamSampleStore)
        |
        v
RamSampleStore::filePaths_ duplicate runtime-path registry
```

It uses synthetic loose WAV filenames with deliberately non-trivial path length and measures `mallinfo2().uordblks` after `malloc_trim(0)`.

These are **host allocator measurements, not Cardputer ADV byte counts**. STL node layout, pointer width, small-string optimization, allocator metadata, filesystem implementation, and path lengths differ on ESP32-S3. Use the numbers to identify ownership scaling and relative duplication, not to predict exact ADV free heap.

## Measured results

| Files | Live heap added by `SampleIndex` scan | Additional live heap after `RamSampleStore` bind | Combined live heap delta |
|---:|---:|---:|---:|
| 0 | 0 B | 0 B | 0 B |
| 172 | 71,792 B | 30,176 B | 101,968 B |
| 500 | 187,440 B | 87,920 B | 275,360 B |

Raw output:

```text
catalog_files=0 scan_delta=0 bind_added_delta=0 combined_delta=0 destroy_residual=0
catalog_files=172 scan_delta=71792 bind_added_delta=30176 combined_delta=101968 destroy_residual=2560
catalog_files=500 scan_delta=187440 bind_added_delta=87920 combined_delta=275360 destroy_residual=0
```

The non-monotonic `destroy_residual` value is treated as a glibc allocator/cache artifact and is **not** used as product evidence. Cardputer fragmentation recovery must be measured with `free8/largest8` directly on hardware.

## Scaling observations

At 172 files:

```text
index live cost ~= 417 B/file on this host corpus
store-bind extra ~= 175 B/file
combined ~= 593 B/file
```

At 500 files:

```text
index live cost ~= 375 B/file on this host corpus
store-bind extra ~= 176 B/file
combined ~= 551 B/file
```

The index per-file average changes because vector capacity, tree shape, allocator size classes, and one-time ownership costs are not perfectly linear.

The important result is the second ownership layer:

```text
RamSampleStore::filePaths_ extra cost
172 files: 30,176 B
500 files: 87,920 B
```

That remains approximately **176 host bytes per indexed file** across both corpus sizes. This is direct evidence that the production-base registry duplicates a library-size-dependent path ownership tax after the full catalog has already been built.

## Relation to #283

PR #283 head `13e754892cd1848f75dc09c64e534a0d70df4464` already removes this duplication by allowing `RamSampleStore` to borrow the session-stable `SampleIndex` for runtime path resolution instead of copying every path into `filePaths_`.

Therefore S1 now has two independent forms of evidence for that cleanup direction:

1. source/host ownership measurement on the production-base control state;
2. real Cardputer ADV hardware stability on the #283 continuation after duplicate ownership was removed.

This does **not** make permanent full-library `SampleIndex` ownership acceptable. #283 reduces the second registry but still retains the full library catalog for the session.

## Updated S1 classification

### A — fixed idle tax

Measured host layout:

```text
SampleSlot[64]       2560 B
SamplerPool[8]        448 B
DrumSamplerTrack      840 B
SamplerPadState[16]   384 B
```

These remain secondary until Cardputer exact cost is known.

### B — catalog tax

Confirmed to scale with file count:

```text
SampleIndex::files_
SampleFileInfo filename/fullPath ownership
SampleIndex::nameToId_
```

This remains the leading S2 target.

### C — duplicate registry / fragmentation candidate

Confirmed independently:

```text
RamSampleStore::filePaths_
```

The approximately constant host per-file bind delta is strong evidence that this is true additional long-lived ownership, not just vector capacity noise.

### D — UI tax

Still requires Cardputer runtime open/leave measurement.

### E — PCM tax

Already architecturally classified: whole-file contiguous PCM allocation is not viable for ordinary ADV one-shots.

## Current ranking

1. **Full persistent catalog -> bounded catalog window** — highest expected topology recovery and removes library-size dependency.
2. **Do not reintroduce duplicated `RamSampleStore` path ownership** — #283 cleanup should be preserved in the bounded-source model.
3. **`SampleSlot[64]` descriptor count** — measurable fixed cost, but no change before ESP32-S3 size evidence and resident-cache design.
4. **Lazy `DrumSamplerTrack`** — small secondary candidate only if Cardputer data justifies ownership complexity.
5. **Do not reduce 8 logical voices or migrate 16 persisted pad states** — host evidence does not justify the product regression/risk.

## What this still does not prove

- exact ESP32-S3 bytes per catalog entry;
- exact allocator/header cost on Cardputer ADV;
- how much `largest8` recovers when full catalog ownership disappears;
- UI page retention/release cost;
- preload/eviction fragmentation behavior;
- repeated browse/select topology trend;
- any streaming concurrency or page-cache feasibility.

S1 therefore remains research-only until the Cardputer hardware timeline is captured on one exact S1 SHA.
