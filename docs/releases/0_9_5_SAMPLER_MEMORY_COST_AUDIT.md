# 0.9.5 Sampler Memory Cost Audit — S1

## Status

Research-only S1 checkpoint. This stage measures and classifies sampler memory ownership. It does not optimize, shrink, migrate, or replace sampler architecture.

Lineage:

```text
S0 base/head: 93eaaa44abf99248566e1ff78f9cef64b0581e4f
S1 branch:    research/0.9.5-sampler-memory-audit
```

PR #294 remains the immutable S0 evidence checkpoint and must stay draft/unmerged while this audit proceeds.

## S1 rule

The order is mandatory:

```text
measure -> classify -> rank -> choose a separate production change
```

S1 must not reduce:

- 8 logical sampler voices;
- 16 internal pads;
- 16 persisted `SamplerPadState` entries;
- stable `SampleRef` persistence;
- sequencer trigger semantics;
- reverse/loop/pitch resident semantics;
- `kMaxSampleSlots = 64` before its exact cost is measured.

No bounded catalog implementation, resolver migration, streaming, page cache, kit model, slicing, recording, or waveform UI belongs in this stage.

## Hardware evidence carried from S0

Target: M5Stack Cardputer ADV, ESP32-S3, no PSRAM production profile, built-in microSD, 22.05 kHz audio, 512-frame audio block.

Real 172-WAV measurements from the hardware-tested sampler continuation:

| Checkpoint | Free internal heap | Largest internal block |
|---|---:|---:|
| before sample indexing | ~50.2 KiB | ~21.5 KiB |
| after indexing 172 WAV | ~33.5 KiB | ~8.7 KiB |
| after SMF startup | ~24.7 KiB | ~8.7 KiB |
| after UI | ~14.3 KiB | ~8.2 KiB |

Cardputer DRAM globals:

```text
176808 bytes
budget 191488 bytes
```

SMF runs, all 172 WAV files index, no systematic underrun was observed, and the device remains stable. Ordinary one-shot preload still regularly fails because the resident path requires one contiguous decoded PCM allocation.

At ~8.2 KiB largest block, mono PCM16 at 22050 Hz admits only about 185–190 ms in one contiguous allocation before allocator overhead.

Even an ideal contiguous 32 KiB logical sampler pool holds only:

```text
32768 / 2 / 22050 ~= 0.743 s total
```

or about 93 ms per pad across eight different resident pads.

Resident-only full-file PCM is therefore already rejected as the production architecture for ordinary ADV sampler one-shots. S1 is not allowed to revisit that conclusion through LRU tuning.

## Baselines being compared

S1 intentionally starts from S0 head `93eaaa44...`, which is the production-base control state.

The hardware-tested #283 continuation `13e75489...` is a separate comparison state. It already removes a redundant second sample scan and duplicate path-registry ownership, derives basename from `fullPath`, and allows `RamSampleStore` to borrow `SampleIndex` for runtime resolution.

Do not mix these states in one number:

- `93eaaa44...` = production-base ownership baseline;
- `13e75489...` = hardware-tested ownership-cleanup continuation;
- future S2 = bounded catalog production experiment.

## Automated host ABI probe

Run:

```bash
bash tests/run_sampler_memory_cost_audit.sh
```

The probe reports the host ABI sizes of:

- `RamSampleStore`;
- `SampleSlot`;
- `SampleSlot[64]`;
- `SamplerVoice`;
- `SamplerPool`;
- `SamplerPad`;
- `DrumSamplerTrack`;
- `SampleIndex`;
- `SampleFileInfo`;
- `SamplerPadState`;
- `SamplerPadState[16]`;
- `SamplerPage`.

Host sizes are classification evidence, not Cardputer byte-exact truth. Pointer width, `size_t`, STL node layout, mutex implementation, and allocator overhead differ between the GitHub x86_64 runner and ESP32-S3.

The focused workflow is:

```text
.github/workflows/sampler-memory-audit.yml
```

It adds coverage; it does not replace Core, SDL, Cardputer ADV, fixed DRAM, SEQTRAK MIDI-only, sampler persistence/registry/SampleRef, Phrase, Four-axis, or Synth persistence gates.

## Ownership classification on the production-base control state

The source classifier records the current categories without changing them.

| Object / ownership | Category | Current baseline | Interpretation |
|---|---|---|---|
| `SampleSlot[64]` | A — fixed idle tax | fixed array inside `RamSampleStore` | exact `sizeof` first; no reduction in S1 |
| `SamplerVoice[8]` | A — fixed idle tax | fixed array inside `SamplerPool` | preserve eight logical voices |
| `SamplerPad[16]` | A — fixed idle tax | fixed array inside `DrumSamplerTrack` | preserve 16 internal pads |
| `SampleIndex::files_` | B — catalog tax | full-library `vector<SampleFileInfo>` | scales with library size |
| `SampleFileInfo::filename` | B/C | heap-owned string per file | duplicated identity/storage candidate |
| `SampleFileInfo::fullPath` | B/C | heap-owned string per file | required source identity in current model |
| `SampleIndex::nameToId_` | B/C | heap node ownership | full-library lookup tax |
| `RamSampleStore::filePaths_` | B/C | heap map of runtime id -> path | duplicate path ownership on production base |
| `SamplerPadState[16]` | persistence state | fixed inside `Scene` | do not migrate for small savings |
| `SamplerPage` components | D — UI tax | lazy shared component ownership | measure runtime open/close delta |
| decoded PCM | E — PCM tax | one whole-file allocation | architecture change required, not tuning |

