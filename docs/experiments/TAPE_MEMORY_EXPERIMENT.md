# Tape memory experiment — Cardputer ADV

## Purpose

Measure the real internal-DRAM cost and fragmentation impact of the current Tape runtime on Cardputer ADV without changing product source.

The experiment is built from one exact source revision and creates three temporary-source variants:

| Variant | TapeFX | TapeLooper object | TapeLooper buffer |
|---|---:|---:|---:|
| A — CURRENT | present | present | current `init(0.5f)` |
| B — NO TAPE FX | absent | present | current `init(0.5f)` |
| C — NO TAPE RUNTIME | absent | absent | absent |

The branch does not remove Tape from the product tree. `scripts/instrument_tape_memory_experiment.py` edits only the temporary source copied by the experiment build.

### Why these variants isolate the cost

`TapeFX` owns two embedded float arrays:

- 1024 samples × 4 bytes = 4096 bytes
- 4096 samples × 4 bytes = 16384 bytes
- arrays alone = **20480 bytes**

The DRAM-only Cardputer path calls `TapeLooper::init(0.5f)`. At the current 22050 Hz sample rate this requests:

- 22050 × 0.5 × 2 bytes = **22050 bytes**

Therefore the lower-bound Tape payload is already **42530 bytes** before object state and allocator overhead.

A→B should be visible immediately after static construction because `TapeFX` is created by the static `MiniAcid` constructor. B→C should show its main delta after `MiniAcid::init()`, when the looper buffer would normally be allocated.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- USB-C data cable
- Same microSD card for all three runs
- Development computer with Arduino CLI and the repository dependencies installed

No external I2C, MIDI, audio, or PORT.A device is required for this experiment.

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

Capture Serial at 115200 baud after every flash. Reboot once after opening the monitor so the complete boot sequence is recorded.

For every variant record these exact lines:

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

Expected causal pattern:

1. **A vs B at `after-static-construction`**: B should recover roughly the 20 KiB embedded TapeFX arrays plus TapeFX object/allocator overhead.
2. **B vs C before `MiniAcid::init()`**: only a small difference is expected because the large looper buffer is not allocated yet.
3. **B vs C at `after-miniacid-init` and later**: C should recover roughly the 22050-byte looper allocation plus TapeLooper object/allocator overhead.
4. `largInt` / `largestInternal8` is the primary fragmentation signal. Total free heap alone is not sufficient.

Do not treat the approximate byte deltas as hard gates. Allocation order and allocator metadata can move the observed free-heap delta; the important result is repeatable separation at the expected lifecycle checkpoint.

## Troubleshooting

### Instrumentation reports a missing anchor

The source moved relative to the experiment base. Do not weaken the script to replace multiple matches. Rebase the experiment and update the exact anchor deliberately.

### Variant B or C resets when opening Tape UI

Expected for these diagnostic images. They remove Tape runtime ownership in the temporary source but do not redesign the Tape page. Reboot and collect only the boot/runtime memory evidence.

### Values differ substantially between repeated runs

Use the same SD card, USB profile, peripherals, scene contents, and boot procedure. Capture at least two clean boots per variant. Prefer the repeated value and inspect `[MEM-BASE]` heap-integrity output.

### `largInt` is unexpectedly much smaller although `freeInt` increased

That is a valid result and indicates fragmentation/allocation-order effects. Preserve the full checkpoint series; do not reduce the experiment to final free bytes.

## Acceptance checklist

- [ ] A, B, and C compile from the same experiment branch SHA.
- [ ] All three images boot to the normal UI without a reset during setup.
- [ ] The same Cardputer ADV, SD card, USB profile, and peripherals are used for every run.
- [ ] All five `tape-exp-*` checkpoints are captured for A, B, and C.
- [ ] `[MEM-BASE] phase=runtime-start` and at least one periodic sample are captured.
- [ ] Heap integrity remains `integrity=1`.
- [ ] A→B separation is already visible at `after-static-construction`.
- [ ] B→C large separation appears at or after `after-miniacid-init`, not before the looper allocation point.
- [ ] `largInt` / `largestInternal8` deltas are recorded alongside total free heap.
- [ ] Results are copied into the PR before any product-code Tape removal is proposed.
