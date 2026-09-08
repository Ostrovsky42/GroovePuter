# 0.9.10 LIFETIME-R1 M1-A — Static Residency Evidence

Date: 2026-09-08
Status: **M1-A STATIC RESIDENCY BASELINE: FROZEN**
Production changes by Memory Research: **NONE**

## 0. Purpose

M1-A freezes the production-equivalent static residency baseline before any
lifetime optimization is proposed or implemented.

This checkpoint answers two questions from one unchanged firmware ELF:

1. which retained symbols occupy fixed internal DRAM, and by how much;
2. in which ELF/linker sections those symbols actually reside.

Runtime heap, fragmentation, phase transitions and task high-water marks are
intentionally out of scope. They belong to **M1-B Runtime Lifetime Evidence**,
which must use a separate diagnostics identity so instrumentation cannot silently
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
fragmentation risk remain separate claims.

## 3. Authoritative identity

### MEASURED CURRENT

| Evidence | Value |
| --- | --- |
| Firmware source SHA | `8581e284fc5b95757ab090e558ef56d8f0c21aa0` |
| Research workflow SHA | `1405577b18d96b28b559ea28b0e6156f2cd1cceb` |
| Workflow run | `34240383982` |
| Workflow job | `102108710300` |
| Run conclusion | `success` |
| Source equivalence | `PASS` |
| Artifact | `lifetime-r1-static-census` |
| Artifact ID | `10061863096` |
| Artifact archive digest | `sha256:63d3cec3b7ecb287824030d231aac3035f39dcb696c59fb7198cce6688ea2963` |
| ELF SHA-256 | `92007d25c50404d6d66d706845d5494de4606ab832111712368c3baac7f81068` |
| Board | Cardputer ADV |
| PSRAM | disabled |

The workflow ran `git diff --exit-code` against the firmware source SHA,
excluding only the research workflow itself and `docs/research/**`, before the
firmware build. That equivalence step, the unchanged firmware build, fixed-DRAM
budget check, evidence-pack generation and artifact upload all completed
successfully in the same job.

The evidence workflow SHA is intentionally different from the firmware source
SHA: only research CI/docs may differ. The later documentation freeze commit is
not a new measured firmware identity.

## 4. Build/toolchain contract

### SOURCE-CONFIRMED build configuration

From `scripts/build.sh` and `scripts/install_arduino_deps.sh` on the pinned
firmware source:

- FQBN:
  `m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`;
- Arduino loop stack request: 32768 bytes;
- production-equivalent extra diagnostic C++ flags: empty;
- M5Stack ESP32 core: 3.2.2;
- M5Cardputer: 1.1.0;
- M5Unified: 0.2.8;
- M5GFX: 0.2.10.

### MEASURED CURRENT toolchain

- Arduino CLI: `1.5.2-rc.1` (`fef6e48df`);
- Xtensa C++: `xtensa-esp-elf-g++ 14.2.0`, crosstool-NG
  `esp-14.2.0_20241119`;
- GNU `nm`, `size`, `objdump`: 2.43.1 from the same Xtensa toolchain.

The 32768-byte Arduino loop-stack request is **not** an M1-A saving candidate.
Actual task stack residency/use and HWM belong to M1-B.

## 5. Evidence pack and ranking correction

The authoritative artifact contains:

```text
lifetime-r1-static-census/
├── build-meta.txt
├── toolchain.txt
├── elf-sections.txt
├── elf-symbol-table.txt
├── elf-symbols-all.txt
├── elf-symbols-size-sort.txt
├── elf-top-owners.txt
└── linker-map.txt
```

`elf-sections.txt` contains both `xtensa-esp32s3-elf-size -A` and
`xtensa-esp32s3-elf-objdump -h`. Raw `nm` output remains preserved without
semantic filtering.

An earlier derived ranking used `nm` symbol letters `b/B/d/D`. That was
insufficient on ESP32-S3 because flash-mapped DROM symbols at `0x3c...` can also
appear with `d/D` type letters. The raw symbols, map and section table were
valid; only that derived ranking was invalid as a RAM-owner list.

The frozen ranking therefore derives section membership from
`xtensa-esp32s3-elf-objdump -t -C` and admits only symbols whose actual section
is `.dram0.data` or `.dram0.bss`. The final top 30 contains no DROM entries.
`.rtc.data` is recorded separately below; `.noinit` is empty in this ELF.

## 6. Fixed internal-DRAM baseline

### MEASURED CURRENT

