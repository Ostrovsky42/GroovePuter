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

This PR is an evidence snapshot. It is intentionally not a release-line product change and should remain separate from the eventual product PRs.

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
- Same microSD card for all three variants and every repeated boot
- Development computer with Arduino CLI and the repository dependencies installed

No external I2C, MIDI, audio, or PORT.A device is required for the boot-cost experiment.

For the later lazy-looper stress validation, use the same normal peripherals and project contents that represent the intended production workload, including SD/project data and MIDI if those are normally active.

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

### Expected causal delta shape

The verdict is based on the lifecycle shape of the median deltas, not only on the final free-byte number.

| Comparison | Before `MiniAcid::init()` | After `MiniAcid::init()` |
|---|---|---|
| A→B | approximately 20 KiB recovered from removing embedded `TapeFX` state | the A→B delta should persist |
| B→C | only `TapeLooper` object/ownership overhead should differ materially | approximately 22 KiB should be recovered when the looper buffer is absent |
| `largInt` / `largestInternal8` | must not degrade when memory is removed | should improve consistently with the released contiguous-memory pressure |

More explicitly:

1. **A vs B at `after-static-construction`**: B should recover roughly the 20 KiB embedded TapeFX arrays plus TapeFX object/allocator overhead.
2. **B vs C before `MiniAcid::init()`**: only a small difference is expected because the large looper buffer is not allocated yet.
3. **B vs C at `after-miniacid-init` and later**: C should recover roughly the 22050-byte looper allocation plus TapeLooper object/allocator overhead.
4. `largInt` / `largestInternal8` is the primary fragmentation signal. Total free heap alone is not sufficient.

Do not treat the approximate byte deltas as hard gates. Allocation order and allocator metadata can move the observed free-heap delta; the important result is repeatable separation at the expected lifecycle checkpoint across the three cold boots.

## Follow-up product design if hardware evidence confirms the hypothesis

Keep the product changes separate from this evidence branch.

### Product PR 1 — remove TapeFX

If the A→B hardware median confirms approximately the expected 20 KiB cost and no product-accessible requirement is found, remove `TapeFX` completely rather than retaining a permanently disabled ADV instance.

### Product PR 2 — lazy-once TapeLooper allocation

Keep `TapeLooper`, but do not allocate its sample buffer during normal boot on DRAM-only ADV.

Required ownership policy:

- Allocate the looper buffer on the **first user activation that needs recording/loop storage**.
- Perform allocation from the control/UI side, never from the audio callback.
- Once allocation succeeds, keep the buffer for the rest of the boot session.
- Do **not** free/reallocate on REC/STOP transitions; repeated heap churn would increase fragmentation and introduce nondeterministic latency.
- If allocation fails, keep the looper in `TapeMode::Stop`, do not arm REC, preserve the running audio engine, and show a clear user-facing error such as `TAPE: NO MEMORY`.

## Lazy-looper late-allocation stress validation

Boot-time memory savings alone are not sufficient evidence that lazy allocation is safe.

When the lazy-once product implementation exists, validate allocation at the worst realistic point in the session:

1. Cold boot the normal production image.
2. Mount/read the normal SD card.
3. Load a representative project/scene and its normal project data.
4. Let the full UI initialize.
5. Start the normal audio task and transport.
6. Start the normal MIDI runtime used on Cardputer ADV.
7. Exercise the application enough for expected lazy pages/runtime services to be created.
8. Record `freeInt`, `largInt`, `free8`, `larg8`, and `largestInternal8` immediately before first Tape activation.
9. Trigger first Tape recording allocation from the UI/control path.
10. Record the same metrics immediately after allocation.
11. Confirm allocation succeeds without audio underruns, reset, heap corruption, or unintended transport state changes.
12. STOP and REC again and confirm there is no second looper-buffer allocation and no corresponding heap drop.

The decisive metric here is whether a sufficiently large contiguous internal block still exists when the user first asks for Tape, not merely whether boot has more free bytes.

## Troubleshooting

### Instrumentation reports a missing anchor

The source moved relative to the experiment base. Do not weaken the script to replace multiple matches. Rebase the experiment and update the exact anchor deliberately.

### Variant B or C resets when opening Tape UI

Expected for these diagnostic images. They remove Tape runtime ownership in the temporary source but do not redesign the Tape page. Reboot and collect only the boot/runtime memory evidence.

### Values differ substantially between repeated runs

Use the same SD card, USB profile, peripherals, scene contents, and boot procedure. Preserve all three cold-boot measurements. Compare medians and inspect the complete checkpoint series plus `[MEM-BASE]` heap-integrity output rather than discarding an inconvenient run.

### `largInt` is unexpectedly much smaller although `freeInt` increased

That is a valid result and indicates fragmentation/allocation-order effects. Preserve the full checkpoint series; do not reduce the experiment to final free bytes.

### Future lazy allocation fails although boot memory improved

Treat that as a product-design failure, not as an acceptable limitation of the experiment. The looper must either acquire its one-time buffer safely after normal runtime initialization or fail cleanly with Tape remaining stopped and the rest of GroovePuter unaffected.

## Acceptance checklist

- [ ] A, B, and C compile from the same experiment branch SHA.
- [ ] CI matrix A/B/C is green on that exact SHA.
- [ ] All three images boot to the normal UI without a reset during setup.
- [ ] The same Cardputer ADV, SD card, USB profile, peripherals, scene/project contents, and physical setup are used for every run.
- [ ] Three complete cold boots are captured for A, three for B, and three for C.
- [ ] All raw measurements are preserved; medians are calculated from all three boots per variant.
- [ ] All five `tape-exp-*` checkpoints are captured for every boot.
- [ ] `[MEM-BASE] phase=runtime-start` and at least one periodic sample are captured for every boot.
- [ ] Heap integrity remains `integrity=1`.
- [ ] A→B separation is already visible at `after-static-construction` and remains later in boot.
- [ ] B→C shows only small pre-init ownership overhead before `MiniAcid::init()`.
- [ ] B→C large separation appears at or after `after-miniacid-init`, consistent with removal of the looper buffer allocation.
- [ ] `largInt` / `largestInternal8` deltas are recorded alongside total free heap and do not regress when Tape memory is removed.
- [ ] Hardware results and median deltas are copied into PR #263 before any product-code Tape removal is proposed.
- [ ] PR #263 remains an evidence snapshot rather than being merged into the release line.
- [ ] Any later lazy-once looper PR is validated after SD, UI, project load, audio, and MIDI initialization at first real Tape activation.
- [ ] Any lazy allocation failure leaves Tape stopped, reports a clear error, and does not damage audio/runtime state.
