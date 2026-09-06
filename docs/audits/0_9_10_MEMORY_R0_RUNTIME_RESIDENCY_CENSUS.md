# 0.9.10-MEMORY-R0-A — AUTHORITATIVE RUNTIME RESIDENCY CENSUS

Status: **OPEN — hardware census required**  
Authoritative source: `feature/20260904-02-0.9.10-pattern-phrase-p3-phrase-lifetime` @ `aded0e183a934f78623030226b67b5d0b598648b`  
Production semantic delta: **NONE**

## Purpose

Establish a byte-accounted INTERNAL|8BIT runtime-residency ledger across Cardputer startup and steady-state operation. This checkpoint is characterization only. It does not authorize stack reductions, lazy initialization, allocator changes, feature removal, DRAM-ceiling changes, SD redesign, UI redesign, or P3 semantic changes.

## Prior evidence — context only, not R0-A measurements

These values were established before this census and must not be silently reused as new per-owner measurements:

- static product DRAM before Stage12 catalog constexpr move: `192904 B`
- static product DRAM after Stage12 catalog constexpr move: `190776 B`
- current static ceiling: `191488 B`
- static headroom: `712 B`
- earlier P3 hardware characterization: free internal heap about `26144 B`, largest block about `16372 B`, heap integrity PASS, fragmentation not observed, per-cycle leak not observed
- later SD-mounted startup observation: after SMF init about `4384 B` free INTERNAL; after UI creation roughly `700–900 B`

The last two observations are correlation evidence only. They do not identify an owner by themselves.

## Exact startup owner order on the authoritative head

`GroovePuter.ino` currently establishes owners in this order:

1. M5/Cardputer hardware and display/I2S driver initialization
2. `AudioTask` via `xTaskCreatePinnedToCore(..., 8192, ...)`
3. `MiniAcid::preallocateConstrainedDelayBuffers()` before SD/SMF
4. `SceneStorageCardputer::initializeStorage()` / SD mount
5. `beginCardputerSmfPlayerService()`
6. global MIDI settings restore
7. USB MIDI dispatcher registration; dispatcher stack itself is statically reserved
8. `Encoder8Miniacid` heap allocation
9. `MiniAcid::init()`
10. sample-index scan and `RamSampleStore::registerFile()` registrations
11. `MiniAcidDisplay` heap allocation; constructor also creates the active page
12. encoder/LED init
13. first UI draw
14. `setup()` complete / runtime loop

This ordering matters: a delta spanning more than one item cannot be attributed to a single owner.

## Existing instrumentation to extend

Authoritative instrument: `scripts/instrument_cardputer_memory_runtime.py`.

It already provides diagnostic-tree-only probes for:

- `free8`
- `largest8`
- `freeInternal8`
- `largestInternal8`
- boot and sampled minimum-free values
- heap integrity
- loop-task stack high-water mark
- AudioTask stack HWM
- SMF task stack HWM
- MIDI-dispatch task stack HWM
- periodic runtime sampling

R0-A must extend this existing instrument narrowly rather than create a competing profiler. The probes must remain outside the product ELF/source graph.

## Mandatory startup checkpoints

For every checkpoint record:

`ms, free INTERNAL|8BIT, largest INTERNAL|8BIT, minimum free INTERNAL|8BIT, heap integrity, loop stack HWM, AudioTask HWM, SMF task HWM, MIDI-dispatch HWM`.

Required sequence:

| Seq | Checkpoint | Current source anchor | R0-A result |
|---:|---|---|---|
| 1 | boot entry | `markBootStage(1, "setup-entry")` | PENDING |
| 2 | after M5/display init | after `g_display.begin()` / display clear | PENDING |
| 3 | before AudioTask create | immediately before `startAudioTask()` | PENDING |
| 4 | after AudioTask create | immediately after `startAudioTask()` | PENDING |
| 5 | before TempoDelay/DSP preallocation | before `preallocateConstrainedDelayBuffers()` | PENDING |
| 6 | after TempoDelay/DSP preallocation | after same | PENDING |
| 7 | before SD mount | before `initializeStorage()` | PENDING |
| 8 | after SD mount | after `initializeStorage()` | PENDING |
| 9 | before SMF init | before `beginCardputerSmfPlayerService()` | PENDING |
| 10 | after SMF timing reserve | add diagnostic hook inside temporary SMF `begin()` after both reserves | PENDING |
| 11 | after SMF task create | end of temporary SMF `begin()` | PENDING |
| 12 | before USB MIDI runtime | before `registerCardputerUsbMidiSink()` | PENDING |
| 13 | after USB MIDI runtime | after successful registration | PENDING |
| 14 | before `MiniAcid::init` | existing boot stage 50 | PENDING |
| 15 | after `MiniAcid::init` | existing boot stage 51 | PENDING |
| 16 | before sample-index scan | existing boot stage 60 | PENDING |
| 17 | after sample-index scan | existing boot stage 61 | PENDING |
| 18 | before `MiniAcidDisplay` allocation | existing boot stage 70 | PENDING |
| 19 | after `MiniAcidDisplay` allocation | existing boot stage 71 | PENDING |
| 20 | after first UI draw | existing boot stage 95 | PENDING |
| 21 | setup complete | existing boot stage 100 | PENDING |
| 22 | runtime +5 s | diagnostic timer | PENDING |
| 23 | runtime +30 s | diagnostic timer | PENDING |
| 24 | runtime +120 s | diagnostic timer | PENDING |

