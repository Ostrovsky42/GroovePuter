# GroovePuter 0.9.3-B — Stable SampleRef

Base: `dev_0.9.3 @ 6cb6875591b2b9a0e623bc0fcbc1a0fb77ea37a5`

Status: DRAFT / CI + hardware smoke required before merge.

## Purpose

Introduce a stable, compact control-side identity for sampler files before changing registry/boot ownership.

0.9.3-B fixes the identity contract only. It does **not** move SD/WAV work into a different task, change preload timing, recover the Sampler UI, change Scene save ownership, alter the 32 KiB sample pool, or modify the audio-thread `SampleHandle` / `SampleSlot` ABI.

## Identity contract

`SampleRef` is a 64-bit FNV-1a value derived from a canonical logical sample path.

Canonicalization guarantees:

- `/sd/samples/kick.wav` and `/samples/kick.wav` resolve to the same logical key;
- repeated `/` separators do not change identity;
- `\\` and `/` separators normalize identically;
- `./` path segments do not change identity;
- directory enumeration/sort order does not change identity;
- two files with the same basename in different folders receive different refs.

The logical folder path is intentionally part of identity. Renaming or physically moving a file to a different logical path is a different sample reference; 0.9.3-B is not a content-addressed asset system.

`SampleRef{0}` is reserved for “no sample”.

## Compatibility boundary

Historical sampler `SampleId` is a 32-bit FNV-1a hash of the basename only. That runtime ID remains unchanged in 0.9.3-B.

`SampleFileInfo` retains its pre-0.9.3-B fields and size class: legacy `SampleId`, filename and full path. Stable `SampleRef` is derived from the already-stored `fullPath` on the control side rather than retained per file.

`SampleIndex::resolveLegacyId()` is the migration bridge for old persisted IDs. It returns a stable ref only when the current index can resolve the legacy ID to exactly one file; missing or ambiguous IDs fail closed.

A regression uses the real colliding legacy filenames `5oetw2k1.wav` and `qp363n87.wav` (both FNV-1a32 `3960902837`) and requires migration to reject the ambiguous legacy ID while their path-derived stable refs remain distinct.

`findByRef()` also fails closed if the current index ever observes two different paths with the same 64-bit stable ref.

The registry/boot cut-over to stable refs belongs to **0.9.3-C**. Scene write-back and save/reboot/load ownership belong to **0.9.3-D**.

## Memory boundary

Accepted Cardputer ADV baseline before this PR:

- `free8 ≈ 38.36 KiB`
- `largest8 ≈ 21.49 KiB`
- audio underruns `0`
- no audio regression after #266

This PR does not replace 32-bit atomics in the audio path with 64-bit atomics. `SampleRef` is control-side metadata only.

There is no new persistent per-file `SampleRef` field, no new fixed sample-pool allocation and no new audio buffer. Stable refs are calculated from `SampleFileInfo::fullPath` only when control-side code asks for them.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- microSD card with the normal GroovePuter project/sample layout
- USB-C data cable
- PSRAM-disabled Cardputer ADV build profile

## Wiring

No external wiring is required. Use the normal Cardputer ADV USB + microSD setup.

PORT.A/I2C is not used by this test.

## Build / flash

Host identity contract:

```bash
bash tests/run_sampler_ref_tests.sh
```

Full regression/build gate:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Hardware smoke after CI:

```bash
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

### Host

`tests/run_sampler_ref_tests.sh` must prove:

- `SampleRef` remains exactly 8 bytes;
- `/sd/...` and `/...` mount aliases produce the same ref;
- same basename in different logical folders produces different refs;
- re-scanning the same directory preserves refs independent of discovery order;
- a known legacy basename ID resolves to the corresponding stable ref;
- unknown legacy IDs fail closed;
- a real legacy FNV-1a32 collision fails closed.

### Cardputer ADV

No visible UI/audio behavior should change in 0.9.3-B.

Normal GroovePuter boot, playback, MIDI and audio output should behave as on the accepted #266 baseline. This stage intentionally does not repair the known sampler boot/preload ordering defect yet.

## Troubleshooting

If a stable ref changes between `/sd/...` and `/...`, inspect `canonicalSampleKey()` before touching registry code.

If two equal basenames in different folders produce the same stable ref, treat that as a release-blocking identity failure for 0.9.3-B.

If an old basename `SampleId` cannot resolve, confirm the corresponding file exists in the currently scanned index. Do not guess a different file on missing or ambiguous legacy identity.

If Cardputer runtime memory regresses materially from the #266 baseline, inspect accidental persistent index metadata/static allocations. This PR is designed to keep stable identity out of the resident per-file structure.

## Acceptance checklist

### Identity

- [ ] `SampleRef` is 64-bit and zero is invalid.
- [ ] Mount alias normalization is deterministic.
- [ ] Same basename in different folders has distinct stable refs.
- [ ] Directory enumeration order does not affect stable refs.
- [ ] Legacy basename ID resolves only through an explicit compatibility API.
- [ ] Missing/ambiguous legacy identity fails closed.
- [ ] Real FNV-1a32 legacy collision is rejected deterministically.

### Regression

- [ ] Dedicated SampleRef host gate green.
- [ ] Full host suite green.
- [ ] SDL green.
- [ ] Cardputer ADV normal build green.
- [ ] Fixed DRAM gate green.
- [ ] SEQTRAK MIDI-only build green.
- [ ] No audio-thread 64-bit identity conversion.
- [ ] No sampler pool/buffer increase.
- [ ] No persistent per-file SampleRef heap growth.

### Hardware smoke

- [ ] Cold boot reaches normal UI.
- [ ] No WDT/reset/crash.
- [ ] Audio underruns remain `0`.
- [ ] Runtime memory remains in the same class as the accepted #266 baseline (`free8 ≈ 38.36 KiB`, `largest8 ≈ 21.49 KiB`).

## Out of scope

Do not pull the following into 0.9.3-B:

- registry-before-Scene boot reordering;
- preload queue/control-side loader lifecycle;
- Scene sampler write-back/persistence ownership;
- SamplerPage recovery;
- kit redesign;
- WAV parser rewrite;
- arena/streaming;
- SYNTH/SAMPLE/LAYER;
- Tape/Voice/Recorder recovery.
