# GVEP R0.1 Memory Feasibility — Cardputer ADV

## Purpose

Determine whether ESP-NOW can coexist with the current GroovePuter Cardputer ADV runtime when all realtime-critical allocations are reserved first.

R0 failed when Wi-Fi/ESP-NOW was initialized before `AudioTask`: internal free heap fell to about 10 KB and `xTaskCreatePinnedToCore()` could not create the 8192-byte AudioTask stack. R0.1 changes only the allocation order for the experiment: the transport-neutral GVEP session initializes during `setup()`, while Wi-Fi/ESP-NOW starts from the first normal `loop()` call after `setup()` has completed.

This is a hardware feasibility test, not production approval. Normal builds keep ESP-NOW disabled.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3.
- USB-C data cable.
- Headphones/speaker used for normal GroovePuter audio acceptance.
- Optional ESP32-family ESP-NOW receiver running the existing GVEP receiver firmware.
- Optional Yamaha SEQTRAK for the final MIDI/clock smoke test.

## Wiring

No GPIO wiring is required for the ESP-NOW test.

Cardputer ADV PORT.A remains unchanged: GPIO2 = SDA and GPIO1 = SCL. Do not move GVEP onto `Wire1` and do not repurpose PORT.A for this experiment.

## Build / flash steps

Checkout the research branch:

```bash
git switch agent/20260810-01-visual-event-protocol-research
git pull --ff-only
```

Run the focused host/source gate first:

```bash
./tests/run_output_ownership_tests.sh
```

Build the experimental firmware. Both defines are required; normal builds must omit them:

```bash
BUILD_PATH="$PWD/build/gvep-r01" \
EXTRA_CPP_FLAGS="-DGROOVEPUTER_ENABLE_DUAL_EYE_ESPNOW=1 -DGROOVEPUTER_GVEP_R01_MEMORY_PROBE=1" \
bash scripts/build.sh
```

Verify that the normal static DRAM ceiling was not weakened:

```bash
bash scripts/check_cardputer_dram_budget.sh \
  build/gvep-r01/GroovePuter.ino.elf
```

Upload to the Cardputer ADV:

```bash
BUILD_PATH="$PWD/build/gvep-r01" \
bash scripts/upload.sh /dev/ttyACM0
```

Monitor serial at 115200 baud with your normal serial monitor.

Do not use the R0.1 defines for a production build.

## Expected behavior

Boot must reach:

```text
[DEBUG] AudioTask created successful, handle: ...
setup() complete
```

Only after `setup()` completes, the first normal loop iteration may initialize Wi-Fi/ESP-NOW. With the probe enabled, serial prints a one-time radio snapshot and then a runtime snapshot approximately every five seconds:

```text
[GVEP-R01] phase=radio-init radio=1 freeInt=... largestInt=... audioHwmBytes=... beforeFree=... beforeLargest=... afterFree=... afterLargest=... initFailures=0 queueDropped=0
[GVEP-R01] phase=runtime radio=1 freeInt=... largestInt=... audioHwmBytes=... beforeFree=... beforeLargest=... afterFree=... afterLargest=... initFailures=0 queueDropped=0
```

`audioHwmBytes` is the minimum lifetime free stack space reported by ESP-IDF for the existing 8192-byte AudioTask stack. On ESP32-S3 this value is in bytes.

A radio-init failure is fail-soft. The expected failure form is:

```text
[GVEP-R01] phase=radio-init radio=0 ... initFailures=1 ...
```

GroovePuter must continue running audio/UI even if the radio cannot initialize.

## Test procedure

1. Capture the first `radio-init` line.
2. Let GroovePuter idle for at least two minutes and capture a `runtime` line.
3. Run normal playback with dense drums, Synth A/B and normal FX for at least ten minutes.
4. Exercise the heaviest normal workflows available in this branch: Song/pattern changes, generation, UI navigation and MIDI routing.
5. If SEQTRAK is available, run the normal clock/transport smoke test while GVEP remains enabled.
6. Record the minimum observed `audioHwmBytes`, `freeInt` and `largestInt`.
7. Record any `queueDropped`, audio underruns, crackle, UI stalls, resets or MIDI timing degradation.
8. If the short run is clean, continue to the existing 30-minute dense audio + visual soak required by the GVEP research brief.

Do not reduce the AudioTask stack during this test. First measure the real high-water mark under worst-case load; stack reduction is a separate experiment and requires its own safety margin.

## Decision rule

R0.1 passes only if all of the following are true:

- normal production default remains `GROOVEPUTER_ENABLE_DUAL_EYE_ESPNOW=0`;
- static Cardputer DRAM gate still passes without changing its ceiling;
- AudioTask is created before radio initialization;
- ESP-NOW reaches `radio=1` after setup;
- no reset, crackle, underrun, scheduler stall or MIDI timing regression is introduced;
- `audioHwmBytes` stays safely above zero throughout worst-case operation;
- internal heap and largest-block measurements remain sufficient for the already-reserved runtime and do not show progressive fragmentation/leakage;
- visual queue overflow, if forced, increments `queueDropped` without blocking music.

If ESP-NOW cannot initialize after all critical allocations are already reserved, or if the remaining runtime margin is unacceptably small, treat internal ESP-NOW as rejected for the current Cardputer ADV architecture and move to an external bridge or another transport. Do not weaken the existing audio or DRAM gates to make GVEP pass.

## Troubleshooting

### `radio=0`, `initFailures=1`

Record the complete `radio-init` line. Compare `beforeFree/beforeLargest` with `afterFree/afterLargest`. This means the radio could not become ready after the production runtime had already reserved its critical memory. Do not add retries in the audio path.

### AudioTask does not start

This is not the intended R0.1 order. Confirm you built the current research branch and that the radio is deferred to `eye_output_mode_flush()` rather than initialized from `eye_output_mode_init()`.

### `audioHwmBytes` becomes small

Do not immediately shrink the stack. The high-water mark is the minimum free stack seen so far, so the smallest value during the heaviest workload is the useful measurement. Keep a safety margin for paths not exercised by the test.

### `largestInt` falls while `freeInt` looks acceptable

Treat this as fragmentation pressure. Total free heap alone is not sufficient evidence that the configuration is safe.

### Visual packets are missing

Check `[GVEP-TX]` counters and `queueDropped`. Packet loss is subordinate to audio correctness; do not add synchronous retries to DSP or sequencer paths.

## Acceptance checklist

- [ ] Focused host/source gate passes.
- [ ] Experimental build uses both R0.1 defines and normal builds use neither.
- [ ] Static DRAM budget check passes unchanged.
- [ ] Serial shows AudioTask creation before the first `[GVEP-R01] radio-init` line.
- [ ] Boot reaches `setup() complete`.
- [ ] `radio=1` and `initFailures=0`, or a fail-soft `radio=0` result is captured for rejection analysis.
- [ ] `beforeFree`, `beforeLargest`, `afterFree`, and `afterLargest` are recorded.
- [ ] Minimum worst-case `audioHwmBytes` is recorded.
- [ ] Dense playback has no crackle or underruns.
- [ ] UI remains responsive.
- [ ] SEQTRAK/MIDI clock smoke remains clean when available.
- [ ] `queueDropped` remains measurable and never blocks audio.
- [ ] No progressive internal-heap leak is visible during the soak.
- [ ] Production default remains ESP-NOW OFF after the experiment.
