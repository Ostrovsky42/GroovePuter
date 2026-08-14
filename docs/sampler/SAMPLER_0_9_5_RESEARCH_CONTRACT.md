# GroovePuter 0.9.5 — Sampler Productization Research Contract

## Purpose

This document freezes the research and implementation boundary for GroovePuter 0.9.5 sampler productization while 0.9.3 recovery and 0.9.4 resource hardening finish independently.

Research base:

- `agent/20260814-0.9.3-sampler-final-acceptance`
- exact base SHA `45db4857606ff37ef1d46afcec5cf1fe5cbfc68c`
- PR #272 remains the 0.9.3 recovery candidate; this branch must not modify its production sampler behavior.

0.9.5-R is contracts/tests/documentation only. Production sampler changes start only after the final 0.9.4 base is frozen.

## Release boundary

### 0.9.3 — Sampler recovery

Owns:

- stable sample identity and registry lifecycle;
- persistence recovery;
- control-side preload safety;
- minimal missing-asset safety;
- recovered standalone Sampler page;
- minimal admission correctness required to prevent unsafe allocation.

Does not own canonical kits, kit transactions, WAV loader redesign, relink UX or sampler memory policy redesign.

### 0.9.4 — Resource/release hardening

Owns:

- Tape/resource recovery;
- final post-Tape hardware memory baseline;
- release hardening.

Recovered DRAM remains general product headroom. 0.9.4 must not spend it on sampler productization.

### 0.9.5 — Sampler productization

Owns, in order:

- A: WAV loader hardening;
- B: canonical kit model;
- C: transactional kit load;
- D: missing-sample/relink UX;
- E: measured sampler memory policy.

SD streaming remains explicitly out of scope unless 0.9.5-E proves RAM playback insufficient for practical kits.

## Current evidence to preserve

The 0.9.3-G sampler already has:

- 64 fixed sample slots;
- 8 recovered user-facing pads mapped to `Q W E R T Y U I`;
- 8 simultaneous sampler voices;
- `SampleHandle`/refcount lifetime protection;
- LRU eviction of unreferenced samples;
- stable path-derived `SampleRef` persistence at the storage boundary;
- control-side WAV preload outside the short audio mutation guard;
- a 32 KiB Cardputer sampler pool policy.

The accepted Tape cleanup evidence for the later 0.9.4 memory baseline is approximately:

```text
free8    = 38360 B
largest8 = 21492 B
```

These are different constraints: `free8` is total available 8-bit heap, while `largest8` is the largest contiguous allocation currently possible.

## 0.9.5-A — WAV loader hardening

### Architecture

The production loader must become a two-stage operation:

```text
inspect/probe
    -> exact validation
    -> decoded-byte calculation
    -> admission
    -> decode
```

No PCM allocation is allowed during `inspect/probe`.

The probe result must contain at least:

```text
sampleRate
sourceChannels
bitsPerSample
blockAlign
dataOffset
sourceDataBytes
frameCount
decodedMonoBytes
```

All offset/size arithmetic must use overflow-safe wide integers before conversion to `size_t`.

### Accepted format for 0.9.5

Exactly:

- RIFF/WAVE;
- PCM format tag `1`;
- 16-bit samples;
- mono or stereo only;
- positive sample rate;
- consistent block align;
- consistent byte rate;
- exactly one usable `fmt ` chunk;
- exactly one usable `data` chunk.

Non-audio RIFF chunks such as `JUNK`, `LIST`, `fact` or metadata may appear before, between or after the required chunks and must be traversed correctly.

WAVE_FORMAT_EXTENSIBLE, IEEE float, 8/24/32-bit PCM and >2 channels are rejected in 0.9.5 rather than silently converted.

### RIFF traversal invariants

For every chunk:

```text
payloadStart = chunkHeaderEnd
payloadEnd   = payloadStart + chunkSize
nextChunk    = payloadEnd + (chunkSize & 1)
```

Required checks:

- complete 12-byte RIFF/WAVE header;
- declared RIFF boundary must not exceed physical file size;
- complete 8-byte chunk header;
- chunk payload must remain inside RIFF boundary;
- required RIFF padding byte must remain inside RIFF boundary;
- `fmt ` size must be at least 16 bytes;
- `channels` must be 1 or 2;
- `bitsPerSample == 16`;
- `blockAlign == channels * 2`;
- `byteRate == sampleRate * blockAlign` with overflow-safe arithmetic;
- `dataSize % blockAlign == 0`;
- `frameCount > 0`;
- decoded-byte calculation must not overflow.

Malformed or truncated WAV always fails closed before PCM publication.

### Allocation/decode invariant

Mono:

```text
one final mono allocation
-> direct chunked file read into final allocation
```

Stereo:

```text
one final mono allocation
+ bounded scratch buffer
-> chunk-wise L/R -> mono conversion
```

Never:

```text
full stereo allocation
+
second full mono allocation
```

Stereo mixdown must use at least 32-bit intermediate arithmetic:

```text
mono = (int32(left) + int32(right)) / 2
```

The scratch buffer must be bounded independently of WAV size and reads must preserve complete stereo frames.

### Admission

Admission uses decoded bytes, not source data bytes:

```text
decodedMonoBytes = frameCount * sizeof(int16_t)
```

A sample that cannot fit the configured store policy must be rejected before allocating or reading PCM.

## 0.9.5-B — canonical kit model

Canonical SD layout:

```text
/samples/
    loose.wav

/kits/
    909Tape/
        kit.json
        kick.wav
        snare.wav

    SP12/
        kit.json
        kick.wav
        snare.wav

    MyKit/
        kit.json
        ...
```

Historical `/bonnethead/...` paths are compatibility history, not canonical 0.9.5 product layout.

### Do not recursively index every kit WAV at boot

The current `SampleIndex` and store registry use dynamic strings/maps. A recursive scan of hundreds of kit files would make resident control-side heap usage scale with the whole SD library.

0.9.5 uses three bounded concepts instead:

```text
SampleIndex
    current loose /samples catalog

KitCatalog
    kit identity + display/directory metadata only

KitLoadPlan
    assets for one selected kit only
```

Memory must scale with the selected kit, not with the total number of WAV files on the card.

### Canonical manifest v1

`kit.json` v1 owns stable kit identity and explicit pad mapping.

Required logical fields:

```json
{
  "schema": 1,
  "id": "sp12.factory.v1",
  "name": "SP12",
  "pads": [
    {"pad": 1, "file": "kick.wav"},
    {"pad": 2, "file": "snare.wav"}
  ]
}
```

Contract:

- `schema == 1`;
- stable `id` is non-empty, bounded and independent from the directory name;
- display `name` is bounded;
- user pad number is 1..8 for the recovered 0.9.x sampler product;
- each pad appears at most once;
- file locator is relative to the kit root;
- absolute paths are rejected;
- `..` path traversal is rejected;
- backslash aliases are rejected in manifest v1;
- locator length is bounded;
- multiple pads may intentionally reference the same asset.

The physical directory may be renamed without changing the stable kit identity.

## 0.9.5-C — transactional kit load

User-visible contract:

```text
LOAD KIT
  -> resolve all required assets
  -> inspect all WAVs
  -> calculate exact decoded memory
  -> protect current kit
  -> admission
  -> prepare new assets
      failure -> rollback, old kit untouched
      success -> atomic pad commit
```

Never publish a partially loaded kit pad-by-pad.

### Transaction phases

#### 1. Resolve

Validate manifest, kit identity, pad range and all physical assets. No pad changes and no PCM allocation.

#### 2. Inspect

Run 0.9.5-A WAV probe for every unique asset. Calculate:

```text
newUniqueDecodedBytes
largestNewAllocation
uniqueAssetCount
```

#### 3. Protect current kit

Acquire store handles for every currently resident sample referenced by the active user-facing kit/pads.

Protected current-kit samples are not LRU candidates during prepare.

