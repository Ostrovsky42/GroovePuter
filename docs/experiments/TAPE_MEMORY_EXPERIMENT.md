# Tape memory experiment — Cardputer ADV

## Purpose

Measure the real internal-DRAM cost and fragmentation impact of the current Tape runtime on Cardputer ADV without changing product source.

The experiment is built from one exact source revision and creates three temporary-source variants:

| Variant | TapeFX | TapeLooper object | TapeLooper buffer |
|---|---:|---:|---:|
| A — CURRENT | present | present | current `init(0.5f)` attempt |
| B — NO TAPE FX | absent | present | current `init(0.5f)` attempt |
| C — NO TAPE RUNTIME | absent | absent | absent |

The branch does not remove Tape from the product tree. `scripts/instrument_tape_memory_experiment.py` edits only the temporary source copied by the experiment build.

This PR is an evidence snapshot. It is intentionally not a release-line product change and should remain separate from the eventual product PRs.

### Source-known allocation sizes

`TapeFX` owns two embedded float arrays:

- 1024 samples × 4 bytes = 4096 bytes
- 4096 samples × 4 bytes = 16384 bytes
- arrays alone = **20480 bytes**

The DRAM-only Cardputer path calls `TapeLooper::init(0.5f)`. At the current 22050 Hz sample rate this requests:

- 22050 × 0.5 × 2 bytes = **22050 bytes**

The original hypothesis expected both allocations to be resident. Hardware evidence below disproved the TapeLooper part of that hypothesis on the current ADV memory profile.

## Preliminary hardware evidence — one cold boot per variant

Status: **PRELIMINARY**. One cold boot has been captured for each A/B/C image. The required three-cold-boot cycle per variant is still pending, so these values are not final medians.

All captured runs reported heap integrity `1`.

| Variant | Checkpoint | freeInt | largest internal block |
|---|---|---:|---:|
| A | after static construction | 163284 | 94196 |
| A | after `MiniAcid::init()` | 17836 | 7668 |
| A | after UI | 16872 | 7668 |
| B | after static construction | 184792 | 114676 |
| B | after SD | 47996 | 21492 |
| B | after `MiniAcid::init()` | 39348 | 21492 |
| B | after UI | 38384 | 21492 |
| C | after SD | 48056 | 21492 |
| C | after `MiniAcid::init()` | 39408 | 21492 |
| C | after UI | 38444 | 21492 |

### Preliminary causal deltas

- `B - A` at static construction: **+21508 bytes freeInt** and **+20480 bytes largest block**.
- `B - A` after `MiniAcid::init()`: **+21512 bytes freeInt**.
- `B - A` after UI: **+21512 bytes freeInt**.
- `C - B` after SD / `MiniAcid::init()` / UI: approximately **+60 bytes freeInt**, with **no largest-block delta**.
- Immediately after SD, the measured largest internal block is **21492 bytes**.
- The requested 0.5 s TapeLooper buffer is **22050 bytes**, therefore the allocation is short by **558 bytes even before allowing allocator metadata or safety margin**.

### What the first hardware pass proves

1. **TapeFX cost is confirmed.** The observed free-heap recovery is about 21.5 KiB and the static largest-block recovery matches the 20480-byte embedded arrays exactly.
2. **The expected TapeLooper allocation is not resident on the current ADV image.** B and C are effectively identical after SD and `MiniAcid::init()`, which is inconsistent with a successful 22050-byte B-only allocation and consistent with the allocation attempt failing.
3. **Late lazy allocation of the current 0.5 s buffer is not viable on this measured memory profile.** The required 22050-byte contiguous block is already larger than the observed 21492-byte largest block after SD, and later runtime/project activity cannot be assumed to improve it.
4. The original expected B→C ~22 KiB delta was therefore a falsified hypothesis, not a failed experiment.

Source behavior is consistent with the measurement: `TapeLooper::init()` returns `false` and resets `maxSamples_` when allocation fails, while the current DRAM `MiniAcid::init()` call does not consume the return value. This creates a separate product-correctness issue: allocation failure is not surfaced as an explicit Tape-unavailable state.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- USB-C data cable
- Same microSD card for all three variants and every repeated boot
- Development computer with Arduino CLI and the repository dependencies installed

No external I2C, MIDI, audio, or PORT.A device is required for the boot-cost experiment.

## Wiring

- Power/program the Cardputer ADV over USB-C.
- Keep the same SD card inserted for A, B, and C.
- Do not change attached peripherals between runs.

