# Runtime panic localized: SD mount cost, not SD hardware — 2026-09-06

Closes the localization step of
[../superpowers/plans/2026-09-06-cardputer-runtime-panic-localization.md](../superpowers/plans/2026-09-06-cardputer-runtime-panic-localization.md)
and supersedes the open items in
[SD_PANIC_LOOP_WITHOUT_AUTOSAVE_2026-09-06.md](SD_PANIC_LOOP_WITHOUT_AUTOSAVE_2026-09-06.md).

## Build and boot identity

- Image: `build/cardputer-runtime-panic-diagnostics-v4`, built by
  `scripts/build_cardputer_recovery_diagnostics.sh` (`DebugLevel=error`,
  `GROOVEPUTER_RUNTIME_DIAGNOSTICS` defined).
- Static DRAM at build: `190968 B` (budget `191488`) — gate green throughout.
- Instrumentation: `src/platform/cardputer_runtime_diagnostics.{h,cpp}`
  (`c472c87d`), plus the `MALLOC_CAP_DEFAULT` snapshot added in this commit.
- Capture: 180 s window, 4 boots, three of them ending in `Reset Reason: 4`
  with `retained stage 100`.

## The evidence that survives the panic

Reproduced near-identically on `boot=1`, `boot=2` and `boot=3`:

```
[RDIAG-PREV]       boot=1 checkpoints=4479 internal8=1400/1012 integrity=1/245us
[RDIAG-PREV-TASK]  loop  seq=4436 phase=control      core=1 stackFree=23288
[RDIAG-PREV-TASK]  audio seq=4473 phase=audio-write  core=1 stackFree=6976
[RDIAG-PREV-TASK]  smf   seq=4494 phase=smf-service  core=0 stackFree=5208
[RDIAG-PREV-TASK]  midi  seq=4492 phase=midi-service core=0 stackFree=2588
[RDIAG-PREV-ALLOC] bytes=1281 caps=0x00001800 task=loop core=1 fn=0x3c11ccdd
```

Counter definitions, so these are not misread later:

- `internal8=free/largest` — `heap_caps_get_free_size` and
  `heap_caps_get_largest_free_block` over `MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT`.
- `caps=0x1800` = `MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT`.
- `fn` is **not** a code address: ESP-IDF's failed-alloc hook passes
  `const char* function_name`. `0x3c11ccdd` resolves in `.flash.rodata` to the
  string `heap_caps_malloc_prefer` — i.e. an ordinary `malloc`/`new`, not a
  DMA- or SPIRAM-specific path. It identifies the allocator entry point, **not
  the caller**.
- `seq` is a per-task monotonic checkpoint counter; the task with the *lowest*
  recent `seq` is the one that stopped making progress.

## Mechanism (proven)

The `loop` task enters `Phase::Control`, requests **1281 bytes**, and the
largest free block is **1012 bytes**. Total free (1400) exceeds the request, so
this is a fragmentation failure, not gross exhaustion. The allocation returns
`nullptr` and the panic follows.

Three independent facts rule out the theories that were live before this
capture:

- `integrity=1` on every sample, cost 245–284 µs — **the heap is not corrupt.**
- `stackFree` of 23288 / 6976 / 5208 / 2588 — **no stack overflow** in any task.
- `loop` sits ~40–60 checkpoints behind `audio`, `smf` and `midi`, which all
  kept ticking — the failure is confined to the control path, it is not a
  system-wide stall or a watchdog on a starved scheduler.

## Why the heap was that small (proven)

Same capture, the two branches observed side by side:

| | freeInt after SD | after SMF-INIT | at UI page create | outcome |
|---|---|---|---|---|
| SD mounted (`result=1 type=2`) | 14208 | 4900 | **1536** | panic loop |
| SD not mounted (`result=0 type=0`) | 44208 | 35372 | **31792** | stable |

Both branches enter the SD step with ~44 KB free. **Mounting the SD card costs
about 30 KB of internal heap** (44208 → 14208) and never returns it; SMF runtime
init then costs a further ~9.3 KB in both branches. What remains — ~1.5 KB — is
the entire budget for everything the running system does afterwards.

## The `CARD_NONE` mystery is the same bug from the other side (strong, not yet confirmed)

On the boot where SD did **not** mount, the retained failed allocation is
`bytes=29512 caps=0x1800 task=loop`, with `largest=20468` at the time, and it
recurs alongside the repeated `[SD] mount result=0` retries in the same window.
29512 B is within rounding of the ~30 000 B that a *successful* mount is
measured to consume.

Reading: **the SD mount needs one contiguous block of ~29.5 KB.** If that block
exists, the card mounts and leaves the system with ~1.5 KB, which then dies in
the control path. If fragmentation has broken the heap into pieces smaller than
that, `SD.begin()` fails and the caller sees `CARD_NONE`. Intermittency,
including a clean `ESP_RST_POWERON` that still failed, follows directly from
whatever the heap happened to look like at that moment.

Status: **strongly supported, not confirmed.** The size match, the co-occurrence
with mount retries and `task=loop` are circumstantial; the allocator hook does
not name the caller. Confirming this requires attributing the 29512 B request to
the SD/FATFS path directly. Given how many times this investigation has closed
early on less, it stays open until then.

## What this retires

- **SPI pin reset** — already disproven from source
  ([CARDPUTER_RECOVERY_REVIEW_2026-09-05.md](CARDPUTER_RECOVERY_REVIEW_2026-09-05.md)).
- **FAT/filesystem failure hidden behind `_pdrv`** — the conflation is real and
  still worth fixing as a diagnostics defect, but it is not the cause.
- **Autosave scene load** — the panic loop reproduces with `scenes/` empty; the
  ~114 KB autosave files made a bad situation worse, they did not create it.
- **Heap corruption** and **stack overflow** — both measured, both negative.

## What the DRAM budget gate did not catch

`scripts/check_cardputer_dram_budget.sh` reported `190968 / 191488` — green —
on the exact image captured dying here. The gate measures `.dram0.data +
.dram0.bss`; the failure is in runtime heap, ~30 KB of it claimed by a single
subsystem at boot. The green result was not gamed, it was **irrelevant**, and
its greenness supported a false sense that memory was under control. Any future
threshold work (`policy: provisional exception; threshold-rule items 5-7
pending`) should carry a runtime-reserve check next to the static one.

## Still open

1. Which caller requests ~29512 B during SD mount, and whether that block can be
   reduced, pooled, or reclaimed after mount.
2. Which caller requests 1281 B in `Phase::Control`, and why its result is
   dereferenced without a null check.
3. Whether the ~30 KB is genuinely retained by the mounted filesystem or is
   simply never returned — different fixes follow.
4. The runtime reserve threshold itself. Per the plan's Task 5, this must be
   derived from the measured maximum request plus capabilities, not invented as
   a round number of bytes.

No production code was changed on the strength of this capture.
