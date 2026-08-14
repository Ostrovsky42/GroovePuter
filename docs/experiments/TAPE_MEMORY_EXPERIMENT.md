# Tape memory experiment — Cardputer ADV

## Purpose

Measure the internal-DRAM cost and fragmentation impact of the dormant Tape runtime on Cardputer ADV without changing release product source.

Temporary-source variants:

| Variant | TapeFX | TapeLooper object | TapeLooper buffer |
|---|---:|---:|---:|
| A — CURRENT | present | present | current `init(0.5f)` attempt |
| B — NO TAPE FX | absent | present | current `init(0.5f)` attempt |
| C — NO TAPE RUNTIME | absent | absent | absent |

The instrumentation edits only a temporary source copy. PR #263 is an evidence snapshot and is not intended for merge into the release line.

### Source-known sizes

- TapeFX embedded arrays: `1024 + 4096` floats = **20480 bytes**.
- ADV TapeLooper request: `22050 Hz × 0.5 s × int16` = **22050 bytes**.

## Preliminary hardware result

Status: **one cold boot per A/B/C captured; final 3× medians pending**.

All three preliminary runs reported heap integrity `1`.

| Variant | Checkpoint | freeInt | largest internal block |
|---|---|---:|---:|
| A | static | 163284 | 94196 |
| A | `MiniAcid::init()` | 17836 | 7668 |
| A | UI | 16872 | 7668 |
| B | static | 184792 | 114676 |
| B | after SD | 47996 | 21492 |
| B | `MiniAcid::init()` | 39348 | 21492 |
| B | UI | 38384 | 21492 |
| C | after SD | 48056 | 21492 |
| C | `MiniAcid::init()` | 39408 | 21492 |
| C | UI | 38444 | 21492 |

### Causal findings

1. **TapeFX cost is confirmed.** B recovers `21508` bytes at static construction and `21512` bytes after MiniAcid/UI. Static `largest` improves by exactly `20480` bytes, matching the embedded arrays.
2. **The current ADV TapeLooper buffer is not resident.** After SD the largest internal block is `21492` bytes, but `init(0.5f)` requests `22050` bytes: a shortfall of `558` bytes before any safety margin.
3. **B→C is only allocator/object noise.** C has about `60` bytes more freeInt than B and the same largest block after SD/init/UI. This falsifies the original expectation of a ~22 KiB B-only looper allocation.
4. **Late lazy allocation of the current 0.5 s buffer is rejected.** The required contiguous block is already unavailable after SD; later project/MIDI/sampler activity cannot be assumed to improve it.
5. `TapeLooper::init()` returns failure and clears `maxSamples_`, but the current DRAM `MiniAcid::init()` call ignores that return value. Mode control can therefore advertise REC while no storage exists.
6. Current product navigation does not expose TapePage. Reserving ~22 KiB early now would spend ADV DRAM on a dormant workflow and would erase almost all memory recovered by removing TapeFX.

The missing B→C delta is therefore a useful hardware result, not an inconclusive experiment.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- USB-C data cable
- Same microSD card for every run
- Development computer with Arduino CLI and repository dependencies

No external I2C, MIDI, audio, or PORT.A device is required for the A/B/C boot-memory test.

## Wiring

- Power/program Cardputer ADV over USB-C.
- Keep the same SD card and peripherals for every A/B/C run.
- Target the normal PSRAM-disabled Cardputer ADV profile.

## Build / Flash

```bash
git checkout agent/20260814-tape-memory-experiment
bash scripts/install_arduino_deps.sh

bash scripts/build_cardputer_tape_memory_experiment.sh A normal --warnings all
bash scripts/build_cardputer_tape_memory_experiment.sh B normal --warnings all
bash scripts/build_cardputer_tape_memory_experiment.sh C normal --warnings all
```

Flash one image at a time:

```bash
BUILD_PATH=build/tape-memory-A-normal bash scripts/upload.sh /dev/ttyACM0
BUILD_PATH=build/tape-memory-B-normal bash scripts/upload.sh /dev/ttyACM0
BUILD_PATH=build/tape-memory-C-normal bash scripts/upload.sh /dev/ttyACM0
```

Capture Serial at 115200 baud.

## Final cold-boot protocol

For each variant A/B/C:

