# GroovePuter 0.9.3-A — Sampler Recovery Evidence / Measurement

Status: **DRAFT / HARDWARE MEASUREMENT PENDING**

Base: `dev_0.9.3 @ dd4528050fd3f53ce166490e83ddd6ed763e76fe`

Branch: `agent/20260814-sampler-evidence`

Scope: evidence and measurement only. This stage does **not** recover the Sampler UI and does not change sampler identity, boot order, persistence ownership, preload lifecycle, WAV architecture, kit architecture, or realtime DSP behavior.

## Purpose

Establish reproducible truth about the sampler already present in GroovePuter before any 0.9.3 recovery fix is accepted.

The evidence stage answers four questions:

1. What sampler code is actually constructed, called, rendered, persisted, and reachable on the published v0.9.2 base?
2. Which suspected integration failures can be proved directly from current source/runtime order?
3. What is the real Cardputer ADV internal-RAM and sampler-pool cost?
4. What is the isolated render cost of 0, 1, 4, and 8 sampler voices against the current 22050 Hz / 512-frame audio budget?

This document deliberately leaves Cardputer ADV numeric results as `PENDING` until captured from the exact evidence branch on hardware.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3.
- microSD card readable by Cardputer ADV.
- USB-C cable for flash and 115200-baud Serial capture.
- No external MIDI device is required for the isolated evidence sketch.
- No external GPIO/I2C/SPI wiring is required beyond the Cardputer ADV built-in SD interface.

## Wiring

None.

The evidence sketch uses the repository's existing Cardputer ADV SD profile:

- SD SCLK: GPIO40
- SD MISO: GPIO39
- SD MOSI: GPIO14
- SD CS: GPIO12
- SD clock: 25 MHz

The sketch writes/replaces `/sampler_evidence_ref.wav` on the SD card. Use a card on which replacing that single file is acceptable.

## CURRENT ARCHITECTURE

### Runtime objects

The current sampler is not a stub. The published base constructs and retains the following runtime path:

```text
GroovePuter.ino
    |
    +-- RamSampleStore g_sampleStore
    |
    +-- MiniAcid
          |
          +-- ISampleStore* sampleStore
          +-- SampleIndex
          +-- DrumSamplerTrack
          |     |
          |     +-- SamplerPad[16]
          |     +-- SamplerPool
          |            |
          |            +-- SamplerVoice[8]
          |
          +-- samplerOutBuffer[512] float
```

Current fixed semantic limits visible in source:

- 16 runtime `SamplerPad` slots;
- 8 simultaneous `SamplerVoice` instances;
- 64 `RamSampleStore` metadata slots;
- 32 KiB sampler-pool policy in Cardputer DRAM-only mode;
- pads 0..7 are the only pads currently proven to be driven by the eight drum-sequencer voices;
- pads 8..15 exist as runtime storage but are not claimed as a second sequenced bank in 0.9.3.

### Audio call graph

```text
DrumPattern / sequencer step
    |
    +-- internal drum synth trigger
    |
    +-- DrumSamplerTrack::triggerPad(0..7, velocity, store, reverseFx)
              |
              +-- choke: SamplerPool::stopByTag(...)
              +-- gain = pad.volume * velocity
              +-- SamplerPool::trigger(...)
                         |
                         +-- inactive SamplerVoice, else voice 0 is stolen
                         +-- SampleStore::acquireHandle(id)

MiniAcid::generateAudioBuffer()
    |
    +-- clear samplerOutBuffer once per block
    +-- DrumSamplerTrack::process(...)
    |      +-- SamplerPool::process(...)
    |             +-- active SamplerVoice::process(...)
    |
    +-- samplerSample = samplerOutBuffer[i]
    +-- sample += samplerSample
    +-- normal master processing/output path
```

`SamplerVoice` already provides:

- `SampleHandle` lifetime ownership;
- fractional playback position;
- source-rate scaling relative to `kSampleRate`;
- linear interpolation;
- start/end frames;
- pitch multiplier;
- reverse;
- loop;
- gain;
- short attack/stop fade;
- one-shot handle release.

`RamSampleStore` already provides:

- fixed 64-slot metadata array;
- registry `SampleId -> path` on the control side;
- preload from WAV;
- bounded pool capacity;
- LRU eviction;
- atomic ready/id/refcount publication;
- acquire-then-revalidate protection against an eviction race;
- handle-based O(1) audio views after acquisition.

