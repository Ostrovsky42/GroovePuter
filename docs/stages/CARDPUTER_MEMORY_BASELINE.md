# Cardputer memory baseline

## Purpose

Establish an evidence-based internal-RAM policy for Cardputer ADV before moving
large objects or declaring a new fixed `.dram0.data + .dram0.bss` ceiling.

This stage is intentionally separate from Hub MIDI PR #65. It changes neither
Hub behavior nor product memory ownership. It restores the last pre-`122880`
repository ceiling under an explicit provisional exception and collects exact
product-ELF and runtime heap evidence.

## Verified gate history

The repository history establishes a two-step measurement failure.

1. At `9d9cdef0980ec063e23c9a2849eae63f5cf6d812`, the PR #63 workflow built to
   `build/cardputer-adv-current` but called the checker with
   `build/cardputer_adv`. The checker expected an ELF file, not a directory,
   and the default was still `191488`. That revision therefore did not measure
   the intended ELF through the shown command.
2. Commit `81689b46448363b91db5256575b30cc1e53c2300` (`81689b4`,
   `ci: enforce measured Cardputer DRAM ceiling`) corrected the invocation to
   the hyphenated ELF path and made a one-line default change from `191488` to
   `122880`.
3. The introducing commit contains no calculation, measurement artifact,
   runtime reserve, profile distinction, or explanation of how `122880` was
   derived.
4. At exact `81689b4`, a missing ELF returned exit code `2`; the corrected gate
   was fail-closed. The defect there is the unsupported number, not a silent
   success.

PR #63 later reported values close to `122880`, including `122676 / 122880`.
That shows that a particular reported image happened to fit the selected
number. It does not derive the number as a hardware safety boundary.

The source at `9d9cdef` contains both `static Scene g_mainScene` and
`static Scene s_tempLoadScene`, while reported PR #63 `.bss` totals are roughly
two `Scene` objects lower than the current correctly inspected ELF. This is
strong evidence of a source/ELF or build-path mismatch. Exact historical symbol
proof still requires rebuilding that immutable source with the corrected
measurement setup.

The older `191488` value is also not accepted as universal. A historical
`190808` MIDI-only figure has not been tied to an immutable source commit, full
FQBN, toolchain, and ELF SHA-256. Normal CDC+MIDI and MIDI-only are separate
binaries and may ultimately require separate limits.

## Provisional rollback

The mandatory default is restored to `191488` only as a rollback of the
undocumented `122880` replacement. This removes an unsupported blocker from PR
#65 while the baseline study runs.

This means:

- `191488` is a provisional repository ceiling, not a proven safe maximum;
- `122880` remains visible only as an unsupported historical reference;
- headroom to `191488` is comparison arithmetic, not measured safety margin;
- no Scene, wavetable, SMF, engine, USB, scheduler, DSP, or UI storage moves;
- #65 remains responsible for its own build, hardware, and functional tests;
- the final policy may be lower, higher, or profile-specific.

The product gate prints its provisional-exception status on every run. Passing
it means only that the fixed sections are below the temporary repository value.

## Product and runtime images

Static ELF measurements and runtime heap measurements use different images on
purpose.

- `product` builds an uninstrumented copy of the exact product source. Its
  `.dram0.data`, `.dram0.bss`, symbols, and ELF SHA-256 are used for gate and
  historical comparisons.
- `runtime` injects heap, fragmentation, stack, and integrity probes into a
  temporary source copy. It is flashed for hardware measurements, but its
  static sections are never presented as the exact product baseline.

Build all four combinations from one clean checkout:

```bash
bash scripts/build_cardputer_memory_baseline.sh normal product --warnings all
bash scripts/build_cardputer_memory_baseline.sh midi-only product --warnings all
bash scripts/build_cardputer_memory_baseline.sh normal runtime --warnings all
bash scripts/build_cardputer_memory_baseline.sh midi-only runtime --warnings all
```

Every accepted record contains:

```text
Memory baseline profile: <normal|midi-only>
Memory baseline image: <product|runtime>
Source commit: <40-hex SHA>
Source dirty entries: 0
FQBN: <complete board options>
ELF path: <resolved file>
ELF sha256: <64-hex digest>
MEMORY_BASELINE kind=... fixed=... data=... bss=...
```

The PR workflow checks out `pull_request.head.sha`, not GitHub's synthetic merge
commit. Workflow-dispatch runs use `github.sha`.

## Current immutable-head static baseline

The fully pinned product records were built from:

```text
head  e3c34bb8244265db1e936a660ec125449eed0c8d
base  dev @ b43e23ff12fb0a31bcb77dd5ec57908889760013
core  m5stack:esp32 3.2.2
PSRAM disabled
```

```text
normal CDC+MIDI
  ELF SHA-256  655d44adef79ed36bb1fc23eff923ae2c376c8fa06d11b55554503a2cec72916
  .dram0.data   22600 B
  .dram0.bss   147392 B
  fixed        169992 B
  comparison to provisional 191488: +21496 B

MIDI-only
  ELF SHA-256  a6798c9816e3f9255ad94ea6b85395139a6bb51fd2eacf5f9d719cfe0f2156ef
  .dram0.data   22584 B
  .dram0.bss   147336 B
  fixed        169920 B
  comparison to provisional 191488: +21568 B

profile delta: 72 B
```

The runtime probes add `16 B` to the normal fixed total and `32 B` to the
MIDI-only fixed total. This validates the decision not to use instrumented ELFs
as exact product baselines.

## Full `.dram0.bss` attribution

The five named candidate groups are a shortlist, not a complete `.bss` model:

| Candidate | ELF size |
|---|---:|
| `s_tempLoadScene` | 25800 B |
| `g_mainScene` | 25800 B |
| Wavetable static arrays ×4 | 16384 B |
| `g_smfPlayer` | 11392 B |
| `g_miniAcidInstance` | 8864 B |
| **Shortlist total** | **88240 B** |

Therefore the bytes outside that shortlist are:

```text
normal:    147392 - 88240 = 59152 B
MIDI-only: 147336 - 88240 = 59096 B
```

Those bytes are not called unknown, free, or waste. The section reporter uses
`objdump -t` and the actual section name instead of treating every `nm` `B/b`
symbol as `.dram0.bss`. For each ELF it prints:

- every positive-size symbol located specifically in `.dram0.bss`;
- exact address, size, and name, sorted by size;
- union coverage of all symbol intervals;
- bytes in the section not covered by named symbol intervals, such as alignment
  or linker-owned space;
- raw overlap from aliases that point at the same storage;
- shortlist coverage and the exact bytes outside the shortlist.

The machine summary includes `bss_symbol_coverage`,
`bss_section_uncovered`, `bss_alias_overlap`, `candidate_bss`, and
`candidate_outside`.

The immutable-head product run at `e3c34bb8244265db1e936a660ec125449eed0c8d`
closed this static inventory:

```text
normal product
  .dram0.bss                 147392 B
  shortlist coverage          88240 B
  outside shortlist           59152 B
  all symbol interval cover  147238 B
  section bytes not covered      154 B
  raw alias overlap              828 B
  positive-size BSS symbols      436

MIDI-only product
  .dram0.bss                 147336 B
  shortlist coverage          88240 B
  outside shortlist           59096 B
  all symbol interval cover  147186 B
  section bytes not covered      150 B
  raw alias overlap              828 B
  positive-size BSS symbols      434
```

The `828 B` raw overlap is explained by two 92-byte newlib mutex objects that
have multiple alias names: one interval has six names and the other five. The
union-coverage calculation counts each storage interval once. The remaining
`154 B` / `150 B` are section bytes outside positive-size named symbol
intervals, not an unidentified 59 KB allocation.

The largest objects outside the original shortlist are distributed rather than
forming one hidden `Scene`:

| Symbol | Size |
|---|---:|
| `ncm_epbuf` | 6416 B |
| `g_output` | 4320 B |
| `_mscd_epbuf` | 4096 B |
| `_dfu_epbuf` | 4096 B |
| `g_dispatchTaskStack` | 4096 B |
| `sp12_clap` | 3000 B |
| `g_sampleStore` | 2092 B |
| `g_externalMidiTransportQueue` | 2076 B |
| `sp12_kick` | 2000 B |
| `sp12_snare` | 2000 B |

These ten account for `34192 B`; the rest is spread across hundreds of smaller
framework, UI, audio, USB, RTOS, and project symbols. The full ordered list is
preserved in both workflow artifacts. Static `.dram0.bss` attribution is now
closed for this head; runtime safety is not.

No candidate is presumed waste. In particular, moving `s_tempLoadScene` to heap
can reduce `.bss` while leaving peak physical RAM unchanged and requiring a
contiguous approximately `25.8 KB` block during save/load.

## Historical `190808` reconstruction

The current MIDI-only product is `169920 B`. The historical figure is
`190808 B`, a difference of:

```text
190808 - 169920 = 20888 B
```

That is too large to treat as rounding or instrumentation noise. Plausible
sources include a different core/toolchain, FQBN, feature configuration, or
source revision, but none is accepted without an immutable rebuild record. The
historical number cannot justify `191488`, and it cannot be compared as
"then versus now", until its source/profile/ELF identity is recovered.

## Runtime records

The runtime image emits:

```text
[MEM-BASE] phase=periodic ms=... freeInt=... minFreeBoot=...
           startBootFloor=... minFreeRuntimeSample=...
           largest=... minLargestRuntimeSample=...
           loopStackWords=... audioStackWords=... integrity=1
```

Metric meanings:

- `freeInt`: current free internal 8-bit heap;
- `minFreeBoot`: ESP-IDF minimum free internal heap since boot;
- `startBootFloor`: boot minimum captured at `setup()` completion;
- `minFreeRuntimeSample`: lowest 10 ms sampled free heap after setup;
- `largest`: current largest contiguous internal allocation;
- `minLargestRuntimeSample`: lowest sampled largest block after setup;
- `loopStackWords` and `audioStackWords`: FreeRTOS high-water marks in words;
- `integrity`: `heap_caps_check_integrity_all(false)` result.

`minLargestRuntimeSample` is sampled, not an allocator-maintained historical
minimum. A short allocation and free inside one blocking call can be missed.
Any proposed relocation must add before/during/after probes at its actual
allocation site.

MIDI-only has no CDC output in its production profile. Its runtime evidence
needs a bounded channel that does not change the USB profile, such as retained
summary data read by a later diagnostic boot. Enabling CDC would measure a
different binary.

## Hardware matrix

Run normal CDC+MIDI and MIDI-only separately, with at least three cold boots per
profile:

1. Idle for two minutes after full UI startup.
2. Visit every workflow and create every lazy page.
3. Run both synths, drums, delay/distortion, and a dense internal pattern.
4. Load and play a small SMF.
5. Load and play the densest reproducible SMF until failure or ten minutes.
6. Exercise MIDI mute, inspectors, browser, and repeated transitions during
   playback.
7. Repeat scene/project save, load, save-as, and reload.
8. Test GP MASTER, SEQ MASTER, Stop/Play, and clock-source changes.
9. Stop the host from draining USB MIDI, then restore it and verify recovery.
10. Run a mixed audio/UI/MIDI soak for at least ten minutes.

Record the minimum free heap, minimum largest block, loop/audio stack
watermarks, reset reason, heap integrity, and whether save/load and dense SMF
complete.

## Threshold derivation rule

No new numeric threshold is accepted unless the changing commit shows how it
was obtained. Required evidence:

1. Source commit and clean-tree status.
2. Full FQBN and toolchain/core versions.
3. Product ELF SHA-256 and exact `.dram0.data` / `.dram0.bss` values.
4. Separate normal and MIDI-only results, or proof that one shared ceiling is
   conservative for both.
