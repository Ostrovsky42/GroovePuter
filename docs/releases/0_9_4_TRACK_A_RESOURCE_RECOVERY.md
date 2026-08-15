# GroovePuter 0.9.4 — ADV Resource Recovery

## Purpose

0.9.4 is a narrow Cardputer ADV resource-recovery hardening release. It adds no product feature and spends none of the recovered memory.

Frozen 0.9.3 parent:

```text
55da9a183f5361f6cf9c2aa560d58c87859cd6a1
```

Release candidate branch:

```text
release/0.9.4-adv-resource-recovery-final
```

The release diff must contain exactly these five Track A files:

```text
.github/workflows/tape-resource-recovery.yml
docs/releases/0_9_4_TRACK_A_RESOURCE_RECOVERY.md
tests/run_tape_resource_recovery_tests.sh
tests/test_tape_resource_recovery_source_regressions.py
tests/test_tape_unavailable_contract.cpp
```

There must be no 0.9.4 integration changes under `src/`, Scene storage, sampler runtime, Tape runtime, UI, MIDI routing, or the audio graph.

Track A permanently protects the already accepted ADV contracts:

```text
TapeFX resident ADV buffers remain reclaimed
TapeLooper storage unavailable -> effective STOP
REC/DUB/PLAY without storage cannot remain runtime state
recovered RAM remains headroom
```

## Reference evidence

Historical Cardputer ADV Tape recovery evidence:

| Metric | pre-recovery | accepted cleanup | Delta |
|---|---:|---:|---:|
| runtime-start `free8` | 16872 | 38360 | +21488 B |
| `largest8` | 7668 | 21492 | +13824 B |
| heap integrity | 1 | 1 | OK |
| stable audio peak | ~11.9% | ~12.0% | same class |
| underruns | 0 | 0 | OK |

The recovered memory is safety headroom, not a new allocation budget.

The frozen 0.9.3 parent already contains the hardware-accepted Drum realtime and sampler workflow fixes. The final sampler UX hardware-tested head was:

```text
6b7585e35a333c6df4d17a819b328308e7b73b0e
```

After merge, the frozen 0.9.3 parent became `55da9a183f5361f6cf9c2aa560d58c87859cd6a1`. 0.9.4 must preserve that runtime behavior unchanged.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- microSD with the same GroovePuter content used for 0.9.3 acceptance
- USB-C data cable
- normal Cardputer ADV no-PSRAM profile
- Yamaha SEQTRAK only for the optional external MIDI smoke

## Wiring

No new wiring is required. Use the normal Cardputer ADV USB-C and microSD configuration.

PORT.A / I2C is not used by this test.

## Build / Flash

Checkout the exact candidate and record its immutable SHA:

```bash
git fetch origin
git checkout release/0.9.4-adv-resource-recovery-final
git pull --ff-only
git rev-parse HEAD
git rev-parse HEAD^
```

`HEAD^` must be exactly:

```text
55da9a183f5361f6cf9c2aa560d58c87859cd6a1
```

Verify the release boundary:

```bash
git diff --name-only 55da9a183f5361f6cf9c2aa560d58c87859cd6a1..HEAD
```

It must list only the five Track A files above.

Run the focused contracts:

```bash
bash tests/run_tape_resource_recovery_tests.sh
bash tests/run_sampler_ref_tests.sh
bash tests/run_sampler_registry_boot_tests.sh
```

Run the normal release matrix:

```bash
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash the same exact SHA:

```bash
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

### Screen / workflow

- cold boot reaches the normal GroovePuter UI;
- normal navigation is unchanged;
- no Tape page or newly recovered Tape workflow appears;
- DRUMS `Tab` still reaches the SAMPLES tab and returns to the main Drum grid.

### Tape contract

On Cardputer ADV:

- `TapeFX` stays a dry compatibility bypass without restored resident DSP buffers;
- `TapeLooper::init()` remains unavailable under the current no-storage policy;
- `storageReady()` remains false;
- requested REC, DUB, or PLAY resolves to `TapeMode::Stop`;
- unavailable TapeLooper contributes zero loop audio;
- MiniAcid mirrors effective STOP back into Scene mode;
- non-mode Tape Scene fields remain preserved.

