# 0.9.10 LIFETIME-R1 M1-A — Static Residency Evidence

Date: 2026-09-08
Status: **IN PROGRESS — static baseline construction**
Production changes by Memory Research: **NONE**

## 0. Purpose

M1-A freezes the production-equivalent static residency baseline before any
lifetime optimization is proposed or implemented.

This checkpoint answers two distinct questions from one unchanged firmware ELF:

1. which retained symbols occupy `.data` / `.bss`, and by how much;
2. where those symbols/sections are linked in the ESP32-S3 memory image.

Runtime heap, fragmentation, phase transitions and task HWM are intentionally
out of scope here. They belong to **M1-B Runtime Lifetime Evidence**, built with a
separate diagnostics identity so the measurement instrumentation cannot silently
change the static baseline being measured.

## 1. Evidence discipline

M1 uses two different evidence binaries.

### M1-A production-equivalent ELF

- no diagnostic compile flags;
- no production source changes relative to the pinned firmware source SHA;
- authoritative for `.data`, `.bss`, section layout and ELF symbol sizes;
- research workflow changes and research documents are excluded from the
  production-source equivalence gate.

### M1-B instrumented probe build

Future checkpoint only. It will carry an explicit diagnostics identity and will
be authoritative only for runtime observations such as `free8`, `largest8`,
minimum-ever heap and task HWM.

**Rule:** M1-B numbers may not be used to rewrite the M1-A static-residency
baseline.

## 2. Evidence categories

| Category | Allowed claim |
| --- | --- |
| **MEASURED CURRENT** | Obtained from the production-equivalent ELF/runtime for the exact stated SHA/configuration. |
| **SOURCE-CONFIRMED** | Ownership or lifetime follows directly from current source on the pinned firmware snapshot. |
| **HISTORICAL** | Measured on an older SHA/configuration; never used as a current budget value. |
| **DESIGN CANDIDATE** | A direction worth investigating; no memory saving or safety claim is implied. |

A finding is not promoted to `CONFIRMED LIFETIME DEFECT` merely because a large
symbol exists. Static residency, semantic lifetime, construction peak and
fragmentation risk must remain separate claims.

## 3. Authoritative identities

### Firmware source identity

`8581e284fc5b95757ab090e558ef56d8f0c21aa0`

### Research branch

`research/20260908-05-0.9.10-lifetime-r1-deep-census`

### Static evidence workflow

`.github/workflows/0_9_10_lifetime_r1_deep_census.yml`

Current authoritative M1-A workflow run/artifact: **PENDING**

The workflow performs a `git diff --exit-code` against the firmware source SHA,
excluding only the research workflow itself and `docs/research/**`, before
building. Therefore the ELF is accepted as production-equivalent only if that
gate passes.

## 4. Build contract

**SOURCE-CONFIRMED** from `scripts/build.sh` and
`scripts/install_arduino_deps.sh` on the firmware snapshot:

- board target: Cardputer ADV via M5Stack Cardputer FQBN;
- PSRAM: disabled;
- partition scheme: `huge_app`;
- USB mode: default/native USB with CDC on boot;
- upload mode: CDC;
- Arduino loop stack: 32768 bytes;
- M5Stack ESP32 core: 3.2.2;
- M5Cardputer: 1.1.0;
- M5Unified: 0.2.8;
- M5GFX: 0.2.10;
- production-equivalent build extra diagnostic C++ flags: empty.

Exact installed tool versions and ELF hash are populated from the evidence
artifact, not inferred from the scripts.

## 5. Evidence pack

The authoritative artifact must contain:

```text
lifetime-r1-static-census/
├── build-meta.txt
├── toolchain.txt
├── elf-sections.txt
├── elf-symbols-all.txt
├── elf-symbols-size-sort.txt
├── elf-top-owners.txt
└── linker-map.txt
```

`elf-sections.txt` contains both `xtensa-esp32s3-elf-size -A` and
`xtensa-esp32s3-elf-objdump -h`. `elf-top-owners.txt` is the 30 largest
`b/B/d/D` symbols by ELF symbol size.

## 6. Static top-owner census

**MEASURED CURRENT: PENDING authoritative M1-A artifact.**

The final table will preserve the raw top 30 even when an entry belongs to a
library/runtime rather than GroovePuter production code. The crosswalk adds
semantic interpretation without deleting inconvenient owners from the census.

| Rank | Symbol | Bytes | Section/type | Source owner | Semantic owner | Impl lifetime | Semantic lifetime | Phase set | R/T/F/L | Production owner |
| ---: | --- | ---: | --- | --- | --- | --- | --- | --- | --- | --- |
| — | **PENDING** | — | — | — | — | — | — | — | — | — |

