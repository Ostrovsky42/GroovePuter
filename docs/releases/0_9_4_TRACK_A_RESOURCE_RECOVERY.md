# GroovePuter 0.9.4 — ADV Resource Recovery

## Purpose

0.9.4 is a narrow Cardputer ADV resource-recovery / hardening release. It adds no product feature and spends none of the recovered memory.

Frozen 0.9.3 parent:

```text
eab4f7acfed9e5e2e2385eba636ce994e7fcfa3f
```

Hardware-test branch:

```text
release/0.9.4-adv-resource-recovery-candidate
```

The candidate must differ from the frozen 0.9.3 parent only by the Track A protection files:

```text
.github/workflows/tape-resource-recovery.yml
tests/run_tape_resource_recovery_tests.sh
tests/test_tape_resource_recovery_source_regressions.py
tests/test_tape_unavailable_contract.cpp
docs/releases/0_9_4_TRACK_A_RESOURCE_RECOVERY.md
```

There must be no 0.9.4 integration changes under `src/`, `scenes.*`, the audio graph, sampler runtime, Tape runtime or UI.

The Tape production cleanup itself is already inherited through the frozen parent. Track A makes its contracts permanent:

```text
TapeFX resident ADV buffers remain reclaimed
TapeLooper storage unavailable -> effective STOP
REC/DUB/PLAY without storage cannot remain visible runtime state
recovered RAM remains headroom
```

## Reference evidence

Pre-Tape-recovery hardware reference:

| Metric | pre-recovery | accepted Tape cleanup | Delta |
|---|---:|---:|---:|
| runtime-start `free8` | 16872 | 38360 | +21488 B |
| `largest8` | 7668 | 21492 | +13824 B |
| heap integrity | 1 | 1 | OK |
| stable audio peak | ~11.9% | ~12.0% | same class |
| underruns | 0 | 0 | OK |

The original cleanup smoke ran for approximately 40 seconds after RESET without watchdog, reboot or freeze. The recovered memory is not a 0.9.4 allocation budget: it remains safety headroom.

Frozen 0.9.3 sampler evidence at the release parent includes fixed DRAM `176792 / 191488`, removal of the one-block sampler trigger delay, sampler hardware PERF peak about 50.3%, and underruns 0. The final 0.9.4 smoke must show no regression relative to that frozen sampler behavior.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- microSD with the same normal GroovePuter content used for the 0.9.3 acceptance
- USB-C data cable
- normal Cardputer ADV no-PSRAM profile
- Yamaha SEQTRAK only if repeating the external MIDI smoke; it is not required for the local memory/audio smoke

## Wiring

No new wiring is required. Use the normal Cardputer ADV USB-C and microSD configuration.

PORT.A / I2C is not used by this test.

## Build / flash

Checkout the exact candidate branch and record its immutable SHA:

```bash
git fetch origin
git checkout release/0.9.4-adv-resource-recovery-candidate
git pull --ff-only
git rev-parse HEAD
```

The parent relationship must be exact:

```bash
git rev-parse HEAD^
# expected: eab4f7acfed9e5e2e2385eba636ce994e7fcfa3f
```

The release diff must contain only the five Track A files:

```bash
git diff --name-only eab4f7acfed9e5e2e2385eba636ce994e7fcfa3f..HEAD
```

Run the dedicated Tape contract:

```bash
bash tests/run_tape_resource_recovery_tests.sh
```

Run the frozen sampler contracts that exist in the 0.9.3 parent:

```bash
bash tests/run_sampler_ref_tests.sh
bash tests/run_sampler_registry_boot_tests.sh
```

Then run the normal software matrix:

```bash
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash the same exact SHA:

```bash
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

### Screen

- cold boot reaches the normal GroovePuter UI;
- normal navigation remains unchanged;
- no Tape page or new Tape workflow appears;
- existing sampler workflow remains unchanged.

### Tape contract

On Cardputer ADV:

- `TapeFX` is the existing dry compatibility bypass and does not regain resident DSP buffers;
- `TapeLooper::init()` remains unavailable under the current no-storage policy;
- `storageReady()` remains false;
- requested REC, DUB or PLAY resolves to `TapeMode::Stop`;
- stutter and DUB auto-exit cannot stay armed without storage;
- unavailable TapeLooper contributes zero loop audio;
- MiniAcid mirrors effective STOP back into Scene mode;
- non-mode Tape Scene fields are preserved.

Do not exercise or recover Tape as a product feature during the 0.9.4 smoke.

