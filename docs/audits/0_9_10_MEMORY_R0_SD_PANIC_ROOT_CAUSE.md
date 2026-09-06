# 0.9.10-MEMORY-R0-D — SD-MOUNTED PANIC ROOT-CAUSE CHARACTERIZATION

Status: **OPEN — first full panic-boundary capture required**  
Authoritative source: `feature/20260904-02-0.9.10-pattern-phrase-p3-phrase-lifetime` @ `aded0e183a934f78623030226b67b5d0b598648b`  
Production semantic delta: **NONE**

## Purpose

Determine the first proven bad operation behind the SD-mounted post-setup panic. This checkpoint must distinguish OOM/failed allocation, stack overflow, heap corruption, SD/FAT temporary allocation, sample/file activity, UI activity, MIDI/SMF activity, or another runtime path exposed by the low-memory floor.

No subsystem may be disabled in the product image merely to make the panic disappear. No DRAM ceiling change, stack reduction, allocator replacement, SD rewrite, UI redesign, SMF lifetime redesign, or P3 semantic change is authorized here.

## Prior hardware evidence — accepted context, not new R0-D proof

The current investigation begins with these already established observations:

- with SD physically present and empty scenes: `21` boots over roughly `180 s`
- reset reason reports software panic
- retained previous boot stage is `100`
- therefore `setup()` completed before the reset
- post-setup INTERNAL free heap has been observed around `700–900 B`
- removing the SD card stops the reboot loop
- autosave alone is not a sufficient explanation
- a complete Guru Meditation / exception / backtrace boundary has not yet been captured

These observations prove a strong correlation with SD presence and a very low runtime heap floor. They do **not** prove that an SD API call is the faulting instruction and do **not** prove OOM.

## Static narrowing on the authoritative head

### Idle SMF task is not continuously reading an SMF file

`SmfPlayerTask` loops every ~2 ms, but with no file loaded it handles transport-failure mailboxes, command queue, project-transport state and then delays. `scheduleAhead()` only runs for active SMF playback/arming. No file read is implied by the idle task alone.

### SMF route persistence is NVS, not SD

`MiniAcidDisplay::servicePersistence_()` calls `serviceCardputerSmfRoutePersistence()`. The Cardputer implementation uses `Preferences` and only performs work when a route-profile load/save request is pending. This path can allocate internally through NVS, but it is not an SD/FAT read/write path.

### UI session persistence is NVS, not SD

`loadCardputerUiSession()` / `saveCardputerUiSession()` also use `Preferences`. A periodic/session save may still matter under an extreme heap floor, but it must not be mislabeled as SD I/O.

### Sample scan is a setup operation

The initial `/sd/samples` scan occurs before `MiniAcidDisplay` allocation and before retained stage 100. A crash whose previous retained stage is 100 occurs after that startup scan completed. Lazy sample preload remains a possible runtime allocator/file path **only after a user action that selects/prelistens a sample**.

### UI remains an active runtime path

After setup, `loop()` repeatedly executes M5 update, LED update, performance ownership synchronization, encoder/input handling and `MiniAcidDisplay::update()` approximately every 40 ms. `MiniAcidDisplay::update()` invokes persistence servicing before drawing. These are all post-stage-100 candidates until the fault boundary is captured.

## Current causal classification

**SD presence is CORRELATED ONLY.**

Plausible mechanisms still include:

1. SD/FAT mount consumes persistent heap and leaves another later subsystem without allocation headroom.
2. A later SD/FAT operation makes a temporary allocation that fails under the low floor.
3. A non-SD runtime allocation fails only because SD/FAT residency lowered the floor.
4. A task stack overflows independently and the SD-present memory layout/timing only exposes it.
5. Heap corruption occurred earlier but is detected later.
6. A UI/NVS/MIDI/driver path performs the first bad allocation or dereference after setup.

The investigation must name the first proven bad operation before selecting among them.

## Mandatory first step — one full raw panic boundary

Use the diagnostic branch script:

```bash
python3 scripts/capture_cardputer_panic_raw.py \
  --port /dev/ttyACM0 \
  --baud 115200 \
  --output build/memory-r0-d/panic.raw.log
```

The script intentionally:

- opens the TTY once
- never reconnects it
- never toggles reset/reopen as part of monitoring
- copies every received byte immediately to a raw file
- flushes stdout as bytes arrive
- exits on disconnect/EOF/read error after preserving the received boundary

If the device enumerates under another TTY, use that exact path. Do not add an auto-reconnect wrapper around this capture.