Legend for the working classification:

- `R` — retained/resident;
- `T` — transaction/transient;
- `F` — fixed-capacity;
- `L` — library/cardinality/externally scaling state.

The classification is descriptive, not a defect verdict.

## 7. Findings ledger

### M-001 — Scene transaction scratch

**Status:** candidate finding; not yet promoted to confirmed lifetime defect.

**SOURCE-CONFIRMED**

- object/API: `sceneTransactionScratch()`;
- implementation: namespace-static `Scene s_tempLoadScene`;
- implementation lifetime: process lifetime;
- semantic role: shared transaction storage for scene parsing and Pattern page
  validation;
- Pattern paging uses it as staging for `readAndValidatePage(...)` before live
  state is accepted;
- source comment explicitly describes it as transaction scratch rather than
  retained history.

**HISTORICAL**

- close-version `sizeof(Scene)` measurement: 26,048 B;
- this value is not a current budget number.

**MEASURED CURRENT**

- authoritative M1-A symbol size/section: **PENDING**.

**DESIGN CANDIDATE**

- investigate validation/materialization split so Pattern page transactions do
  not require a second complete `Scene`;
- do **not** replace the static object with a late `new Scene` merely to reduce
  `.bss`: a contiguous ~26 KiB late allocation could be less reliable than the
  current retained object.

`production_change: NOT APPLIED`

### M-002 — SMF hot/cold residency

**Status:** candidate finding; runtime evidence required.

**SOURCE-CONFIRMED**

- `LazyCardputerSmfPlayer` owns `CardputerSmfPlayerService player_` for process
  lifetime once the global registry object exists;
- the service contains the bounded event queue, `SdByteSource`/`File`, parsed
  file/timing state, snapshots, command queue storage and worker handle;
- MIDI registration stores the service event queue address after begin, so the
  whole service cannot simply be destroyed without changing dispatcher
  ownership;
- loaded path and playback/timing fields are distinct from the worker/source
  lifetime.

Semantic model to test in M1-B:

```text
Unloaded       cold shell, no hot worker/source residency required
Loading        worker/source active
Playing/Armed  worker/source active
Paused         preserve tick; hot worker/source may be hibernatable
Stopped/EOF    preserve replay identity/state; STOP != UNLOAD
Error          settle worker/source, retain only required diagnostics/identity
```

**MEASURED CURRENT**

- static SMF object/stack symbols and sections: **PENDING authoritative M1-A artifact**;
- dynamic worker/task allocation and open-`File` heap cost are not derivable
  from static ELF symbols and remain M1-B measurements.

**DESIGN CANDIDATE**

- persistent cold shell + disposable/restartable hot worker/source;
- replay path candidate: reopen -> stream open/index -> prepare at saved tick ->
  worker start;
- page leave is not a teardown boundary because Sequencer Hub MIDI controls SMF
  outside the SMF page.

`production_change: NOT APPLIED`

### M-003 — Phrase full-buffer residency

**Status:** candidate finding; routed to Pattern/Phrase runtime owner for any
production design/change.

**SOURCE-CONFIRMED**

- `RuntimeSynthEventBuffer` is fixed at 1,284 B (`128` events x 10 B + metadata);
- `MiniAcid` retains `currentPhrase_[2]` inline: 2 x 1,284 = 2,568 B;
- `MiniAcid` also allocates one pending `RuntimeSynthEventBuffer` per synth at
  construction via `new (std::nothrow)`: another 2,568 B when successful;
- total full Phrase capacity resident after successful construction is therefore
  5,136 B even before runtime phase necessity is considered;
- Pattern runtime itself remains bounded by a separate <=5,500 B bank contract.

**MEASURED CURRENT**

- inline Phrase bytes are part of the `MiniAcid` object symbol rather than
  necessarily emitted as separately named ELF symbols;
- exact enclosing `MiniAcid` static symbol/section: **PENDING authoritative M1-A artifact**;
- pending buffers are heap allocations and require M1-B runtime measurement.

**DESIGN CANDIDATE / desired semantic invariant**

```text
Pattern, no NEXT              -> 0 Phrase buffers
Pattern, Phrase queued NEXT   -> 1
Phrase sounding               -> 1
Phrase sounding + Phrase NEXT -> 2
```

Capacity remains 8 bars; only residency would become state-dependent.

`owner: Pattern/Phrase runtime lane`
`production_change: NOT APPLIED`

### M-004 — Sample-library metadata cardinality

**Status:** candidate unbounded-cardinality finding; experiment required.

