# Cardputer-Adv build and stall diagnostics

## Purpose

Build and flash GroovePuter with the repository's supported Arduino toolchain, then distinguish audio underruns from UI or heap stalls.

GroovePuter is currently an Arduino project. The repository root does not contain an ESP-IDF `CMakeLists.txt`, so `idf.py build`, `idf.py flash`, and `idf.py menuconfig` are not supported entry points.

## Hardware

- M5Stack Cardputer-Adv
- USB-C data cable
- Optional microSD card for scenes, paging, and samples
- Optional 3.5 mm headphones or powered speakers

Cardputer-Adv uses Stamp-S3A / ESP32-S3FN8 with 8 MB flash and 512 KB internal SRAM. It has no PSRAM. The supported build therefore uses `PSRAM=disabled`.

## Wiring

No external wiring is required.

Internal Cardputer-Adv assumptions used by GroovePuter:

- ES8311 control I2C: GPIO8 SDA, GPIO9 SCL
- I2S BCLK: GPIO41
- I2S LRCK: GPIO43
- I2S data out: GPIO42
- amplifier enable: GPIO21
- microSD: GPIO12 CS, GPIO14 MOSI, GPIO40 CLK, GPIO39 MISO
- PORT.A / Grove I2C: GPIO2 SDA, GPIO1 SCL

## Install dependencies

From the repository root:

```bash
bash scripts/install_arduino_deps.sh
```

## Build

```bash
bash scripts/build.sh --warnings all
```

The canonical FQBN is:

```text
m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app
```

## Flash

Identify the USB serial port first:

```bash
arduino-cli board list
```

Then flash, replacing the port as needed:

```bash
PORT=/dev/ttyACM0
arduino-cli upload \
  --port "$PORT" \
  --fqbn m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app \
  .
```

Arduino CLI writes the bootloader, partition table, and application to their correct offsets. Do not pass application offset `0x10000` manually to this workflow.

Open the serial monitor:

```bash
arduino-cli monitor --port "$PORT" --config baudrate=115200
```

## Expected behavior

Boot diagnostics should report `PSRAM: 0 KB free`; this is expected because the hardware has no PSRAM.

During operation the firmware prints one line every five seconds:

```text
[PERF] audio=... peak=... underruns=... ui=...us uiPeak=...us freeInt=... largest=... dsp=.../.../.../...
```

Interpretation:

- `underruns` increasing: the audio producer or I2S output missed its deadline.
- large `uiPeak`: a display update or page construction caused a long control-plane stall.
- `largest` falling while `freeInt` remains similar: internal-heap fragmentation.
- `audio` or `peak` near or above 100%: DSP workload exceeds the 512-frame block budget.

## ESP-IDF note

`idf.py menuconfig` only configures an ESP-IDF CMake project and writes that project's `sdkconfig`. It does not modify this Arduino build. A separate Arduino-as-an-ESP-IDF-component wrapper would be required before `idf.py` could build GroovePuter.

Do not enable `CONFIG_SPIRAM` for Cardputer-Adv: Stamp-S3A has no PSRAM.

## Troubleshooting

### `CMakeLists.txt not found in project directory`

The command was run through `idf.py`. Use `scripts/build.sh` and `arduino-cli upload` instead.

### Audio pauses when pressing keys or applying a recipe

Use the latest `agent/fix-core-reliability` head. It releases the audio mutation gate before full UI redraws. Check whether `underruns` still increases.

### Pauses while changing pages

Record `uiPeak`, `freeInt`, and `largest` before and after several page changes. A high `uiPeak` with stable underruns points to page allocation or full-screen redraw rather than the DSP engine.

### Continuous pauses without interaction

Check `audio`, `peak`, and `underruns`. Avoid enabling verbose audio diagnostics during playback because serial output can itself consume the real-time budget.

## Acceptance checklist

This document does not mark the firmware accepted. It only validates the build and captures evidence for the next repair iteration.

- [ ] Arduino build succeeds with `PSRAM=disabled`.
- [ ] Firmware flashes through Arduino CLI.
- [ ] Serial boot completes without reset loop.
- [ ] `PSRAM: 0 KB free` is observed and treated as expected.
- [ ] Ten seconds of idle playback telemetry is captured.
- [ ] Ten seconds of playback while switching pages is captured.
- [ ] Ten seconds around Chicago Jack Apply is captured.
- [ ] The first interval where `underruns`, `uiPeak`, or heap values diverge is recorded.