| Section | Bytes |
| --- | ---: |
| `.dram0.data` | **35,304** |
| `.dram0.bss` | **151,408** |
| **`.dram0.data + .dram0.bss`** | **186,712 B / 182.34 KiB** |
| `.rtc.data` | 4 |
| `.noinit` | 0 |

The frozen M1-A budget scope is `.dram0.data + .dram0.bss` = **186,712 B**.
This is a static retained/link-time quantity. It makes no statement about
runtime `free8`, `largest8`, allocation peaks or fragmentation.

### HISTORICAL comparison

A prior P3 fixed `.data + .bss` measurement was 192,904 B. Against that older
measurement, the current production-equivalent fixed footprint is lower by
**6,192 B / 6.05 KiB / 3.21%**. This comparison does not identify the causal
changes and is not a runtime heap comparison.

## 7. RAM-only top-30 owner census

The top 30 section-qualified symbols account for **157,796 B / 84.51%** of the
frozen `.dram0.data + .dram0.bss` scope.

Legend:

- `R` — implementation is retained/resident beyond the operation that may use it;
- `T` — semantic role is transaction/transient;
- `F` — bounded/fixed-capacity storage;
- `L` — library/cardinality/externally scaling state is involved; static shell
  size does not describe all backing storage.

`Phase set` is a source-level semantic description, not measured runtime
liveness. Runtime overlap belongs to M1-B.

