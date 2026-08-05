# Cardputer memory baseline

## Purpose

Establish an evidence-based internal-RAM policy for Cardputer ADV before moving
large objects or declaring a new fixed `.dram0.data + .dram0.bss` ceiling.

This stage is intentionally separate from Hub MIDI PR #65. It changes neither
Hub behavior nor product memory ownership. It restores the last pre-`122880`
repository ceiling provisionally and collects exact product-ELF and runtime
heap evidence.

## Verified gate history

The repository history now establishes a two-step measurement failure.

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
- no Scene, wavetable, SMF, engine, USB, scheduler, DSP, or UI storage moves;
- #65 remains responsible for its own build, hardware, and functional tests;
- the final policy may be lower, higher, or profile-specific.

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

The fully pinned product records built from the unchanged product source are:

```text
normal CDC+MIDI
  .dram0.data   22600 B
  .dram0.bss   147392 B
  fixed        169992 B
  headroom to provisional 191488: 21496 B

MIDI-only
  .dram0.data   22584 B
  .dram0.bss   147336 B
  fixed        169920 B
  headroom to provisional 191488: 21568 B

profile delta: 72 B
```

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
symbol as `.dram0.bss`. For each ELF it prints every positive-size symbol,
interval-union coverage, bytes outside named intervals, alias overlap, shortlist
coverage, and the exact bytes outside the shortlist.

The immutable product run closed this static inventory:

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

The `828 B` raw overlap is explained by two 92-byte newlib mutex objects with
multiple aliases. The remaining `154 B` / `150 B` are section bytes outside
positive-size named symbol intervals, not an unidentified 59 KB allocation.
Static `.dram0.bss` attribution is closed; runtime safety is not.

## Runtime records

The corrected runtime telemetry contract is defined in
`docs/stages/CARDPUTER_MEMORY_RUNTIME_TELEMETRY.md`. In particular:

- stack high-water marks are named `*StackFreeBytes`, never words;
- loop, audio, SMF-player, and MIDI-dispatch tasks are measured independently;
- `MALLOC_CAP_8BIT` and `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` are reported
  separately;
- the runtime diagnostic uses a temporary source copy and direct runtime-only
  task-handle accessors;
- the separate product ELF remains uninstrumented.

The sampled minima begin after setup. They are not automatically accepted as a
stable baseline: UI, SD, MIDI, audio, and lazy task warm-up must occur first.

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

Record the minimum free heap, minimum largest block, per-task free-stack
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
5. Worst-case free-heap, largest-block, and per-task stack measurements.
6. Declared required total-heap, contiguous-block, and stack reserves.
7. A calculation showing how proposed fixed-DRAM growth preserves those
   reserves, followed by a build at or near the proposed boundary.

A linker total alone cannot prove runtime safety. A low runtime heap number
alone cannot identify which static object should move. Both exact product ELF
identity and runtime behavior are required.

## Provisional `191488` self-audit

| Rule item | Status |
|---|---|
| 1. Immutable source and clean tree | PASS |
| 2. Full FQBN and pinned toolchain/core | PASS |
| 3. Product ELF identity and exact sections | PASS |
| 4. Separate normal/MIDI-only records | PASS |
| 5. Worst-case hardware runtime minima | **MISSING** |
| 6. Declared total-heap/block/stack reserves | **MISSING** |
| 7. Deriving calculation and boundary build | **MISSING** |

The provisional value passes **4 of 7** requirements. It remains an explicit
policy exception, not a safety-derived ceiling. Reported headroom is arithmetic
comparison only.

## Decision after measurement

Only after hardware records exist should #70 choose among:

- reducing genuinely unnecessary fixed residency;
- deriving a documented profile-specific gate;
- combining a small low-risk optimization with a justified gate change.

No candidate is presumed waste. In particular, moving `s_tempLoadScene` to heap
can reduce `.bss` while leaving peak physical RAM unchanged and requiring a
large contiguous block during save/load.

The TinyUSB NCM/MSC/DFU buffers are now confirmed as a separate prebuilt-core
configuration candidate. They are not disabled in #70.

## Boundary audit

- `191488` remains explicitly provisional.
- Cardputer ADV remains `PSRAM=disabled`.
- No blind static-to-heap relocation.
- No removal of transactional scene rollback without an equivalent design.
- No change to #65 or its stacked branches.
- No production TinyUSB, scheduler, RX, clock, note-ownership, DSP, or UI
  behavior change.
- A missing ELF, missing binutils, failed report, or failure hidden behind
  `tee` must fail CI.

PR #70 changes only:

```text
.github/workflows/cardputer-memory-baseline.yml
docs/stages/CARDPUTER_MEMORY_BASELINE.md
docs/stages/CARDPUTER_MEMORY_RUNTIME_TELEMETRY.md
scripts/build_cardputer_memory_baseline.sh
scripts/check_cardputer_dram_budget.sh
scripts/instrument_cardputer_memory_runtime.py
scripts/report_cardputer_memory_baseline.sh
scripts/report_cardputer_tinyusb_class_buffers.sh
tests/test_cardputer_memory_baseline_source_regressions.py
```

No production C/C++ implementation file is changed.
