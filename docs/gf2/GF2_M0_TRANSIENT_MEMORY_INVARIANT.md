# GF2-M0 — Transient Memory Invariant Resolution

Status: **BLOCKED ON CURRENT-E0 HARDWARE RUNTIME EVIDENCE**

This is a research / characterization / release-contract checkpoint. It is not a
memory optimization task and does not authorize GF2-I1 or GF2-C2 Gate B.

## Frozen ancestry

```text
GF2-E0 exact base
commit 36981c72dd481118b3aa966a4e5c143d39bab270
tree   ff7fc33cd1edba54ff0df7a1a0fa7c1bd657c00a
branch infra/20260902-03-0.9.10-gf2-e0-target-validation-environment

GF2-M0 branch
research/20260902-04-0.9.10-gf2-m0-transient-memory-invariant
```

The M0 branch was created directly from the exact E0 commit. E0 remains frozen.
No production C++ is changed by M0.

The GitHub-side checkout proves the exact commit and tree. The research runner
used for M0 could not perform the requested local `git status --short` /
`git rev-parse` sequence because outbound DNS to github.com was unavailable in
that runtime; no alternate local checkout or synthetic merge SHA was used.

## Scope firewall

```text
MEMORY OPTIMIZATION PERFORMED        NO
PRODUCTION MUSICAL SEMANTIC DELTA    NONE
GF2-I1                               NOT STARTED
GF2-C2 GATE B                        NOT STARTED
```

M0 does not change allocator strategy, buffer ownership/lifetime, GENRE, RECIPE,
FEEL, PHRASE musical semantics, GenerationCorridor, DEPTH, migration, generation
selection, musical material, or UI behavior.

## Executive conclusion

The apparent inequality

```text
~11.4 KB > 2292 B
```

was real in the old pre-PMB-P1 implementation, but the two numbers were never
the same metric:

- `2,292 B` was an **observed largest free contiguous INTERNAL|8BIT heap block**
  on Cardputer ADV after the normal boot/runtime footprint had settled;
- `11,428 B` was the **size of one contiguous C++ heap allocation request** for
  the old `PreparedPhraseArrangement` object, of which 11,328 B was the inline
  eight-bar `material[8]` array.

The old implementation therefore had a genuine allocation-feasibility conflict:
`request_size=11,428 > largest_free_block=2,292`, and real hardware produced
`PHRASE PREPARE OOM`.

PMB-P1 removed that allocation. On the exact E0 source,
`GeneratedPhraseSong::generate()` uses caller/local stack storage
`PreparedPhraseArrangement preparedStorage{}` and the old
`new(std::nothrow) PreparedPhraseArrangement(...)` request no longer exists.
The `OutOfMemory` result/text is retained for API compatibility but is documented
as unreachable from that call site.

Therefore **"all transient memory must remain below 2292 B" was never a valid
release invariant**. It conflates an availability measurement with a usage
measurement and loses the memory region, contiguity, checkpoint, and lineage.

What remains unresolved is the absolute runtime headroom contract for the
current E0 tree. Historical PMB-P1 hardware evidence is relevant lineage, but
E0 is 15 commits after the exact hardware-green implementation SHA and includes
later UI/PHRASE production changes. E0 target validation is build evidence, not
current hardware runtime evidence. A fresh E0 Cardputer ADV run is therefore
required before M0 may publish an absolute numeric free-internal/largest-block
threshold.

## Phase 0 — evidence inventory and classification

### E1 — earliest recoverable repository evidence for `2292`

Source:

```text
commit b256ff180165e8db37e61be8658b13c0ae2bcd5c
2026-08-02
docs/audits/SMF_STABILITY_REGRESSION_AUDIT_2026-08-02.md
```

The file was introduced by that commit. Its hardware section records:

```text
freeInt=4100
largest=2292
audio underruns=0
```

and explicitly describes RAM as low but stable. The value is not called a
transient allocation limit.

Classification: **AUTHORITATIVE RUNTIME MEASUREMENT / HISTORICAL NOTE**.