The experiment targets the normal Cardputer ADV DRAM-only build: PSRAM disabled, internal heap is the quantity under test.

## Build / Flash steps

Start from the experiment branch:

```bash
git checkout agent/20260814-tape-memory-experiment
bash scripts/install_arduino_deps.sh
```

Build all three variants:

```bash
bash scripts/build_cardputer_tape_memory_experiment.sh A normal --warnings all
bash scripts/build_cardputer_tape_memory_experiment.sh B normal --warnings all
bash scripts/build_cardputer_tape_memory_experiment.sh C normal --warnings all
```

Flash one variant at a time:

```bash
BUILD_PATH=build/tape-memory-A-normal bash scripts/upload.sh /dev/ttyACM0
BUILD_PATH=build/tape-memory-B-normal bash scripts/upload.sh /dev/ttyACM0
BUILD_PATH=build/tape-memory-C-normal bash scripts/upload.sh /dev/ttyACM0
```

Capture Serial at 115200 baud after every flash.

### Cold-boot measurement protocol

For each of A, B, and C:

1. Flash the selected image once.
2. Remove power completely from the Cardputer ADV.
3. Restore power and capture the complete boot log from the first boot.
4. Repeat the full power-off/power-on cycle until **three cold boots** have been captured for that variant.
5. Keep the same SD card, USB profile, peripherals, scene/project contents, and physical setup for all nine boots.
6. Record every raw value from every boot.
7. Use the **median of the three cold boots** for each checkpoint and metric when comparing A/B/C. Do not select a preferred or merely repeated run.

For every boot record these exact lines:

```text
[tape-exp-after-static-construction]
[tape-exp-after-audio-task]
[tape-exp-after-sd]
[tape-exp-after-miniacid-init]
[tape-exp-after-ui]
[MEM-BASE] phase=runtime-start
[MEM-BASE] phase=periodic
```

Each Tape checkpoint contains:

```text
freeInt
largInt
free8
larg8
```

The existing runtime memory instrumentation additionally records minimum sampled heap values, largest-block watermarks, task stack high-water marks, and heap integrity.

## Expected behavior

### Screen

- All three images should reach the normal GroovePuter UI.
- The experiment is for boot/runtime memory evidence only.
- Do not navigate to or operate the Tape page in variants B or C. Those variants intentionally remove runtime owners from the temporary build and are not product-functionality images.

### Serial

Variant A prints:

```text
[TAPE-MEM] variant=A
```

Variant B prints:

```text
[TAPE-MEM] variant=B
```

Variant C prints:

```text
[TAPE-MEM] variant=C
```

### Causal delta interpretation

The original expected shape has now been partially validated and partially falsified by hardware:

| Comparison | Original expectation | Preliminary hardware result |
|---|---|---|
| A→B | ~20 KiB TapeFX recovery from static construction onward | **confirmed**: +21508…21512 bytes freeInt; +20480 largest at static |
| B→C before `MiniAcid::init()` | small ownership-only delta | **confirmed** within allocator noise |
| B→C after `MiniAcid::init()` | ~22 KiB recovery if B owns the looper buffer | **falsified**: ~60 bytes only; B never acquired the 22050-byte buffer |
| `largInt` | should expose fragmentation/contiguous allocation constraints | **confirmed**: 21492-byte block explains failure of a 22050-byte request |

Do not reinterpret the missing B→C delta as noise to be averaged away. The contiguous-block measurement provides the causal explanation and must remain part of the final evidence.

## Product conclusions and follow-up boundaries

Keep product changes separate from this evidence branch.

### Product PR 1 — remove TapeFX

Hardware already confirms the intended result strongly enough to design this PR:

- remove the disabled/unexposed `TapeFX` runtime owner on Cardputer ADV;
- remove or simplify UI/state paths that only control TapeFX rather than leaving dangling dereferences;
- preserve Scene backward decoding if old TapeFX-related state still exists in persisted scenes;
- run the normal Cardputer ADV memory gate and product regressions on the removal PR.

Final merge can still reference the completed 3× A/B/C median table from this evidence PR.

### TapeLooper — do not implement the previously proposed late lazy allocation

The preliminary hardware evidence rejects the prior `lazy-after-SD/UI/project` design for the current 0.5 s / 22050-byte buffer.

The next product decision must choose an explicit ADV allocation policy instead of relying on opportunistic heap state:

