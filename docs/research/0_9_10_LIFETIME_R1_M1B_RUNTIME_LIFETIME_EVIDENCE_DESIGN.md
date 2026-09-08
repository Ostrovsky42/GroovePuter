# 0.9.10 LIFETIME-R1 M1-B — Runtime Lifetime Evidence Design

Date: 2026-09-08
Status: **DESIGN APPROVED — SPEC REVIEW PENDING**
Branch: `research/20260908-06-0.9.10-lifetime-r1-m1b-runtime-evidence`
Base: `3b071d92f341bd313797c4b6d63e223f152b0d35`
Frozen M1-A firmware source: `8581e284fc5b95757ab090e558ef56d8f0c21aa0`
Frozen M1-A fixed internal DRAM: **186,712 B / 182.34 KiB**
Production changes in M1-B research lane: **FORBIDDEN until measurement matrix closes**

## 0. Purpose

M1-A answers **what is statically retained in the production-equivalent ELF**.
M1-B answers a different question:

> **When does memory become live, which owners coexist at each musical/runtime
> transition, and what allocation capability remains available at those
> boundaries?**

M1-B must not reinterpret or replace the M1-A static baseline. It uses a
separate instrumented firmware identity whose results are authoritative only for
runtime lifetime observations.

The checkpoint is evidence-only. It may expose implementation candidates, but
it does not implement memory optimizations.

## 1. Non-negotiable rules

1. **Separate identities.** The M1-A production-equivalent ELF remains the only
   authority for frozen `.dram0.data/.dram0.bss`. The M1-B diagnostic ELF has a
   new exact identity and never rewrites the M1-A numbers.
2. **The measurement system is part of the measured system.** M1-B records the
   static and runtime overhead introduced by the diagnostic harness.
3. **Phase transition is the unit of evidence.** Records are attached to named
   lifecycle edges, not arbitrary timestamps.
4. **Observe before optimizing.** No production lifetime, allocation, stack,
   Scene, Phrase, SMF, sampler, filesystem or DSP optimization may be made by
   the Memory Research lane until the required matrix is complete.
5. **Capability is explicit.** `MALLOC_CAP_8BIT` and
   `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` remain separate quantities.
6. **Historical minima are not attribution.** A minimum-ever heap value proves
   that a low point occurred, not which owner caused it.
7. **Logging happens after capture.** Snapshot collection must finish before
   `Serial.printf` or other reporting work that can perturb the state.

## 2. Architecture

### 2.1 Recommended measurement model

Use the existing research-only instrumentation pattern rather than adding
permanent diagnostics to production C++:

1. checkout the exact production source SHA;
2. copy or instrument only a temporary build tree;
3. apply anchor-checked research instrumentation;
4. preserve the generated instrumentation patch as evidence;
5. build a control ELF from unchanged source;
6. build a diagnostic ELF from the temporary instrumented tree;
7. record both identities and their deltas;
8. run hardware/runtime experiments only with the diagnostic identity.

The checked-out production source remains unchanged.

### 2.2 Why not production `#ifdef` hooks

Permanent `#ifdef LIFETIME_R1_M1B` hooks would be convenient but would place a
research contract inside product ownership boundaries. That is unnecessary for
M1-B and increases the chance that diagnostics become coupled to firmware
semantics.

### 2.3 Why not copied diagnostic source files

Maintaining parallel diagnostic copies of production files creates source drift.
The instrumentation script must instead patch exact anchors and fail closed when
source structure changes.

## 3. Evidence identities

Every M1-B runtime evidence set must record:

```text
production_source_sha
m1a_frozen_audit_commit
research_branch_head
research_workflow_sha
instrumentation_script_sha256
instrumented_source_patch_sha256
control_elf_sha256
diagnostic_elf_sha256
board_fqbn
build_flags
arduino_cli_version
compiler_version
framework/core versions
```

The diagnostic identity is invalid if the instrumentation script cannot prove
that the only source differences from the pinned firmware are the generated
research hooks.

## 4. M1-B0 — diagnostic self-overhead gate

Before collecting musical/runtime evidence, build both control and diagnostic
images under the same toolchain/configuration.

### 4.1 Static overhead

Record for both images:

```text
.dram0.data
.dram0.bss
.rtc.data
.noinit
ELF SHA-256
```

Calculate:

```text
diagnostic_static_delta = diagnostic(.dram0.data + .dram0.bss)
                        - control(.dram0.data + .dram0.bss)
```

The diagnostic delta is not subtracted blindly from runtime measurements. It is
recorded as measurement-system overhead.

### 4.2 Task overhead

The M1-B harness must create **no new telemetry task**. CI/source verification
must fail if the diagnostic patch introduces a new `xTaskCreate*` call or a new
static task stack owned by the harness.

Existing task handles may be exposed through temporary diagnostic accessors.

### 4.3 Heap initialization overhead

Any diagnostic helper that can lazily allocate bookkeeping must be prewarmed
before the runtime baseline starts. Record snapshots immediately before and
after diagnostic initialization:

```text
free8
largest8
freeInternal8
largestInternal8
```

This delta becomes `diagnostic_runtime_init_overhead`.

### 4.4 Stack-probe caveat

`uxTaskGetStackHighWaterMark()` values are minimum free bytes since task start.
Because M1-B code executes on existing tasks, especially the loop task, observed
HWM is **probe-inclusive**.

Therefore M1-B may identify a stack candidate, but no stack-size reduction is
accepted solely from M1-B HWM. A later narrow A/B proof is required before any
stack-reservation change.

## 5. Snapshot schema

Each phase-transition record uses one normalized schema.

```text
seq
phase
transition
edge
monotonic_ms

free8
largest8
minimumEver8

freeInternal8
largestInternal8
minimumEverInternal8

loopStackFreeBytes
audioStackFreeBytes
smfStackFreeBytes
dispatchStackFreeBytes

loopTaskPresent
audioTaskPresent
smfTaskPresent
dispatchTaskPresent

sample_count
pcm_loaded_bytes
heap_integrity
```

### 5.1 Edge vocabulary

Use only named lifecycle edges such as:

```text
before
begin
constructed
prepared
published
commit
rollback
accepted
steady
stop
after
```

An experiment may use a subset, but its edges must correspond to concrete
source/runtime transitions.

### 5.2 Primary acceptance signal

`largest8` and `largestInternal8` are primary allocation-capability signals.
`free8` and `freeInternal8` remain important but secondary because total free
memory does not prove that the next contiguous allocation can succeed.

### 5.3 Minimum-ever interpretation

`minimumEver8` and `minimumEverInternal8` are historical context only. They may
show that a lower point occurred before a named phase snapshot. They cannot be
used to assign responsibility to Phrase, Scene, SMF, SD, sampler or any other
owner without transition-local evidence.

## 6. Sampling model

M1-B combines two mechanisms.

### 6.1 Transition snapshots

Named hooks capture exact state at lifecycle boundaries. This is authoritative
for before/after/commit-style comparisons.

### 6.2 Background sampling by the existing loop task

A lightweight periodic sample may update diagnostic minima between transition
hooks. It must not run in a new task. Its cadence and code size are part of the
diagnostic identity.

For long-running asynchronous operations such as SMF playback, background
samples provide within-phase pressure evidence.

### 6.3 Operation-local minimum monitor

For synchronous operations that block the loop task, use the existing ESP-IDF
local minimum-free monitor around the operation where safe. It measures the
window's minimum free heap, not owner-specific allocated bytes.

ESP-IDF provides no equivalent operation-local monitor for largest free block;
therefore largest-block evidence is transition snapshots plus repeated-run drift,
not an invented peak value.

## 7. Required phase matrix

### 7.1 Boot and steady state

| Experiment | Required transitions |
| --- | --- |
| Boot/setup | setup begin -> major services constructed -> setup complete |
| Warm idle | setup complete -> lazy service warmup -> stable idle |

The warm-idle baseline is selected only after UI, SD, audio, MIDI and other
required lazy services have been exercised.

### 7.2 Pattern/Phrase

| Experiment | Required transitions |
| --- | --- |
| Pattern playback | before start -> steady playback -> stop -> after |
| Pattern -> Phrase | before prepare -> pending ready -> switch/commit -> steady Phrase |
| Phrase prepare | begin -> generated/materialized -> publish/end |
| Phrase editing | before edit -> mutation prepared -> mutation committed -> steady |

