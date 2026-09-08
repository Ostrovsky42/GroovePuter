# 0.9.10 LIFETIME-R1 — Explicit Residency / Lifetime Engineering

Status: **L1 CENSUS / CONTRACT — implementation not yet accepted**

Authoritative base at checkpoint start:

`integration/20260908-0.9.10-memory-r1-product-closure @ a3e821b4f1bb2b118e7d13540518b249ef8b7721`

Work branch:

`feature/20260908-03-0.9.10-lifetime-r1-explicit-residency`

## 1. Product invariant

GroovePuter does not budget RAM or CPU as the sum of every feature that can exist in the product. It budgets the maximum set of resources that are legitimately alive at the same time:

```text
RAM_peak = max_t sum(size(resource_i) * alive(resource_i, t))
CPU_peak = max_t sum(cost(subsystem_i) * active(subsystem_i, t))
```

A feature is allowed to consume persistent runtime resources only while those resources are required by its current musical or system responsibility. Feature availability alone is not sufficient reason for residency.

**Optimize lifetime before capacity.** Before shrinking an object, stack, or buffer, first ask whether it must overlap in lifetime with the resources around it.

This rule must not be used to remove musically natural concurrency. A forbidden overlap is valid only when the user cannot reasonably need both responsibilities at once, or when an existing explicit product transition already defines exclusivity.

## 2. Non-goals

LIFETIME-R1 is not:

- a new sequencer architecture;
- a Pattern/Phrase rewrite;
- an allocator redesign;
- a framebuffer redesign;
- permission to reduce task stacks without hardware HWM evidence;
- permission to add allocation, filesystem, task creation, or waiting to AudioTask;
- permission to trade musical semantics for a smaller headline RAM number.

## 3. Runtime phases

These names are the L1 census vocabulary. They do **not** require a production enum unless implementation later needs one authoritative runtime owner.

| Phase | Meaning |
|---|---|
| `BOOT` | construction and one-time service initialization before the normal instrument loop |
| `MUSIC` | normal instrument runtime, including idle and active transport |
| `MATERIAL_PREPARE` | control-side preparation of a future Pattern/Phrase/Melody value |
| `MATERIAL_ACTIVATE` | bounded publication at an accepted musical boundary |
| `STORAGE_TRANSACTION` | bounded save/load/metadata/recovery filesystem operation |
| `SAMPLE_STREAM` | sample playback activity that genuinely requires streaming/I/O residency |
| `SMF_PLAY` | external SMF service has runtime work and an active worker |
| `RECOVERY` | explicit autosave/recovery transaction |

A resource may span several phases. The important property is that its owner, acquisition point, last reader, and release point are explicit.

## 4. Evidence classes

Every entry below is tagged so historical MEMORY-R1 observations do not silently become current facts.

- **FACT** — directly present in the authoritative LIFETIME-R1 base source or accepted MEMORY-R1 closure.
- **MEASURED-HISTORICAL** — previously measured, but current residency must be re-proven on this line.
- **TO-VERIFY** — current ownership/lifetime not yet proven.
- **CANDIDATE** — proposed implementation, not accepted production behavior.

## 5. Initial resource ownership census

