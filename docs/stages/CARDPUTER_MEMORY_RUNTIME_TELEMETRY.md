# Cardputer runtime memory telemetry

This note supersedes the field-name examples in the earlier runtime section of
`CARDPUTER_MEMORY_BASELINE.md`. The older `loopStackWords` and
`audioStackWords` names were incorrect for ESP-IDF: the raw
`uxTaskGetStackHighWaterMark()` value is already a minimum free-stack value in
bytes. No multiplication is applied or permitted.

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
`xTaskGetHandle()` name lookup or alter the product source/ELF. A zero watermark
is interpreted only together with the corresponding `TaskPresent` field.

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

The current product inventory contains these candidates outside GroovePuter's
MIDI product logic:

```text
ncm_epbuf       6416 B
_mscd_epbuf     4096 B
_dfu_epbuf      4096 B
```

They are not removed in PR #70. Product builds now print linker-map evidence and
generated/installed configuration evidence for `CFG_TUD_*` and
`CONFIG_TINYUSB_*` controls. A class can be disabled only after all of the
following are proven:

1. the exact controlling compile-time option is identified for the pinned
   M5Stack core;
2. a product ELF proves the expected bytes disappeared;
3. normal CDC+MIDI still enumerates and transfers MIDI;
4. MIDI-only still enumerates and transfers MIDI;
5. upload/recovery behavior required by Cardputer is unchanged.

This is a lower-risk optimization candidate than moving Scene storage, but it is
still a separate configuration experiment and not part of the runtime-baseline
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