### Measurement rule

A `free heap delta` is a phase delta, not automatically the allocation size of the named object. Background FreeRTOS tasks, driver activity, deferred frees and temporary allocations must be accounted for separately. Task-stack reservation is residency, not a leak.

## Mandatory physical variants

| Variant | SD | Scenes | Samples | User interaction | Status |
|---|---|---|---|---|---|
| A | physically absent | N/A | N/A | none until +120 s | PENDING |
| B | physically present | empty | existing/default | none until +120 s | PENDING |
| C | physically present | empty | empty directory | none until +120 s | PENDING if setup can be kept semantics-identical |

A/B must use the same exact diagnostic ELF except for physical SD presence/content. C may change filesystem contents, not firmware semantics.

## Phase residency table

Fill only from captured R0-A logs. Do not back-compute missing cells from old screenshots/logs.

| owner / phase | free before | free after | delta | largest before | largest after | persistent? | evidence |
|---|---:|---:|---:|---:|---:|---|---|
| M5/display/I2S init | PENDING | PENDING | PENDING | PENDING | PENDING | mixed | hardware phase pair |
| AudioTask create | PENDING | PENDING | PENDING | PENDING | PENDING | yes while audio runtime exists | phase pair + task handle/HWM |
| TempoDelay/preallocated DSP buffers | PENDING | PENDING | PENDING | PENDING | PENDING | yes | isolated pre/post pair |
| SD/FAT mount | PENDING | PENDING | PENDING | PENDING | PENDING | mixed | A/B physical differential + phase pair |
| SMF timing reserves | PENDING | PENDING | PENDING | PENDING | PENDING | yes after `begin()` | temporary hook inside `begin()` |
| SMF task create | PENDING | PENDING | PENDING | PENDING | PENDING | yes after `begin()` | isolated task-create pair + HWM |
| USB MIDI runtime | PENDING | PENDING | PENDING | PENDING | PENDING | yes | phase pair; dispatcher stack is static, not heap |
| Encoder8 allocation | PENDING | PENDING | PENDING | PENDING | PENDING | yes | optional narrow pair |
| `MiniAcid::init()` | PENDING | PENDING | PENDING | PENDING | PENDING | mixed | phase pair |
| sample index scan / registration | PENDING | PENDING | PENDING | PENDING | PENDING | index persistent; scan temporaries unknown | phase pair + file count |
| `MiniAcidDisplay` ctor including initial page | PENDING | PENDING | PENDING | PENDING | PENDING | yes for root/current page | phase pair + `[UI] Page ... created` |
| first draw | PENDING | PENDING | PENDING | PENDING | PENDING | mixed | phase pair |
| setup -> +5 s | PENDING | PENDING | PENDING | PENDING | PENDING | classify from trend | runtime samples |
| +5 s -> +30 s | PENDING | PENDING | PENDING | PENDING | PENDING | classify from trend | runtime samples |
| +30 s -> +120 s | PENDING | PENDING | PENDING | PENDING | PENDING | classify from trend | runtime samples |

## Owner ledger — static facts vs hardware evidence