| Resource | Size / order | Current owner and creation | Current lifetime | LIFETIME-R1 ruling |
|---|---:|---|---|---|
| AudioTask stack | **8192 B FACT** | `startAudioTask()` in `GroovePuter.ino`, pinned core 1 | created during boot and intentionally resident | **KEEP**. Realtime core owner. Do not shrink without HWM evidence. |
| Direct I2S/DMA | TO-VERIFY | `g_audioOut.begin(...)` before AudioTask | created during boot | **KEEP pending census**. Contiguous/driver residency is part of realtime baseline. |
| TempoDelay constrained buffers | existing measured critical allocations | preallocated before SD/SMF/UI fragmentation | MUSIC resident | **KEEP** unless separate DSP redesign proves another lifetime. |
| SMF task stack | **6144 B FACT** | `CardputerSmfPlayerService::begin()` | boot-deferred, then `taskLoop()` is currently unbounded `while (true)` | **L2 target**. First use must not imply permanent session residency without a semantic reason. |
| SMF timing/parser dynamic storage | **~part of accepted 7.0–7.4 KiB boot-lazy recovery FACT for class** | reserved in `CardputerSmfPlayerService::begin()` | boot-deferred; release path currently absent/to verify | **L2 census + ownership**. Task teardown alone is insufficient if reserved vectors retain capacity. |
| SMF `SdByteSource::File` | one file object, exact dynamic footprint depends on accepted FatFs profile | opened on SMF load | source has explicit `close()` but enclosing service lifetime requires tracing | **L2 census**. File lifetime must follow loaded/streaming semantics, not task convenience. |
| USB MIDI dispatcher stack | **4096 B static reservation FACT from boot comment** | Cardputer USB MIDI service | normal MUSIC service | **KEEP** for L1; MIDI is a normal concurrent instrument responsibility. |
| Pattern/Phrase active A+B runtime material | **2 × 1284 B = 2568 B MEASURED-HISTORICAL/current architecture to verify** | per-synth current runtime buffers | MUSIC resident | **KEEP**. This is musical working truth, not temporary scratch. |
| Phrase/Melody NEXT M2 candidate | **+2568 B CANDIDATE** | future per-synth pending preparation | `MATERIAL_PREPARE` → `MATERIAL_ACTIVATE` only | **Measure before acceptance**. Preferred over M1 if peak live set remains safe. |
| Sampler fixed pages/cache | **4096 B MEASURED-HISTORICAL** | sampler/sample-store architecture | TO-VERIFY on current line | **Census first**; do not infer a task from old measurements. |
| Sampler I/O task stack | **4096 B MEASURED-HISTORICAL** | old MEMORY evidence | current `src/sampler/` inspection has not yet identified a dedicated current task owner | **DO NOT OPTIMIZE YET**. First prove whether this task still exists and who owns it. |
| FatFs static structure class | old stock **24,832 B** = `5*FIL + FATFS`; current MEMORY-R1 uses accepted dynamic-buffer FatFs profile | framework/filesystem | current static/runtime split changed in MEMORY-R1 | **Use current FS1B evidence only**. Old 24,832 B is comparison baseline, not current residency. |
| Storage transaction scratch/handles | TO-VERIFY | Scene/sample/SMF/autosave owners | mixed | **L1 map each owner** before shared scratch or handle pooling. |
| Display/UI heap objects | TO-VERIFY | display + `MiniAcidDisplay` created during boot | MUSIC resident | **Census only** in R1; no framebuffer redesign. |
| Encoder/UI heap owner | TO-VERIFY | `Encoder8Miniacid` created during boot | MUSIC resident | **Census only** unless a genuine phase-local allocation is found. |
| Undo/autosave/recovery state | TO-VERIFY | existing application owners | mixed | **Map last-reader lifetime**; no duplicate full material buffer merely for convenience. |

## 6. First proven lifetime defect: SMF post-use residency

The accepted MEMORY-R1 closure already made SMF **boot-lazy**. Boot no longer creates the task; the closure records a measured **7.0–7.4 KiB** residency recovery before first use.

Current source still has this lifecycle:

```text
BOOT
  -> SMF runtime deferred

first SMF load
  -> begin()
  -> reserve timing/parser capacity
  -> create 6144 B SmfPlayerTask
  -> taskLoop(): while (true)

stop / EOF / return to other pages
  -> no service end()/release API exists
  -> task residency persists
```

This is therefore the first concrete L2 target. The fix is **not** simply `vTaskDelete()` after Stop: loaded-file replay, pause/resume, Project/SEQTRAK transport ownership, parser capacity, queue ownership, file handles, and restart behavior must all be specified before teardown.

L2 must first write RED tests for the desired semantic boundary and only then add a release path.

## 7. Realtime invariant