Caveat: this is the earliest recoverable committed repository evidence found by
M0, not a claim that no earlier uncommitted serial log ever contained the same
number.

### E2 — PMB-A1 memory audit

Source:

```text
commit 03f633d2e431c75ec9b2f4402507b9253f785a74
branch research/20260830-01-0.9.9-phrase-memory-budget-audit
docs/contracts/0_9_9_PHRASE_MEMORY_BUDGET_STREAMING_PREPARE_AUDIT.md
```

The frozen audit records the cold-boot heap trajectory and first failing G:

```text
setup steady state        freeInt ~= 6440   largest = 2292
first G                   free    = 6180   largest = 2292
required single new       11428 bytes contiguous
```

It defines `2,292 B` as the observed largest contiguous free block and warns
that it is an observed value from one boot/session, **not a guaranteed allocator
contract**. It also records repeated failed G attempts with stable free memory
and constant largest block, ruling out progressive fragmentation/leak as the
cause of that old failure.

Classification:

```text
2,292 B runtime sample     AUTHORITATIVE RUNTIME MEASUREMENT
2,292 B as universal cap   NEVER AN AUTHORITATIVE CONTRACT
candidate <= floor rule    RESEARCH / HEURISTIC CANDIDATE
```

### E3 — exact old 11.4 KB decomposition

The same PMB-A1 audit compiler-froze the old object sizes:

```text
PhraseGenerator::PhraseBar                         1,416 B
material[8]                                        11,328 B
PreparedPhraseArrangement                          11,428 B
PreparedPhraseExecution                               324 B
PhraseExecutionScratch                              1,416 B
```

The old path used a single ordinary C++
`new(std::nothrow) PreparedPhraseArrangement(...)`. No explicit
`MALLOC_CAP_SPIRAM`/PSRAM allocation owner was present for that request.

Classification: **TEST CONTRACT + DERIVED ALLOCATION REQUIREMENT**, corroborated
by **AUTHORITATIVE RUNTIME FAILURE** (`PHRASE PREPARE OOM`).

### E4 — PMB-P1 production replacement

Relevant implementation/hardware SHA:

```text
a52cf32a2cb6ec4ebea8f846574db19667fb3015
fix(0.9.9-pmb-p1): stream phrase materialization
```

`a52cf32...` is a direct ancestor of E0. Comparing it to E0 gives
`ahead_by=15`, `behind_by=0` for E0.

PMB-P1 removed the one 11,428 B heap request and changed PREPARE/COMMIT to a
bounded one-bar replay strategy. Exact E0 retains that architecture.

Classification: **PRODUCTION IMPLEMENTATION / TEST CONTRACT**.

### E5 — PMB-P1 historical hardware-green record

A later documentation-only commit on the PMB-P1 side branch:

```text
df426a5d596888580b88499b0b1aad03bc9006e4
```

records Cardputer ADV hardware acceptance against exact implementation SHA
`a52cf32...` / PR #410:

```text
before first G          free ~= 6180 B   largest ~= 2292 B
1B / 2B / 4B G          PASS
PREPARE_OOM              zero occurrences
repeated G               largest stayed 2292 B; no monotonic decrease
post-navigation G        PASS; largest unchanged
```

The documentation commit itself is not an E0 ancestor; its merge base with E0
is the hardware-tested `a52cf32...` implementation SHA. This is valid historical
runtime evidence for that exact implementation lineage, not a fresh measurement
of the E0 head.

Classification: **AUTHORITATIVE HISTORICAL RUNTIME EVIDENCE**, current-E0
threshold applicability **STALE / REQUIRES RE-MEASUREMENT**.

### E6 — exact E0 source

At exact E0 SHA `36981c72...`:

- `src/dsp/generated_phrase_song.h` uses
  `PreparedPhraseArrangement preparedStorage{}`;
- the old heap allocation for PREPARE is absent;
- one-bar materialization/replay remains the implementation strategy;
- `GroovePuter.ino` exposes generic ESP32 heap telemetry using
  `heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)` and
  `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`;
- boot logs include a `POST-BOOT` checkpoint and periodic `[PERF]` output.

