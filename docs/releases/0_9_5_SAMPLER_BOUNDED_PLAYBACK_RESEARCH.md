# 0.9.5 Sampler Bounded Playback Research

Status: **S0 baseline / evidence freeze**  
Target: **M5Stack Cardputer ADV / ESP32-S3 / no PSRAM production profile**  
Research branch: `research/0.9.5-sampler-bounded-playback`  
Production base at S0 start: `dev_0.9.5` @ `d3db4e48ebc08862bdaf9f62532414f009839192`  
Experimental storage/WAV continuation inspected for evidence: PR #283 @ `13e754892cd1848f75dc09c64e534a0d70df4464`

This document freezes the measured reason for changing the Cardputer ADV sampler playback architecture. It intentionally changes **no production behavior**.

> Streaming was previously deferred until hardware proved resident PCM insufficient. Hardware has now satisfied that condition.

## 1. Decision

The Cardputer ADV sampler must not be developed further as a resident-only full-PCM system for ordinary musical one-shots.

The production direction is:

```text
PAD ASSIGNMENT
     |
SampleRef + locator
     |
WAV source metadata
   /       \
  /         \
tiny sample  normal sample
   |             |
resident PCM   streamed/paged
   \             /
    \           /
     SamplerVoice
```

The reason is hardware evidence, not a theoretical preference. LRU tuning, a larger logical pool limit, or catalog truncation cannot remove the requirement for one sufficiently large contiguous decoded-PCM allocation in the current resident path.

The target architecture therefore becomes:

1. bounded catalog ownership;
2. bounded persistent pad/source binding;
3. measured SD streaming feasibility;
4. fixed/preallocated page cache;
5. hybrid resident + streamed playback;
6. hardware acceptance with realistic eight-pad material.

## 2. Hardware facts frozen as baseline

Real Cardputer ADV measurement, 172 WAV files:

| Checkpoint | Free internal heap | Largest internal block |
|---|---:|---:|
| before sample indexing | ~50.2 KiB | ~21.5 KiB |
| after indexing 172 WAV | ~33.5 KiB | ~8.7 KiB |
| after SMF startup | ~24.7 KiB | ~8.7 KiB |
| after UI | ~14.3 KiB | ~8.2 KiB |

Additional frozen facts:

- Cardputer DRAM globals: **176808 bytes**;
- fixed DRAM budget: **191488 bytes**;
- SMF works;
- all 172 WAV files index successfully;
- no systematic audio underrun was observed in this baseline;
- device remains stable;
- ordinary one-shot preload regularly fails when decoded PCM cannot fit one contiguous internal-heap block.

The primary ADV memory health indicator for the sampler track is therefore **largest internal free block plus long-lived ownership topology**, not free-heap total alone.

## 3. Why full-PCM resident playback failed

Current playback admission is improved but still resident-only:

```text
resolve file
  -> inspect WAV without PCM allocation
  -> compute exact decoded mono bytes
  -> enforce logical pool admission / evict LRU
  -> decode whole sample
  -> allocate one contiguous int16_t PCM buffer
  -> publish the complete buffer in a SampleSlot
  -> SamplerVoice keeps a pointer to the resident PCM
```

This is correct as a bounded *resident fast path*, but insufficient as the sole production playback model on ADV.

At a largest allocatable block of roughly 8.2 KiB, mono PCM16 at 22050 Hz allows only about:

```text
8192 / 2 / 22050 ~= 0.186 s
```

before allocator overhead and other constraints.

Even an ideal contiguous 32 KiB logical sampler budget would provide only:

```text
32768 / 2 / 22050 ~= 0.743 s total mono PCM16
```

Across eight different resident pad samples, that averages about 93 ms per pad. This is below normal kick/snare/open-hat/cymbal one-shot lengths.

Therefore:

**FULL PCM RESIDENT MODEL FOR AN ORDINARY 8-PAD SAMPLER ON CARDPUTER ADV IS ARCHITECTURALLY INSUFFICIENT.**

This conclusion must not be reopened by small LRU/cache tuning unless contradictory hardware evidence appears.

## 4. Current sampler capabilities that must survive

The existing subsystem already contains real sampler behavior. Future bounded-playback work must preserve it unless a later hardware-gated stage explicitly narrows streamed capabilities.

