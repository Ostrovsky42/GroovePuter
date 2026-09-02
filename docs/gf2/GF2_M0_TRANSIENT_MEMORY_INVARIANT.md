# GF2-M0 — Transient Memory Invariant Resolution

Status: **BLOCKED ON CURRENT-E0 HARDWARE RUNTIME EVIDENCE**

GF2-M0 is a research / characterization / release-contract checkpoint. It is
not a memory optimization task, GF2-I1, or GF2-C2 Gate B.

## Frozen ancestry

```text
GF2-E0 exact base
commit 36981c72dd481118b3aa966a4e5c143d39bab270
tree   ff7fc33cd1edba54ff0df7a1a0fa7c1bd657c00a
branch infra/20260902-03-0.9.10-gf2-e0-target-validation-environment

GF2-M0 branch
research/20260902-04-0.9.10-gf2-m0-transient-memory-invariant
```

The first M0 commit is a direct child of the exact E0 SHA. No synthetic merge,
`dev_0.9.9`, or unrelated newer branch is used as the base.

The research runtime could not execute the requested local
`git status --short` / `git rev-parse` sequence because its checkout path was
unavailable after outbound DNS failure. Exact commit/tree/ancestry were instead
verified through GitHub objects. This limitation is evidence provenance, not a
substitute for runtime hardware evidence.

## Scope firewall

```text
MEMORY OPTIMIZATION PERFORMED        NO
PRODUCTION MUSICAL SEMANTIC DELTA    NONE
GF2-I1                               NOT STARTED
GF2-C2 GATE B                        NOT STARTED
```

M0 changes no allocator strategy, ownership/lifetime, GENRE, RECIPE, FEEL,
PHRASE musical semantics, GenerationCorridor, DEPTH, migration behavior,
generation selection behavior, musical material, or UI behavior.

## Executive determination

The historical comparison

```text
11,428 B > 2,292 B
```

was a real **allocation-feasibility** problem, but the two values are not the
same metric:

- **2,292 B** was an observed **largest free contiguous INTERNAL|8BIT heap
  block** on Cardputer ADV at a low-memory runtime checkpoint;
- **11,428 B** was the size of **one contiguous C++ heap allocation request**
  for the old `PreparedPhraseArrangement`, dominated by its inline
  `material[8]` array (`11,328 B`).

The old implementation therefore could not satisfy that one allocation when
`largest_free_block == 2,292 B`, and real hardware produced
`PHRASE PREPARE OOM`.

PMB-P1 removed that allocation. On exact E0,
`GeneratedPhraseSong::generate()` uses local
`PreparedPhraseArrangement preparedStorage{}` and the compact plan is guarded by
`static_assert(sizeof(PreparedPhraseArrangement) <= 1024)`. The old
`new(std::nothrow) PreparedPhraseArrangement(...)` request is gone.

Therefore:

```text
all transient memory < 2292 B
```

was **never a valid release invariant**. It reverses an availability floor into
a usage ceiling and loses region, contiguity, checkpoint, and code-lineage
semantics.

The remaining unresolved question is different: what absolute runtime headroom
and recovery properties must the **current E0/M0 exact firmware** preserve on a
real Cardputer ADV? Existing historical PMB-P1 hardware logs do not answer that
for a tree that moved 15 commits after the hardware-tested PMB-P1 implementation.

## Phase 0 — evidence inventory and classification

### E1 — earliest recoverable committed `2292` evidence

```text
commit b256ff180165e8db37e61be8658b13c0ae2bcd5c
file   docs/audits/SMF_STABILITY_REGRESSION_AUDIT_2026-08-02.md
```

The hardware audit records:

```text
freeInt=4100
largest=2292
audio underruns=0
```

It describes RAM as low but stable; it does not define `2292` as transient
usage or as a release ceiling.

Classification:

```text
AUTHORITATIVE RUNTIME MEASUREMENT
HISTORICAL NOTE
```

This is the earliest recoverable committed repository evidence found by M0, not
a claim that no earlier uncommitted serial log ever contained the value.

### E2 — PMB-A1 frozen memory audit

```text
commit 03f633d2e431c75ec9b2f4402507b9253f785a74
file   docs/contracts/0_9_9_PHRASE_MEMORY_BUDGET_STREAMING_PREPARE_AUDIT.md
```

The audit records:

```text
setup steady state   freeInt ~= 6440 B   largest = 2292 B
first G              free    = 6180 B   largest = 2292 B
old single request   11428 B contiguous
```