## Why catalog ownership is the leading S2 candidate

The 172-file hardware transition is already large:

```text
free:    ~50.2 KiB -> ~33.5 KiB
largest: ~21.5 KiB ->  ~8.7 KiB
```

This is approximately 16.7 KiB of free internal heap loss plus a much larger collapse in largest-block topology.

That makes permanent full-library ownership the highest-priority production candidate after S1, but S1 itself does not replace it.

The expected S2 direction remains:

```text
full persistent recursive catalog
        ->
bounded directory window (small fixed entry count)
```

with all files still reachable and no first-N truncation.

## Hardware measurement protocol

The primary metric is not only total free heap. Record both:

```text
free8
largest8
```

and, where available, the stricter internal-capability pair:

```text
freeInt
largInt
```

The existing Cardputer boot helper `logHeapCaps()` already reports:

```text
freeInt / largInt / free8 / larg8
```

Capture the following timeline on one exact firmware SHA and one unchanged SD card:

| Order | Checkpoint | Required observation |
|---:|---|---|
| 1 | setup entry / post-global construction | initial long-lived topology |
| 2 | after direct I2S/audio buffers | effect of realtime audio reservation |
| 3 | after AudioTask creation | task-stack effect |
| 4 | after constrained DSP buffer preallocation | early contiguous DSP reservations |
| 5 | after SD initialization | filesystem/driver cost |
| 6 | after SMF runtime initialization | SMF stack/buffer cost |
| 7 | immediately after sampler store binding | store bind delta |
| 8 | immediately before sample catalog scan | catalog baseline |
| 9 | immediately after sample catalog + registry binding | catalog + path ownership cost |
| 10 | after `MiniAcidDisplay` allocation | UI owner cost |
| 11 | after first UI draw | first-render/lazy allocation cost |
| 12 | first entry into SAMPLES | lazy `SamplerPage` cost |
| 13 | after leaving SAMPLES | whether page memory is retained or released |
| 14 | after one successful small preload | PCM + descriptor cost |
| 15 | after eviction | whether PCM returns and topology recovers |
| 16 | after repeated browse/select cycles | fragmentation trend |

For every line record:

```text
SHA=<exact firmware sha>
checkpoint=<tag>
freeInt=<bytes>
largInt=<bytes>
free8=<bytes>
larg8=<bytes>
underruns=<count>
reset_reason=<value if rebooted>
```

Do not compare measurements taken from different SHAs as if they formed one boot timeline.

## Dynamic idle questions S1 must answer

`sizeof` does not answer STL/allocator cost. Hardware/runtime deltas must determine:

1. Does default construction of `RamSampleStore` allocate any heap beyond its fixed object layout?
2. What is the production-base cost of the `filePaths_` map before and after catalog binding?
3. What is the production-base cost of `SampleIndex::files_`, `filename`, `fullPath`, and `nameToId_` for 172 WAV?
4. How much of the 172-file loss disappears on #283 `13e75489...` after duplicate ownership cleanup?
5. Is `SamplerPage` actually destroyed when leaving the page, or retained by lazy-page ownership?
6. Does a preload/evict cycle return total free heap but leave `largest8` degraded?
7. Which repeated control-side operation causes monotonic largest-block decay, if any?

## Ranking rules after measurements

Candidates will be ranked by:

1. largest-block recovery;
2. long-lived dynamic ownership removed;
3. free internal heap recovery;
4. risk to persistence/realtime behavior;
5. implementation scope.

Expected categories:

### High priority

- bounded sample catalog ownership;
- removal of duplicate full-path ownership already demonstrated by #283;
- later fixed/preallocated streaming page storage after the streaming benchmark.

### Evidence-dependent

- reducing `SampleSlot` descriptor count only after exact Cardputer cost is known;
- lazy `DrumSamplerTrack` only if its measured fixed/dynamic tax is meaningful;
- reuse of path scratch buffers if hardware shows allocator churn.

### Protected from S1 optimization

- eight logical voices;
- 16 internal pads;
- `SamplerPadState[16]` persistence;
- `SampleRef`;
- sequencer integration;
- existing resident reverse/loop/pitch semantics.

## S1 acceptance checklist

- [ ] Host memory-layout probe compiles with `-Wall -Wextra -Werror`.
- [ ] Source ownership classifier passes.
- [ ] Exact host ABI values are captured from CI.
- [ ] Cardputer ADV build remains green.
- [ ] Fixed DRAM remains within the existing 191488-byte budget.
- [ ] Hardware timeline is captured on one exact S1 firmware SHA.
- [ ] `free8` and `largest8` are recorded for every required checkpoint.
- [ ] Catalog cost is separated from PCM cost.
- [ ] UI page cost is separated from permanent catalog ownership.
- [ ] Preload and eviction topology are compared.
- [ ] No voice/pad/persistence/identity semantics are reduced.
- [ ] Candidates are ranked only after measurements.

S1 is complete only when the table has measured host evidence and the required Cardputer hardware checkpoints. Until then this PR remains research-only and must not be described as production-ready.