Current functional surface:

- 16 internal pads;
- 8 logical playback voices;
- velocity/gain;
- pitch;
- reverse;
- loop;
- start/end frame bounds;
- choke groups;
- SD WAV loading;
- Scene persistence;
- stable `SampleRef` identity;
- drum sequencer triggering;
- hardened WAV inspect/admission/decode path;
- nested folder browsing on the experimental #283 continuation;
- duplicate-basename-safe identity on the experimental #283 continuation.

Do not remove sampler state from Scene or migrate persistence merely to save hundreds of bytes.

## 5. Current ownership topology

### 5.1 Resident playback store

`RamSampleStore` owns a fixed descriptor table of 64 `SampleSlot` objects. Each occupied slot points at separately allocated decoded PCM. The logical byte limit is tracked through `currentPoolUsage_` / `maxPoolBytes_`; it is not itself proof of a single preallocated PCM arena.

The audio-facing handle API is already separated from control-side preload/registry operations. That separation should be retained.

Relevant current roles:

```text
RamSampleStore
  |
  +-- SampleSlot[64]
  |     id / ready / data* / frames / sampleRate
  |     sizeBytes / refCount / lastAccess
  |
  +-- preload()          control side
  +-- evictLRU()         control side
  +-- acquire/view       audio-facing lock-free reads
```

### 5.2 Voice layer

`SamplerVoice` is already an audio-thread playback state object. It caches playback metadata and a pinned PCM pointer acquired from the store. It implements start/end, pitch step, interpolation choice, reverse, loop and short fades.

This is an important architectural seam: future streamed playback should adapt the sample source behind a voice rather than force a sequencer rewrite.

### 5.3 Logical polyphony

`SamplerPool` owns exactly eight logical `SamplerVoice` objects.

Streaming concurrency must **not** automatically redefine sampler logical polyphony. A later measured architecture may support, for example, N streamed voices plus resident voices while retaining eight logical sampler voices overall.

### 5.4 Pad semantics

`DrumSamplerTrack` owns 16 `SamplerPad` definitions plus the eight-voice pool. Pad state currently includes:

- sample ID/runtime binding;
- volume;
- pitch;
- start/end;
- choke group;
- reverse;
- loop.

These semantics are part of the preserved sampler contract.

### 5.5 Experimental #283 catalog ownership improvement

The hardware-tested continuation `13e75489` already removes important duplication and must not be casually reverted:

- removes the redundant second sample scan from startup;
- removes duplicated basename storage from `SampleFileInfo`;
- makes `SampleIndex` the session-stable path owner;
- allows `RamSampleStore` to borrow `SampleIndex` for runtime path resolution instead of copying every path into another registry;
- retains WAV file size for browser display.

This reduces duplicated ownership, but it does **not** solve the larger permanent full-catalog cost because `SampleIndex` still owns a recursive `std::vector<SampleFileInfo>` containing full paths for the whole library.

## 6. Two problems that must be separated

The existing index currently couples two responsibilities:

```text
SampleIndex
  = library catalog
  + runtime/stable source resolution
```

The bounded architecture must separate them.

### A. Browser catalog

Target contract:

```text
SampleBrowserWindow
  fixed small capacity, initially <= 8 entries

BrowserEntry
  type
  short display name
  file size
  optional lightweight locator metadata
```

Properties:

- no permanent vector of every recursive full path;
- no arbitrary first-N file limit;
- all files remain reachable through directory/page navigation;
- SD scanning belongs to control-side navigation/refill, not render/audio code;
- duplicate filenames in different directories remain distinct;
- hidden/non-WAV filtering remains preserved.

### B. Assigned sample identity

A pad should retain only what is required after browser closure:

```text
SampleBinding
  stable SampleRef
  canonical/logical locator
  runtime playback metadata
  resolved/missing state
```

The number of such bindings is bounded by sampler pad count, not library size.

Scene restore must not require a recursive permanent index of the entire SD card. Future resolution may use targeted path lookup, a bounded scan over at most referenced assets, or a later canonical kit manifest.

Contract:

```text
library catalog lifetime != pad assignment lifetime
```

## 7. Realtime boundary

The final sampler must preserve a strict ownership boundary.

### Audio thread may do