It explicitly treats `2292 B` as an **observed largest-free-block value from one
boot/session**, not a guaranteed allocator constant. Repeated failed G attempts
had stable free memory and constant largest block, which rules out progressive
fragmentation/leak as the cause of that historical first-attempt OOM.

Classification:

```text
2292 B sample                  AUTHORITATIVE RUNTIME MEASUREMENT
2292 B universal transient cap NEVER AN AUTHORITATIVE CONTRACT
<= observed floor proposal     HEURISTIC / RESEARCH CANDIDATE
```

### E3 — exact old `~11.4 KB` decomposition

PMB-A1 compiler-froze the relevant sizes:

```text
PhraseGenerator::PhraseBar          1,416 B
material[8]                        11,328 B
PreparedPhraseArrangement          11,428 B
PreparedPhraseExecution               324 B
PhraseExecutionScratch              1,416 B
```

The old path made one ordinary C++
`new(std::nothrow) PreparedPhraseArrangement(...)` request. It was not eight
independent allocations, not cumulative allocation traffic, and not a measured
successful peak-live interval.

Classification:

```text
TEST CONTRACT
DERIVED SINGLE-ALLOCATION REQUIREMENT
corroborated by AUTHORITATIVE RUNTIME FAILURE
```

### E4 — PMB-P1 production replacement

```text
implementation / hardware-tested SHA
a52cf32a2cb6ec4ebea8f846574db19667fb3015
```

`a52cf32...` is an ancestor of E0; E0 is `ahead_by=15`, `behind_by=0` relative
to that SHA. PMB-P1 removed the 11,428-byte heap allocation and introduced the
bounded replay/materialization design retained by exact E0.

Classification:

```text
PRODUCTION IMPLEMENTATION
TEST CONTRACT
```

### E5 — PMB-P1 historical hardware-green evidence

The documentation-only side-branch commit
`df426a5d596888580b88499b0b1aad03bc9006e4` records hardware acceptance against
exact implementation SHA `a52cf32...`:

```text
before first G       free ~= 6180 B   largest ~= 2292 B
1B / 2B / 4B G       PASS
PREPARE_OOM           zero occurrences
repeated G            largest stayed 2292 B; no monotonic decrease
post-navigation G     PASS; largest unchanged
```

The docs commit itself is not an E0 ancestor, but its hardware-tested
implementation SHA is. This is valid historical evidence for that exact
lineage, not current-E0 threshold evidence.

Classification:

```text
AUTHORITATIVE HISTORICAL RUNTIME EVIDENCE
STALE / REQUIRES RE-MEASUREMENT for current E0 numeric thresholds
```

### E6 — exact E0 implementation and heap instrumentation

At `36981c72...`:

- `src/dsp/generated_phrase_song.h` contains the compact PMB-P1 plan and local
  `PreparedPhraseArrangement preparedStorage{}`;
- the old PREPARE heap allocation is absent;
- `GroovePuter.ino` contains `logHeapCaps()` using:

```cpp
heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
```

- `logHeapCaps()` is called at several **boot** checkpoints;
- `markBootStage(100, "setup-complete")` marks boot completion but does **not**
  itself capture heap values;
- the loop contains `// logHeapCaps("periodic");`, i.e. periodic heap logging is
  **disabled** on exact E0;
- `src/ui/pages/phrase_page.cpp` reports generation status/failure but contains
  no `heap_caps` calls.

Classification:

```text
AUTHORITATIVE CURRENT SOURCE
MEASUREMENT API CAPABILITY EXISTS
OPERATION-SCOPED PHRASE PROBE DOES NOT
```

## Phase 1 — reconstruct `2292 B`

| Question | Finding |
| --- | --- |
| First recoverable committed appearance? | `b256ff18...`, SMF stability audit, 2026-08-02 |
| Operation/context? | Cardputer ADV low-memory runtime after normal boot/SMF activity; later reproduced around first PHRASE G |
| Metric? | largest free contiguous `INTERNAL|8BIT` heap block |
| Target? | Cardputer ADV / ESP32-S3 |
| Runtime point? | absolute low-memory checkpoint / steady-state observation |
| Internal or total heap? | capability-qualified internal 8-bit heap |
| Total free or largest block? | largest block |
| Absolute or delta? | absolute availability value |
| Peak/live allocation? | no |
| Remaining headroom? | yes, contiguous headroom |
| Runtime or estimate? | runtime measurement |
| Global release invariant? | no evidence; primary PMB source rejects that interpretation |

