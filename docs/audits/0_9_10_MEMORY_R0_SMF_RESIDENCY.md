# 0.9.10-MEMORY-R0-B — SMF PLAYER RESIDENCY AND TASK LIFETIME AUDIT

Status: **OPEN — static ownership mapped; hardware HWM/load cases required**  
Authoritative source: `feature/20260904-02-0.9.10-pattern-phrase-p3-phrase-lifetime` @ `aded0e183a934f78623030226b67b5d0b598648b`  
Production semantic delta: **NONE**

## Primary question

Does the complete SMF runtime have to remain physically resident from boot, or can some part of its lifetime follow MIDI PLAYER usage without violating realtime scheduling, USB-MIDI single-writer ownership, external-clock behavior, pending NoteOff cleanup, routing persistence, or project-transport synchronization?

This document characterizes the current model. It does not authorize implementation.

## Current construction / lifetime topology

### Important correction: the service object is not lazy

`src/platform/cardputer_smf_player_registry.cpp` declares a global:

```text
LazyCardputerSmfPlayer g_smfPlayer;
```

and `LazyCardputerSmfPlayer` embeds:

```text
CardputerSmfPlayerService player_;
```

Therefore the **CardputerSmfPlayerService object and all of its inline buffers are static-image residents before `begin()`**. `ensureStarted()` is lazy only with respect to `player_.begin()`, queue registration, timing-vector reservations and task creation.

This means that Models B/C cannot recover the service's existing inline static storage merely by delaying or deleting the FreeRTOS task. Recovering static bytes would require a separate ownership/storage design decision and is outside R0-B.

## Explicit static SMF residency already proven from source

The following are embedded inside the global service object:

- `ScheduledSmfMidiEventQueue eventQueue_`
- `SdByteSource source_` including a `File` object
- `SmfFileIndex fileIndex_`
- `SmfEventStreamMerger stream_`
- `SmfTimingMap timing_` vector objects
- `SmfDocument timingDocument_` vector/string container state
- `SmfMidiVisualTimeline midiVisualTimeline_`
- pending event / transport / snapshot / inspector / route state
- static command queue control/storage
- task handle

Two explicit inline buffers alone account for at least `6144 B` of static object payload:

1. `SmfEventStreamMerger::cachePool_[4096]` = **4096 B**.
2. `ScheduledSmfMidiEventQueue::events_[128]`, with `static_assert(sizeof(ScheduledSmfMidiEvent) == 16)` = **2048 B**.

That `6144 B` is a lower bound, not the total `sizeof(CardputerSmfPlayerService)`. It excludes 32 stream objects, 32 prefetched stream events, the file index, active-note ownership, command queue storage, snapshots and ordinary member state. Exact target `sizeof(...)` still needs a diagnostic-build size probe or ELF symbol accounting.

## `begin()` allocations

`CardputerSmfPlayerService::begin()` currently performs:

1. `timingDocument_.events.reserve(32)`
2. `timing_.reserveForEvents(32)`
   - reserves `33` tempo points
   - reserves `33` time-signature points
3. `xQueueCreateStatic(...)`
4. `xTaskCreatePinnedToCore(..., 6144, ...)`

### Queue distinction

The command queue uses `xQueueCreateStatic` with `StaticQueue_t commandQueueStruct_` and inline `commandQueueStorage_`. Queue payload/control storage is therefore static service-object residency, **not begin-time heap residency**.

### Timing distinction

The timing vectors reserve capacity at boot and retain that capacity. Their exact heap bytes must be captured immediately after each reserve in a diagnostic tree. A total `begin()` free-heap delta must not be assigned wholly to the task stack.

## Load-time / file lifetime

`loadFile()` does the following without intentionally growing the stream cache:

- opens the SD `File` through `SdByteSource`
- builds a bounded `SmfFileIndex`
- opens the fixed `SmfEventStreamMerger`
- scans the file to populate at most 32 timing events in the already-reserved timing document
- builds tempo/signature maps into already-reserved timing vectors
- requests the route profile
- leaves the file open for streaming playback

The source specifically preserves `timingDocument_.events` capacity across loads rather than assigning a temporary document.

### Lifetime classes

| owner | storage class | current lifetime | expected allocation phase | evidence status |
|---|---|---|---|---|
| global service object | static DRAM | image lifetime | before `setup()` | PROVEN |
| stream cache pool 4096 B | static DRAM | image lifetime | before `setup()` | PROVEN |
| scheduled event array 2048 B | static DRAM | image lifetime | before `setup()` | PROVEN |
| command queue object/storage | static DRAM | image lifetime | before `setup()` | PROVEN |
| timing document vector capacity | heap | `begin()` -> reboot current design | `begin()` | PROVEN topology, bytes PENDING |
| timing map vector capacities | heap | `begin()` -> reboot current design | `begin()` | PROVEN topology, bytes PENDING |
| SMF task stack | task-stack allocation | `begin()` -> reboot current design | `begin()` | configured 6144 B; used HWM PENDING |
| `File` / FAT handle state | SD/FAT runtime | loaded-file lifetime | `loadFile()` | PROVEN topology, bytes PENDING |
| file index | inline static object | image lifetime; content overwritten per load | no new owner expected | PROVEN |
| stream/index runtime state | inline static object | image lifetime; active while loaded | no cache allocation on load | PROVEN |
| route-profile NVS temporaries | operation-local | pending profile request only | UI loop persistence service | PROVEN topology, bytes PENDING |
| playback queue contents | inline fixed queue | image lifetime storage; logical contents playback-only | no capacity growth | PROVEN |