#### 4. Admission

The safe-load peak is not merely `newKitBytes`.

It is logically:

```text
protectedOldUniqueBytes
+
newUniqueBytesNotAlreadyResident
```

with overlapping old/new assets counted once.

Also validate available sample slots and physical contiguous-allocation constraints.

If the safe peak does not fit, fail without mutating the active kit.

#### 5. Prepare

Load new unique assets. LRU may reclaim unrelated warehouse samples but must not evict protected current-kit samples.

Track every asset newly materialized by the transaction.

#### 6. Rollback

On any failure:

- active pad states remain bit-for-bit unchanged;
- Scene revision is unchanged;
- old kit handles are released only after rollback is complete;
- newly staged transaction-only assets become removable/unpublished according to the store implementation;
- no wrong sample substitution is allowed.

#### 7. Commit

Only after complete prepare succeeds:

- enter one short audio mutation boundary;
- publish all user pad identities and canonical kit identity as one logical state transition;
- leave the mutation boundary;
- mark Scene dirty once;
- release temporary old-kit protection handles.

No SD I/O, WAV parsing, allocation, conversion or LRU work is allowed while the audio mutation boundary is held.

### Fundamental memory consequence

If old resident kit PCM is 25 KiB and the next kit needs 27 KiB, a strict safe transaction can require approximately 52 KiB even though each kit individually fits a 32 KiB policy.

0.9.5 must report this as a safe-load admission failure rather than weakening the rollback guarantee.

## 0.9.5-D — missing sample UX

`SampleRef` is a stable hash identity, not a reversible path. A missing file cannot display its former path from the hash alone.

0.9.5-D therefore needs a bounded persisted logical locator at the storage/persistence boundary, not an expanded realtime Scene ABI.

Preferred persisted concepts:

```text
SampleRef          stable identity
locator            bounded logical path, or
kitId + assetKey   bounded canonical kit locator
```

Runtime audio still uses compact `SampleId`/`SampleHandle`.

Expected UI state:

```text
PAD 3
MISSING
kits/SP12/snare.wav
```

Relink is itself transactional:

```text
select replacement
-> inspect
-> admission
-> preload
-> resolve stable identity
-> short atomic pad publication
-> persist new locator
-> mark Scene dirty
```

Failure leaves the missing reference state unchanged.

## 0.9.5-E — memory policy decision gate

No arena decision is allowed before the final post-0.9.4 Tape baseline is measured on hardware.

Test total sampler PCM policies:

```text
8 KiB
16 KiB
24 KiB
32 KiB
48 KiB negative/admission control
```

For each total size also vary the largest individual allocation. At minimum compare:

```text
32 KiB total = 8 x 4 KiB
32 KiB total = 24 KiB + 8 KiB
```

Record at every checkpoint:

```text
freeInt
largestInt
free8
largest8
resident PCM bytes
resident sample count
largest resident sample
sampler pool limit
audio underruns
audio CPU peak
heap integrity
```

Checkpoints:

```text
runtime baseline
after PCM load
Sampler page
Scene Save
Scene Load
Song page
1 active voice
4 active voices
8 active voices
Stop/Play
after eviction
```

### Fragmentation campaign

Run at least 50 kit switches across differently sized assets and track both `free8` and `largest8`.

If total free heap recovers while `largest8` degrades monotonically enough to break practical WAV loads, a fixed sampler arena becomes justified.

Otherwise retain per-sample allocations.

Arena is not the default conclusion of 0.9.5-E.

### Current hypothesis

Keep per-sample allocations unless measurements disprove them.

A fixed 32 KiB arena reserved early would consume most of the approximately 38 KiB post-Tape runtime free heap even when Sampler is unused. A late arena allocation is also constrained by the approximately 21 KiB largest contiguous block observed in accepted Tape cleanup evidence.

## Streaming boundary

SD streaming is not part of 0.9.5.

Streaming adds realtime SD latency, refill scheduling, buffering, multi-voice contention and new semantics for reverse/loop/pitch while existing evidence already shows 8-voice RAM playback is viable.