| # | Symbol | Bytes | Section | Source owner | Semantic owner | Impl lifetime | Semantic lifetime | Phase set | R/T/F/L | Production owner |
| ---: | --- | ---: | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `g_mainScene` | 26,080 | `.dram0.bss` | `scenes.cpp` | active Scene state | process/static | session musical state | boot + all scene/runtime phases | R/F | Scene/state |
| 2 | `s_tempLoadScene` | 26,080 | `.dram0.bss` | `scenes.cpp` | scene/page transaction staging | process/static | scene/page transaction | scene load + Pattern page validation | R/T/F | Scene / Pattern paging |
| 3 | `g_miniAcidInstance` | 16,912 | `.dram0.bss` | `GroovePuter.ino.cpp` | core audio/sequencer engine object | process/static | device runtime | setup + musical runtime | R/F | DSP/runtime |
| 4 | `g_smfPlayer` | 11,392 | `.dram0.bss` | `cardputer_smf_player_registry.cpp` | SMF service shell + bounded player state | process/static | mixed cold identity / hot playback | SMF control + playback | R/F | SMF/MIDI |
| 5 | `g_phraseAuditionScratch` | 8,220 | `.dram0.data` | `strong_rhythm_live_bridge.cpp` | audition rollback/control scratch | process/static | synchronous audition transaction | phrase audition only | R/T/F | GF2 phrase audition |
| 6 | `ncm_epbuf` | 6,416 | `.dram0.bss` | TinyUSB `ncm_device.c` | USB NCM endpoint buffer | linked process/static | USB class I/O | platform USB | R/F | platform/TinyUSB |
| 7 | `g_patternMusicalEventQueue` | 4,512 | `.dram0.data` | `GroovePuter.ino.cpp` | Pattern musical-event queue | process/static | active runtime queue | Pattern/audio runtime | R/F | sequencer/input |
| 8 | `port_IntStack` | 4,192 | `.dram0.data` | FreeRTOS `port.c` | interrupt stack | process/static | platform runtime | all runtime | R/F | ESP-IDF/FreeRTOS |
| 9 | `g_dispatchTaskStack` | 4,096 | `.dram0.bss` | `cardputer_usb_midi_transport.cpp` | USB MIDI dispatcher task stack | process/static | dispatcher task | MIDI runtime | R/F | MIDI/platform |
| 10 | `Wavetable::sawTable_` | 4,096 | `.dram0.bss` | `audio_wavetables.cpp` | saw wavetable | process/static | synth resource | synth runtime | R/F | DSP |
| 11 | `Wavetable::sineTable_` | 4,096 | `.dram0.bss` | `audio_wavetables.cpp` | sine wavetable | process/static | synth resource | synth runtime | R/F | DSP |
| 12 | `Wavetable::squareTable_` | 4,096 | `.dram0.bss` | `audio_wavetables.cpp` | square wavetable | process/static | synth resource | synth runtime | R/F | DSP |
| 13 | `_dfu_epbuf` | 4,096 | `.dram0.bss` | TinyUSB `dfu_device.c` | USB DFU endpoint buffer | linked process/static | USB class I/O | platform USB | R/F | platform/TinyUSB |
| 14 | `_mscd_epbuf` | 4,096 | `.dram0.bss` | TinyUSB `msc_device.c` | USB MSC endpoint buffer | linked process/static | USB class I/O | platform USB | R/F | platform/TinyUSB |
| 15 | `QuantizedGenerationDetail::g_slots` | 3,600 | `.dram0.data` | generation activation (`miniacid_engine.cpp` linkage) | prepared/quantized generation slots | process/static | generation prepare/commit transaction | GEN prepare -> boundary commit/cancel | R/T/F | generation/runtime |
| 16 | `sp12_clap` | 3,000 | `.dram0.bss` | `mini_drumvoices.cpp` | SP-12 clap sample/table | process/static | drum-engine resource | drum synthesis | R/F | DSP/drums |
| 17 | `g_performanceKeyboard` | 2,184 | `.dram0.bss` | `GroovePuter.ino.cpp` | live performance keyboard state | process/static | interactive runtime | performance/input | R/F | input/performance |
| 18 | `g_sampleStore` | 2,092 | `.dram0.bss` | `GroovePuter.ino.cpp` / `RamSampleStore` | fixed sampler-store shell/slots | process/static | sampler service | sampler runtime | R/F/L | sampler |
| 19 | `g_externalMidiTransportQueue` | 2,076 | `.dram0.bss` | `GroovePuter.ino.cpp` | external MIDI transport queue | process/static | active transport queue | MIDI/transport runtime | R/F | MIDI/transport |
| 20 | `sp12_kick` | 2,000 | `.dram0.bss` | `mini_drumvoices.cpp` | SP-12 kick sample/table | process/static | drum-engine resource | drum synthesis | R/F | DSP/drums |
| 21 | `sp12_snare` | 2,000 | `.dram0.bss` | `mini_drumvoices.cpp` | SP-12 snare sample/table | process/static | drum-engine resource | drum synthesis | R/F | DSP/drums |
| 22 | `s_coredump_stack` | 1,892 | `.dram0.bss` | ESP-IDF `core_dump_common.c` | core-dump stack | process/static | fault handling | panic/core dump | R/F | ESP-IDF platform |
| 23 | `smfStructuralInspectorState()::state` | 1,824 | `.dram0.data` | `smf_stream.cpp` | SMF structural-inspection state | process/function-static | SMF inspection/diagnostics state | SMF load/inspect | R/F | SMF/MIDI |
| 24 | `M5` | 1,720 | `.dram0.bss` | M5Unified | board/device facade state | process/static | device runtime | all hardware runtime | R/F | M5Unified/platform |
| 25 | `GroovePuterUndo::undoOwner()::owner` | 1,552 | `.dram0.bss` | `miniacid_display.cpp` | user Undo state | process/function-static | retained edit history | UI/edit runtime | R/F | UI/Undo |
| 26 | `g_drum_pattern_clipboard` | 1,196 | `.dram0.data` | `ui_clipboard.cpp` | drum Pattern clipboard | process/static | user clipboard state | edit/copy/paste | R/F | UI/Pattern editing |
| 27 | `kEmptyDrumPatternSet` | 1,192 | `.dram0.bss` | `miniacid_engine.cpp` | empty-pattern sentinel | process/static | fallback/sentinel resource | engine runtime | R/F | DSP/runtime |
| 28 | `bus` | 1,064 | `.dram0.bss` | Arduino ESP32 `esp32-hal-i2c-ng.c` | I2C bus runtime state | process/static | hardware bus runtime | hardware I/O | R/F | Arduino/ESP32 platform |
| 29 | `g_audioBuffer` | 1,024 | `.dram0.bss` | `GroovePuter.ino.cpp` | audio output block buffer | process/static | audio runtime | audio | R/F | audio runtime |
| 30 | `sp12_hat` | 1,000 | `.dram0.bss` | `mini_drumvoices.cpp` | SP-12 hat sample/table | process/static | drum-engine resource | drum synthesis | R/F | DSP/drums |

The table is an ownership census, not a deletion list. Platform buffers,
wavetables, engine state and user-state objects remain visible even where M1 has
no lifetime hypothesis for them.

## 8. Findings ledger

### M-001 — Scene transaction scratch

**Status:** **CONFIRMED STATIC LIFETIME MISMATCH; runtime/construction evidence
still required before design selection.**

#### SOURCE-CONFIRMED

- API: `sceneTransactionScratch()`;
- implementation: namespace-static `Scene s_tempLoadScene`;
- implementation lifetime: process lifetime;
- semantic role: shared transaction storage for scene parsing and Pattern-page
  validation;
