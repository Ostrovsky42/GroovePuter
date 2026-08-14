# GroovePuter 0.9.3-C — Sampler Registry / Boot Lifecycle

Base: `dev_0.9.3 @ a6373b1f5cd14a95186ab799d8faa2d994a6cba6`

Status: DRAFT / CI + Cardputer ADV acceptance required before merge.

## Purpose

Make the existing sampler registry ready before Scene sampler state is applied on Cardputer ADV.

Before C, `MiniAcid::init()` loaded/applied Scene state and called `sampleStore->preload()` before `setup()` scanned `/sd/samples` and registered file paths. A valid saved pad could therefore fail with `Preload: ID ... not found in registry` even though the WAV was present.

C fixes lifecycle ownership only:

1. the shared Cardputer SD owner mounts SD;
2. a one-shot SD-ready hook scans the existing sampler index;
3. stable `SampleRef` validates each discovered path;
4. only unambiguous current legacy IDs are bound into the 32-bit runtime store;
5. `MiniAcid::init()` may then load/apply Scene and perform the existing preload.

No PCM is loaded from the SD-ready hook.

## Identity / compatibility boundary

0.9.3-B introduced 64-bit path-derived `SampleRef`. C uses it as the control-side validation authority while retaining the existing 32-bit `SampleId` in `SampleSlot`, `SampleHandle`, pads and old Scene JSON.

This is deliberate. Stable Scene persistence/write-back belongs to **0.9.3-D**.

Until D lands:

- an old basename ID that resolves to exactly one stable path is accepted;
- a missing/ambiguous old ID is rejected;
- a runtime ID can never silently rebind from one path to another;
- duplicate/legacy hash collisions fail closed instead of last-write-wins corruption.

The frozen real legacy collision pair remains:

- `5oetw2k1.wav`
- `qp363n87.wav`
- both legacy FNV-1a32 `3960902837`

Both are rejected from a legacy runtime bind when indexed together.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- microSD card
- USB-C data cable
- PSRAM-disabled GroovePuter build
- optional existing sampler WAVs under `/samples` (the historical `/sd/samples` alias is also probed)

## Wiring

No external wiring is required.

PORT.A / I2C is not used by this recovery test.

## Build / flash

Dedicated host contract:

```bash
bash tests/run_sampler_registry_boot_tests.sh
```

Normal product gates:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash / monitor:

```bash
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

### Host

The dedicated contract must prove:

- unique samples bind successfully;
- stable refs resolve back to the same indexed path;
- known legacy IDs resolve only when unambiguous;
- the real FNV32 collision pair is rejected before store registration;
- `RamSampleStore::registerFile()` accepts an idempotent same-path bind;
- `RamSampleStore::registerFile()` rejects the same runtime ID being rebound to another path;
- source ordering keeps the existing early storage initialization before `MiniAcid::init()`;
- the SD-ready hook performs scan/register only and does not preload PCM;
- Scene schema is still legacy `uint32_t sampleId` in C.

### Cardputer ADV

On cold boot, Serial should show this ordering:

```text
[SD] mount result=1 ...
SampleIndex::scanDirectory: Scanning ...
[SAMPLER-REGISTRY] ready discovered=... registered=...
...
6. Engine Init...
  - MiniAcid::init: Loading scene from storage...
  - MiniAcid::init: applySceneStateFromManager()...
```

The important invariant is that `[SAMPLER-REGISTRY] ready` occurs before Scene load/apply.

If the current Scene already contains a non-zero legacy sampler pad ID whose WAV is present and unambiguous, boot preload should log `Preload: Loading ...` rather than `Preload: ID ... not found in registry`.

If no current Scene contains a sampler assignment, the boot-order log plus dedicated host registry contract is still valid C evidence; the full save/reboot/load ownership test belongs to D.

## Memory / audio baseline

Pre-C accepted Cardputer ADV baseline from #267:

- product global variables: `176656 B`
- runtime `freeInt ≈ 36408 B`
- largest internal block `≈ 21492 B`
- audio underruns `0`

C adds only a one-shot SD-ready function pointer/flag in fixed state. The index/path registry already existed; it is constructed earlier in boot, not duplicated.

Actual PCM allocation still occurs only when Scene apply calls the existing preload. A Scene that restores a sample can therefore legitimately consume PCM bytes from the existing 32 KiB sampler pool; compare registry overhead separately from sample payload.

## Troubleshooting

If `[SAMPLER-REGISTRY]` appears after `MiniAcid::init`, do not patch preload retries. The lifecycle order is wrong.

If `legacyReject` is non-zero, inspect duplicate basenames/hash collisions. Do not choose the first/last path as a fallback.

If `storeReject` is non-zero, inspect an attempted runtime ID rebind. `RamSampleStore` intentionally refuses conflicting path ownership.

If there are zero discovered files, verify `/samples` on the microSD. The firmware probes historical `/sd/samples` first and then `/samples`.

If a saved pad is present but preload still prints `ID ... not found in registry`, capture the preceding `[SAMPLER-REGISTRY]` line and the sample discovery lines; that is a C blocker.

USB-MIDI endpoint stall tracking remains separate in issue #268. Do not fold that defect into this sampler lifecycle PR unless C directly changes USB-MIDI code (it should not).

## Acceptance checklist

### Registry contract

- [ ] Stable SampleRef validates every accepted binding.
- [ ] Unique legacy ID -> exact stable path -> runtime registration succeeds.
- [ ] Missing/ambiguous legacy identity fails closed.
- [ ] Real FNV32 collision pair is rejected.
- [ ] Runtime registry cannot silently rebind an ID to a different path.
- [ ] No new 64-bit atomic or SampleSlot/SampleHandle expansion.
- [ ] No PCM load in the SD-ready hook.

### Boot lifecycle

- [ ] Shared SD mount remains the only mount owner.
- [ ] `[SAMPLER-REGISTRY] ready` occurs before `MiniAcid::init()` Scene load/apply.
- [ ] Existing post-init scan, if encountered, is idempotent and cannot overwrite conflicting ownership.
- [ ] Cold boot reaches normal UI.
- [ ] No WDT/reset/crash.

### Regression

- [ ] Dedicated sampler registry/boot workflow green.
- [ ] Full host/Core suite green.
- [ ] SDL green.
- [ ] Cardputer ADV normal + fixed DRAM green.
- [ ] SEQTRAK MIDI-only green.
- [ ] Synth persistence / Stage 15 matrix green.
- [ ] Audio underruns remain `0` in hardware smoke.
- [ ] Registry-only memory remains in the same class as #267 baseline.

### Saved-sample smoke when a suitable legacy Scene is available

- [ ] Present unambiguous saved sample resolves at cold boot.
- [ ] `Preload: Loading ...` occurs after registry-ready.
- [ ] No `Preload: ID ... not found in registry` for that present sample.
- [ ] PLAY/STOP does not corrupt memory or add sampler/audio underruns.

## Out of scope

Do not pull into 0.9.3-C:

- stable SampleRef Scene schema/write-back (`0.9.3-D`)
- async/control-side preload queue (`0.9.3-E`)
- SamplerPage workflow recovery (`0.9.3-F`)
- WAV parser/productization changes
- kit redesign
- streaming/arena work
- SYNTH/SAMPLE/LAYER output ownership
- USB-MIDI stall issue #268
- Tape/Voice/Recorder recovery
