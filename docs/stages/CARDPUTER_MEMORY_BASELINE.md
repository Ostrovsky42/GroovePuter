# Cardputer memory baseline

## Purpose

Establish an evidence-based internal-RAM policy for Cardputer ADV before moving
large objects or declaring a new fixed `.dram0.data + .dram0.bss` ceiling.

This stage is intentionally separate from Hub MIDI PR #65. It changes neither
Hub behavior nor product memory ownership. It adds a diagnostic build, restores
the last pre-`122880` ceiling provisionally, and collects the missing ELF and
runtime evidence.

## Gate provenance

Repository history establishes the following sequence.

1. `scripts/check_cardputer_dram_budget.sh` previously defaulted to `191488`
   bytes.
2. Commit `81689b46448363b91db5256575b30cc1e53c2300` (`81689b4`,
   `ci: enforce measured Cardputer DRAM ceiling`) made a one-line change from
   `191488` to `122880`.
3. The introducing commit contains no calculation, measurement artifact,
   runtime reserve, profile distinction, or explanation of how `122880` was
   derived.
4. PR #63 later reported an ELF result of `122676 / 122880`, leaving `204`
   bytes. That demonstrates proximity to the chosen number; it does not derive
   the number as a device safety boundary.

One related claim must be kept precise. At exact commit `81689b4`, the Core
workflow built to `build/cardputer-adv-current`, passed that same hyphenated ELF
path to the gate, and the gate returned exit code `2` when the ELF did not
exist. Therefore the unsupported provenance of `122880` is verified, but a
wrong-path or fail-open defect is not attributed to that exact commit without a
separate immutable revision that demonstrates it.

The previous `191488` value is also not accepted as a universal hardware limit.
Its original derivation has not yet been recovered as a repository artifact,
and a historical `190808` MIDI-only measurement mentioned during the audit has
not yet been tied to an immutable source commit, full FQBN, and exact ELF. In
particular, normal CDC+MIDI and MIDI-only are different binaries and may require
separate policies.

## Provisional gate rollback

The mandatory default is restored to `191488` only as a rollback of the
undocumented `122880` replacement. This removes an unsupported blocker from PR
#65 while the baseline study runs.

This rollback means:

- `191488` is a provisional repository ceiling, not a proven safe maximum;
- `122880` remains visible in reports as an unsupported historical reference;
- no Scene, wavetable, SMF, engine, USB, scheduler, or UI storage is moved;
- #65 remains responsible for its own compile, host, hardware, and functional
  acceptance;
- the final policy may be lower, higher, or profile-specific after measurement.

## First reproducible build results

The first #70 workflow artifacts were produced from:

```text
source  fff2ac54912029bc90fa97e70835aa97ce39c991
base    dev @ b43e23ff12fb0a31bcb77dd5ec57908889760013
PSRAM   disabled
```

Arduino reported:

```text
normal CDC+MIDI global variables  170016 B
MIDI-only global variables        169952 B
profile delta                         64 B
```

These are compiler summary totals, not yet the corrected section-by-section
baseline report. The original workflow piped the wrapper through `tee` without
`pipefail`; the build succeeded, but a later report failure could be masked and
the uploaded logs contained only compiler output. #70 now uses `set -o
pipefail`, captures stderr, records Source commit, full FQBN, ELF path and ELF
SHA-256, and fails if the report does not complete.

The current MIDI-only result does not reproduce the historical `190808` value.
That does not disprove the older measurement because the source revision and
binary may differ. It means the historical number cannot validate the current
gate until its original source/profile/ELF identity is recovered.

The close numerical match between an earlier `.bss` difference and two
`Scene` objects remains a useful hypothesis, not a proven conclusion. It must be
checked against exact historical ELF symbols rather than inferred from totals
alone.

## Primary investigation candidates

The ELF report calls out these objects without moving or deleting them:

| Candidate | Observed size | Main question |
|---|---:|---|
| `s_tempLoadScene` | 25800 B | Does it provide atomic/rollback scene loading, and what is the true peak if moved to heap? |
| Wavetable static arrays ×4 | 16384 B | Are all four tables required in internal writable RAM, or can representation/storage change without realtime regression? |
| `g_mainScene` | 25800 B | Is resident internal storage required, and would relocation merely exchange `.bss` for equally resident heap? |
| `g_smfPlayer` | 11392 B | Which portions are stacks, bounded queues, caches, or rarely used state? |
| `g_miniAcidInstance` | 8864 B | Which state is realtime-critical and which state is duplicated elsewhere? |

No candidate is presumed waste. In particular, replacing a static temporary
Scene with a heap allocation can reduce `.bss` while leaving peak physical RAM
unchanged and making success depend on a contiguous approximately 25.8 KB
block.

## Reproducible diagnostic build

Build both profiles from the same immutable checkout:

```bash
bash scripts/build_cardputer_memory_baseline.sh normal --warnings all
bash scripts/build_cardputer_memory_baseline.sh midi-only --warnings all
```

Every accepted build record must contain:

```text
Source commit: <40-hex SHA>
FQBN: <complete board options>
ELF path: <resolved file>
ELF sha256: <64-hex digest>
MEMORY_BASELINE fixed=... data=... bss=...
```

The diagnostic build instruments a temporary source copy only. It adds less
than 64 bytes of watermark state and emits:

```text
[MEM-BASE] phase=periodic ms=... freeInt=... minFreeBoot=...
           startBootFloor=... minFreeRuntimeSample=...
           largest=... minLargestRuntimeSample=...
           loopStackWords=... audioStackWords=... integrity=1
