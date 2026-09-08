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
| SD mount & FATFS buffers | heap | 30 144 | 8BIT | AfterSdMount | no runtime unmount in firmware | yes |
| SMF runtime (lazy) | heap / stack | 9 428 (6144 stack + queues) | 8BIT | AfterSmfBegin (deferred) | resident once started (no teardown) | conditional |
| USB MIDI Dispatch Task | static (BSS) | 4 096 (stack) + structs | STATIC (BSS) | boot | never | yes |
| USB Host stack install (M1) | heap | free(S0) − free(S1) (~4.4 KB est.) | 8BIT | usb_host_install | never (resident) | yes (Host role) |
| USB Host session & transfer (M1) | heap | free(S1) − free(S2) (~2.9 KB est.) | 8BIT / DMA | claim() & alloc | detach() | per-session |
| USB Host note reception (M1) | heap | 0 B net (free(S2) − free(S3)) | — | preallocated transfer | recycled in-flight | transient |
| `probe_ui_task` stack (M1) | stack | 4 096 + TCB (~4 236) | DEFAULT | probe setup | probe only | NO (probe-only) |
| Engine scene load & Sample index | heap | 3 200 | 8BIT | AfterSceneLoad | unverified teardown | yes |
| UI active page | heap | 1 200 – 1 500 | 8BIT | AfterFirstUiFrame | on verified page delete | yes |
| UI Draw / HUD transient | stack (Loop) | ~2 000 – 4 000 | LOOP STACK | UI render | return to loop | yes |

ELF-статика (.dram0.data + .dram0.bss) и heap-runtime разделены и не учитываются дважды.

## M1 Host Cost & Boot-Role Decision

### 1. Текущий статус готовности M0 и M1

* **Инструментация и программные контракты:** **PASS**
  - M0: `CardputerRuntimeDiagnostics` расширена (3 capability-пула: free, minFree, largestBlock + HWM всех задач); расставлены фазы setup; `[DIAG]` логгер подключён.
  - M1: `nanokey2_h0.ino` инструментирован снимками S0–S5 по трём пулам, счётчиком 20 циклов и HWM UI-задачи; `[HOST]` логгер подключён.
  - Тесты хоста: `test_cardputer_runtime_diagnostics`, `test_nanokey2_packet_diagnostics`, `test_cardputer_usb_role_runtime` — **PASS**.
  - Сборки: `scripts/build.sh` (CDC dev), `scripts/build_usb_host_midi.sh` (Host boot), `scripts/build_cardputer_memory_baseline.sh` — **PASS**.
  - DRAM static gate: 186 712 B (CDC) / 186 656 B (Host) $\le$ 191 488 B — **PASS** (+4 776 B / +4 832 B headroom).
* **Аппаратная приёмка M0:** **PENDING**
  - Требует реальных логов с физического устройства: 3 холодных старта, прогон всех страниц UI, scene load/save, запуск SMF и загрузка сэмплов.
* **Аппаратная приёмка M1:** **PENDING**
  - Требует серийного лога с реального устройства при подключённой клавиатуре nanoKEY2 с фиксацией точек S0–S5 и прогона 20 циклов attach/detach с указанием sha256 ELF-образа.

### 2. Методология расчёта стоимости Host

* **Формула расхода:** потребление вычисляется как уменьшение свободного пула: `cost = free(S_before) − free(S_after)`.
* **S0 baseline и атрибуция:** фиксируется в probe после запуска задачи `drawTask`. Так как стек задачи (4 096 B + TCB) выделяется до вызова S0, его стоимость уже отсутствует в `free(S0)`. Разность `free(S0) − free(S1)` отражает изменение всей кучи за интервал; чтобы отнести её строго к Host, требуется подтверждение отсутствия скрытых аллокаций в параллельной задаче UI.
* **Разделение resident vs per-session:**
  - `free(S0) − free(S1)`: стоимость ядра стека USB Host и регистрации клиента (резидентная память).
  - `free(S1) − free(S2)`: сеансовая стоимость перечисления устройства, claim интерфейса и аллокации `usb_transfer_t` (включая MPS-буфер 64 байта и системные структуры драйвера). Transfer выделяется один раз при подключении и циклически отправляется повторно (`inFlight`); он **не** освобождается после каждого пакета.
  - `free(S2) − free(S3)`: равенство 0 означает **отсутствие наблюдаемого изменения свободной памяти** между контрольными точками. Это не исключает кратковременных временных аллокаций внутри интервала; отсутствие динамических выделений в audio/ingress пути подтверждается анализом кода и отслеживанием `minFree`.
  - `free(S4) − free(S1)`: дельта после detach показывает полноту освобождения сеансовых ресурсов (в предварительном тесте detach вернул 2 924 B).
* **Контракт 20 циклов:**
  - `resident_bytes = free8(S5) − free8(S0)` не должен нарастать между циклами 1 и 20.
  - Возврат `largest_block` на шаге S4 подтверждает отсутствие фрагментации кучи сеансовыми структурами.

### 3. Решение по Boot-Role (Анализ Core и Feasibility)

* **Анализ Core:**
  - При `CDCOnBoot=cdc` (`ARDUINO_USB_CDC_ON_BOOT == 1`) `app_main()` вызывает `Serial.begin()` до `setup()`. Контроллер USB-OTG захватывается Device-стеком TinyUSB. В ядре ESP32-S3 деинициализация TinyUSB не поддерживается, что исключает запуск Host-режима в этой сборке.
  - При `CDCOnBoot=default` (`ARDUINO_USB_CDC_ON_BOOT == 0`) `app_main()` не инициализирует USB. Контроллер USB-OTG свободен на момент старта `setup()`.
* **Feasibility:**
  - Концепция единого бинарника с переключением роли при перезагрузке (NVS `UsbBootRole`: Off / Device / Host) **обоснована теоретически и подтверждена компиляцией**.
  - Создан компонент `CardputerUsbRoleRuntime` (`src/platform/cardputer_usb_role_runtime.{h,cpp}`).
  - Создан сборочный скрипт `scripts/build_usb_host_midi.sh` (`CDCOnBoot=default`).
  - **Ограничение:** подтверждение работы обеих ролей на реальном оборудовании без артефактов перезагрузки остаётся предметом аппаратного этапа.

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