- consume ready resident PCM;
- consume already-published streaming pages;
- advance voice/playhead state;
- mix samples;
- perform bounded per-frame math;
- stop/drop a sampler voice on failure.

### Audio thread must not do

- `SD.open`;
- filesystem read/seek;
- allocation/free;
- `new`/`delete`;
- path-string construction;
- unbounded `std::vector`/`std::string` growth;
- JSON;
- browser/catalog work;
- blocking mutex waits.

### Control/I/O side owns

- filesystem traversal;
- source resolution;
- WAV metadata inspection;
- stream open/seek/read;
- decode/refill;
- cache/page publication;
- error recovery and counters.

## 8. Hybrid target model

The resident path remains useful for genuinely small samples:

```text
if decodedBytes <= measured safe resident threshold
and resident cache admits it:
    resident fast path
else:
    streamed/paged path
```

Resident samples retain the existing feature set, including reverse and loop.

Streaming V1 is deliberately narrower until hardware proves more:

Required first:

- forward one-shot;
- velocity/gain;
- choke;
- bounded start/end support where implementation does not compromise refill safety.

Later, separately hardware-gated:

1. pitch range;
2. loop refill/jump;
3. reverse traversal.

Do not implement reverse-streaming or loop-streaming before basic forward streaming is proven.

## 9. Streaming research gate

Production streaming implementation is blocked on a hardware feasibility benchmark.

Benchmark matrix:

- simultaneous streams: 1 / 2 / 4 / 8;
- page/read sizes: 512 / 1024 / 2048 / 4096 bytes;
- PCM16 mono first;
- stereo source -> mono decode only after mono baseline.

Run concurrently with normal GroovePuter load:

- synth engine;
- drums;
- SMF playback;
- audio output.

Measure:

- worst SD read latency;
- average SD read latency;
- refill deadline misses;
- audio callback maximum duration;
- CPU/audio load;
- underruns;
- free internal heap;
- largest internal block;
- WDT/reset;
- cache starvation.

Do **not** assume that eight concurrent SD streams are required.

Practical viability gate:

> If four concurrent streamed forward one-shots are stable together with normal GroovePuter + SMF load, bounded streaming is considered practically viable. Higher concurrency may be adopted only if hardware proves it.

## 10. Fixed page-cache direction after benchmark

Expected bounded shape, subject to S4 hardware evidence:

```text
SampleStreamManager
  fixed StreamVoiceState[N]
  fixed PCM page buffers
  fixed queues/state

I/O owner
  SD.open/read/seek
  decode/refill
  publish READY page

Audio thread
  read READY PCM only
  never touch filesystem
```

Prefer early preallocation of the chosen page/cache working set before later UI/SMF fragmentation **only if measurements show that this improves the real ADV memory topology**.

No per-refill allocation is allowed in the production design.

## 11. Playback failure policy direction

Sampler failure must be local and predictable.

Preferred principle:

```text
one sample voice may drop/stop
rather than
whole audio engine underruns / blocks / WDT resets
```

Required future diagnostics:

- `SAMPLER_STREAM_STARVE`;
- `SAMPLER_STREAM_DROP`;
- `SAMPLER_SD_MAX_US`;
- `SAMPLER_CACHE_HIT` / `SAMPLER_CACHE_MISS`;
- `SAMPLER_ACTIVE_STREAMS`.

No trigger retry is required in the realtime path.

## 12. Memory topology measurements required in later stages

Track a trend, not one post-boot number:

1. boot;
2. after audio buffers;
3. after SD;
4. after sample browser/catalog setup;
5. after SMF;
6. after UI;
7. after opening sampler;
8. after assigning eight pads;
9. during PLAY;
10. after stopping;
11. after repeated browse/select cycles.

Record at minimum:

- free internal heap;
- largest internal block;
- underruns;
- WDT/reset state.

## 13. Work stages and gates

### S0 — baseline/evidence freeze

This document. No production behavior change.

### S1 — memory cost audit

Determine exact static/BSS, dynamic idle, catalog, PCM and UI-page costs for:

- `RamSampleStore`;
- `SampleSlot`;
- `SamplerVoice`;
- `SamplerPool`;
- `DrumSamplerTrack`;
- `SamplerPad`;
- `SampleIndex`;
- `SampleFileInfo`;
- `SamplerPadState`;
- `SamplerPage` where practical.