## Stack reality — mandatory hardware matrix

Current configured SMF task stack: **6144 B**.

Use `uxTaskGetStackHighWaterMark(taskHandle)` from the existing memory instrumentation accessor. On ESP-IDF/Cardputer this checkpoint records the returned unused-stack watermark in bytes as already done by `instrument_cardputer_memory_runtime.py`.

Do not derive a new stack size from host tests or idle-only HWM.

Required cases:

| Case | File / state | Required evidence |
|---|---|---|
| idle after boot | no file loaded | minimum unused stack over >=120 s |
| dense Format-0 playback | representative corpus file | min HWM during load + playback |
| multitrack Format-1 playback | representative corpus file | min HWM during load + playback |
| second representative file | structurally different from above | min HWM |
| seek | repeated forward/backward | min HWM + cleanup result |
| tempo change | ORIGINAL mode adjustment | min HWM |
| PROJECT tempo reanchor | project timeline active | min HWM + no ownership failure |
| stop | active notes present | min HWM + panic/cleanup completion |
| panic | active notes present | min HWM + dispatcher cleanup completion |
| SEQTRAK external clock | Start/Stop/Continue / clock | min HWM + project transport state |

### Corpus selection rule

Use 2–3 existing repository/hardware corpus MIDI files. At least one must be dense Format-0 and one multitrack Format-1. Record filename, bytes, format, retained track count, timing-event count and longest test operation. Do not introduce a synthetic file if an existing accepted corpus file covers the case.

## Ownership constraints

### MidiDispatchTask is the physical cleanup boundary

`ScheduledSmfMidiEventQueue` is SPSC from `SmfPlayerTask` to `MidiDispatchTask`. Logical SMF track ownership is committed only after physical USB dispatch succeeds. STOP, pause, seek, route changes and panic can invalidate generations and request scoped/global cleanup.

`stopAndCleanup()` does **not** synchronously prove that all physical NoteOffs have reached the wire. It calls `eventQueue_.invalidateAndRequestPanic()`. `MidiDispatchTask` later consumes that panic mailbox and owns wire cleanup/retry/backpressure behavior.

Therefore a destroy-on-exit model cannot safely free queue/player state merely because the UI left MIDI PLAYER or because a Stop command was enqueued.

### Pending-note shutdown condition required for any destructible model

Before destruction, an implementation would need a proved bounded handshake establishing all of the following:

1. producer stopped publishing new scheduled SMF events;
2. scheduled generations invalidated;
3. dispatcher observed the cleanup request;
4. every SMF-owned physical note was released or ownership was safely transferred to a persistent owner;
5. no cleanup retry/backpressure mailbox still references destructed state;
6. no route/profile request still expects the service generation;
7. project-transport callbacks/reads cannot race destroyed state;
8. USB queue registration no longer exposes a dangling `ScheduledSmfMidiEventQueue*`.

No such destruct lifecycle is implemented today. R0-B therefore treats Model C as compatibility-sensitive, not a trivial `vTaskDelete()` change.

## What must survive leaving MIDI PLAYER today?

The UI page itself is not the SMF owner. Playback can continue independently of the page, and SMF state participates in:

- `MidiDispatchTask`
- scheduled SMF MIDI queue and track-note ownership
- USB MIDI physical output ownership
- SEQTRAK / external clock following
- PROJECT transport timeline synchronization
- track routing and route-profile persistence
- panic/recovery/backpressure handling
- UI snapshots consumed on re-entry

Therefore “exit MIDI PLAYER” is not currently equivalent to “SMF session ended”. Any lifecycle model keyed directly to page exit would change user-visible behavior unless it explicitly preserves background playback semantics or first proves that background playback is not part of the product contract.

## Lifecycle model comparison — characterization only

### MODEL A — current always-started runtime

**Description:** global service object always static; boot `begin()` reserves timing capacities and creates the 6144-B task; SMF queue registered for dispatcher.

- memory gain: none
- fragmentation risk: low after startup; large allocations are front-loaded
- startup latency: paid at boot
- realtime safety: currently accepted baseline
- MIDI cleanup risk: lowest relative to alternatives
- architecture complexity: lowest
- compatibility risk: lowest

### MODEL B — lazy task/reserves on first MIDI PLAYER use, retain forever

**Description:** retain global static service object, but do not call `begin()` at boot; first actual SMF operation creates task and timing capacities, then retains them.

- memory gain before first use: potentially timing-vector heap + 6144-B task stack; **not** existing static service buffers
- memory gain after first use: none relative to current begin-time residency
- fragmentation risk: higher than A because a 6144-B task stack and timing vectors must be allocated after other runtime/UI/SD allocations
- startup latency: moved to first use
- realtime safety: potentially safe only if creation occurs outside audio-critical mutation and enough contiguous heap remains
- MIDI cleanup risk: low after successful start; startup registration race must be proved
- architecture complexity: low/moderate
- compatibility risk: moderate