Classification: **AUTHORITATIVE CURRENT SOURCE / MEASUREMENT CAPABILITY**.

Generic periodic telemetry is not equivalent to an operation-scoped PHRASE
peak/recovery probe.

## Phase 1 — reconstruct `2292 B`

### Answer matrix

| Question | Finding |
| --- | --- |
| Where first recoverably appears? | `b256ff18...`, SMF stability audit, 2026-08-02 |
| Operation/context? | Cardputer ADV post-boot/SMF runtime; later reproduced immediately before PHRASE G |
| Exact metric? | largest free contiguous `INTERNAL|8BIT` heap block |
| Target? | Cardputer ADV / ESP32-S3 |
| Runtime point? | low-memory steady state after normal boot/runtime initialization; PMB-A1 also immediately before first G |
| Internal or total heap? | INTERNAL + 8BIT capability-qualified heap |
| Total free or largest block? | largest block |
| Absolute or delta? | absolute available block size at checkpoint |
| Peak/live allocation or headroom? | remaining contiguous headroom |
| Single allocation or aggregate? | neither; allocator availability metric |
| Runtime or estimate? | runtime measurement |
| Approved global release invariant? | no evidence; PMB-A1 explicitly warns against that interpretation |
| Context lost later? | yes, when paraphrased as a generic `2292 B transient` limit |

### Determination

```text
2292 B ORIGINAL MEANING
Observed largest free contiguous INTERNAL|8BIT heap block on Cardputer ADV
at a low-memory post-boot/runtime checkpoint.

2292 B EVIDENCE QUALITY
PROVEN
```

`2,292 B` is **not**:

- total transient usage;
- total free internal DRAM;
- a before/after delta;
- a peak live allocation;
- a cumulative allocation total;
- stack allowance;
- a platform constant;
- an E0 release threshold.

## Phase 2 — reconstruct `~11.4 KB`

### Exact value

The approximate label `~11.4 KB` refers to the old allocation requirement:

```text
PreparedPhraseArrangement total     11,428 B
  material[8]                        11,328 B
  other fields                          100 B
```

It was one contiguous heap request made through ordinary
`new(std::nothrow)`. It was not eight independent allocations and not cumulative
allocator traffic.

### Lifetime and recovery semantics

The request failed on the old Cardputer ADV runtime because no contiguous block
was large enough. Consequently the historical OOM case does not provide a
successful 11,428 B peak-live interval followed by recovery; it provides a
failed single-allocation request. The observed free/largest values remained
stable across repeated failures, which argues against retained allocation/leak
for that specific old failure mode.

### Current E0 relevance

The old allocation owner has been removed. The exact E0 PREPARE path uses stack
storage and bounded per-bar materialization. Therefore `11,428 B` is historical
pre-PMB-P1 allocation provenance, **not a measured current-E0 transient
footprint**.

```text
~11.4 KB ORIGINAL MEANING
Historical size of one contiguous heap allocation request for the pre-PMB-P1
PreparedPhraseArrangement object (11,428 B exact; 11,328 B inline material[8]).

~11.4 KB EVIDENCE QUALITY
PROVEN
```

## Phase 3 — `[PHRASE-OOM-PROBE]`

M0 searched the frozen E0 source/docs/tests/scripts/workflows and relevant PMB
history for the literal committed instrument `[PHRASE-OOM-PROBE]` and did not
recover an authoritative committed implementation with defined operation-scoped
semantics.

What is recoverable:

1. historical serial/PMB evidence carrying `PHRASE PREPARE OOM` and paired
   free/largest measurements;
2. the PMB-P1 hardware acceptance protocol, which explicitly requires paired
   `heap_caps_get_free_size` and `heap_caps_get_largest_free_block` measurements
   before/after G and repeated-operation observation;
3. current E0 generic `POST-BOOT` and periodic `[PERF]` heap telemetry.

What is not recoverable as a committed probe contract:

- a literal `[PHRASE-OOM-PROBE]` owner;
- a defined phrase-operation minimum free value;
- a defined phrase-operation minimum largest block value;
- an operation-resettable peak-live measurement;
- a post-cleanup settled checkpoint owned by the probe;
- a stack high-water measurement for the current stack-based PREPARE path.