1. Use the same Cardputer ADV, SD card, USB profile, peripherals, scene/project contents, and physical setup.
2. Remove power completely and capture a full boot log.
3. Repeat until **three cold boots** exist for that variant.
4. Preserve all raw values.
5. Compare variants using the **median of all three boots** for every checkpoint/metric.

Capture:

```text
[tape-exp-after-static-construction]
[tape-exp-after-audio-task]
[tape-exp-after-sd]
[tape-exp-after-miniacid-init]
[tape-exp-after-ui]
[MEM-BASE] phase=runtime-start
[MEM-BASE] phase=periodic
```

Record `freeInt`, `largInt`, `free8`, `larg8`, runtime minimum largest-block watermarks, task stack watermarks, and heap integrity.

## Expected behavior

### Screen

- A/B/C should boot to normal GroovePuter UI.
- Do not open/operate Tape UI in B/C; those temporary images remove owners without product UI redesign.

### Serial / expected final shape

| Comparison | Expected after preliminary hardware pass |
|---|---|
| A→B | approximately `+21.5 KiB freeInt`; static `largest` approximately `+20480` |
| B→C | only small object/allocator delta; **no 22 KiB buffer delta** |
| B `largest` after SD | remains below the `22050`-byte 0.5 s request on the current profile |
| integrity | `1` for all accepted runs |

## Product decision

### TapeFX

Proceed as a separate product PR after the evidence baseline is preserved:

- remove the dormant TapeFX runtime owner and DSP implementation from the active product;
- remove dead TapeFX control dependencies;
- preserve legacy Scene decode compatibility if old fields remain in persisted data;
- rerun Cardputer ADV memory, audio, Scene, UI, and normal regression gates.

### TapeLooper on current ADV

Do **not** reserve a 0.5 s buffer early and do **not** implement late lazy allocation merely to make the dormant subsystem resident.

Current ADV policy should be explicit and deterministic:

- no TapeLooper sample-buffer reservation while Tape is absent from the current workflow;
- no opportunistic “allocate whatever fits” behavior;
- no REC state when storage is unavailable;
- preserve TapeLooper source for a later intentional Tape recovery/productization pass.

This retains the ~21.5 KiB TapeFX recovery for sampler/runtime instead of exchanging it for ~22.05 KiB of unreachable looper storage.

### Future Tape recovery gate

When Tape is intentionally restored to the workflow, reopen the allocation question with a separate evidence build.

First candidate:

- reserve the existing 0.5 s / 22050-byte looper **after the critical AudioTask and constrained delay allocations, but before SD/SMF fragmentation**;
- then run full sampler/runtime memory and musical acceptance.

If that is too expensive, test a smaller deterministic fixed ADV duration with deliberate contiguous-block safety margin. Allocation must stay outside the audio callback, a successful buffer must remain allocated for the boot session, and failure must leave Tape in STOP with a clear unavailable/no-memory indication.

This future test is **not a blocker for closing PR #263 or continuing sampler recovery**.

## Troubleshooting

### Instrumentation reports a missing anchor

The source moved relative to the experiment base. Rebase/update the exact anchor deliberately; do not weaken the script to patch multiple matches.

### B/C resets when opening Tape UI

Expected for diagnostic B/C images. Reboot and collect only boot/runtime memory evidence.

### Tape appears to enter REC but records nothing

The current ADV allocation may have failed. Visible REC state is not proof that a looper buffer exists.

### `freeInt` improves but `largInt` does not

Treat this as real fragmentation evidence. Preserve the complete checkpoint series; do not reduce the result to total free heap.

## Acceptance checklist

- [x] A/B/C compile from the same experiment source.
- [x] A/B/C CI matrix is green on the experiment checkpoint.
- [x] One preliminary cold boot captured for A/B/C.
- [x] Preliminary heap integrity is `1` for A/B/C.
- [x] Preliminary A→B confirms TapeFX cost.
- [x] Preliminary B→C + `largest=21492` confirms the 0.5 s looper allocation is not resident.
- [ ] Three complete cold boots captured for A/B/C.
- [ ] All raw values preserved and medians calculated from all three boots.
- [ ] All five Tape checkpoints plus runtime baseline captured for final accepted boots.
- [ ] Final medians remain consistent with TapeFX recovery and failed 0.5 s looper allocation.
- [ ] Final hardware table recorded in PR #263.
- [ ] PR #263 closed without merge after evidence completion.