```

Metric meanings:

- `freeInt`: current free internal 8-bit heap;
- `minFreeBoot`: ESP-IDF minimum free internal heap since boot;
- `startBootFloor`: `minFreeBoot` at `setup()` completion;
- `minFreeRuntimeSample`: lowest 10 ms sampled free heap after setup;
- `largest`: current largest contiguous internal allocation;
- `minLargestRuntimeSample`: lowest sampled largest block after setup;
- `loopStackWords` / `audioStackWords`: FreeRTOS high-water marks in words;
- `integrity`: result of `heap_caps_check_integrity_all(false)`.

`minLargestRuntimeSample` is sampled, not an allocator-provided historical
minimum. A short allocation created and freed inside one blocking call may not
be observed. Any candidate relocation must add focused before/during/after
measurement at its actual allocation site before acceptance.

## ELF report

For any compiled image:

```bash
bash scripts/report_cardputer_memory_baseline.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

The report prints:

- `.dram0.data`, `.dram0.bss`, and their sum;
- headroom to provisional `191488`;
- comparison with unsupported `122880`;
- exact candidate symbols found in the ELF;
- the largest fixed DRAM symbols;
- one machine-readable `MEMORY_BASELINE` line.

The report always exits successfully after a valid analysis. Missing ELF or
binutils is an error. It is evidence collection, not the mandatory gate.

## Hardware measurement matrix

Run normal CDC+MIDI and SEQTRAK MIDI-only separately. For each profile, use at
least three cold boots and record the lowest result from the following sequence:

1. Idle for two minutes after full UI startup.
2. Visit every workflow and force creation of every lazy page.
3. Run both synths, drums, delay/distortion, and a dense internal pattern.
4. Load and play a small SMF.
5. Load and play the densest reproducible SMF until failure or ten minutes.
6. Exercise MIDI mute table, structural inspector, channel inspector, browser,
   and repeated page transitions during playback.
7. Save, load, save-as, and reload a project/scene.
8. Connect SEQTRAK, test GP MASTER and SEQ MASTER, Stop/Play, and source changes.
9. Stop the host from draining USB MIDI, then restore it and verify recovery.
10. Leave a mixed audio/UI/MIDI soak running for at least ten minutes.

Record for every scenario:

- minimum `minFreeBoot`;
- minimum `minFreeRuntimeSample`;
- minimum `minLargestRuntimeSample`;
- loop/audio stack watermarks;
- reset reason or WDT diagnostic;
- whether heap integrity ever becomes `0`;
- whether save/load and dense SMF complete.

MIDI-only has no CDC output in its production profile. Its runtime evidence
therefore needs a measurement channel that does not change the USB profile,
such as a bounded retained summary read by a later diagnostic boot. Enabling
CDC for convenience would measure a different binary.

## Threshold derivation rule

No new numeric threshold is accepted unless the changing commit shows how it
was obtained. The evidence must include:

1. Source commit and clean-tree status.
2. Full FQBN and toolchain/core versions.
3. ELF SHA-256 and exact `.dram0.data` / `.dram0.bss` values.
4. Separate normal and MIDI-only results, or an explicit reason one shared
   ceiling is conservative for both.
5. Worst-case `minFreeBoot`, `minFreeRuntimeSample`,
   `minLargestRuntimeSample`, and stack watermarks from the hardware matrix.
6. Declared required total-heap, contiguous-block, and stack reserves.
7. A calculation that shows how proposed fixed-DRAM growth preserves all those
   reserves, followed by a build at or near the proposed boundary.

A fixed linker total alone cannot establish runtime safety. Conversely, a low
sampled heap number alone cannot identify which static object should move. Both
ELF identity and runtime behavior are required.

## Decision model

Only after the corrected normal and MIDI-only ELF reports and hardware matrix
exist should #70 choose one of these outcomes.

### A. Return below the unsupported `122880` reference

Use this only when low-risk reductions remove genuinely unnecessary resident
state while preserving transactional behavior and runtime margins.

### B. Derive a new gate

Use this when the existing static image demonstrates repeatable safe runtime
reserve and a documented ceiling can be calculated from measured total-heap,
contiguous-block, and stack requirements. Normal and MIDI-only may receive
separate limits.

### C. Combine a small optimization with a justified gate

Use this when a modest reduction removes clear waste but forcing the image to
`122880` would require risky relocation, duplicate work, or loss of atomic
behavior.

## Acceptance boundaries

- `191488` remains explicitly provisional until runtime evidence exists.
- No PSRAM assumption: Cardputer ADV remains `PSRAM=disabled`.
- No blind static-to-heap relocation.
- No removal of transactional scene rollback without an explicit replacement.
- No change to #65 or its stacked branches.
- No direct TinyUSB, scheduler, RX, clock, or note-ownership changes.
- A missing ELF, failed report, or failed instrumented build must fail CI even
  when output is piped through `tee`.