| OWNER | estimated / observed bytes | static vs heap vs task stack | lifetime | needed during normal groove playback? | mode/page-only? | coexistence | confidence |
|---|---:|---|---|---|---|---|---|
| AudioTask stack | configured `8192` | dynamic FreeRTOS task stack | boot -> runtime | yes | no | coexists with all runtime | HIGH for configured size; HWM PENDING |
| SMF task stack | configured `6144` | dynamic FreeRTOS task stack | `begin()` -> reboot on current design | no SMF playback needed for normal groove; ownership interactions must be audited | MIDI PLAYER / SMF runtime | currently yes | HIGH for configured size; HWM PENDING |
| MIDI dispatch task | configured `4096` | **static** task stack (`xTaskCreateStaticPinnedToCore`) | whole image/runtime | yes for MIDI runtime | no | yes | HIGH; static symbol/layout, no heap-create delta expected |
| TempoDelay buffers | prior source comment: two contiguous ~8.6 KB buffers | heap/preallocated DSP storage | preallocation -> runtime | yes when corresponding FX can be used | feature-dependent but current design reserves both | yes by design | MEDIUM until exact R0-A delta |
| SD/FAT runtime | PENDING | driver/FAT heap + static library state | mount -> runtime plus operation temporaries | storage features yes; exact idle residency unknown | storage operations | yes | LOW until A/B pair |
| SMF timing document | capacity for 32 timing events reserved in `begin()` | heap vector capacity | SMF `begin()` -> reboot current design | no for groove-only | SMF | yes | HIGH lifetime, bytes PENDING |
| SMF timing map | tempo/signature vector capacities reserved in `begin()` | heap vector capacities | SMF `begin()` -> reboot current design | no for groove-only | SMF | yes | HIGH lifetime, bytes PENDING |
| SMF stream/index | substantial object state is inline/static because global SMF player exists before `begin()`; load-time File handle is separate | mostly static object + load-time SD handle | static object whole image; file handle loaded-file lifetime | no for groove-only | SMF | yes | HIGH topology; exact sizeof PENDING |
| MiniAcid engine | global static instance plus init-time allocations | static + heap | whole runtime | yes | no | yes | HIGH topology, bytes need symbol/phase attribution |
| sample index | vector/string/file metadata depends on scan result | heap | after sample scan -> runtime | sampler feature | SAMPLER/sample selection | yes | MEDIUM topology; bytes PENDING |
| UI root | `new MiniAcidDisplay`; owns skin, page vector, overlays/session state | heap object + child heap | after UI init -> runtime | yes | UI | yes | HIGH topology; bytes PENDING |
| UI pages | current low-memory policy destroys non-target pages during navigation | heap | current page; previous page retained only when not in aggressive low-memory mode | current page only | page-specific | low-memory hardware normally one target page | HIGH topology; per-page bytes PENDING |

## Important static distinctions already proven

### MIDI dispatch stack is not an R0 runtime-heap leak

The dispatcher uses static `StaticTask_t` + static stack storage and `xTaskCreateStaticPinnedToCore`. It contributes to static DRAM pressure, not to the dynamic free-heap drop at dispatcher task creation.

### SMF queue storage is not allocated by `xQueueCreateStatic`

`CardputerSmfPlayerService` embeds `StaticQueue_t` plus command queue storage and passes them to `xQueueCreateStatic`. The begin-time heap drop must not be attributed to command-queue storage.

### UI pages do not necessarily accumulate

`MiniAcidDisplay::getPage_()` sets `aggressive = freeDRAM < 16384`; when creating a missing target page in this mode it resets every other page before allocation. On the observed sub-1-KB floor, the hypothesis “all lazy pages eventually accumulate” is false. The UI cost still requires root/current-page/draw accounting.

## Accounting protocol

1. Parse every `[MEM-BASE]` and `[MEM-STACK]` record by `phase`.
2. Pair before/after checkpoints by boot and exact ELF identity.
3. For each phase compute `delta = free_before - free_after` and largest-block delta.
4. Separate configured task-stack reservation from used-stack HWM.
5. For A/B/C compare **same checkpoint**, not arbitrary nearby log lines.
6. Mark an owner `persistent=yes` only when the delta remains present at setup complete and steady-state or is independently known static/task residency.
7. A temporary local minimum is safety evidence, not resident bytes.
8. A falling free value across +5/+30/+120 s is a leak candidate only if monotonic/repeatable; heap integrity and largest-block trend must be reported alongside it.

## Stop condition — do not fill by inference

### TOTAL POST-SETUP INTERNAL FREE

- SD absent: **PENDING R0-A HARDWARE RUN**
- SD present: **PENDING R0-A HARDWARE RUN**

### TOP 5 RESIDENT OWNERS / DELTAS

1. **PENDING**
2. **PENDING**
3. **PENDING**
4. **PENDING**
5. **PENDING**

### UNEXPLAINED DELTA

**PENDING**

If the final unexplained differential is greater than approximately `2 KB`, this census remains incomplete and must not feed an architectural recommendation as though byte ownership were closed.

### CURRENT MODEL

**UNKNOWN pending R0-A hardware data.** Static pressure is already independently real; runtime residency is strongly indicated by the startup floor. Fragmentation/leak remain unproven by this census until the requested time series is captured.

### PRODUCTION CHANGES

**NONE**