These mechanisms are recovery assets and are not candidates for replacement in PR A.

### Scene state

`Scene` already contains `samplerPads[16]` with:

```text
sampleId
volume
pitch
startFrame
endFrame
chokeGroup
reverse
loop
```

The JSON writer emits the corresponding sampler pad fields. `MiniAcid::applySceneStateFromManager()` projects Scene sampler state into runtime pads and attempts to preload each non-zero sample ID.

A reliable reverse mutation path from runtime pad edits back into the authoritative Scene state has **not yet been proved by a Save/destroy/Load regression**. PR 0.9.3-D must add that regression before changing production ownership.

### Existing UI source versus current workflow

`src/ui/pages/sampler_page.cpp` still contains a substantial Sampler editor and kit browser. The active `workflow_mode.h` has no Sampler page/workspace; current HUB remains `OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS`.

Therefore the lower sampler runtime remains integrated while the Sampler user workflow is amputated. PR A records this only; PR 0.9.3-F owns recovery into the current DRUMS workflow.

## OBSERVED MEMORY

### Source-known fixed facts

These are source facts, not hardware measurements:

| Item | Current contract |
|---|---:|
| Audio sample rate | 22050 Hz |
| Audio block | 512 frames |
| `samplerOutBuffer` payload | 512 × 4 = 2048 B |
| Sampler voices | 8 |
| Runtime pads | 16 |
| Sample metadata slots | 64 |
| Cardputer DRAM sampler pool policy | 32768 B |
| Preferred evidence WAV | mono PCM16 / 22050 Hz |

The evidence sketch prints actual compiler ABI sizes for:

- `sizeof(SampleSlot)`
- `sizeof(SamplerVoice)`
- `sizeof(SamplerPool)`
- `sizeof(SamplerPad)`
- `sizeof(DrumSamplerTrack)`
- `sizeof(RamSampleStore)`

### Cardputer ADV measurements

Do not replace `PENDING` values with estimates.

| Stage | freeInt | largestInt | free8 | largest8 | pool used | resident |
|---|---:|---:|---:|---:|---:|---:|
| BOOT_BASELINE | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| AFTER_SD_INIT | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| AFTER_REGISTRY | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| AFTER_ONE_PRELOAD | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| AFTER_0_VOICES | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| AFTER_1_VOICE | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| AFTER_4_VOICES | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| AFTER_8_VOICES | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |

Reference preload latency: **PENDING us**.

Reference fixture payload: 8192 mono PCM16 frames = 16384 bytes, plus a 44-byte WAV header on disk. This fixture is intentionally below the existing 32 KiB pool policy.

## OBSERVED CPU

Current audio budget:

```text
512 / 22050 s = approximately 23.22 ms per block
```

The standalone evidence sketch measures only `DrumSamplerTrack::process()` for the selected sampler voices. It does not claim that isolated sampler timing equals total GroovePuter DSP timing.

| Active voices | Average sampler render | Peak sampler render | Avg % of block | Peak % of block |
|---:|---:|---:|---:|---:|
| 0 | PENDING | PENDING | PENDING | PENDING |
| 1 | PENDING | PENDING | PENDING | PENDING |
| 4 | PENDING | PENDING | PENDING | PENDING |
| 8 | PENDING | PENDING | PENDING | PENDING |

Production firmware already publishes `[PERF]` telemetry with total audio CPU, peak, output underruns, internal heap and DSP buckets. Note: the current `dspSamplerUs` field includes sampler time **plus vocal time**, so it must not be presented as pure sampler timing. PR A's isolated sketch exists specifically to obtain a sampler-only number without changing the production audio callback.

## KNOWN FAILURES

### A. UI amputation — PROVEN

- `SamplerPage` implementation exists.
- Current workflow has no Sampler page/workspace.
- No 0.9.3-A UI behavior is changed.

Owner: PR 0.9.3-F.

### B. Boot restore / registry order — PROVEN

Current Cardputer boot order is:

```text
sampleStore attached
    -> MiniAcid::init()
       -> Scene load
       -> applySceneStateFromManager()
       -> preload(saved SampleId)
    -> SampleIndex scan
    -> registerFile(id, path)
```

`RamSampleStore::preload()` requires the ID to exist in its file registry and otherwise reports `Preload: ID ... not found in registry`.

A saved sampler pad can therefore restore metadata before its PCM path is resolvable.