Verify descriptor and fragmentation costs before deleting anything.

### S2 — bounded catalog prototype

Replace permanent recursive full-library ownership with a small reusable browser window. Host proof must include 500+ synthetic WAVs and duplicate basenames in distinct directories.

### S3 — pad binding without full index

Scene/pad assignment becomes independent of permanent full-library catalog lifetime while preserving stable `SampleRef`, nested paths, duplicate basenames and safe missing-source behavior.

### S4 — streaming feasibility benchmark

Hardware research gate described above. No claim of production readiness.

### S5 — fixed page cache

Implement the measured, fixed-size stream working set with all filesystem work outside the audio thread.

### S6 — hybrid resident + streamed

Route tiny admitted samples through resident playback and ordinary one-shots through streaming while preserving eight logical sampler voices.

### S7 — failure policy and counters

Make starvation/latency failures local, bounded and observable.

### S8 — feature recovery

Reintroduce streamed pitch/start-end/loop/reverse one capability at a time, each with hardware proof.

### S9 — realistic eight-pad product acceptance

Use a real kit-like assignment:

- kick ~300-600 ms;
- snare ~400-900 ms;
- closed hat ~100-300 ms;
- open hat ~500-1500 ms;
- clap;
- percussion;
- tom;
- cymbal potentially several seconds.

Run sequencer, manual Q-I audition, choke, overlaps, SMF, Synth A/B, MIDI output, Scene Save/reboot/Load and a 30-minute soak.

## 14. Explicit non-goals for this track phase

Do not use any of these as a substitute for bounded playback:

- first-N browser limits;
- simply enlarging sampler pool;
- resident storage of eight ordinary one-shots;
- PSRAM in the production ADV profile;
- SD I/O in audio callback;
- allocation/free in audio callback;
- giant framework rewrite;
- sequencer rewrite;
- persistence migration without strong evidence;
- unrelated generation/music changes;
- kits transaction implementation before source/binding model exists;
- recording;
- slicing;
- waveform UI;
- premature streamed reverse/loop implementation.

## 15. Relation to future kits

Future kit loading must be built on source bindings, not full-PCM preload:

```text
LOAD KIT
  |
resolve required assets
  |
inspect metadata
  |
prepare bounded bindings
  |
validate source/cache feasibility
  |
atomic commit bindings
```

Do not define kit transaction success as "all decoded PCM fits in RAM".

## 16. CI invariants for future production steps

Every production sampler PR must preserve the existing broad gates, including:

- Core host suite;
- SDL;
- Cardputer ADV;
- fixed DRAM;
- SEQTRAK MIDI-only;
- sampler SampleRef;
- sampler persistence;
- sampler registry;
- WAV loader corpus;
- Phrase;
- Four-axis;
- Synth persistence.

Focused sampler tests supplement Core; they do not replace it.

## 17. Branch / PR discipline

PR #283 remains **draft / unmerged** pending a separate decision. Its hardened loader and catalog-ownership improvements are evidence and useful work, but its merge is not a prerequisite for this research track or unrelated 0.9.5 work.

This bounded-playback research branch must remain docs/benchmark-focused. Production work should be split into small reviewable PRs rather than one streaming mega-PR.

Suggested production sequence after research proof:

1. bounded catalog;
2. bounded pad binding/resolver;
3. streaming benchmark/prototype;
4. fixed page cache;
5. hybrid integration and product acceptance.

Each stage report must include:

1. branch;
2. PR;
3. exact base SHA;
4. exact head SHA;
5. changed files;
6. what is proven;
7. what is not proven;
8. host/CI status;
9. Cardputer build status;
10. DRAM globals;
11. hardware free heap / largest block / underruns / WDT-reset;
12. next gate.

No stage is production-ready until its hardware condition is actually checked.

## 18. S0 conclusion

The sampler is not missing a feature set; it is constrained by the ownership/playback model used for normal WAVs on no-PSRAM Cardputer ADV.

The correct next move is **not** to keep optimizing full-file preload. The path forward is:

```text
bounded ownership
  -> bounded catalog
  -> bounded source binding
  -> measured streaming
  -> fixed page cache
  -> hybrid resident/streamed sampler
  -> hardware acceptance
```

The hardware stop condition for resident-only playback has already been met.