1. **Early one-time reservation** — allocate the full 0.5 s buffer before SD and other fragmentation-prone allocations. This can make TapeLooper deterministic, but it spends almost all memory recovered by deleting TapeFX and must be measured against sampler/runtime headroom.
2. **Smaller fixed ADV buffer** — reduce the requested duration to a size with a deliberate contiguous-block safety margin. Do not choose a size merely because it fits the current 21492-byte observation; later project/MIDI/sampler activity can reduce the available block.
3. **ADV unavailable policy** — keep TapeLooper disabled on the current DRAM-only profile and expose a clear unavailable state rather than silently entering REC without storage.

Do not implement “allocate whatever currently fits”. Tape duration and feature availability must remain deterministic and reproducible for a given build/profile.

### Required failure-path correction for any retained ADV TapeLooper

Regardless of the chosen memory policy:

- consume and preserve the result of looper buffer preparation;
- never enter/advertise `TapeMode::Rec` when no buffer is available;
- keep mode `Stop` on preparation failure;
- show a clear user-facing status such as `TAPE: NO MEMORY` or `TAPE: UNAVAILABLE`;
- do not allocate from the audio callback;
- if a buffer is successfully allocated, retain it for the boot session rather than free/reallocate on REC/STOP.

## Next TapeLooper evidence before product implementation

The current A/B/C experiment answers the original memory-accounting question. A separate narrow allocation-policy experiment should answer the remaining product question.

Recommended candidate to measure first:

- **D — NO TAPE FX + EARLY 0.5 s LOOPER RESERVATION**

Reserve the existing 22050-byte looper buffer before SD/fragmentation, then capture the same boot checkpoints plus normal production runtime and sampler memory/performance evidence.

This test answers whether the dead TapeFX allocation can be exchanged for a working full-length TapeLooper without creating an unacceptable later `largestInt` / sampler regression. It should not be inferred from total free bytes alone.

If D is unacceptable, evaluate a deliberately smaller fixed ADV buffer in a separate candidate with an explicit safety margin and musical acceptance test.

## Troubleshooting

### Instrumentation reports a missing anchor

The source moved relative to the experiment base. Do not weaken the script to replace multiple matches. Rebase the experiment and update the exact anchor deliberately.

### Variant B or C resets when opening Tape UI

Expected for these diagnostic images. They remove Tape runtime ownership in the temporary source but do not redesign the Tape page. Reboot and collect only the boot/runtime memory evidence.

### Values differ substantially between repeated runs

Use the same SD card, USB profile, peripherals, scene contents, and boot procedure. Preserve all three cold-boot measurements. Compare medians and inspect the complete checkpoint series plus `[MEM-BASE]` heap-integrity output rather than discarding an inconvenient run.

### `largInt` is unexpectedly much smaller although `freeInt` increased

That is a valid result and indicates fragmentation/allocation-order effects. Preserve the full checkpoint series; do not reduce the experiment to final free bytes.

### Tape mode enters REC but no loop is recorded

On the current ADV implementation this can be caused by the 0.5 s looper allocation failing during boot while the failure result is not surfaced to the UI/mode state. Preserve the Serial memory evidence; do not treat a visible REC state alone as proof that TapeLooper storage exists.

## Acceptance checklist

- [x] A, B, and C compile from the same experiment branch SHA.
- [x] CI matrix A/B/C is green on the exact experiment SHA.
- [x] One preliminary cold boot has been captured for A, B, and C.
- [x] Preliminary heap integrity remains `1` for all three variants.
- [x] Preliminary A→B separation confirms the TapeFX memory cost.
- [x] Preliminary B→C result plus `largInt=21492` demonstrates that the current 22050-byte TapeLooper request is not successfully resident.
- [ ] Three complete cold boots are captured for A, three for B, and three for C.
- [ ] The same Cardputer ADV, SD card, USB profile, peripherals, scene/project contents, and physical setup are used for every repeated run.
- [ ] All raw measurements are preserved; medians are calculated from all three boots per variant.
- [ ] All five `tape-exp-*` checkpoints are captured for every final evidence boot.
- [ ] `[MEM-BASE] phase=runtime-start` and at least one periodic sample are captured for every final evidence boot.
- [ ] Final median heap integrity remains `integrity=1`.
- [ ] Final median A→B behavior remains consistent with the preliminary TapeFX result.
- [ ] Final B→C behavior and largest-block measurements remain consistent with failed 0.5 s looper allocation.
- [ ] Final hardware table and medians are copied into PR #263.
- [ ] PR #263 remains an evidence snapshot and is closed without merge after evidence is complete.
