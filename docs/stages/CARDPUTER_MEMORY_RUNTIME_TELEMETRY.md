# Cardputer runtime memory telemetry

This note supersedes the field-name examples in the earlier runtime section of
`CARDPUTER_MEMORY_BASELINE.md`. The older `loopStackWords` and
`audioStackWords` names were incorrect for ESP-IDF: the raw
`uxTaskGetStackHighWaterMark()` value is already a minimum free-stack value in
bytes. No multiplication is applied or permitted.

Full `.dram0.bss` attribution remains documented in the baseline report; this
note defines the separate runtime measurement contract.

## Runtime fields

The diagnostic image emits two records at the same timestamp.

```text
[MEM-BASE]
  free8
  minFree8Boot
  minFree8RuntimeSample
  largest8
  minLargest8RuntimeSample
  freeInternal8
  minFreeInternal8Boot
  minFreeInternal8RuntimeSample
  largestInternal8
  minLargestInternal8RuntimeSample
  integrity

[MEM-STACK]
  loopStackFreeBytes
  audioStackFreeBytes
  smfStackFreeBytes
  dispatchStackFreeBytes
  smfTaskPresent
  dispatchTaskPresent
```

The task watermarks are minimum free bytes since each task started, not peak
used bytes. Peak used stack is `configuredBytes - stackFreeBytes`. Safety is
assessed independently per task; stack reserves are never summed into one
interchangeable pool.

The temporary runtime source injects direct runtime-only accessors for the
`SmfPlayerTask` and `MidiDispatchTask` handles. It does not rely on optional
task-name lookup, import the TinyUSB transport header into the main sketch, or
alter the checked-out product source. The runtime diagnostic ELF necessarily
contains the probes; the separate product ELF remains uninstrumented. A zero
watermark is interpreted only together with the corresponding `TaskPresent`
field.

The first completed firmware-bearing record with this telemetry is
`59c4ec36d5a1f6b80a0fd5d387b3fb8557cab98f`: all four product/runtime profile
builds and all Core regressions passed. Each later PR head is still rebuilt and
must be green before it is used as the recorded hardware source SHA.

## Capability separation

`MALLOC_CAP_8BIT` and `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` are recorded
separately. A contiguous-allocation requirement must name the exact capability
used by the real allocator path. No 30 KiB Scene gate is assumed for the current
product because both Scene objects remain statically resident in `.dram0.bss`.

The sampled minima begin when setup completes. They are lifetime diagnostic
minima, not an automatically certified stable baseline. The hardware procedure
must first warm UI pages, SD, audio, MIDI services, and lazy tasks, then select a
stable 5–10 second idle window from the log as the runtime baseline.

## TinyUSB class-buffer investigation

The current normal and MIDI-only product linker maps both attribute these
objects to the pinned prebuilt TinyUSB archive:

```text
_mscd_epbuf  4096 B  libarduino_tinyusb.a(msc_device.c.obj)
_dfu_epbuf   4096 B  libarduino_tinyusb.a(dfu_device.c.obj)
ncm_epbuf    6416 B  libarduino_tinyusb.a(ncm_device.c.obj)
                         total: 14608 B
```

The pinned M5Stack ESP32 Arduino libs configuration contains:

```text
CONFIG_TINYUSB_MSC_ENABLED=y
CONFIG_TINYUSB_MSC_BUFSIZE=4096
CONFIG_TINYUSB_DFU_RT_ENABLED=y
CONFIG_TINYUSB_DFU_ENABLED=y
CONFIG_TINYUSB_DFU_BUFSIZE=4096
CONFIG_TINYUSB_NCM_ENABLED=y
```

Its TinyUSB configuration maps these values to `CFG_TUD_MSC`,
`CFG_TUD_DFU_RUNTIME`, `CFG_TUD_DFU`, `CFG_TUD_NCM`,
`CFG_TUD_MSC_BUFSIZE`, and `CFG_TUD_DFU_XFER_BUFSIZE`.

This confirms that the candidate bytes are real and compiled into the prebuilt
core archive in both product profiles. It also means that removing them is not
assumed to be a sketch-only or ordinary FQBN toggle: the likely experiment is a
custom/rebuilt Arduino core or SDK configuration with those classes disabled.
That conclusion must be verified by an isolated build experiment.

They are not removed in PR #70. A class can be disabled only after all of the
following are proven:

1. the exact controlling build configuration is changed in a reproducible core;
2. a product ELF proves the expected bytes disappeared;
3. normal CDC+MIDI still enumerates and transfers MIDI;
4. MIDI-only still enumerates and transfers MIDI;
5. upload/recovery behavior required by Cardputer is unchanged.

This is a lower-risk optimization candidate than moving Scene storage, but it is
still a separate toolchain/configuration PR and not part of the runtime-baseline
measurement itself.

## Current boundary audit

PR #70 remains limited to CI, documentation, reporting, and temporary runtime
instrumentation:

```text
.github/workflows/cardputer-memory-baseline.yml
docs/stages/CARDPUTER_MEMORY_BASELINE.md
docs/stages/CARDPUTER_MEMORY_RUNTIME_TELEMETRY.md
scripts/build_cardputer_memory_baseline.sh
scripts/check_cardputer_dram_budget.sh
scripts/instrument_cardputer_memory_runtime.py
scripts/report_cardputer_memory_baseline.sh
scripts/report_cardputer_tinyusb_class_buffers.sh
tests/test_cardputer_memory_baseline_source_regressions.py
```

No production C/C++ implementation file is changed.