Owner: PR 0.9.3-C.

### C. Kit registration — PROVEN

Historical `SamplerPage::loadKit()` scans the selected kit directory and assigns IDs, then calls `preload()` inside the same UI operation. That function does not populate `RamSampleStore::registerFile()` for the newly scanned kit files.

Owner: PR 0.9.3-C. Fix at the catalog/registry lifecycle boundary; do not scatter ad-hoc registrations through UI code.

### D. Persistence ownership — SOURCE RISK PROVEN; ROUND-TRIP FAILURE NOT YET PROVEN

The current Scene projects sampler pad state into runtime. `SamplerPage` edits runtime `samplerTrack->pad(...)` directly and marks Scene dirty.

A complete runtime-to-Scene write-back before serialization has not been established by the evidence gathered so far. Do not label data loss as proven until the required regression reproduces it.

Owner: PR 0.9.3-D. Required first step: Load -> modify every sampler parameter -> Save -> destroy runtime -> Load -> compare exact values.

### E. Preload inside AudioGuard — PROVEN

`SamplerPage` wraps sample selection/kit loading in `audio_guard_`, and calls `sampleStore->preload()` from inside that guarded lambda. `preload()` performs registry lookup, WAV open/read, allocation/conversion and possibly eviction.

Filesystem and sample preparation must leave the long audio mutation boundary in PR 0.9.3-E. PR A does not change it.

### F. Sample identity collision — PROVEN

`SampleIndex::scanDirectory()` currently calculates FNV-1a from `info.filename`, not `info.fullPath`.

Therefore two files such as:

```text
/kits/909/kick.wav
/kits/606/kick.wav
```

receive the same current ID when both are scanned under the filename-only contract.

Owner: PR 0.9.3-B. Compatibility with old persisted basename IDs must be explicit; do not silently reinterpret old scenes.

### Additional UI inconsistency — PROVEN

The historical SamplerPage comment says `Q-I` triggers pads 1..8, while the literal mapping is `qwertyu`, which contains only seven keys.

Owner: PR 0.9.3-F. Use an explicit tested eight-key mapping; do not invent a second sequencer bank for pads 8..15.

## HARDWARE STATUS

**PENDING.**

The evidence sketch exists specifically so this section can be filled from real Cardputer ADV Serial output rather than estimates.

Required evidence before PR A can be accepted:

- ABI `sizeof` line;
- all eight heap snapshots;
- preload latency;
- pool capacity/usage and resident count;
- isolated 0/1/4/8 voice render timing;
- reference-count return to zero after each scenario;
- no reset/WDT during the evidence run.

Production 0.9.3 firmware should also be booted once on the same exact base/head and its existing `[PERF]` line captured at idle for comparison. Do not compare numbers from a Tape-memory branch or another SHA without stating that different base explicitly.

## 0.9.3 RECOMMENDATIONS

Do not reorder the recovery plan based on convenience. Evidence currently supports the original staged sequence:

1. **0.9.3-B — Stable SampleRef**: path-derived current identity plus bounded legacy resolution/migration.
2. **0.9.3-C — Registry + boot lifecycle**: catalog/registry ready before saved-pad resolution, while missing SD/assets remain non-fatal.
3. **0.9.3-D — Persistence ownership**: regression first, then one authoritative persisted state and controlled runtime mutation/projection.
4. **0.9.3-E — Control-side preload**: filesystem/WAV/allocation outside the long AudioGuard/audio path, bounded publication only.
5. **0.9.3-F — DRUMS / SAMPLER UI recovery**: integrate native Sampler editing under current DRUMS architecture; pads 0..7 only as proven sequenced lanes.
6. **0.9.3-G — End-to-end ADV acceptance**: no new feature scope.

Keep the following in 0.9.4: WAV parser productization, optimized stereo-to-mono conversion, memory arena, transactional kits, canonical kit layout, SYNTH/SAMPLE/LAYER, polished missing-sample UX and extended production soak.

## Build / Flash steps

### 1. Install the pinned Arduino dependencies

From repository root:

```bash
bash scripts/install_arduino_deps.sh
```

### 2. Compile the evidence sketch

```bash
arduino-cli compile --clean --warnings all \
  --fqbn 'm5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc' \
  tests/hardware/CardputerAdvSamplerEvidence
```

### 3. Upload

Replace the port if necessary:

```bash
arduino-cli upload \
  -p /dev/ttyACM0 \
  --fqbn 'm5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc' \
  tests/hardware/CardputerAdvSamplerEvidence
```

### 4. Capture Serial

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Save the complete block from:

```text
[SAMPLER-EVIDENCE] base=...
```

through:

```text
[SAMPLER-EVIDENCE][DONE] ...
```

Do not trim warnings or failure lines from the hardware record.

### 5. Production baseline

After the standalone evidence capture, flash the normal firmware from the same branch:

```bash
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Capture boot heap diagnostics and at least several stable `[PERF]` lines with no sampler sample active. This gives a production-runtime baseline next to the isolated sampler measurements.

## Expected behavior

The standalone sketch must:

1. boot without starting the full GroovePuter UI/audio runtime;
2. print ABI sizes and the first four heap-cap fields;
3. mount the built-in SD interface;
4. create a deterministic mono PCM16/22050 reference WAV;
5. register and preload that file under the existing store API;
6. report preload latency, pool usage and one resident sample;
7. run isolated render measurements for 0, 1, 4 and 8 voices;
8. release all sample handles after each voice scenario;
9. complete with no watchdog reset or crash.

If SD is absent, the sketch must print a failure/status line and remain alive rather than crash. A missing-SD evidence run is useful secondary evidence but does not replace the successful preload/voice measurement run.

## Troubleshooting

### `SD unavailable`

Power down, re-seat the microSD card and rerun. The evidence sketch intentionally does not fall back to a host estimate.

### `Preload: ID ... not found in registry`

That is unexpected in the standalone evidence sketch because it explicitly registers the deterministic fixture before preload. Keep the full log; do not add a workaround in the sketch without finding the cause.

### Reference WAV creation fails

Confirm the SD card is writable and that replacing `/sampler_evidence_ref.wav` is permitted. The evidence sketch does not need the user's existing kit directory.

### Pool reports less than the fixture size available

Record the complete Serial log. Do not increase the 32 KiB pool for PR A.

### Voice refs do not return to zero

Treat that as a sampler lifetime failure. Do not compensate by forcibly evicting/resetting the store in the measurement code.

### Evidence timing appears unusually high

Repeat once after a cold reboot and preserve both records. Do not enable detailed production audio diagnostics while interpreting the isolated sketch; the sketch already measures only the sampler render call.

## Acceptance checklist

### Source / CI

- [ ] `python3 tests/test_sampler_recovery_evidence.py` passes.
- [ ] existing `tests/test_sampler_voice.cpp` remains green through the normal host suite.
- [ ] standalone Cardputer ADV evidence sketch compiles with PSRAM disabled.
- [ ] full host regression suite passes.
- [ ] SDL build passes.
- [ ] Cardputer ADV normal build passes.
- [ ] fixed DRAM gate passes.
- [ ] SEQTRAK MIDI-only build/gate passes.
- [ ] current Phrase/Four-axis/release gates remain green where applicable.
- [ ] diff contains no sampler production behavior change.

### Cardputer ADV evidence

- [ ] exact branch/head SHA recorded.
- [ ] `sizeof` line captured.
- [ ] BOOT_BASELINE heap captured.
- [ ] AFTER_SD_INIT heap captured.
- [ ] AFTER_REGISTRY heap captured.
- [ ] AFTER_ONE_PRELOAD heap captured.
- [ ] AFTER_0_VOICES heap captured.
- [ ] AFTER_1_VOICE heap captured.
- [ ] AFTER_4_VOICES heap captured.
- [ ] AFTER_8_VOICES heap captured.
- [ ] preload latency captured.
- [ ] pool capacity/usage captured.
- [ ] resident slots captured.
- [ ] 0-voice sampler render timing captured.
- [ ] 1-voice sampler render timing captured.
- [ ] 4-voice sampler render timing captured.
- [ ] 8-voice sampler render timing captured.
- [ ] refs return to zero after each scenario.
- [ ] no WDT/reset/crash.
- [ ] normal production firmware boot/perf baseline captured on the exact same head.

## Exit condition for PR A

PR A may be accepted only after CI is green and the `PENDING` Cardputer ADV measurements above are replaced by measurements from the exact PR candidate SHA.

No sampler recovery production fix belongs in this PR. If hardware measurement shows the current sampler cannot fit the Cardputer ADV release gates, stop after documenting the evidence and reassess the 0.9.3 plan rather than silently importing 0.9.4 memory architecture.
