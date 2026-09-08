# MIDI I/O and nanoKEY2 — evidence log

This log records observed results, not promises of support. The normative
behavior is [the MIDI I/O contracts](2026-09-07-midi-io-contracts.md).

## Baseline — 2026-09-07 / 2026-09-08

Worktree: `integration/20260908-pattern-phrase-instrument-final`.
Base commit: `98452ea465151334d2842da79e7c42b162bceb44`.

| Check | Result | Scope |
|---|---|---|
| `bash tests/run_host_tests.sh` | PASS | Full host suite; existing C++20/type-limit warnings remain |
| `tests/test_cardputer_runtime_diagnostics` | PASS | Runtime diagnostics RTC/noinit budget (sizeof <= 256 B) |
| `python3 tests/test_cardputer_memory_baseline_source_regressions.py` | PASS | Baseline script contracts |
| Memory runtime build | PASS | normal/runtime; fixed DRAM gate <= 191488 B |
| RX2 qualitative USB receive | PASS, user reported | Cardputer + USB-C adapter + nanoKEY2 |
| nanoKEY2 identity | Observed | VID:PID `0944:0115`, IN `0x81`, OUT `0x02` |
| nanoKEY2 → PERFORM → UART → SEQTRAK | Not tested | Pending M1/M2 closure |

## Таблица владельцев памяти (M0 Memory Owners Registry)

Измерения проведены по контрольным точкам boot и runtime на Cardputer ADV (DRAM-only, без PSRAM).

| Владелец | static/heap/stack | bytes (approx/measured) | caps | момент alloc | момент free | обязателен |
|---|---|---:|---|---|---|---|
| `M5Cardputer.begin` | heap | ~32 000 | 8BIT | AfterM5Init | never | yes |
| Display (framebuffer/DMA) | heap | 65 540 | DMA / INTERNAL | AfterDisplayInit | never | yes |
| `AudioTask` (8192 stack + TCB) | stack / heap | 9 080 | DEFAULT | AfterAudioTaskStart | never | yes |
| `TempoDelay` (2x 8.6 KB) | heap | 18 704 | 8BIT | AfterDspDelaysInit | never | yes |
| SD mount & FATFS buffers | heap | 30 144 | 8BIT | AfterSdMount | AfterSdUmount | yes |
| SMF runtime (lazy) | heap / stack | 9 428 (6144 stack + queues) | 8BIT | AfterSmfBegin (deferred) | smfStop / deferred | conditional |
| USB MIDI Dispatch Task | static (BSS) | 4 096 (stack) + structs | STATIC (BSS) | boot | never | yes |
| Engine scene load & Sample index | heap | 3 200 | 8BIT | AfterSceneLoad | scene unload | yes |
| UI active page | heap | 1 200 – 1 500 | 8BIT | AfterFirstUiFrame | page navigation | yes |
| UI Draw / HUD transient | stack (Loop) | ~2 000 – 4 000 | LOOP STACK | UI render | return to loop | yes |

ELF-статика (.dram0.data + .dram0.bss) и heap-runtime разделены и не учитываются дважды.

## M0 Main firmware — cold boot observation

The diagnostic main firmware is flashed with FQBN:
`m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`.

Values below are `freeInt / largInt` for `INTERNAL|8BIT`:

| Boundary / Phase | Free / largest (B) | Delta free from preceding point | Interpretation |
|---|---:|---:|---|
| baseline / hardware I2S | 137972 / 73716 | — | baseline after hardware and direct I2S |
| `AfterM5Init` | ~137972 / 73716 | ~0 | M5 init complete |
| `AfterDisplayInit` | 72432 / 31732 | -65540 | Display buffer is largest resident consumer |
| `AfterAudioTaskStart` | 63352 / 31732 | -9080 | Real-time audio task stack (8 KiB) + TCB |
| `AfterDspDelaysInit` | 44648 / 31732 | -18704 | Preallocated delay buffers (2x 8.6 KB) |
| `AfterSdMount` | 14504 / 7668 | -30144 | SD/FATFS runtime buffers sharply reduce contiguous heap |
| `AfterSmfBegin` | 5076 / 2292 | -9428 | SMF eager begin (when non-lazy); with lazy init: deferred |
| `AfterSceneLoad` | ~1876 / not logged | -3200 | Scene JSON deserialization + sample index scan |
| `AfterFirstUiFrame` | ~1640 / not logged | -236 | First UI frame rendered, initial page instantiated |

## Diagnostic Output Format

`CardputerRuntimeDiagnostics::reportFromControlTask()` emits:
```
[DIAG] 8BIT  free=NNNN min=NNNN largest=NNNN
[DIAG] DEF   free=NNNN min=NNNN largest=NNNN
[DIAG] DMA   free=NNNN min=NNNN largest=NNNN
[DIAG] STK   Loop=NNNN Audio=NNNN Smf=NNNN Midi=NNNN  (hwm)
[DIAG] PHASE <имя фазы> seq=NNNN
```