- Pattern paging uses it as staging for `readAndValidatePage(...)` before live
  state is accepted;
- source comments explicitly describe it as transaction scratch rather than
  retained history.

#### MEASURED CURRENT

- `s_tempLoadScene`: **26,080 B**, `.dram0.bss`;
- `g_mainScene`: **26,080 B**, `.dram0.bss`;
- two complete Scene objects: **52,160 B / 50.94 KiB / 27.94%** of the frozen
  `.dram0.data + .dram0.bss` scope.

#### HISTORICAL

- close-version `sizeof(Scene)`: 26,048 B;
- that value is superseded for current static budgeting by the ELF-measured
  26,080 B symbol.

#### DESIGN CANDIDATE

Investigate a validation/materialization split so Pattern-page transactions do
not require a second complete `Scene`. Do **not** replace the static object with
a late `new Scene` merely to reduce `.bss`: a contiguous ~26 KiB late allocation
could be less reliable than the current retained object.

No 26,080-B saving is claimed. M1-B/construction-overlap evidence must first
show what may safely stop overlapping.

`production_change: NOT APPLIED`

### M-002 — SMF hot/cold residency

**Status:** static owner measured; runtime lifetime mismatch remains a candidate
until M1-B.

#### SOURCE-CONFIRMED

- `LazyCardputerSmfPlayer` owns `CardputerSmfPlayerService player_` inside a
  process-lifetime global registry object;
- the service contains the bounded event queue, `SdByteSource`/`File`, parsed
  file/timing state, snapshots, command queue storage and worker handle;
- MIDI registration stores the service event-queue address after begin, so the
  whole service cannot simply be destroyed without changing dispatcher
  ownership;
- STOP/EOF semantics preserve replay identity: `STOP != UNLOAD`;
- page leave is not a teardown boundary because SMF is controllable outside the
  SMF page.

#### MEASURED CURRENT

- `g_smfPlayer`: **11,392 B**, `.dram0.bss`.

This is the fixed service object only. Worker task allocation, open-`File` heap
cost and hot/cold runtime deltas are not static-ELF quantities.

#### DESIGN CANDIDATE

Persistent cold shell + disposable/restartable hot worker/source, preserving
loaded identity and replay state. Candidate replay path:
`reopen -> stream open/index -> prepare at saved tick -> worker start`.

`production_change: NOT APPLIED`

### M-003 — Phrase full-buffer residency

**Status:** source-confirmed residency mismatch candidate; any production design
or patch is owned by the Pattern/Phrase runtime lane.

#### SOURCE-CONFIRMED

- `RuntimeSynthEventBuffer` is exactly **1,284 B**;
- `MiniAcid::currentPhrase_[2]` therefore embeds **2,568 B** of full Phrase
  capacity;
- `MiniAcid::initPendingMaterial()` allocates one additional 1,284-B buffer per
  synth with `new (std::nothrow)`, another **2,568 B** when both allocations
  succeed;
- full active + pending Phrase capacity is therefore **5,136 B** after successful
  construction, before asking which musical state actually needs it;
- Pattern runtime remains separately bounded by
  `sizeof(RuntimePatternEventBank) <= 5,500 B`.

#### MEASURED CURRENT

- enclosing `g_miniAcidInstance`: **16,912 B**, `.dram0.bss`;
- `currentPhrase_[2]` is an inline member and is deliberately **not** presented
  as an independent `nm` symbol;
- pending Phrase buffers are heap allocations and therefore not part of the
  fixed static census.

#### DESIGN CANDIDATE / desired semantic invariant

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

**Status:** source-confirmed unbounded-cardinality candidate; M1-B/sample-scan
experiment required.

#### SOURCE-CONFIRMED

- `SampleIndex` retains `std::vector<SampleFileInfo> files_`;
- each file record retains `filename` and `fullPath` strings;
- `SampleIndex` also retains `std::map<std::string, SampleId> nameToId_`;
- `RamSampleStore` retains `std::map<uint32_t, std::string> filePaths_` after
  registration;
- current source has no separate sampler I/O worker to reclaim as a fixed task
  budget item.

#### MEASURED CURRENT

- `g_sampleStore`: **2,092 B**, `.dram0.bss`.

This 2,092 B is only the fixed object/slot shell. It is **not** the sample-library
metadata footprint; vector/map/string backing storage is dynamic and may scale
with library cardinality.

#### M1-B experiment

For `/samples = 0 / 32 / 128 / 512`, record:

```text
post-setup free8
post-setup largest8
metadata count
PCM loaded bytes = 0
```

