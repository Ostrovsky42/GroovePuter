# 0.9.10-MEMORY-R0-D — SD-MOUNTED PANIC ROOT-CAUSE CHARACTERIZATION

Status: **OPEN — first full panic-boundary capture required**  
Authoritative source: `feature/20260904-02-0.9.10-pattern-phrase-p3-phrase-lifetime` @ `aded0e183a934f78623030226b67b5d0b598648b`  
Production semantic delta: **NONE**

## R0 execution freeze

Until both hardware proofs below exist on one exact diagnostic firmware, no memory-optimization implementation is authorized:

1. one complete raw panic boundary, or a controlled untouched-idle run proving that the current reproducer is not idle-failing;
2. repeatable startup phase census plus task-stack high-watermarks on the same exact instrumentation ELF.

This explicitly blocks SMF lifetime redesign, UI arena implementation, stack-size reductions, allocator changes, DRAM-ceiling changes, feature removal and SD subsystem rewrites. `MEMORY-R1` must not start from source inspection alone.

## Purpose

Determine the first proven bad operation behind the SD-mounted post-setup panic. This checkpoint must distinguish OOM/failed allocation, stack overflow, heap corruption, SD/FAT temporary allocation, sample/file activity, UI construction/reconstruction activity, MIDI/SMF activity, or another runtime path merely exposed by the low-memory floor.

No subsystem may be disabled in the product image merely to make the panic disappear.

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

The initial `/sd/samples` scan occurs before `MiniAcidDisplay` allocation and before retained stage 100. A crash whose previous retained stage is 100 occurs after that startup scan completed. Lazy sample preload remains a possible runtime allocator/file path only after a user action that selects or prelistens a sample.

### UI remains an active runtime path

After setup, `loop()` repeatedly executes M5 update, LED update, performance ownership synchronization, encoder/input handling and `MiniAcidDisplay::update()` approximately every 40 ms. `MiniAcidDisplay::update()` invokes persistence servicing before drawing. These are all post-stage-100 candidates until the fault boundary is captured.

## New explicit hypothesis — transient low-memory UI reconstruction

The source audit ruled out the simplistic hypothesis that all lazy pages monotonically accumulate. Under `free INTERNAL|8BIT < 16384`, `MiniAcidDisplay::getPage_()` aggressively destroys non-target pages before constructing a missing target page.

That does **not** make the UI safe at a `700–900 B` heap floor. It creates a different failure mechanism:

```text
destroy old page
        ↓
construct new page
        ↓
page object + strings/vectors/shared_ptr control graph
        ↓
constructor / setBoundaries / first-draw temporaries
        ↓
transient local minimum / failed allocation / fragmentation exposure
```

The important quantity is therefore not only retained page cost. For each navigation operation R0-C/R0-D must distinguish:

- free heap before transition
- largest block before transition
- operation-local minimum free heap
- free/largest immediately after old-page destruction when observable without semantic perturbation
- free/largest after new-page construction
- free/largest after first draw
- heap integrity
- whether the page survives leaving/revisiting

A correct destroy/recreate ownership model may still be physically unusable when the runtime floor leaves less memory than the next page's temporary construction peak.

This hypothesis is interaction-dependent. It must be separated from an untouched-idle crash.

## Current causal classification

**SD presence is CORRELATED ONLY.**

Plausible mechanisms still include:

1. SD/FAT mount consumes persistent heap and leaves another later subsystem without allocation headroom.
2. A later SD/FAT operation makes a temporary allocation that fails under the low floor.
3. A non-SD runtime allocation fails only because SD/FAT residency lowered the floor.
4. A task stack overflows independently and the SD-present memory layout/timing only exposes it.
5. Heap corruption occurred earlier but is detected later.
6. An idle UI/NVS/MIDI/driver path performs the first bad allocation or dereference after setup.
7. Low-memory page reconstruction produces a transient allocation spike during navigation even though old pages are correctly destroyed.

The investigation must name the first proven bad operation before selecting among them.

## Mandatory first step — one full raw panic boundary

Use the diagnostic branch script:

```bash
python3 scripts/capture_cardputer_panic_raw.py \
  --port /dev/ttyACM0 \
  --baud 115200 \
  --output build/memory-r0-d/panic.raw.log
```

Initial conditions for the first run:

- SD physically present
- scenes empty
- no user interaction after boot
- do not navigate pages
- do not load an SMF
- do not select/preload a sample

The script intentionally opens the TTY once, never reconnects it, copies every received byte immediately to a raw file and exits on disconnect/EOF/read error after preserving the received boundary.

### First-run binary outcome

**A. Device panics while untouched idle**

This strongly prioritizes a periodic/asynchronous/runtime-owner path. User-only sample preload, SMF load and navigation-only page reconstruction become secondary hypotheses.

**B. Device remains stable while untouched idle for the accepted observation window**

