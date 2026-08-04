# Cardputer memory baseline

## Purpose

Establish an evidence-based internal-RAM baseline for Cardputer ADV before
changing the fixed `.dram0.data + .dram0.bss` gate or moving large objects.

This stage is intentionally separate from Hub MIDI PR #65. It changes neither
Hub behavior nor product memory ownership. It adds a diagnostic build and an
ELF report only.

## Gate provenance

Repository history establishes the following sequence:

1. `scripts/check_cardputer_dram_budget.sh` previously defaulted to `191488` bytes.
2. The PR #63 merge changed the shared normal/MIDI-only default to `122880` bytes.
3. The same reviewed PR reported a final ELF of `122676 / 122880`, leaving only
   `204` bytes of fixed-gate headroom.

This proves where the current number entered the repository. The history found
so far does **not** document a derivation from measured worst-case runtime heap,
stack watermarks, fragmentation, or required contiguous allocation size.
Therefore `122880` currently behaves like a reviewed fixed-image ceiling close
to the PR #63 result, not yet like a demonstrated device safety boundary.

That observation does not prove the limit is too low. It means the limit must
be validated against runtime evidence before it is retained, lowered, or raised.

## Current baseline problem

Current `dev` produces approximately:

```text
.dram0.data  22584 B
.dram0.bss  147336 B
fixed total 169920 B
current gate 122880 B
excess       47040 B
```

Against the earlier `191488` reference, the same image has `21568 B` of fixed
headroom. Neither comparison is sufficient on its own: static fixed DRAM and
runtime heap compete for the same internal-RAM budget, while allocation success
also depends on contiguous block size and task stacks.

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
unchanged and making success depend on a contiguous ~25.8 KB block.

## Diagnostic build

The baseline profile instruments a temporary source copy only:

```bash
bash scripts/build_cardputer_memory_baseline.sh normal --warnings all
bash scripts/build_cardputer_memory_baseline.sh midi-only --warnings all
```

The ordinary checkout and product build remain untouched. The diagnostic build
adds less than 64 bytes of watermark state and emits:

```text
[MEM-BASE] phase=periodic ms=... freeInt=... minFreeBoot=...
           startBootFloor=... minFreeRuntimeSample=...
           largest=... minLargestRuntimeSample=...
           loopStackWords=... audioStackWords=... integrity=1
```

Metric meanings:

- `freeInt`: current free internal 8-bit heap;
- `minFreeBoot`: ESP-IDF minimum free internal heap since boot; this captures
  transient total-heap lows even when they occur between periodic log lines;
- `startBootFloor`: `minFreeBoot` at `setup()` completion;
- `minFreeRuntimeSample`: lowest 10 ms sampled free heap after setup;
- `largest`: current largest contiguous internal allocation;
- `minLargestRuntimeSample`: lowest sampled largest block after setup;
- `loopStackWords` / `audioStackWords`: FreeRTOS high-water marks in words;
- `integrity`: result of `heap_caps_check_integrity_all(false)`.

`minLargestRuntimeSample` is sampled, not an allocator-provided historical
minimum. A short allocation that is created and freed inside one blocking call
may not be observed. Any candidate relocation must therefore add focused
before/during/after measurement at its actual allocation site before acceptance.

## ELF report

For any compiled image:

```bash
bash scripts/report_cardputer_memory_baseline.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

The report prints:

- `.dram0.data`, `.dram0.bss`, and their sum;
- delta to both `122880` and historical `191488` references;
- exact candidate symbols found in the ELF;
- the largest fixed DRAM symbols;
- one machine-readable `MEMORY_BASELINE` summary line.

The report always exits successfully. It is evidence collection, not a gate.
The existing `check_cardputer_dram_budget.sh` remains the sole mandatory gate.

## Hardware measurement matrix

Run both normal CDC+MIDI and SEQTRAK MIDI-only profiles. For each profile, use at
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

## Decision model

Only after the matrix is recorded should the baseline PR choose one of these
outcomes.

### A. Return the baseline below `122880`

Use this when one or more low-risk reductions remove genuinely unnecessary
resident state and preserve runtime minimum heap, contiguous-block headroom,
and stack margins.

### B. Recalculate the gate

Use this when the current static image demonstrates a repeatable safe runtime
reserve and the existing limit is shown to be a PR-head-sized ceiling rather
than a hardware-derived boundary.

A first-order upper bound for additional fixed globals is:

```text
measured fixed total
+ measured worst-case free internal heap
- required runtime reserve
```

This is not sufficient by itself. The chosen limit must also preserve the
required largest contiguous block and task stack margins.

### C. Combine a small optimization with a justified limit change

Use this when a modest reduction removes clear waste but returning all the way
to `122880` would require risky relocation, duplicate work, or loss of atomic
behavior.

## Acceptance boundaries

- **No gate change** before normal and MIDI-only hardware measurements exist.
- No PSRAM assumption: Cardputer ADV remains `PSRAM=disabled`.
- No blind static-to-heap relocation.
- No removal of transactional scene rollback without an explicit replacement.
- No change to #65 or its stacked branches.
- No direct TinyUSB, scheduler, RX, clock, or note-ownership changes.
- The ordinary Core regression fixed-DRAM step remains red while `dev` exceeds
  the current gate; the baseline workflow must not hide that failure.
