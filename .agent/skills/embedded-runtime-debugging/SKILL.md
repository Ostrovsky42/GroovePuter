---
name: embedded-runtime-debugging
description: Evidence-driven debugging and optimization of constrained C/C++ firmware on ESP32-class and other RTOS microcontrollers. Use when diagnosing boot or initialization failures, task creation errors, heap fragmentation, watchdog resets, USB enumeration or backpressure, disappearing SD/filesystem entries, realtime stalls, queue saturation, hardware-only regressions, or when changing task stacks, buffers, initialization order, drivers, caches, and memory-heavy UI pages. Establish layered state contracts, instrument before editing, isolate PC-versus-device behavior, implement one minimal fix, and prove it with host, build, and hardware evidence.
---

# Embedded Runtime Debugging

Debug constrained firmware by turning ambiguous symptoms into layered, measurable
state transitions. Treat memory layout, initialization order, host behavior, and
task scheduling as explicit APIs rather than incidental implementation details.

## Apply the Evidence Rule

Do not translate a symptom directly into a cause:

- "files disappeared" does not prove that the card detached;
- "USB mounted" does not prove that the host drains the IN endpoint;
- "enough free heap" does not prove that a task stack can be allocated;
- "the retry branches delay" does not prove that the success path yields;
- "the test passes" does not prove that the test encodes the intended invariant.

Require a new observation before each new behavioral patch. If a hardware oracle
returns only one bit such as "sounds/does not sound", first build a richer oracle
on a PC or in host tests.

## Execute the Workflow

### 1. Freeze a Baseline

Record the exact revision, build profile, board/chip, framework version, connected
hardware, reproduction sequence, and known-good behavior. Keep unrelated feature
work out of the diagnostic branch. Do not stack speculative fixes faster than the
hardware can verify them.

### 2. Model the Layers

Write the state chain before editing code. Common chains are:

```text
boot:    resource reserved -> driver registered -> stack started -> task running
USB:     device configured -> class mounted -> endpoint draining -> app consuming
storage: card mounted -> path exists -> directory opened -> iteration completed
memory:  static budget -> total free -> largest block -> allocation -> high-water
RTOS:    event queued -> task runnable -> deadline handled -> CPU yielded
```

Expose each boundary independently. Never collapse adjacent states into one
boolean such as `ready`.

### 3. Instrument Before Fixing

Prefer bounded counters and edge logs over per-event spam:

- attempts, successes, rejects, drops, retries, and queue depth;
- current and minimum queue depth;
- maximum latency or blocked duration, not only averages;
- mount/unmount, suspend/resume, stall/clear, task start/failure edges;
- static DRAM from the linker plus runtime `free` and `largest` heap blocks;
- reset reason, watchdog task/core, and retained boot stage.

Give field users a short human-readable screen status when serial logging is not
available. Keep the machine-readable counters in serial logs.

### 4. Reproduce on the Most Transparent Host

Use a PC before a black-box appliance whenever possible. Inspect descriptors,
kernel binding, device nodes, and endpoint traffic. Deliberately test both with
the host consumer closed and open: many hosts do not submit IN transfers until an
application opens the MIDI/serial/audio endpoint.

### 5. Change One Failure Dimension

Form one falsifiable hypothesis and predict the counter transition that would
confirm it. Change one subsystem, constant, or initialization boundary. If the
result supplies no new information, stop rather than adding another patch.

### 6. Encode the Correct Contract

Use the smallest durable fix:

- reserve critical contiguous resources before fragmenting initialization;
- register interfaces before starting the bus or descriptor builder;
- replace unbounded retries with explicit bounded backpressure policy;
- guarantee a scheduler yield on every long-running path, including success;
- reuse storage for mutually exclusive states instead of duplicating large data;
- allocate page/dialog data lazily and release it at lifecycle boundaries;
- distinguish physical absence from allocation/open/iteration failure;
- prioritize cleanup and ownership release over new data after recovery.

Add compile-time size assertions and behavioral tests where a regression can be
detected without hardware.

### 7. Prove the Result

Run, in order:

1. focused host tests;
2. the complete host regression suite;
3. a clean firmware build and static flash/DRAM comparison;
4. ELF/map inspection for changed object and section sizes;
5. a controlled PC integration test;
6. the real hardware matrix, including long-duration and recovery cases.

Do not flash hardware without explicit permission. Preserve raw logs and report
what remained unverified.

## Use Diagnostic Decision Rules

- Stable total heap with falling `largest` indicates fragmentation.
- Task creation failure with apparently sufficient total heap usually means no
  contiguous block fits stack + TCB, wrong capability memory, or bad init order.
- Card mounted plus directory-open failure indicates filesystem allocation or I/O,
  not necessarily card removal.
- USB configured but class not mounted indicates descriptor, registration order,
  endpoint allocation, or host binding.
- Class mounted plus one FIFO of successful writes followed by rejects indicates a
  non-draining host endpoint, not necessarily a broken cable.
- A watchdog in a high-priority dispatcher indicates a runnable loop that never
  blocks; inspect successful and already-late paths, not only retry paths.
- Recovery only after reboot indicates a latched driver/application state; recovery
  after opening the PC consumer indicates normal host-side backpressure.

## Keep Realtime Contracts Non-Negotiable

- Do not allocate, log, or perform blocking I/O in audio/ISR callbacks.
- Bound work per iteration and preserve queue ownership rules.
- Use the realtime clock for event timing and wall-clock time for fairness limits.
- Do not guess receiver throughput. Measure sustained rate and burst capacity
  before adding pacing or token buckets.
- On disconnect or unrecoverable backpressure, release logical note/resource
  ownership even when physical cleanup cannot be emitted.

## Read the References Selectively

- Read [memory-and-initialization.md](references/memory-and-initialization.md) for
  fragmentation, task stacks, boot order, object sizing, and storage overlays.
- Read [io-transport-and-storage.md](references/io-transport-and-storage.md) for
  USB endpoint state, host polling, bounded backpressure, SD, and file browsing.
- Read [realtime-observability-and-verification.md](references/realtime-observability-and-verification.md)
  for watchdog fairness, diagnostics, test matrices, and process controls.

## Report Completion

Report these separately:

1. failure model and root cause evidence;
2. exact fix and preserved contracts;
3. numbers before/after, including static DRAM and runtime largest block;
4. host/build/integration/hardware tests and their verdicts;
5. residual risks and unverified hardware behavior.