### Sampler

The frozen 0.9.3 sampler behavior must remain unchanged:

- short `Alt+K` sampler smoke works;
- no audible one-block trigger delay returns;
- no double-trigger appears;
- pitch/reverse/loop/choke behavior is not changed by 0.9.4;
- the 32 KiB sampler pool policy is unchanged;
- no new sampler allocation is introduced by the Track A overlay.

### Runtime memory / audio smoke

After a cold boot, record these Serial values from the exact candidate SHA:

```text
freeInt=
largestInt=
free8=
largest8=
heap integrity=
audio peak=
underruns=
```

Then run 5–10 minutes of ordinary PLAY using:

- internal drums;
- Synth A and Synth B;
- Pattern and Song playback;
- a short sampler smoke through `Alt+K`;
- Stop -> Play transitions.

Do not invoke any Tape function.

The acceptance question is not whether every heap value exactly matches the old #266 smoke. The frozen 0.9.3 sampler line has evolved since that measurement. The required invariant is that the Track A-only overlay does not add product allocations and does not collapse the recovered contiguous-memory class.

## Automated regression gate

`tests/run_tape_resource_recovery_tests.sh` verifies two layers:

1. source ownership guards: ADV TapeFX remains buffer-free, ADV TapeLooper init performs no storage allocation, and MiniAcid retains requested-mode -> effective-mode -> Scene mirroring;
2. executable C++ ADV policy: TapeFX stays a tiny bypass object, unavailable looper modes resolve to STOP, loop output is zero, and non-mode Tape Scene state is preserved.

GitHub Actions entrypoint:

```text
.github/workflows/tape-resource-recovery.yml
```

## Troubleshooting

### Candidate diff contains `src/`, Scene, sampler runtime or UI files

Reject the candidate. 0.9.4 integration must be Track A-only. Fix the branch ancestry/diff instead of reviewing those product changes as part of this release.

### Tape resource test fails

Do not weaken the test. Check whether ADV TapeFX regained resident buffers, TapeLooper attempts storage allocation, or MiniAcid stopped mirroring the effective mode.

### `largest8` collapses toward the old 7–8 KiB class

Treat it as a release blocker. Inspect allocations and confirm that no Tape buffers were reintroduced. Do not shrink sampler/audio buffers to hide the regression.

### Sampler timing or behavior changes

Treat it as a release blocker. Because 0.9.4 contains no sampler implementation diff, first verify the flashed SHA and compare it directly with the frozen `eab4f7ac...` parent.

### Underruns grow during the smoke

Capture the audio PERF line and the workload. The Track A overlay has no audio-path implementation diff, so systematic growth indicates an integration/build mismatch or an inherited 0.9.3 issue that must be understood before freeze.

## Acceptance checklist

### Exact-SHA software matrix

- [ ] candidate parent is exactly `eab4f7acfed9e5e2e2385eba636ce994e7fcfa3f`;
- [ ] diff contains exactly the five Track A files listed above;
- [ ] dedicated Tape resource-recovery contract green;
- [ ] sampler SampleRef regression green;
- [ ] sampler registry/boot regression green;
- [ ] full Core/host suite green;
- [ ] SDL green;
- [ ] Cardputer ADV normal compile green;
- [ ] fixed DRAM budget green;
- [ ] Cardputer ADV SEQTRAK MIDI-only compile green;
- [ ] existing Synth persistence / tonal / generation workflows green.

### Cardputer ADV hardware smoke

- [ ] same exact candidate SHA flashed;
- [ ] cold boot reaches normal UI;
- [ ] `freeInt`, `largestInt`, `free8`, `largest8` recorded;
- [ ] heap integrity OK;
- [ ] 5–10 minute PLAY smoke completed;
- [ ] internal drums OK;
- [ ] Synth A/B OK;
- [ ] Pattern/Song OK;
- [ ] short `Alt+K` sampler smoke OK;
- [ ] Stop/Play OK;
- [ ] sampler timing did not regress;
- [ ] WDT/reset count = 0;
- [ ] underruns = 0 or show no systematic growth;
- [ ] Tape remains unavailable and was not recovered as a feature;
- [ ] recovered RAM was not allocated back to another subsystem.

### Freeze

After CLEAN acceptance, merge this Track A-only candidate to `dev_0.9.4`, record the resulting immutable SHA as `0.9.4 FINAL`, and stop product work on the release line. New Output Ownership work starts from that frozen 0.9.4 SHA.
