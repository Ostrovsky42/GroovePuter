# Realtime Observability And Verification

## Contents

- Watchdog and fairness
- Diagnostic design
- Test oracles
- Hardware verification matrix
- Process controls
- Regression contracts

## Guarantee Scheduler Fairness

A task does not need an explicit infinite loop bug to starve the scheduler. A
high-priority dispatcher can remain runnable when:

- queued deadlines are already in the past;
- every write succeeds immediately;
- retry/empty branches delay but the success branch does not;
- recovery repeatedly resumes into a full queue;
- work arrives faster than a lower-priority producer/consumer can run.

Inspect every loop path, especially the successful fast path. Yield by wall-clock
budget rather than event count because event cost and backlog density vary:

```cpp
if (millis() - fairnessWindowStart >= kFairnessWindowMs) {
    vTaskDelay(1);
    fairnessWindowStart = millis();
    ++fairnessYields;
}
```

Use the realtime/audio clock for event deadlines and wall-clock time only for CPU
fairness. Confirm task priorities, core affinity, watchdog subscription, and idle
task opportunity. A delay in an error branch does not protect the normal branch.

## Design Diagnostics Around Decisions

Periodic window logs should answer:

- Is the producer generating work?
- Is the queue growing or draining?
- Is the physical write accepted?
- Is the failure unmounted, suspended, full, late, or dropped?
- Did state recover, and how long was it blocked?
- Is memory leaking or fragmenting?
- Did audio miss a deadline?

Useful fields include:

```text
attempt / accepted / rejected / unmounted
queue current / min / max / overflow
stall enter / clear / current blocked ms / max blocked ms
mount up/down / suspend/resume
late sent / late dropped / max lateness
free heap / largest block / task stack high-water
audio load / peak / underruns
```

Emit separate edge logs for meaningful state transitions. Suppress rapid healthy
flaps that would bury the signal. Keep counters monotonic across diagnostic windows
unless the log explicitly labels per-window values.

When serial is unavailable, show concise states such as:

```text
USB NOT MOUNTED
USB BLOCKED - KEYS WAIT
MIDI FOLDER OPEN FAILED
LOW MEMORY
PRESS PLAY TO RESUME
```

Do not expose unexplained numeric tuples as the only field diagnostic.

## Build A Verdict-Producing Oracle

Prefer probes that save raw events and return exit status 0/1. Keep capture and
analysis separable so logs can be replayed without hardware. A useful MIDI/event
probe measures:

- unmatched ownership (`NoteOn` without `NoteOff`);
- gaps against an adaptive threshold;
- timing/clock mean, variance, and maximum deviation;
- channel/routing histograms;
- active resources at capture end;
- whether explicit cleanup was observed after stop.

Do not mistake an uncontrolled capture end for a firmware cleanup failure. Leave
the probe open, trigger stop/panic, and observe the cleanup tail.

## Use A Layered Verification Matrix

### Code and Build

- focused unit/policy tests;
- full host suite;
- clean firmware build;
- flash and static DRAM deltas;
- map/ELF object-size inspection;
- compile-time queue/object/buffer limits.

### PC Integration

- descriptor and driver binding inspection;
- consumer open at start;
- consumer opened after induced stall;
- consumer close/reopen;
- endpoint traffic capture;
- scripted verdict.

### Hardware-In-The-Loop

- known-good simple file/workload;
- dense worst-case workload;
- long-duration run;
- disconnect/reconnect;
- stop/panic and zero owned resources;
- browse storage and enter memory-heavy pages during streaming;
- cold boot, warm reset, and repeated lifecycle transitions;
- no-receiver run proving no watchdog reset.

Record exact firmware revision and scenario in every log filename or header.

## Prevent Patch Loops

Hardware verification is often much slower than code generation. Enforce these
controls:

1. Freeze the branch after an unexplained regression.
2. Require one new measured fact before the next behavioral commit.
3. Keep diagnostics, transport policy, storage optimization, and UI memory changes
   in separable commits.
4. Prefer a scripted PC oracle over repeated manual black-box tests.
5. Use `git bisect run` once the failure is reproducible without human input.
6. Do not let CI or an agent continuously author speculative fixes.
7. Park unrelated improvements until the blocking failure has a proven cause.

## Test Invariants, Not Syntax

Structural tests are useful when embedded behavior cannot run on the host, but
they must encode a contract rather than a spelling:

- Good: all write outcomes report into shared endpoint health.
- Bad: a particular helper call appears with exact argument text.
- Good: cleanup cannot auto-resume playback.
- Bad: a historical function name remains present.
- Good: a page/object stays below a measured size limit.
- Bad: a member declaration has a specific formatting shape.

Audit old tests when behavior changes. A regression test can preserve the defect
it was originally written around. Replace it with the intended invariant and
explain why the former expectation was wrong.

## Report Evidence Without Overclaiming

Separate:

- host-verified behavior;
- build-verified memory/layout;
- PC integration evidence;
- real target evidence;
- residual assumptions.

Do not call a hardware bug fixed solely because the firmware builds or because a
different host behaves correctly.