### Capture acceptance criteria

A useful boundary contains as many of these as the panic handler exposes:

- `Guru Meditation Error` / panic text
- exception cause
- core number
- task name when available
- PC / EPC registers
- backtrace addresses
- heap/abort/assert text if present
- last retained loop stage from the next boot
- firmware ELF identity matching the diagnostic image

Do not power-cycle before saving the first panic capture unless the device does not auto-reset; the reset itself is part of the evidence.

## Backtrace symbolization

Use the **exact diagnostic ELF that produced the capture**. For every PC/backtrace address, symbolize with the installed ESP32-S3 Xtensa toolchain, for example:

```bash
xtensa-esp32s3-elf-addr2line -pfiaC \
  -e build/cardputer-recovery-diagnostics/GroovePuter.ino.elf \
  0xADDRESS1 0xADDRESS2
```

If the actual build directory/tool prefix differs, use the paths emitted by the build rather than copying this example literally. Record both raw addresses and decoded symbols in this audit.

## Retained loop-stage localization — diagnostic-only

The existing `RetainedBootStage` writes a compact stage to RTC no-init memory and is safe for panic-reset localization. Extend **only the diagnostic image** with low-cost stage writes around post-setup operations. Do not emit serial text for each loop edge.

Suggested non-overlapping stage map:

| Stage | Meaning |
|---:|---|
| 110 | loop begin |
| 111 | before `M5Cardputer.update()` |
| 112 | after `M5Cardputer.update()` |
| 113 | before LED update |
| 114 | after LED update |
| 115 | before performance ownership / transport sync |
| 116 | after performance ownership / transport sync |
| 117 | before encoder update |
| 118 | after encoder update |
| 119 | before keyboard/input processing |
| 120 | after keyboard/input processing |
| 121 | before periodic UI update/draw |
| 122 | after periodic UI update/draw |
| 123 | before persistence/file-service sub-boundary when that path actually runs |
| 124 | after persistence/file-service sub-boundary |
| 125 | loop end / before delay |
| 126 | after delay / next iteration boundary |

The exact insertion point for 123/124 must reflect a real operation. Do not create a fake “SD stage” around code that only calls NVS.

### Stage interpretation

If the next boot reports an odd `before` stage repeatedly and never its paired `after`, that narrows the failure window but does not identify a specific instruction. The raw panic PC/backtrace still has priority.

## Heap integrity and floor capture

Use the existing memory instrumentation rather than a second profiler.

At minimum, diagnostic runtime records must include:

- `free INTERNAL|8BIT`
- largest `INTERNAL|8BIT` block
- minimum free
- heap integrity
- loop-task HWM
- AudioTask HWM
- SMF-task HWM
- MIDI-dispatch HWM

Immediately before a suspected failure window, capture the values **before any verbose `Serial.printf` that could itself allocate or perturb timing**.

Do not run full heap-integrity checks on every ~5 ms loop iteration. Use a bounded diagnostic cadence and operation boundaries so the probe does not become the workload.

## Post-setup allocation trace

Audit every operation reachable after stage 100 and classify it by trigger.

| allocation / resource path | idle possible? | user input required? | page navigation? | file/storage access? | periodic / one-shot | evidence status |
|---|---|---|---|---|---|---|
| `new` / `new[]` page construction | only if current/target page missing | often | yes | no | transition/first use | SOURCE AUDIT REQUIRED |
| `std::vector` growth | depends on owner | depends | depends | often for file lists | operation-dependent | SOURCE AUDIT REQUIRED |
| `std::string` growth / formatting | yes in some UI/persistence paths | depends | depends | depends | operation-dependent | SOURCE AUDIT REQUIRED |
| `File` open / directory iteration | no generic proof | usually storage feature or persistence | can be | yes | operation-dependent | SOURCE AUDIT REQUIRED |
| JSON scene load/save/autosave | possible only when corresponding state/request is due | mutation may be prerequisite | no | yes | deferred/one-shot/retry | SOURCE AUDIT REQUIRED |
| sample lazy preload | no during untouched idle | yes | SAMPLER | yes | user-triggered | STATICALLY NARROWED |
| SMF file open/stream | no when no SMF loaded | yes/load command | MIDI PLAYER/SMF session | yes | playback session | STATICALLY NARROWED |
| SMF route persistence | only if pending route request | request-dependent | may originate from SMF UI | **NVS, not SD** | one-shot | STATICALLY NARROWED |
| UI-session persistence | possible after UI session change and due timer | prior state change | navigation may trigger | **NVS, not SD** | deferred | STATICALLY NARROWED |
| toast/help/component construction | depends on path | often | often | no | event/transition | SOURCE/HW REQUIRED |
| TinyUSB/MIDI driver work | yes | no | no | no | asynchronous | HW/PANIC TRACE REQUIRED |