**Static evidence warning:** with the currently observed post-UI memory floor, first-use creation may be impossible. B cannot be recommended until a pre-first-use heap/largest-block trace proves the stack/reserves can still be allocated.

### MODEL C — construct/start on entry, cleanup/destroy on exit

There are two materially different interpretations:

1. delete only task + release dynamic capacities while global static object remains;
2. make the service object itself dynamically owned so inline buffers can be reclaimed.

The second is a substantially larger architecture change.

- memory gain: potentially task stack + timing heap; static >6144-B explicit buffers only if object ownership itself changes
- fragmentation risk: highest due repeated large create/delete cycles on constrained DRAM
- startup latency: every session entry
- realtime safety: requires bounded creation/destruction and dispatcher quiescence
- MIDI cleanup risk: **high** until shutdown handshake is proven
- architecture complexity: high
- compatibility risk: high because playback currently outlives UI page lifetime

Current verdict: **not implementation-ready**.

### MODEL D — shared bounded I/O/playback worker

**Description:** retain common dispatcher/I/O worker and make SMF scheduling/session state a bounded client rather than a dedicated always-running task.

- memory gain: potentially removes dedicated task stack and duplicate worker state; static service buffers still require separate ownership decision
- fragmentation risk: can be low if worker is static/bounded
- startup latency: low if worker already resident
- realtime safety: requires priority/work-budget proof and must not block other clients on SD scans
- MIDI cleanup risk: manageable only with explicit per-client ownership epochs
- architecture complexity: highest
- compatibility risk: high without a broader worker contract

Current verdict: research option only; R0-B does not justify it by itself.

## Required begin/load instrumentation

Extend the existing diagnostic instrumentation in the temporary build to log exact heap/largest/integrity at:

1. SMF `begin()` entry
2. after `timingDocument_.events.reserve(32)`
3. after `timing_.reserveForEvents(32)`
4. after static command-queue creation (expected near-zero dynamic residency; still verify driver/control effects)
5. after task creation
6. before `loadFile()`
7. after file open
8. after index build
9. after stream open
10. after metadata scan/timing build
11. load complete
12. stop complete
13. file close / subsequent file load

For operation windows, record local minimum free heap separately from post-operation residency.

## Provisional memory classes

These are classes, not an implementation promise:

- **static:** at least `6144 B` of explicit inline SMF buffers are proven resident before `begin()`, plus additional inline service state. This memory is **not** recoverable by only changing task lifetime.
- **heap:** timing document/map capacity reserved by `begin()`; exact bytes PENDING.
- **task stack:** configured `6144 B`; exact unused minimum PENDING hardware HWM.
- **load-time:** File/FAT handle and possible library temporaries; exact persistent/peak bytes PENDING.

## Final verdict gate

### SMF ALWAYS-RESIDENT REQUIREMENT

**NOT PROVEN** for the dedicated SMF **task and timing heap**.  
However, current product semantics do prove that SMF playback/cleanup ownership can outlive the MIDI PLAYER page, so page lifetime cannot be used as a destruction boundary without additional contract changes and a cleanup handshake.

### SMF TASK STACK CURRENT

**6144 B**

### MEASURED MINIMUM UNUSED STACK

**PENDING HARDWARE HWM MATRIX**

### SAFE STACK REDUCTION

**NOT PROVEN**

No stack reduction is authorized by static inspection.

### BEST LIFETIME MODEL

**UNDECIDED**

A remains the accepted baseline. B is the smallest plausible lifetime experiment but may fail physically because allocation would move to a much lower/fragmented heap point. C conflicts with background playback/cleanup lifetime unless a new explicit session contract is designed. D requires wider architecture evidence.

### POTENTIAL RECOVERABLE INTERNAL MEMORY

- static: **0 B proven recoverable by task-lifetime change alone**; `>=6144 B` explicit SMF inline buffers exist but reclaiming them requires service-object ownership redesign and is not authorized here
- heap: **PENDING** exact timing-reserve residency
- task stack: **up to configured 6144 B class exists, but safe reclaim/reduction is NOT PROVEN**
- total class: **PENDING hardware + exact `sizeof`/ELF accounting**

### BLOCKERS BEFORE IMPLEMENTATION

1. complete HWM matrix across idle/load/dense playback/Format-1/seek/tempo/stop/panic/SEQTRAK
2. isolate begin-time timing-vector heap from task-stack allocation
3. target-size/ELF accounting for `CardputerSmfPlayerService`, `ScheduledSmfMidiEventQueue`, `SmfEventStreamMerger`, timing objects and route state
4. prove whether background SMF playback after leaving MIDI PLAYER is a required product semantic
5. define and test a dispatcher cleanup/quiescence handshake before any destructible model
6. prove first-use largest-block availability if Model B is considered
7. measure repeated create/destroy fragmentation before Model C can be considered safe

### PRODUCTION CHANGES

**NONE**
