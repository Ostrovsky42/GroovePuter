# GroovePuter 0.9.4 — Track A: Resource Recovery

## Purpose

0.9.4 Track A is a resource-recovery / hardening release. It does not add a new user feature.
Its job is to return internal DRAM held by hidden or unavailable subsystems to the active
Cardputer ADV product while preserving audio, Sampler behavior and Scene compatibility.

Release branch start:

- `dev_0.9.4 @ 6cb6875591b2b9a0e623bc0fcbc1a0fb77ea37a5`
- that commit already contains the hardware-accepted Tape cleanup from PR #266;
- Track A adds executable regression gates and release acceptance around that behavior.

The primary recovered resource is dormant Cardputer ADV `TapeFX` state. The looper contract is:

```text
storage unavailable / allocation unavailable
-> effective TapeLooper mode = STOP
-> zero loop contribution
-> Scene mode is normalized back to STOP by MiniAcid runtime ownership
```

Never:

```text
Scene says REC/DUB/PLAY
-> no looper storage
-> silent false state
```

## Reference evidence

Pre-recovery hardware reference from #265:

| Metric | pre-recovery | accepted #266 cleanup | Delta |
|---|---:|---:|---:|
| runtime-start `free8` | 16872 | 38360 | **+21488 B** |
| `largest8` | 7668 | 21492 | **+13824 B** |
| heap integrity | 1 | 1 | OK |
| stable audio peak | ~11.9% | ~12.0% | same class |
| underruns | 0 | 0 | OK |

The #266 smoke ran for approximately 40 seconds after RESET with no watchdog, reboot or freeze.
The accepted `largest8=21492` also matched the post-SD contiguous block measured in the Tape
experiment, proving that the unavailable 22050-byte looper buffer was no longer reserved.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- microSD with the same normal GroovePuter project/content set used for the comparison smoke
- USB-C data cable
- normal Cardputer ADV profile with PSRAM disabled

No PORT.A, external I2C, MIDI or external display hardware is required for the memory smoke.
A SEQTRAK may be attached only for the existing MIDI-only regression; it is not required by Track A.

## Wiring

No wiring changes. Use the standard Cardputer ADV USB-C and microSD setup.

## Build / flash

Host Track A contract:

```bash
git checkout dev_0.9.4
bash tests/run_tape_resource_recovery_tests.sh
```

Full release validation:

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

### Screen

- normal GroovePuter UI and navigation;
- no new Tape page or Tape workflow;
- Sampler UI/workflow is unchanged by Track A.

### Audio

- TapeFX is a dry compatibility bypass on Cardputer ADV;
- normal audio output remains in the same CPU-load class as the pre-recovery build;
- underruns remain zero under the same smoke workload.

### Memory

The accepted hardware class is approximately:

```text
runtime-start free8  ~= 38.3 KiB
largest8             ~= 21.5 KiB
heap integrity       = 1
```

Do not spend the recovered memory inside Track A. It remains general runtime/Sampler headroom.

### TapeLooper unavailable-state contract

On Cardputer ADV:

- `init()` returns false for the current no-storage product policy;
- `storageReady()` is false;
- REC, DUB and PLAY requests resolve to `TapeMode::Stop`;
- stutter and DUB auto-exit cannot remain armed without storage;
- `process()` emits zero loop contribution;
- MiniAcid mirrors effective STOP back into `Scene.tape.mode`;
- non-mode Tape Scene fields remain compatible and unchanged.

### Sampler / Scene boundary

- no Sampler source file is part of Track A;
- sampler pool policy and voice/render behavior are unchanged;
- Scene layout/schema is unchanged;
- legacy Tape mode values remain decodable; only an unavailable runtime mode is normalized to STOP.

## Automated regression gate

`tests/run_tape_resource_recovery_tests.sh` performs two checks:

1. source ownership guards verify that Cardputer ADV TapeFX remains buffer-free, Cardputer ADV
   TapeLooper init performs no sample-buffer allocation, and MiniAcid retains effective-mode mirroring;
2. an executable C++ host test compiles the Cardputer ADV policy and verifies TapeFX bypass size,
   unavailable looper STOP behavior, zero loop output and preservation of all non-mode Tape Scene fields.

The dedicated GitHub Actions workflow is `.github/workflows/tape-resource-recovery.yml`.

## Troubleshooting

### `free8` returns near 16-17 KiB

Confirm the flashed SHA descends from `6cb6875591b2b9a0e623bc0fcbc1a0fb77ea37a5` and that the
Cardputer build defines `ARDUINO_M5STACK_CARDPUTER`. Do not shrink Sampler or realtime audio buffers
to compensate; first verify that the full TapeFX object was not reintroduced.

### `largest8` falls back near 7-8 KiB

Inspect early allocations and confirm that no TapeLooper sample buffer is reserved. Track A must not
add lazy or reduced-duration looper allocation as a workaround.

### Scene appears to stay in REC/DUB/PLAY

Check, in order:

1. `TapeLooper::storageReady()`;
2. effective `TapeLooper::mode()`;
3. MiniAcid mode mirroring back to `Scene.tape.mode`.

With unavailable storage, the final effective and Scene-visible state must become STOP.

### Audio changes

Treat a meaningful CPU increase, new underruns or changed non-Tape output as a regression. Track A
is a resource-recovery release, not a DSP redesign.

## Acceptance checklist

### Automated

- [ ] dedicated Tape resource-recovery workflow green;
- [ ] full host suite green;
- [ ] SDL green;
- [ ] Cardputer ADV normal compile green;
- [ ] fixed DRAM budget green;
- [ ] Cardputer ADV SEQTRAK MIDI-only compile green;
- [ ] existing Synth persistence / tonal / generation regressions green.

### Cardputer ADV

- [x] cleanup evidence: `free8 16872 -> 38360` on exact #266 accepted head;
- [x] cleanup evidence: `largest8 7668 -> 21492`;
- [x] cleanup evidence: heap integrity remains `1`;
- [x] cleanup evidence: stable audio peak remains ~11.9% -> ~12.0%;
- [x] cleanup evidence: underruns remain `0`;
- [x] cleanup evidence: no WDT/reset/freeze during ~40 s smoke;
- [x] cleanup evidence: no 22050-byte TapeLooper buffer reservation;
- [ ] final 0.9.4 candidate cold boot reaches normal UI;
- [ ] final 0.9.4 candidate remains in the accepted memory class;
- [ ] final 0.9.4 candidate has no fake Tape REC/DUB/PLAY state.

### Boundaries

- [ ] no Sampler implementation changes in Track A;
- [ ] no Scene schema/layout change;
- [ ] no Tape UI recovery;
- [ ] no TapeFX redesign;
- [ ] no TapeLooper buffer allocation policy added;
- [ ] recovered DRAM remains available to the active product.