Reconsider streaming only if the completed 0.9.5-E measurement campaign proves that practical target kits cannot be served safely by RAM playback.

## Research executable gates

0.9.5-R adds executable host contracts for:

1. WAV RIFF traversal, validation, decoded-byte admission and bounded stereo conversion;
2. canonical kit manifest validation and transactional load invariants.

These are reference contracts only. They deliberately do not replace `src/sampler/sample_loader.cpp` on the research branch.

Run:

```bash
bash tests/run_sampler_0_9_5_research_tests.sh
```

## Hardware list

Research gate:

- Linux/macOS host with Python 3;
- repository checkout.

Future 0.9.5-E hardware campaign:

- M5Stack Cardputer ADV / ESP32-S3;
- microSD card containing controlled WAV/kit fixtures;
- USB-C data cable;
- normal Cardputer ADV PSRAM-disabled product profile.

## Wiring

Research gate: no hardware wiring.

Future Cardputer ADV campaign: standard Cardputer USB-C + microSD setup. No PORT.A, external I2C or external display is required for sampler memory measurements.

## Build / flash steps

### Research branch

```bash
git checkout agent/20260814-0.9.5-sampler-research
bash tests/run_sampler_0_9_5_research_tests.sh
```

No firmware flash is required for 0.9.5-R.

### Future production stages

Production 0.9.5-A must be rebased/started from the exact frozen final 0.9.4 SHA before implementation and Cardputer flashing.

## Expected behavior

Research tests print successful WAV and kit contract completion and exit zero.

No runtime UI, sampler playback, Scene schema, sample pool or Cardputer firmware behavior changes on 0.9.5-R.

## Troubleshooting

### Research test fails on a WAV case

Treat the failing fixture as a contract disagreement. Do not weaken truncation, padding, format, channel or admission checks merely to match the current V1 loader.

### Research test fails on a kit locator

Confirm the manifest path is relative, contains no `..`, is within the user pad range and does not duplicate a pad assignment.

### Transaction test reports insufficient peak capacity

This is expected when protected old PCM plus staged new PCM cannot coexist. Do not evict the old active kit to force success; the safe transaction must fail closed.

### Future 48 KiB policy case fails admission

That is an expected negative control unless the final 0.9.4 hardware baseline proves enough physical memory exists.

## Acceptance checklist

### 0.9.5-R

- [ ] research branch contains no production sampler implementation change;
- [ ] release boundary explicitly moves canonical kits/WAV productization out of 0.9.4 and into 0.9.5;
- [ ] WAV contract covers RIFF traversal and odd-chunk padding;
- [ ] malformed/truncated WAV fails closed;
- [ ] PCM16 mono/stereo only is explicit;
- [ ] decoded-byte admission is explicit;
- [ ] stereo conversion requires one final allocation plus bounded scratch;
- [ ] canonical `/samples` + `/kits/<kit>` layout is explicit;
- [ ] kit identity is stable and independent of directory name;
- [ ] recursive whole-library WAV indexing at boot is rejected;
- [ ] transactional load preserves the old kit on every failure;
- [ ] missing-sample design includes a reversible persisted locator;
- [ ] memory arena remains a measurement-driven decision;
- [ ] SD streaming remains out of scope;
- [ ] `bash tests/run_sampler_0_9_5_research_tests.sh` passes.

### Future hardware acceptance

- [ ] final 0.9.4 Tape baseline is recorded before choosing sampler policy;
- [ ] all 8/16/24/32/48 KiB policy cases are measured;
- [ ] largest-allocation shape cases are measured;
- [ ] Scene Save/Load remains functional under sampler load;
- [ ] Song/UI operation does not introduce sampler-induced resets or heap corruption;
- [ ] 1/4/8 voice workloads record CPU and underruns;
- [ ] 50+ kit-switch fragmentation campaign records `free8` and `largest8`;
- [ ] final keep-per-sample vs arena decision cites measured evidence.