Primary metric: `heap_after_scan(N) - heap_before_scan`.

`production_change: NOT APPLIED`

### M-005 — FatFs open-file lifetime

**Status:** candidate lifetime finding; current runtime measurement required.

#### HISTORICAL

- older FatFs configuration used roughly 4,176 B per `FIL`, dominated by an
  inline 4,096-B sector cache;
- older SD-mount heap-loss measurements predate FS1B and are not current budget
  values.

#### MEASURED CURRENT

Static ELF evidence does not establish per-open-file runtime heap cost.
Post-FS1B `File` lifetime and heap deltas remain M1-B work.

`production_change: NOT APPLIED`

### M-006 — Phrase audition rollback scratch

**Status:** **CONFIRMED STATIC LIFETIME MISMATCH; runtime overlap evidence still
required before design selection.**

#### SOURCE-CONFIRMED

- `strong_rhythm_live_bridge.cpp` defines namespace-static
  `PhraseAuditionScratch g_phraseAuditionScratch{}`;
- source explicitly describes it as synchronous phrase-audition
  rollback/control scratch, not a persistent phrase cache;
- it holds temporary selection/frozen-selection/evolution results and previous
  drum/synth material for destructive audition/rollback.

#### MEASURED CURRENT

- `g_phraseAuditionScratch`: **8,220 B**, `.dram0.data`;
- **4.40%** of the frozen `.dram0.data + .dram0.bss` scope.

Implementation lifetime is process/static while the stated semantic lifetime is
an audition transaction. That establishes the static lifetime mismatch, not an
8,220-B guaranteed saving.

#### DESIGN CANDIDATE

First measure phase overlap and rollback requirements; then ask whether storage
can be narrowed or shared without increasing control-stack pressure or creating
a large late allocation.

`production_change: NOT APPLIED`

## 9. Explicit non-findings / observations not promoted to fixes

- Pattern runtime bank is compact and bounded (`sizeof(RuntimePatternEventBank)
  <= 5,500 B`); it is not a tens-of-KiB target.
- no standalone sampler I/O worker exists in the current architecture, so no
  speculative 4 KiB sampler-task saving belongs in the budget.
- the three measured wavetable arrays total **12,288 B / 6.58%** of the frozen
  fixed scope, but M1-A establishes only their residency, not that capacity can
  be removed.
- TinyUSB contributes visible fixed class buffers including `ncm_epbuf`,
  `_mscd_epbuf` and `_dfu_epbuf`; M1-A records them but does not change the USB
  contract or claim they are removable.
- the compile-time Arduino loop-stack request is 32768 B, but no finding is
  opened until M1-B measures actual task HWM/reservation behavior.
- Tape is not promoted as a large static target by this checkpoint.
- DSP capacity changes are deferred until lifetime evidence is complete.

## 10. M1-B handoff contract

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

At minimum record:

```text
free8
largest8
minimum-ever
relevant task HWM
```

Transient operations require `BEFORE / DURING-PEAK / AFTER` so ordinary pressure,
retained residue and fragmentation are not conflated.

Construction-peak work must reason about:

```text
PeakLiveSet = max_t(sum(alive(resource_i, t)))
```

rather than summing every potential consumer. In particular, M-001 must not turn
into a `.bss`-only optimization that introduces an unreliable late ~26 KiB
contiguous allocation.

## 11. M1-A closure

Evidence identity: workflow run `34240383982`, job `102108710300`, artifact
`10061863096`, ELF SHA-256
`92007d25c50404d6d66d706845d5494de4606ab832111712368c3baac7f81068`.

- [x] production-source equivalence gate PASS;
- [x] unchanged Cardputer ADV firmware build PASS;
- [x] fixed DRAM budget check PASS;
- [x] evidence artifact contains build metadata, toolchain, sections, complete
      raw symbols, size-sorted symbols, section-qualified symbol table,
      RAM-only top 30 and linker map;
- [x] exact ELF SHA-256 recorded;
- [x] top-30 RAM-owner crosswalk completed without deleting library/runtime
      owners;
- [x] M-001..M-006 static/measured fields updated within their evidence limits;
- [x] historical numbers remain labeled historical;
- [x] no runtime `free/largest/HWM` claims inferred from static evidence;
- [x] no production source changes made by Memory Research.

> **M1-A STATIC RESIDENCY BASELINE: FROZEN**
>
> Production changes by Memory Research: **NONE**

Next checkpoint is M1-B Runtime Lifetime Evidence, using a distinct instrumented
firmware identity. No optimization is authorized by this audit alone.