5. Worst-case `minFreeBoot`, `minFreeRuntimeSample`,
   `minLargestRuntimeSample`, and stack watermarks.
6. Declared required total-heap, contiguous-block, and stack reserves.
7. A calculation showing how proposed fixed-DRAM growth preserves those
   reserves, followed by a build at or near the proposed boundary.

A linker total alone cannot prove runtime safety. A low runtime heap number
alone cannot identify which static object should move. Both exact product ELF
identity and runtime behavior are required.

## Provisional `191488` self-audit

The restored value is subjected to the same rule rather than being silently
grandfathered:

| Rule item | Status for `191488` | Evidence |
|---|---|---|
| 1. Immutable source and clean tree | PASS | pinned head and `Source dirty entries: 0` |
| 2. FQBN and toolchain/core versions | PASS | separate full FQBNs; pinned M5Stack core `3.2.2` and libraries |
| 3. Product ELF identity and sections | PASS | separate ELF SHA-256 and exact sections |
| 4. Profile separation | PASS | normal and MIDI-only product/runtime builds |
| 5. Worst-case hardware minima | **MISSING** | hardware matrix not run |
| 6. Declared reserves | **MISSING** | no accepted heap/block/stack reserves yet |
| 7. Deriving calculation and boundary build | **MISSING** | no formula can be completed before items 5-6 |

Result: `191488` currently passes **4 of 7** evidence requirements. It is an
explicit temporary exception, not a threshold derived by this PR's rule. Its
`21496 B` and `21568 B` headroom values are arithmetic comparisons only. The
exception may be removed, replaced with profile-specific limits, or confirmed
only after hardware evidence and the reserve calculation exist.

## Boundary audit

The PR diff is restricted to these six paths:

```text
.github/workflows/cardputer-memory-baseline.yml
docs/stages/CARDPUTER_MEMORY_BASELINE.md
scripts/build_cardputer_memory_baseline.sh
scripts/check_cardputer_dram_budget.sh
scripts/report_cardputer_memory_baseline.sh
tests/test_cardputer_memory_baseline_source_regressions.py
```

Post-line audit:

- **No change to #65 or stacked branches:** PASS; PR base remains `dev`, and no
  Hub MIDI source is changed.
- **No PSRAM assumption:** PASS; both FQBNs retain `PSRAM=disabled`.
- **No static-to-heap relocation:** PASS; no product C/C++ storage definition is
  changed.
- **No transactional Scene change:** PASS; Scene codec/load/save source is not
  in the diff.
- **No TinyUSB, scheduler, RX, clock, note ownership, DSP, or UI behavior
  change:** PASS; no product implementation file is in the diff.
- **Fail closed:** PASS; scripts use `set -euo pipefail`, the workflow uses
  `pipefail` around `tee`, missing ELF/binutils/report errors are non-zero, and
  the final source-contract, four baseline builds, Cardputer build/gate, host
  tests, and SDL build all passed on `e3c34bb`.

## Decision after measurement

After exact BSS inventory, historical reconstruction, and the hardware matrix,
#70 may choose among:

- reducing genuinely unnecessary fixed residency;
- deriving a documented profile-specific gate;
- combining a small low-risk optimization with a justified gate change.

## Acceptance boundaries

- `191488` remains explicitly provisional and marked as a 4/7 exception.
- Cardputer ADV remains `PSRAM=disabled`.
- No blind static-to-heap relocation.
- No removal of transactional scene rollback without an equivalent design.
- No change to #65 or its stacked branches.
- No TinyUSB, scheduler, RX, clock, note-ownership, DSP, or UI behavior change.
- A missing ELF, missing binutils, failed report, or failure hidden behind
  `tee` must fail CI.
- The two product BSS inventories are captured and the static ELF stage is
  closed. The PR remains draft because the hardware/runtime threshold stage is
  still open and `191488` remains a 4/7 exception.