```text
PHRASE-OOM-PROBE
INSUFFICIENT
```

M0 intentionally does not add a production probe before hardware characterization.
On this memory-constrained target, instrumentation itself can perturb stack/heap
headroom; adding a new measurement owner without a proven gap-specific design
would weaken rather than strengthen the characterization.

## Required memory model

### 1. TOTAL FREE INTERNAL DRAM

```text
definition
  Aggregate bytes currently free in capability-qualified internal 8-bit heap.

API
  heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)

measurement point
  stable boot baseline; immediately pre-PHRASE; immediately post-PHRASE;
  settled post-operation; repeated settled checkpoints

units
  bytes

proves
  aggregate internal heap headroom and retained aggregate change

does not prove
  one allocation of that size can succeed; stack headroom; operation peak;
  absence of fragmentation
```

### 2. LARGEST FREE INTERNAL BLOCK

```text
definition
  Largest currently allocatable contiguous block in internal 8-bit heap.

API
  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)

measurement point
  same paired checkpoints as TOTAL FREE INTERNAL DRAM

units
  bytes

proves
  current contiguous heap-allocation ceiling for that capability set

does not prove
  total free memory; transient usage; stack headroom; future allocator state
```

### 3. PEAK LIVE TRANSIENT INTERNAL DRAM

```text
definition
  Maximum simultaneously-live internal heap bytes attributable to the bounded
  operation interval, relative to an operation baseline.

API
  No authoritative operation-scoped owner is present in frozen E0.
  heap_caps_get_minimum_free_size(...) is lifetime-since-boot minimum and cannot
  by itself attribute a minimum to one PHRASE operation.

measurement point
  bounded PHRASE operation interval

units
  bytes

proves
  operation-attributable heap peak only if measurement is correctly scoped

does not prove
  stack peak; fragmentation; cumulative allocation traffic
```

Status: **MEASUREMENT GAP for an absolute current-E0 peak claim**.

### 4. PHRASE OPERATION DELTA

```text
definition
  post_metric - pre_metric for the same metric/capability/checkpoint pair.

API
  paired free-size / largest-block calls

measurement point
  immediately before PHRASE and immediately after synchronous completion;
  additionally at a settled post-operation checkpoint

units
  bytes

proves
  retained change between checkpoints

does not prove
  peak live usage during the interval
```

### 5. POST-OPERATION RECOVERY

```text
definition
  Whether settled post-operation free/largest values return to the stable
  pre-operation envelope.

API
  paired free-size / largest-block calls

measurement point
  stable pre and settled post

units
  bytes plus PASS/FAIL predicate

proves
  absence/presence of retained heap-state change above the evidence-defined
  tolerance

does not prove
  exact peak during the operation
```

The numeric tolerance is **not set by M0 without current E0 hardware data**.

### 6. PROGRESSIVE / UNRECOVERED LOSS

```text
definition
  Directional deterioration in settled free/largest observations across
  repeated equivalent PHRASE operations.

API
  repeated paired free-size / largest-block measurements

measurement point
  settled post-operation checkpoint for each repetition

units
  bytes per repetition / trend

proves
  progressive retained loss when a consistent downward trend exists

does not prove
  root cause; one-off allocator noise by itself is not a leak
```

Historical PMB-P1 acceptance used repeated G with no monotonic decrease in the
largest block. M0 retains that as a useful **predicate shape**, not as fresh E0
runtime evidence.

### 7. FRAGMENTATION

```text
definition
  Loss of contiguous allocatability relative to aggregate free heap.

API
  inspect TOTAL FREE INTERNAL DRAM and LARGEST FREE INTERNAL BLOCK together

measurement point
  same pre/post/repeated settled checkpoints

units
  bytes; optionally derived ratio/gap for diagnosis

proves
  worsening contiguity when largest deteriorates without equivalent aggregate
  free loss

does not prove
  allocator cause from one sample
```