```text
2292 B ORIGINAL MEANING
Observed largest free contiguous INTERNAL|8BIT heap block on Cardputer ADV at a
low-memory runtime checkpoint.

2292 B EVIDENCE QUALITY
PROVEN
```

`2292 B` is not total transient usage, total free DRAM, operation delta,
peak-live bytes, cumulative traffic, stack allowance, or a platform constant.

## Phase 2 — reconstruct `~11.4 KB`

```text
PreparedPhraseArrangement total   11,428 B
material[8]                       11,328 B
other fields                         100 B
```

The value is the size requirement of one historical contiguous heap request.
Because the request failed on the observed hardware state, the old OOM trace is
not evidence of a successful 11,428-byte peak followed by recovery. Stable
free/largest readings across repeated failures show that this particular failure
was not caused by progressive retained allocation.

Exact E0 no longer makes this request.

```text
~11.4 KB ORIGINAL MEANING
Historical size of one contiguous heap allocation request for the pre-PMB-P1
PreparedPhraseArrangement object: 11,428 B exact, dominated by 11,328 B
material[8].

~11.4 KB EVIDENCE QUALITY
PROVEN
```

## Phase 3 — `[PHRASE-OOM-PROBE]`

M0 searched frozen E0 source/docs/tests/scripts/workflows and relevant PMB
history for a literal committed instrument named `[PHRASE-OOM-PROBE]` with a
defined operation-scoped contract. No authoritative committed owner was
recovered.

Recoverable evidence is narrower:

1. historical `PHRASE PREPARE OOM` serial evidence;
2. historical paired free/largest measurements captured during PMB diagnosis;
3. PMB-P1 hardware acceptance instructions/results using repeated PHRASE G;
4. exact-E0 generic boot-only `logHeapCaps()` capability measurements.

Missing from exact E0:

- a phrase-operation pre checkpoint owned by a committed probe;
- operation-scoped minimum free internal DRAM;
- operation-scoped minimum largest internal block;
- operation-scoped peak-live attribution;
- settled post-operation recovery checkpoint;
- repeat-series drift owner;
- stack high-water measurement for the task executing the current stack-based
  PREPARE path.

```text
PHRASE-OOM-PROBE
INSUFFICIENT
```

M0 does not pretend the boot logger is an operation probe. No production
instrumentation is added in this checkpoint before hardware characterization;
measurement instrumentation on this constrained target can itself perturb
headroom, and the exact missing observations are now explicitly specified.

## Required memory model

### TOTAL FREE INTERNAL DRAM

```text
definition
  Aggregate currently-free internal 8-bit-capable heap bytes.
API
  heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
points
  stable baseline; immediately pre-PHRASE; immediately post-PHRASE; settled
  post-operation; repeated settled checkpoints
units
  bytes
proves
  aggregate heap headroom and retained aggregate change
not proves
  contiguous allocatability; stack headroom; unobserved operation peak;
  absence of fragmentation
```

### LARGEST FREE INTERNAL BLOCK

```text
definition
  Largest currently allocatable contiguous internal 8-bit heap block.
API
  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
points
  same paired checkpoints as total free internal DRAM
units
  bytes
proves
  contiguous heap allocatability at the checkpoint
not proves
  total free memory; transient consumption; stack headroom; future allocator
  state
```

### PEAK LIVE TRANSIENT INTERNAL DRAM

```text
definition
  Maximum simultaneously-live internal heap attributable to the bounded PHRASE
  interval relative to a valid operation baseline.
API
  no authoritative operation-resettable owner exists in frozen E0;
  heap_caps_get_minimum_free_size(...) is lifetime-since-boot and cannot by
  itself attribute a minimum to one PHRASE operation
points
  bounded PHRASE interval
units
  bytes
proves
  operation heap peak only with correctly scoped measurement
not proves
  stack peak; fragmentation; cumulative allocation traffic
```

Status: **MEASUREMENT GAP**.

### PHRASE OPERATION DELTA

```text
definition
  post_metric - pre_metric for the same capability-qualified metric.
API
  paired free-size / largest-block calls
points
  immediate pre/post and stable pre/settled-post
units
  bytes
proves
  retained checkpoint-to-checkpoint change
not proves
  peak live usage inside the interval
```

### POST-OPERATION RECOVERY

```text
definition
  Whether settled post-operation free/largest values return to the stable
  pre-operation envelope.
API
  paired free-size / largest-block calls
points
  stable pre and settled post
units
  bytes plus PASS/FAIL predicate
proves
  presence/absence of retained heap-state change above an evidence-defined
  tolerance
not proves
  exact transient peak
```

