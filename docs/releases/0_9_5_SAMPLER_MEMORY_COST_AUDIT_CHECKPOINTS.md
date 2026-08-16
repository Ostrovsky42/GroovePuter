# 0.9.5 Sampler Memory Audit — Hardware Checkpoints

This companion sheet is intentionally measurement-only. It exists so Cardputer ADV serial captures use the same checkpoint names across runs without changing sampler behavior.

## Target

```text
M5Stack Cardputer ADV
ESP32-S3
no PSRAM production profile
built-in microSD
22.05 kHz audio
512-frame audio block
```

## Required serial values

The existing Cardputer helper prints:

```text
[tag] freeInt=<bytes> largInt=<bytes> free8=<bytes> larg8=<bytes>
```

For S1, preserve the raw serial line. Do not convert only to KiB because allocator-level comparisons need byte precision.

## Capture order

Use one exact firmware SHA and one unchanged SD card for a complete run.

```text
01 setup-entry/post-global-construction
02 after-direct-i2s
03 after-audio-task
04 after-critical-dsp-buffers
05 after-sd
06 after-smf
07 after-sampler-store-bind
08 before-sample-catalog
09 after-sample-catalog-and-registry
10 after-ui-allocation
11 after-first-ui-draw
12 first-open-samples
13 after-leaving-samples
14 after-small-preload
15 after-eviction
16 after-repeated-browse-select-cycles
```

## Result row format

Copy each result as one row:

```text
SHA=<40-char sha> checkpoint=<name> freeInt=<bytes> largInt=<bytes> free8=<bytes> larg8=<bytes> underruns=<count> reset=<value-or-none>
```

## Comparison discipline

Do not splice checkpoints from different firmware SHAs into one timeline.

Keep these states separate:

```text
production-base control: 93eaaa44abf99248566e1ff78f9cef64b0581e4f
hardware-tested #283:    13e754892cd1848f75dc09c64e534a0d70df4464
S1 probe head:            record exact tested S1 SHA
```

## What each delta means

- setup -> audio/DSP: fixed realtime reservation cost;
- before catalog -> after catalog: catalog plus registry ownership;
- UI allocation/draw: UI fixed/lazy ownership;
- open -> leave SAMPLES: sampler page retention versus release;
- preload -> eviction: PCM allocation recovery and fragmentation;
- repeated browse/select cycles: monotonic allocator fragmentation, if present.

## Pass condition for S1 measurement

S1 hardware measurement is complete when all relevant rows have exact byte values, underrun/reset state is recorded, and no optimization is introduced between checkpoints.