No arbitrary fragmentation ratio is promoted to a release threshold by M0.

### 8. STACK HIGH-WATER MARK

Current PMB-P1/E0 PREPARE moved the arrangement carrier and scratch lifetime away
from the old heap allocation and into bounded stack/local storage. Heap-only
telemetry therefore cannot characterize the complete transient working set.

```text
definition
  Minimum remaining stack headroom / maximum stack consumption for the task
  executing PHRASE generation.

API
  FreeRTOS task stack high-water API appropriate to the pinned ESP32/Arduino
  runtime; exact API/unit contract must be verified before adding a release gate.

measurement point
  before/after controlled PHRASE runtime and/or task lifetime high-water

units
  MUST be normalized to bytes after verifying the pinned API semantics

proves
  stack headroom for the executing task

does not prove
  heap headroom or fragmentation
```

Status: **REQUIRED FOR A COMPLETE CURRENT-E0 TRANSIENT MODEL; NOT YET MEASURED**.

### 9. PSRAM / EXTERNAL MEMORY

Current generic telemetry exposes PSRAM/free-PSRAM information, but the historical
11,428 B request was not explicitly routed to PSRAM and the `2,292 B` evidence
is specifically INTERNAL|8BIT. External RAM must not be used to reinterpret or
inflate an internal-memory threshold unless a production allocation owner is
explicitly changed in a separate implementation task.

## Five required answers

### Q1 — What exactly was 2292 B?

**Observed largest free contiguous INTERNAL|8BIT heap block on Cardputer ADV at
post-boot/runtime low-memory checkpoints.**

### Q2 — What exactly was ~11.4 KB?

**One historical contiguous heap allocation request: 11,428 B for the old
`PreparedPhraseArrangement`, dominated by the inline eight-bar `material[8]`.**

### Q3 — Are these the same metric?

```text
NO
```

They are different metrics but were correctly comparable for the old allocation
feasibility question:

```text
requested contiguous allocation size <= largest available contiguous block ?
11428 <= 2292 -> false
```

### Q4 — Is there an actual runtime conflict?

```text
HISTORICAL PRE-PMB-P1 CONFLICT    YES
CURRENT E0 CONFLICT               UNPROVEN
```

The historical conflict was observed and fixed structurally. The specific old
11,428 B request no longer exists on E0. Current E0 absolute headroom/recovery
cannot be declared conflict-free without current hardware runtime evidence.

For the required single current-checkpoint classification:

```text
REAL CONFLICT
UNPROVEN
```

### Q5 — Is `all transient < 2292 B` a valid release invariant?

```text
NO — NEVER VALID
```

No recovered contract defines `2292 B` as total transient usage. The statement
mixes a largest-free-block availability sample with transient consumption and
must not be used as a release gate.

## Release-contract resolution

### Removed interpretation

The following global rule is explicitly invalid and must not be propagated:

```text
all transient memory < 2292 B
```

Reason:

1. wrong metric (`largest free block`, not transient usage);
2. wrong semantic direction (availability floor vs usage ceiling);
3. missing region/capability;
4. missing runtime checkpoint;
5. historical single-session absolute value treated as a platform constant;
6. ignores stack, which is now relevant to PREPARE.

### Contract that is already authoritative

The existing **fixed/static DRAM gate remains authoritative** and is independent
of transient runtime characterization:

```text
bash scripts/check_cardputer_dram_budget.sh <Cardputer ADV ELF>
```

GF2-E0 aggregates it through:

```text
bash scripts/validate_gf2_targets.sh --all
```

### Runtime contract shape that M0 can justify

On Cardputer ADV, at the exact candidate SHA, after a stable normal boot:

1. capture paired `free_internal` and `largest_internal` with
   `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` immediately before PHRASE;
2. execute a successful representative PHRASE operation;
3. capture the same pair immediately after and at a settled checkpoint;
4. repeat the operation enough times to expose retained loss/fragmentation
   (historical PMB-P1 used 20+ repeated G operations);