No numeric recovery tolerance is set without current exact-SHA hardware data.

### PROGRESSIVE / UNRECOVERED LOSS

```text
definition
  Directional deterioration in settled free/largest values across repeated
  equivalent PHRASE operations.
API
  repeated paired free-size / largest-block measurements
points
  settled checkpoint after each repetition
units
  bytes / trend
proves
  progressive retained loss when deterioration is directional and repeatable
not proves
  root cause; bounded non-directional allocator noise is not itself a leak
```

### FRAGMENTATION

```text
definition
  Loss of contiguous allocatability relative to aggregate free heap.
API
  interpret total free internal and largest free internal together
points
  paired pre/post/repeated settled checkpoints
units
  bytes; derived ratio/gap may be diagnostic only
proves
  worsening contiguity when largest degrades without equivalent aggregate loss
not proves
  allocator root cause from a single sample
```

No arbitrary fragmentation ratio is promoted to a release threshold.

### STACK HIGH-WATER MARK

The current PREPARE path uses bounded local/stack storage, so heap-only telemetry
is not a complete transient-memory model.

```text
definition
  Minimum remaining stack headroom / maximum observed stack use for the task
  executing PHRASE generation.
API
  FreeRTOS stack high-water API appropriate to the pinned ESP32/Arduino runtime;
  exact API and unit semantics must be verified before making it a gate
points
  controlled current-E0 PHRASE runtime / task lifetime high-water
units
  normalize to bytes only after API semantics are verified
proves
  stack headroom for the executing task
not proves
  heap headroom or fragmentation
```

Status: **REQUIRED FOR A COMPLETE CURRENT-E0 TRANSIENT MODEL; NOT YET MEASURED**.

### PSRAM / EXTERNAL MEMORY

The historical `2292 B` evidence is explicitly `INTERNAL|8BIT`. The old
11,428-byte request was not explicitly routed through a PSRAM-capability owner.
PSRAM therefore cannot be used to reinterpret the old comparison or inflate the
new internal-memory threshold. Any allocation-routing change is a separate
implementation task, outside M0.

## Five required answers

### Q1 — What exactly was 2292 B?

**Observed largest free contiguous INTERNAL|8BIT heap block on Cardputer ADV at
low-memory runtime checkpoints.**

### Q2 — What exactly was ~11.4 KB?

**One historical contiguous 11,428-byte heap allocation request for the old
`PreparedPhraseArrangement`.**

### Q3 — Are these the same metric?

```text
NO
```

They are different metrics but were legitimately comparable for one old
feasibility predicate:

```text
requested_contiguous_bytes <= largest_available_contiguous_bytes
11428 <= 2292 -> false
```

### Q4 — Is there an actual runtime conflict?

```text
HISTORICAL PRE-PMB-P1 CONFLICT    YES
CURRENT E0 CONFLICT               UNPROVEN

REAL CONFLICT
UNPROVEN
```

The historical conflict was observed. That specific allocation no longer exists.
Current E0 runtime headroom/recovery still requires current hardware evidence.

### Q5 — Is `all transient < 2292 B` a valid release invariant?

```text
NO — NEVER VALID
```

No authoritative source defines `2292 B` as total transient usage.

## Release-contract resolution

### Invalid rule removed

Do not propagate:

```text
all transient memory < 2292 B
```

It has the wrong metric, wrong inequality semantics, no capability region, no
checkpoint, treats one observed runtime value as a platform constant, and omits
stack headroom.

### Contract already authoritative

The existing fixed/static DRAM gate remains valid and independent of runtime
transient characterization:

```text
bash scripts/check_cardputer_dram_budget.sh <Cardputer ADV ELF>
```

GF2-E0 aggregates it through:

```text
bash scripts/validate_gf2_targets.sh --all
```

### Runtime contract shape justified by M0

On Cardputer ADV, at the **exact candidate SHA**, after a stable production boot:

1. measure `free_internal` and `largest_internal` using
   `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` immediately before PHRASE;
2. run a successful representative PHRASE operation;
3. capture the same metrics immediately after and at a settled checkpoint;
4. repeat successful PHRASE sufficiently to reveal retained loss/fragmentation
   (historical PMB-P1 used 20+ repetitions);
5. exercise normal navigation / PLAY / STOP / edit state and repeat PHRASE;
6. measure stack high-water with a verified API/unit contract;
7. fail on allocation/stack failure or progressive unrecovered loss;
8. derive absolute minimum free-internal, largest-block, stack-headroom and
   recovery-tolerance thresholds only from exact-SHA hardware evidence plus an
   explicit safety rationale.