Interaction-dependent paths become materially more likely. Continue with the D2-D4 matrix below before blaming SD/FAT directly.

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

Use the **exact diagnostic ELF that produced the capture**:

```bash
xtensa-esp32s3-elf-addr2line -pfiaC \
  -e build/cardputer-recovery-diagnostics/GroovePuter.ino.elf \
  0xADDRESS1 0xADDRESS2
```

If the actual build directory/tool prefix differs, use the paths emitted by the build. Record both raw addresses and decoded symbols.

## Retained loop-stage localization — diagnostic-only

Extend only the diagnostic image with low-cost retained stage writes around post-setup operations. Do not emit serial text for each loop edge.

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
| 123 | before a real persistence/file-service sub-boundary when it actually runs |
| 124 | after that sub-boundary |
| 125 | loop end / before delay |
| 126 | after delay / next iteration boundary |

If the next boot repeatedly reports a `before` stage and never its paired `after`, that narrows the failure window but does not identify a specific instruction. Raw PC/backtrace has priority.

## Heap integrity and floor capture

Use the existing memory instrumentation rather than a second profiler.

At minimum records must include:

- `free INTERNAL|8BIT`
- largest `INTERNAL|8BIT` block
- minimum free
- heap integrity
- loop-task HWM
- AudioTask HWM
- SMF-task HWM
- MIDI-dispatch HWM

Immediately before a suspected failure window, capture values before verbose logging can perturb the path. Do not run full integrity checks on every ~5 ms loop iteration.

## Post-setup allocation trace

| allocation / resource path | idle possible? | input required? | navigation? | storage access? | evidence status |
|---|---|---|---|---|---|
| page `new` / component construction | no unless target missing/current reconstruction occurs | usually | yes | no | D2-D4 HW REQUIRED |
| page-local `std::vector` / `std::string` / `shared_ptr` graph | transition-dependent | usually | yes | no/depends | D2-D4 HW REQUIRED |
| `File` open / directory iteration | no generic proof | usually | can be | yes | TRACE REQUIRED |
| JSON scene load/save/autosave | due/request dependent | mutation may be prerequisite | no | yes | TRACE REQUIRED |
| sample lazy preload | no during untouched idle | yes | SAMPLER | yes | STATICALLY NARROWED |
| SMF file open/stream | no with no SMF loaded | yes/load command | MIDI PLAYER | yes | STATICALLY NARROWED |
| SMF route persistence | only if request pending | request dependent | may originate from SMF UI | **NVS** | STATICALLY NARROWED |
| UI-session persistence | possible after state change + due timer | prior state change | navigation may trigger | **NVS** | STATICALLY NARROWED |
| TinyUSB/MIDI driver work | yes | no | no | no | PANIC TRACE REQUIRED |

## Controlled hardware matrix — D0 through D4

All rows must use the **same exact diagnostic ELF** unless a later probe is separately justified. Record physical SD state/content, exact user action script, duration, panic result, raw log, retained final stage, free/largest/min-free, integrity and task HWMs.

| Scenario | SD/filesystem | Interaction | Purpose | Status |
|---|---|---|---|---|
| D0 | present; reproducing empty-scenes state | **none after setup** | prove/disprove untouched-idle panic | PENDING |
| D1 | present; same state | no input; ordinary periodic runtime for extended window | distinguish short startup-aftershock from steady periodic/async path | PENDING |
| D2 | present; same state | navigation only; do not open file/sample actions | isolate page reconstruction/navigation | PENDING |
| D3 | present; same state, samples content controlled | enter SAMPLER; first visit/leave/revisit, no preload first; preload only as separate subcase | isolate Sampler page/component graph from sample I/O | PENDING |
| D4 | present; same state | enter MIDI PLAYER; first visit/leave/revisit, no file load first; file load only as separate subcase | isolate MIDI Player page from SMF file load/playback | PENDING |

Use SD-absent A census as a control, but do not dilute D0 by mixing it with navigation or stress actions.

### Interpretation rules for D0-D4

- **D0 fails:** navigation-only UI reconstruction is not necessary for the crash. Prioritize periodic/async allocation, stack, driver, persistence and low-floor failure paths.
- **D0/D1 stable; D2 fails:** C and D converge on page transition/reconstruction. Measure the exact construction local minimum before any arena proposal.
- **D2 stable; D3 fails before preload:** Sampler page/component construction is implicated; sample file I/O is not yet required.
- **D3 stable until preload:** sample/file allocation path becomes a leading candidate.
- **D2 stable; D4 fails before file load:** MIDI Player page/browser construction is implicated; SMF parser/playback is not yet required.
- **D4 stable until SMF load/play:** move investigation into SMF load/playback allocations and B HWM/ownership evidence.

## Cross-stream hardware order

Do not flash separate diagnostic firmwares merely for convenience. Preferred order:

1. **D raw panic capture** on the exact diagnostic firmware, SD present, untouched after setup.
2. **A census RUN 1:** same instrumentation image, SD absent.
3. **A census RUN 2:** same image, SD present.
4. **A census RUN 3:** repeat SD-present run for reproducibility.
5. On the same instrumentation image, collect **B task HWM** during idle, representative SMF loads, dense playback, seek, tempo/reanchor, stop, panic and SEQTRAK external-clock cases.
6. Only if untouched idle is stable, execute D2-D4 interaction matrix and C per-page local-minimum measurements.

The critical startup accounting chain is:

```text
after DSP buffers
before SD
          ↓
after SD
before SMF
          ↓
after timing reserve
          ↓
after SMF task
          ↓
after MIDI dispatcher
          ↓
after MiniAcid::init
          ↓
after sample scan
          ↓
after MiniAcidDisplay allocation
          ↓
after first draw
          ↓
setup complete
```

A phase delta is not automatically an object's exact size. Background tasks and operation-local allocations must remain visible in the evidence.

## Decision rules

### OOM / failed allocation = YES only if

- allocator/new failure is reported and reaches a fatal path; or
- exception/assert/abort symbol resolves to allocation failure; or
- a checked allocation returns failure immediately before the proven bad operation and the next operation depends on it.

`free heap < 1 KB` alone is not proof.

### STACK OVERFLOW = YES only if

FreeRTOS overflow diagnostics, canary/watchpoint panic evidence, or a decoded fault boundary proves task-stack exhaustion. A low HWM is risk evidence, not sufficient by itself.

### HEAP CORRUPTION = YES only if

Heap-integrity check fails before the panic or the panic decoder reports heap poisoning/corruption with a defensible first failing boundary.

### SD DIRECTLY CAUSAL = YES only if

The faulting PC/symbol or immediately preceding proven bad operation is in an SD/FAT/File path and controlled variants rule out a merely lower-memory floor as the explanation.

If SD presence changes the floor but the faulting operation is elsewhere, classify `CORRELATED ONLY` unless a causal allocation chain is proven.

## R1 contract candidate — hypothesis only, not yet adopted

If R0 proves an otherwise healthy image can pass the static `.data + .bss` ceiling while entering a post-setup state with insufficient runtime allocation reserve, the current static-only release gate is incomplete.

R1 may then need independent contracts of the form:

```text
STATIC PRODUCT DRAM
    <= X

POST-SETUP INTERNAL FREE
    >= Y

LARGEST INTERNAL BLOCK
    >= Z

CRITICAL OPERATION LOCAL MINIMUM
    >= W
```

No values `X/Y/Z/W` are authorized here. Their existence as release gates is also not yet a verdict; it becomes justified only by the hardware root cause and census.

The architectural principle to test is:

> capability does not imply simultaneous residency.

R1, if reached, should decide residency topology rather than cosmetically pack objects while leaving every subsystem effectively always-on.

## Root-cause ledger

| Question | Current answer | Evidence needed to change it |
|---|---|---|
| panic is OOM/failed allocation? | UNKNOWN | raw boundary + decoded allocation failure |
| stack overflow? | UNKNOWN | panic/task/canary evidence + HWM |
| heap corruption? | UNKNOWN | integrity failure / heap panic evidence |
| SD/FAT temporary allocation? | UNKNOWN | PC/path + operation-local memory trace |
| sample scanning/file activity? | setup scan excluded by stage 100; runtime preload conditional | D0-D3 + trace |
| UI operation? | POSSIBLE | D0-D4 + retained stage + PC/backtrace |
| transient page reconstruction spike? | POSSIBLE, explicit hypothesis | D2-D4 local minimum + fault boundary |
| another runtime path exposed by low floor? | POSSIBLE | A differential + D0/D1 + decoded fault |

## Final status — current evidence only

### PANIC ROOT CAUSE

**UNKNOWN**

### FAULTING TASK

**UNKNOWN**

### FAULTING PC / SYMBOL

**UNKNOWN — raw panic boundary not yet captured**

### HEAP INTEGRITY BEFORE PANIC

**UNKNOWN for the immediate pre-panic boundary**

Earlier P3 integrity evidence does not substitute for R0-D pre-failure evidence.

### FREE INTERNAL BEFORE PANIC

**UNKNOWN at the immediate boundary**

The prior post-setup observation of approximately `700–900 B` is context only.

### STACK OVERFLOW

**UNKNOWN**

### OOM / FAILED ALLOCATION

**UNKNOWN**

### SD DIRECTLY CAUSAL

**CORRELATED ONLY**

### FIRST PROVEN BAD OPERATION

**UNKNOWN**

### PRODUCTION CHANGE

**NONE**

No memory-optimization PR or minimal root-cause fix is authorized until the two R0 hardware proofs are captured and reviewed separately.