5. exercise normal navigation/PLAY/STOP/edit state and repeat PHRASE;
6. record task stack high-water in bytes using a verified API/unit contract;
7. fail on allocation/stack failure or evidence of progressive unrecovered loss;
8. set absolute minimum free-internal, largest-block, and stack-headroom gates
   only from the current exact-SHA hardware evidence plus an explicitly justified
   safety margin.

### Why this is not yet the final numeric release contract

M0 does **not** have current-E0 Cardputer ADV runtime samples for these exact
checkpoints. The historical `2292 B` value cannot supply the missing current-E0
absolute threshold because the runtime tree moved 15 commits after the tested
PMB-P1 implementation and E0 itself explicitly proves builds, not hardware
runtime.

Therefore the fields

```text
minimum free_internal threshold
minimum largest_internal threshold
minimum stack headroom threshold
post-operation recovery tolerance
```

remain intentionally **UNSET**.

Publishing guessed numbers here would violate the M0 central rule: define the
metric before optimizing or gating it.

## Required current-E0 hardware experiment

Target:

```text
M5Stack Cardputer ADV / ESP32-S3
exact M0 candidate SHA
normal production boot/runtime configuration
```

Scenario:

```text
A. cold boot -> stable baseline
B. immediately before PHRASE
C. PHRASE operation
D. immediately after PHRASE
E. settled post-operation
F. repeat successful PHRASE >= 20 times
G. navigation + PLAY + STOP + edit + another successful PHRASE
```

At A/B/D/E and every repeated settled checkpoint, save:

```text
free_internal_bytes
largest_internal_bytes
```

Additionally save:

```text
PHRASE result/status
allocation failure if any
stack high-water normalized to bytes
exact firmware SHA
device/target
boot/runtime scenario
```

If a correctly scoped peak measurement is added later, its instrumentation cost
must itself be characterized. A build succeeding is not a substitute for these
runtime observations.

Interpretation rules:

```text
normal transient peak
  values dip during/around the operation but recover to the stable envelope

retained allocation
  settled total free remains lower after the operation

fragmentation drift
  settled largest block trends downward without equivalent total-free loss

progressive leak/loss
  settled free and/or largest deteriorate attempt over attempt

measurement noise
  bounded non-directional variation without progressive trend
```

## Current blocker

```text
GF2-M0 STATUS
BLOCKED ON HARDWARE RUNTIME EVIDENCE

BLOCKER
No current-E0 Cardputer ADV operation-scoped free_internal / largest_internal /
stack-headroom evidence exists from which an absolute numeric runtime release
threshold and recovery tolerance can be justified.
```

Historical PMB-P1 hardware evidence proves that the old OOM fix worked on
`a52cf32...`; it does not prove the final transient headroom contract for
`36981c72...` or the M0 head.

## Regression contract

M0 changes documentation only. Before M0 can become GREEN, the final head must
still run:

```text
bash scripts/validate_gf2_targets.sh --all
```

Required matrix:

```text
HOST                 PASS
SDL                  PASS
CARDPUTER ADV         PASS
FIXED DRAM            PASS
SEQTRAK MIDI-ONLY     PASS
```

The HOST gate includes the existing 0.9.9-C / GF2-I0R and GF2-C2-V0R chains via
`scripts/validate_gf2_targets.sh`.

M0 does not reinterpret previous E0 evidence as a final-head regression run.
Final-head results remain required before GREEN.

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

OLD GLOBAL 2292 B INVARIANT
NEVER VALID

AUTHORITATIVE MEMORY INVARIANT
STATIC: existing fixed DRAM gate remains authoritative.
RUNTIME: metric model and failure predicates are defined, but absolute current-E0
free/largest/stack thresholds are BLOCKED pending exact-SHA Cardputer ADV evidence.

PHRASE-OOM-PROBE
INSUFFICIENT

RUNTIME HARDWARE EVIDENCE
REQUIRED BUT UNAVAILABLE FOR CURRENT E0

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
The next action for M0 is only to acquire the exact-SHA hardware runtime evidence
above, derive justified thresholds, update this contract, rerun the authoritative
E0 matrix on the final head, and then decide GREEN vs a separate memory
implementation follow-up.