The following numeric fields are intentionally unset:

```text
minimum free_internal threshold
minimum largest_internal threshold
minimum stack headroom threshold
post-operation recovery tolerance
```

Reusing `2292 B` as any of these would recreate the provenance error M0 exists
to remove.

## Required current-E0/M0 hardware experiment

```text
TARGET
M5Stack Cardputer ADV / ESP32-S3
exact M0 candidate SHA
normal production boot/runtime configuration

SCENARIO
A. cold boot -> stable baseline
B. immediately before PHRASE
C. successful PHRASE operation
D. immediately after PHRASE
E. settled post-operation
F. repeat successful PHRASE >= 20 times
G. navigation + PLAY + STOP + edit + successful PHRASE

SAVE AT A/B/D/E AND EACH REPEATED SETTLED CHECKPOINT
free_internal_bytes
largest_internal_bytes

ALSO SAVE
PHRASE status/result
allocation/stack failure if any
stack high-water with verified units
exact firmware SHA
device/target
scenario identity
```

Interpretation:

```text
normal transient behavior
  operation perturbs memory but settled values return to the stable envelope

retained allocation
  settled aggregate free remains lower

fragmentation drift
  settled largest block trends downward without equivalent aggregate-free loss

progressive leak/loss
  settled free and/or largest deteriorate attempt over attempt

measurement noise
  bounded non-directional variation without progressive trend
```

A successful build is not runtime evidence.

## Current blocker

```text
GF2-M0 STATUS
BLOCKED ON HARDWARE RUNTIME EVIDENCE

BLOCKER
No current exact-E0/M0 Cardputer ADV operation-scoped free_internal /
largest_internal / stack-headroom series exists from which numeric runtime
thresholds and recovery tolerance can be justified.
```

Historical PMB-P1 hardware evidence proves the old OOM fix on `a52cf32...`; it
does not establish the final transient release contract for E0/M0.

## Regression contract

Before M0 can become GREEN, the final head must run:

```text
bash scripts/validate_gf2_targets.sh --all
```

Required result:

```text
HOST                 PASS
SDL                  PASS
CARDPUTER ADV         PASS
FIXED DRAM            PASS
SEQTRAK MIDI-ONLY     PASS
```

The HOST gate executes:

```text
tests/run_host_tests.sh
tests/run_generation_0_9_9_c_tests.sh
tests/run_gf2_c2_v0r_tests.sh
```

so the final matrix also protects the GF2-I0R and GF2-C2-V0R regression chains.
Previous E0 evidence is not relabeled as a final-M0-head run.

## Current checkpoint summary

```text
2292 B ORIGINAL MEANING
largest free contiguous INTERNAL|8BIT heap block; historical runtime sample

2292 B EVIDENCE QUALITY
PROVEN

~11.4 KB ORIGINAL MEANING
11,428 B old single contiguous PreparedPhraseArrangement heap request

~11.4 KB EVIDENCE QUALITY
PROVEN

SAME METRIC
NO

HISTORICAL REAL CONFLICT
YES

CURRENT E0 REAL CONFLICT
UNPROVEN

REAL CONFLICT
UNPROVEN

OLD GLOBAL 2292 B INVARIANT
NEVER VALID

AUTHORITATIVE MEMORY INVARIANT
STATIC: existing fixed DRAM gate remains authoritative.
RUNTIME: metrics and failure predicates are defined; numeric exact-SHA
free/largest/stack/recovery gates remain BLOCKED pending Cardputer ADV evidence.

PHRASE-OOM-PROBE
INSUFFICIENT

RUNTIME HARDWARE EVIDENCE
REQUIRED BUT UNAVAILABLE

TARGET BUILD VALIDATED
YES at frozen E0; final M0 head must be rerun before GREEN

MEMORY OPTIMIZATION PERFORMED
NO

PRODUCTION MUSICAL SEMANTIC DELTA
NONE

GF2-I1
NOT STARTED

GF2-C2 GATE B
NOT STARTED
```

## Stop condition

Do not optimize memory and do not start GF2-I1 or GF2-C2 Gate B from this branch.
The only next M0 actions are:

1. obtain exact-SHA Cardputer ADV runtime evidence described above;
2. derive justified thresholds/recovery tolerance;
3. update this contract;
4. run the authoritative target matrix on the final head;
5. decide GREEN or open a separate memory implementation follow-up if a real
   runtime defect is demonstrated.