The following paths must remain allocation-free and blocking-free:

```text
AudioTask callback/render loop
sequencer tick/event processing
note-on / note-off lifetime transitions
bar-boundary material activation
```

They may select or publish already-prepared fixed state. They may not call `malloc/new/free/delete`, filesystem APIs, task creation/destruction, or wait for a control-side owner.

## 8. ACTIVE / NEXT / OLD material lifetime

The required material publication model is:

```text
ACTIVE
  + prepare NEXT on control side
ACTIVE + NEXT
  + atomic accepted-boundary publication
NEW ACTIVE + OLD
  + release OLD immediately after its last legitimate realtime reader
NEW ACTIVE
```

`OLD` is not an automatic Undo buffer. Undo should use the existing semantic ownership model and must not keep a full duplicate runtime material alive without an explicit requirement.

## 9. Disk-backed Melody direction

Accepted design direction for later L3:

```text
MaterialSlot = PATTERN | MELODY
MELODY slot -> compact storage descriptor/reference
SD          -> serialized Melody payload
RAM         -> current working Melody + bounded NEXT only
```

`MAKE MELODY` is transactional:

```text
PATTERN
 -> prepare candidate
 -> write candidate to SD
 -> validate completed payload
 -> publish slot kind=MELODY + storage reference
MELODY
```

Any failure before publish leaves the original PATTERN unchanged.

If SD disappears after a Melody is loaded, the working RAM truth remains playable/editable and becomes `DIRTY / NOT SAVED`; SD absence must not destroy current musical material.

## 10. Peak-live-set acceptance

For each phase transition capture, at minimum:

```text
free internal heap
largest internal free block
minimum-ever internal free heap when available
task stack HWM for the task under test
audio underrun/panic/reset evidence
```

Allocation count is required only when supported by trustworthy instrumentation; do not invent a proxy metric and call it allocation count.

Required repeated-cycle class:

```text
baseline X/Y
operation x50
after X'/Y'
```

where free memory must return to an explained plateau and `largest` must not ratchet downward.

- stable free + falling largest = fragmentation regression;
- falling free + stable largest = residency/leak regression.

## 11. Forbidden-overlap rule

A forbidden overlap must be documented with:

1. the two owners;
2. why simultaneous use is not a legitimate musical/system action;
3. the transition that enforces exclusion;
4. a regression test for that transition.

Do not pause normal realtime audio merely because serialization makes an implementation easier when a musician reasonably expects the operation during playback.

## 12. Stack policy

For every task considered for stack reduction record:

```text
configured stack
observed hardware HWM
worst-case scenario
required safety margin
```

Lifetime optimization comes first. A task that can disappear when unused is a stronger target than shaving an unmeasured kilobyte from its stack.

## 13. LIFETIME-R1 implementation order

1. **L1 — Census/instrumentation:** resource matrix, current owners, phase snapshots, no semantic product changes.
2. **L2 — Low-risk residency:** finish SMF lifetime; identify current sampler-task truth; map current FatFs/file-handle lifetime before further changes.
3. **L3 — Material lifetime:** measure M2; ACTIVE/NEXT/OLD; bounded storage transaction; disk-backed Melody.
4. **L4 — Shared cold scratch:** only after mutual exclusion is proved by control flow and tests.
5. **L5 — CPU lifetime:** measure idle DSP/FX/display work, then short-circuit only evidence-backed waste.

No later stage may be used to bypass an unresolved earlier ownership ambiguity.

## 14. Acceptance

LIFETIME-R1 is not GREEN because `.data + .bss` decreased. It is GREEN only when evidence shows:

- optional idle subsystems are non-resident when their responsibility ended;
- transient phases return to the expected heap plateau;
- repeated churn does not degrade `largest`;
- Pattern/Phrase/Melody semantics and natural musical concurrency remain correct;
- no new realtime allocation/blocking exists;
- task stack changes, if any, are backed by hardware HWM;
- CPU-idle wins, if any, are measured rather than inferred.