**SOURCE-CONFIRMED**

- `SampleIndex` retains `std::vector<SampleFileInfo> files_`;
- each `SampleFileInfo` retains both `filename` and `fullPath` strings;
- `SampleIndex` separately retains `std::map<std::string, SampleId> nameToId_`;
- `RamSampleStore` retains a further `std::map<uint32_t, std::string> filePaths_`
  after registration;
- therefore library metadata can scale with discovered file count even when PCM
  loaded bytes are zero;
- current source has no separate sampler I/O worker to reclaim as a fixed task
  budget item.

**MEASURED CURRENT**

- fixed `g_sampleStore` shell/slot symbol: **PENDING authoritative M1-A artifact**;
- N-dependent heap residency is not a static-ELF quantity.

**M1-B experiment**

Measure for `/samples = 0 / 32 / 128 / 512`:

```text
post-setup free8
post-setup largest8
metadata count
PCM loaded bytes = 0
```

Primary metric: `heap_after_scan(N) - heap_before_scan`.

`production_change: NOT APPLIED`

### M-005 — FatFs open-file lifetime

**Status:** candidate lifetime finding; post-FS1B baseline only.

**HISTORICAL**

- older FatFs configuration used roughly 4,176 B per `FIL`, dominated by an
  inline 4,096 B sector cache;
- older SD-mount heap-loss measurements predate FS1B and must not be used as the
  current fixed-FatFs budget.

**MEASURED CURRENT**

- static ELF does not establish per-open-file runtime heap cost;
- current post-FS1B `File` lifetime/heap delta belongs to M1-B.

**DESIGN CANDIDATE**

- audit open-file lifetime only after current runtime measurements identify
  which handles remain live across musical phases.

`production_change: NOT APPLIED`

### M-006 — Phrase audition rollback scratch

**Status:** newly discovered candidate from the preliminary ELF census; M1-A
crosswalk pending.

**SOURCE-CONFIRMED**

- `strong_rhythm_live_bridge.cpp` defines namespace-static
  `PhraseAuditionScratch g_phraseAuditionScratch{}`;
- source describes it as synchronous phrase-audition rollback/control scratch,
  not as a persistent phrase cache;
- the object holds temporary selection/frozen-selection/evolution results plus
  previous drum and synth material used by destructive audition/rollback.

**MEASURED CURRENT**

- authoritative M1-A symbol size/section: **PENDING**.

**DESIGN CANDIDATE**

- first prove its phase overlap and required rollback semantics; only then ask
  whether the static aggregate can be narrowed/reused without increasing stack
  or late contiguous-allocation risk.

`production_change: NOT APPLIED`

## 8. Explicit non-findings / removed assumptions

- Pattern runtime bank is compact and bounded (`sizeof(RuntimePatternEventBank)
  <= 5,500 B`); it is not a tens-of-KiB target.
- no standalone sampler I/O worker exists in the current architecture, so no
  speculative 4 KiB sampler-task saving belongs in the budget.
- Tape is not promoted as a large static target by this checkpoint.
- DSP capacity is deferred until lifetime owners above are measured.

## 9. M1-B handoff contract

M1-B must use a separately identifiable diagnostics firmware and record its own
overhead. The common phase matrix is:

```text
BOOT
POST_SETUP
IDLE
PATTERN_PLAY
PHRASE_PLAY
PHRASE_EDIT
SMF_LOAD
SMF_PLAY
SMF_STOP
SMF_REPLAY
SCENE_LOAD
SCENE_SAVE
SAMPLE_SCAN
SAMPLE_PRELOAD
```

At minimum record `free8`, `largest8`, minimum-ever heap, and task HWM where
applicable. Transient operations require `BEFORE / DURING-PEAK / AFTER` so
ordinary pressure, retained residue and fragmentation are not conflated.

## 10. M1-A closure gate

M1-A is closed only when all of the following are true on one evidence identity:

- [ ] production-source equivalence gate PASS;
- [ ] unchanged Cardputer ADV firmware build PASS;
- [ ] fixed DRAM budget check PASS;
- [ ] evidence artifact contains build metadata, toolchain, sections, all symbols,
      size-sorted symbols, top 30 BSS/DATA and linker map;
- [ ] exact ELF SHA-256 recorded;
- [ ] top 30 crosswalk completed without deleting library/runtime owners;
- [ ] M-001..M-006 measured/static fields updated from that artifact;
- [ ] historical numbers remain labeled historical;
- [ ] no production source changes were made by Memory Research.

Until every box is supported by fresh evidence, status remains **IN PROGRESS**.