The audit must be extended with concrete symbol/file/line references for any allocation path that survives the controlled idle matrix.

## Controlled hardware matrix

All rows must record exact firmware ELF identity, physical SD state, directory contents, user action script, runtime duration, panic result, retained final stage, free/largest/min-free, integrity and task HWMs.

| Variant | SD state | filesystem state | interaction | purpose | status |
|---|---|---|---|---|---|
| A | absent | N/A | none | control for post-setup stability and heap floor | PENDING |
| B | present | no scenes | default samples state | none | reproduce current correlation | PENDING |
| C | present | scenes empty, samples empty | none | remove sample-directory content from equation without firmware semantic change | PENDING |
| D | present | same as reproducing case | **no user interaction after setup** | distinguish periodic/async crash from input path | PENDING |
| E | present | same as D | navigation only; no file load/sample preload | expose page-construction/UI path | PENDING |

Run D first after obtaining one raw boundary if it reproduces the panic. A reproducible untouched-idle crash is strong evidence against user-only sample/SMF load paths.

## Decision rules

### OOM / failed allocation = YES only if

At least one of the following directly identifies allocation failure at the fault boundary:

- allocator/new failure is reported and reaches a fatal path
- exception/assert/abort symbol resolves to an allocation-failure path
- a checked allocation returns null/failure immediately before the proven bad operation and the next operation depends on it

`free heap < 1 KB` by itself is not proof.

### STACK OVERFLOW = YES only if

FreeRTOS overflow diagnostics, canary/watchpoint panic evidence, or a decoded fault boundary proves task-stack exhaustion. A low HWM is risk evidence, not sufficient by itself unless it reaches the configured safety boundary/corruption evidence.

### HEAP CORRUPTION = YES only if

Heap-integrity check fails before the panic or the panic decoder reports heap poisoning/corruption with a defensible first failing boundary.

### SD DIRECTLY CAUSAL = YES only if

The faulting PC/symbol or immediately preceding proven bad operation is in an SD/FAT/File path and controlled variants rule out a merely lower-memory floor as the explanation.

If SD presence changes the floor but the faulting operation is elsewhere, classify `CORRELATED ONLY` unless a causal allocation chain is proven.

## Root-cause ledger

| Question | Current answer | Evidence needed to change it |
|---|---|---|
| panic is OOM/failed allocation? | UNKNOWN | raw boundary + decoded allocation failure |
| stack overflow? | UNKNOWN | panic/task/canary evidence + HWM |
| heap corruption? | UNKNOWN | integrity failure / heap panic evidence |
| SD/FAT temporary allocation? | UNKNOWN | PC/path + operation-local memory trace |
| sample scanning/file activity? | initial setup scan excluded by stage 100; runtime lazy sample path still conditional | D/E matrix + trace |
| UI operation? | POSSIBLE | retained loop stage + PC/backtrace |
| another runtime path exposed by low floor? | POSSIBLE | A/B/D differential + decoded fault |

## Final status — current evidence only

### PANIC ROOT CAUSE

**UNKNOWN**

The failure is localized after `setup()` but not yet to an instruction/task/operation.

### FAULTING TASK

**UNKNOWN**

### FAULTING PC / SYMBOL

**UNKNOWN — raw panic boundary not yet captured**

### HEAP INTEGRITY BEFORE PANIC

**UNKNOWN for the immediate pre-panic boundary**

Earlier P3 characterization passed integrity checks, but that does not substitute for R0-D pre-failure evidence.

### FREE INTERNAL BEFORE PANIC

**UNKNOWN at the immediate boundary**

The prior post-setup observation of approximately `700–900 B` is context only.

### STACK OVERFLOW

**UNKNOWN**

### OOM / FAILED ALLOCATION

**UNKNOWN**

### SD DIRECTLY CAUSAL

**CORRELATED ONLY**

Removing the card stops the loop, but no faulting SD/FAT operation has yet been captured.

### FIRST PROVEN BAD OPERATION

**UNKNOWN**

### PRODUCTION CHANGE

**NONE**

A minimal root-cause fix, if later obvious, must be separately authorized after the fault is proven. It is not part of R0-D characterization.