M1-B records residency and coexistence only. Any production change to Phrase
buffer ownership remains with the Pattern/Phrase runtime owner.

### 7.3 Phrase audition / M-006

Required transitions:

```text
steady
-> audition begin
-> rollback state fully captured
-> audition material applied
-> rollback OR accept
-> transaction end
-> steady
```

The critical question is not whether the statically retained 8,220-B scratch
exists; M1-A already proved that. M1-B determines which dynamic owners coexist
with it during capture/apply/rollback and whether any residue remains afterward.

### 7.4 Scene transaction / M-001

Required transitions:

```text
steady
-> transaction begin
-> temporary Scene fully materialized
-> validation complete
-> commit begins
-> commit complete / old transaction lifetime ended
-> transaction end
-> steady
```

The `temporary Scene fully materialized` edge is mandatory. A simple
before/after scene-load comparison is insufficient to establish construction
coexistence.

M1-B does not assume that the second Scene can or should be removed.

### 7.5 Filesystem and save paths

Required experiments:

```text
SD mount/open/close
scene load
scene save
manual save
AUTOSAVE/recovery write path where reproducible
```

For each transaction capture at least `before`, `active/constructed`, and
`after`. Current post-FS1B behavior is authoritative; historical pre-FS1B file
budgets are not reused.

### 7.6 SMF / M-002

Required transitions:

```text
unloaded
-> load begin
-> loaded/armed
-> worker live
-> playing
-> stopped
-> replay begin
-> playing after replay
-> unload/error settlement where applicable
```

The experiment must preserve current semantics: `STOP != UNLOAD`.

Record task-presence and SMF HWM together with heap snapshots. Page leave is not
used as a teardown boundary.

### 7.7 Sample metadata / M-004

Use a controlled fixture with PCM residency held at zero while scanning:

```text
N = 0, 1, 8, 16, 32, 64, 128
```

If hardware/media capacity makes a larger deterministic fixture cheap, a later
extension to 256/512 is allowed without changing the base experiment.

For each N capture:

```text
pre-scan
post-index
post-registration
settled
```

Record:

```text
sample_count
pcm_loaded_bytes = 0
free8 / largest8
freeInternal8 / largestInternal8
```

Primary derived quantities:

```text
retained_free8_delta(N)
retained_freeInternal8_delta(N)
largest_block_delta(N)
local slope between adjacent N values
```

Do not assume linearity. Vector growth, map nodes and allocator size classes may
produce stepwise cost.

### 7.8 Sample playback

Separately from metadata scanning, measure one representative sample playback
path:

```text
before
-> preload/open if applicable
-> active playback
-> stop
-> settled
```

This experiment addresses cache/I/O/playback residency, not metadata
cardinality.

## 8. Construction-peak analysis

After raw phase evidence is collected, construct overlap diagrams for expensive
transactions. The purpose is to approximate the actual live-set maximum:

```text
PeakLiveSet = max_t(sum(alive(resource_i, t)))
```

This is an analytical model derived from measured transitions and source
ownership; it is not a replacement for heap evidence.

At minimum build overlap analyses for:

- Scene load/commit;
- Phrase prepare / Pattern -> Phrase;
- Phrase audition rollback;
- SMF load/start/stop/replay;
- sample scan/registration;
- save/autosave.

The key question is:

> Which expensive temporary owners overlap in implementation even though their
> semantic lifetimes might not require simultaneous residency?

## 9. Existing telemetry reused

M1-B should reuse the already-established primitives where their semantics are
correct:

- `heap_caps_get_free_size`;
- `heap_caps_get_largest_free_block`;
- `heap_caps_get_minimum_free_size`;
- `heap_caps_monitor_local_minimum_free_size_start/stop`;
- `uxTaskGetStackHighWaterMark`;
- direct temporary accessors for SMF and MIDI dispatcher task handles;
- loop/audio task handles already available to the firmware/runtime harness;
- `heap_caps_check_integrity_all(false)`.

No task-name lookup is required for owned tasks when direct handles are
available.

## 10. Evidence pack

The M1-B evidence artifact should contain:

```text
lifetime-r1-m1b-runtime-evidence/
├── identity.txt
├── control-build-meta.txt
├── diagnostic-build-meta.txt
├── control-sections.txt
├── diagnostic-sections.txt
├── diagnostic-overhead.txt
├── instrumentation.patch
├── instrumentation.sha256
├── serial-raw.log
├── phase-records.tsv
├── task-hwm.tsv
├── sample-cardinality.tsv
├── experiment-manifest.txt
└── checksums.txt
```

`serial-raw.log` is immutable primary runtime evidence. Parsed TSVs are derived
views and must be reproducible from the raw log.

## 11. Instrumentation failure policy

Instrumentation fails closed when:

- an expected source anchor is missing or appears more than once;
- the production source identity does not match the pinned SHA;
- control and diagnostic builds do not use the same board/toolchain contract;
- a new telemetry task is introduced;
- generated patch identity is not recorded;
- parser encounters duplicate sequence IDs or unknown phase/edge values;
- heap integrity fails;
- required experiment transitions are missing.

A failed or partial experiment is retained as diagnostic evidence but cannot
close the corresponding matrix row.

## 12. Ownership and routing

Memory Research owns:

- measurement harness;
- evidence identities;
- runtime phase matrix;
- static-vs-runtime cross-analysis;
- finding classification;
- measurement audit documents.

Memory Research does **not** own production fixes in other domains.

Current routing:

- M-001 Scene transaction scratch -> Scene/state owner after evidence;
- M-002 SMF hot/cold residency -> SMF/MIDI owner after evidence;
- M-003 Phrase buffer residency -> Pattern/Phrase runtime owner;
- M-004 sample metadata cardinality -> sampler owner after evidence;
- M-005 FatFs open-file lifetime -> filesystem/platform owner after evidence;
- M-006 Phrase audition rollback scratch -> GF2 phrase-audition owner after evidence.

A finding may be confirmed by Memory Research without Memory Research taking
production ownership of its fix.

## 13. Explicit non-goals

M1-B does not:

- modify M1-A baseline numbers;
- shrink `Scene` or allocate it dynamically;
- change Phrase capacity or ownership;
- hibernate or destroy SMF workers;
- change sample metadata structures;
- change FatFs/file lifetime;
- resize loop/audio/SMF/dispatcher stacks;
- remove TinyUSB buffers;
- change wavetable placement;
- optimize DSP capacity;
- introduce a sampler I/O task;
- redesign UI, generation, sequencer or MIDI architecture.

## 14. M1-B closure gate

M1-B measurement phase is complete only when one documented diagnostics identity
supports all of the following:

- control and diagnostic builds use the same pinned production source/toolchain;
- diagnostic static overhead is recorded;
- diagnostic runtime-init overhead is recorded;
- no diagnostic task was created;
- boot/setup and warm-idle baseline are captured;
- Pattern playback matrix is captured;
- Pattern -> Phrase and Phrase-prepare transitions are captured;
- Phrase audition capture/apply/rollback-or-accept/end is captured;
- Scene transaction includes fully-materialized temporary Scene and commit/end;
- filesystem/open-close and save/autosave transaction evidence is captured;
- SMF load/play/stop/replay evidence includes task presence/HWM;
- sample-cardinality fixture reaches at least N=128 with PCM loaded bytes zero;
- representative sample playback evidence is captured;
- raw serial log and parsed records are both retained;
- construction-overlap analysis is written for the required expensive operations;
- confirmed findings are routed to production owners;
- Memory Research has made **no production optimization changes**.

Only after this gate may M1-A and M1-B evidence be intersected to create
MEMORY-R1 implementation candidates.

## 15. Acceptance interpretation

M1-B does not close by claiming an amount of memory that can be saved. It closes
when the project can answer, with exact identities and phase evidence:

```text
What is statically retained?                 -> M1-A
What is live at each transition?             -> M1-B
What is the worst observed contiguous block? -> M1-B largest*
What construction owners coexist?            -> M1-B overlap analysis
What runtime state remains after operation?  -> M1-B before/after evidence
Which findings belong to which owner?        -> routing ledger
```

The output is a measured lifetime map, not an optimization patch.
