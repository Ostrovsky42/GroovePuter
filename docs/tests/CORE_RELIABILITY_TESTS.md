# Core reliability regression tests

## Purpose

Verify the first recovery fixes before flashing hardware:

- every 96-PPQN tick reaches the sequencer dispatcher;
- swing and microtiming offsets from -23 to +23 ticks are reachable;
- Cardputer ADV GPIO21 is reserved for `PA_EN`, not WS2812 output;
- reverse sampler playback with `endFrame == 0` starts at the real sample end;
- sampler handles are released after one-shot playback.

## Hardware list

No hardware is required for the host tests.

The later hardware acceptance run requires:

- M5Stack Cardputer ADV;
- USB-C data cable;
- microSD card used by GroovePuter;
- headphones or the built-in speaker.

## Wiring

No external wiring is required.

Cardputer ADV assumptions used by the tests:

- GPIO21 is power-amplifier enable (`PA_EN`);
- RGB output is disabled until a different verified LED data pin is documented;
- USB-C supplies power and serial/programming access.

## Build and run

From the repository root:

```bash
chmod +x tests/run_host_tests.sh
./tests/run_host_tests.sh
```

Requirements:

- Python 3;
- a C++17 compiler available as `g++`, or set `CXX` explicitly.

Example with Clang:

```bash
CXX=clang++ ./tests/run_host_tests.sh
```

## Expected behavior

The command exits with status 0 and prints:

```text
source regressions: OK
host regressions: OK
```

No audio device, SDL installation, Arduino libraries, or SD card is needed.

## Troubleshooting

### `g++: command not found`

Install a C++ compiler or run with an existing compiler:

```bash
CXX=clang++ ./tests/run_host_tests.sh
```

### PPQN source regression fails

Inspect the tick accumulation loop in `src/dsp/miniacid_engine.cpp`. It must call `advanceTick()` for every value consumed from `ticksToAdvance`; it must not wrap that call in `currentTick_ % 24 == 0`.

### GPIO regression fails

Do not re-enable `neopixelWrite(21, ...)`. Confirm the real Cardputer ADV LED data pin first, then update `src/platform/cardputer_adv_hardware.h` and add a hardware acceptance result.

### Sampler test fails

Check that `SamplerVoice::trigger()` resolves `endFrame == 0` using the acquired sample view before setting the reverse start position. A one-shot voice must release its handle exactly once.

## Acceptance checklist

- [ ] `tests/run_host_tests.sh` exits with status 0.
- [ ] All offsets from -23 to +23 ticks are covered.
- [ ] No literal `neopixelWrite(21, ...)` remains.
- [ ] Reverse loop starts from the last PCM frame after fade-in.
- [ ] Reverse one-shot playback releases its sample handle exactly once.
- [ ] Cardputer ADV hardware test confirms clean audio with beat/step LED modes selected, while physical RGB output remains intentionally disabled.