Do not recover Tape as a product feature in 0.9.4.

### Sampler / DRUMS regression smoke

The inherited 0.9.3 behavior must remain unchanged:

- `DRUMS -> Tab -> SAMPLES` is reachable;
- assigned samples trigger without the recovered one-block delay;
- `M` sample-layer OFF/ON restores the assigned sampler layer state;
- `Backspace/Delete` on SAMPLE can clear one selected pad assignment;
- `Enter` previews the current sample and `Space` remains transport;
- no double-trigger appears;
- pitch/reverse/loop/choke remain unchanged;
- the 32 KiB sampler pool policy remains unchanged;
- entering DRUMS and switching 808/909/606/SP12 character does not reproduce the previously fixed realtime lag class.

### Runtime memory / audio smoke

After cold boot, record Serial values:

```text
freeInt=
largestInt=
free8=
largest8=
heap integrity=
audio peak=
underruns=
```

Run 5–10 minutes of ordinary PLAY with internal drums, Synth A/B, Pattern/Song, a short SAMPLES-tab smoke, and Stop/Play transitions.

Do not invoke any Tape function. The acceptance requirement is that this Track A-only overlay adds no product allocation and does not collapse the recovered contiguous-memory class.

## Troubleshooting

### Candidate diff contains `src/`, Scene, sampler runtime, Tape runtime, or UI files

Reject the candidate. Fix branch ancestry/diff instead of accepting product changes into 0.9.4.

### Tape resource test fails

Do not weaken the test. Check whether ADV TapeFX regained resident buffers, TapeLooper attempts storage allocation, or MiniAcid stopped mirroring effective STOP.

### `largest8` collapses toward the old 7–8 KiB class

Treat it as a release blocker. Inspect allocations and verify that no Tape buffers returned. Do not shrink sampler/audio buffers to hide the regression.

### Sampler or Drum behavior changes

Treat it as a release blocker. 0.9.4 has no runtime implementation diff, so first verify the flashed SHA and compare it directly with frozen parent `55da9a18...`.

### Underruns grow

Capture the PERF line and workload. A systematic increase must be understood before freeze because Track A itself does not alter the audio path.

## Acceptance checklist

### Exact-SHA software

- [ ] parent is exactly `55da9a183f5361f6cf9c2aa560d58c87859cd6a1`;
- [ ] diff contains exactly the five Track A files;
- [ ] no `src/` or runtime implementation changes;
- [ ] Tape resource-recovery gate green;
- [ ] sampler SampleRef gate green;
- [ ] sampler registry/boot gate green;
- [ ] full Core/host suite green;
- [ ] SDL green;
- [ ] Cardputer ADV normal compile green;
- [ ] fixed DRAM budget green;
- [ ] Cardputer ADV SEQTRAK MIDI-only compile green;
- [ ] existing Synth persistence / tonal / generation workflows green.

### Cardputer ADV hardware

- [ ] exact candidate SHA flashed;
- [ ] cold boot reaches normal UI;
- [ ] `freeInt`, `largestInt`, `free8`, `largest8` recorded;
- [ ] heap integrity OK;
- [ ] 5–10 minute PLAY smoke complete;
- [ ] internal drums OK;
- [ ] Synth A/B OK;
- [ ] Pattern/Song OK;
- [ ] DRUMS -> SAMPLES smoke OK;
- [ ] sample layer OFF/ON restores assigned state;
- [ ] Stop/Play OK;
- [ ] sampler timing did not regress;
- [ ] Drum Character switching does not reproduce the fixed lag;
- [ ] WDT/reset count = 0;
- [ ] underruns = 0 or show no systematic growth;
- [ ] Tape remains unavailable and was not recovered as a feature;
- [ ] recovered RAM remains unspent headroom.

## Freeze

After CLEAN software and hardware acceptance, merge this Track A-only candidate into `dev_0.9.4`, record the resulting immutable SHA as `0.9.4 FINAL`, and stop product work on the release line.

Output Ownership work starts from that frozen 0.9.4 SHA.
