# 0.9.9-E0 Temporal Material Carrier Audit

Status: **RESEARCH / CONTRACT — DO NOT MERGE WITHOUT A SEPARATE CARRIER-FREEZE DECISION**

Authoritative base: `dev_0.9.9 @ 78bc8394ede5e6d81464cff5878c29bbf754c555` (merged D3 including #340 Undo and #341 generation-reset fixes).

Measured E0 host checkpoint: GitHub Actions run `32594318185`, Ubuntu 24.04 / GCC 13, E0 harness PASS.

This branch changes no production semantics. It exists to choose the physical carrier for deterministic multi-bar evolution without introducing a second sequencer, scheduler, Pattern format, persistence schema, runtime semantic-tag system, or lifecycle engine.

## Decision summary

The measurements do not reveal a reason to enlarge `SynthPattern` or regenerate material at every playback boundary.

**Rank 1 / recommended carrier: existing one-bar Pattern slots referenced by Phrase.**

Pattern remains the editable one-bar value. Phrase owns ordered multi-bar realization topology. Song remains arrangement/reference state. Transport state remains transport-only.

The implementation should be split after this research freeze:

- **E0a:** route explicit Phrase-local/evolution coordinates through existing PREPARE owners and materialize semantic bars before publication.
- **E0b:** add full-aligned-bar reference reuse/deduplication without changing the Pattern format or persistence schema.

## Problem

`SynthPattern::kSteps == 16`, so a Pattern is physically one editable bar. Passing a real transport bar clock into one-shot `migrateStrongRhythmMaterial()` cannot create temporal evolution after materialization: the same physical bar simply loops.

Existing state already provides the pieces required for a reference-based carrier:

- `PhraseCore::PhraseSlot`
- `lengthBars = 1/2/4/8`
- `patternRefs[8][3]`
- D2 generated-Phrase PREPARE for 1/2/4/8 bars
- Song references
- the bounded C/D2/D3 activation owner

The audit therefore compares only four candidates:

A. existing Pattern slots + Phrase refs;
B. automatic per-bar regeneration;
C. expanded resident multi-bar Pattern storage;
D. playback role gating as a restricted fallback.

## Ownership map

| Concept | Frozen meaning for E0 |
|---|---|
| Pattern | one editable physical bar; never an implicit 2/4/8-bar object |
| Phrase | candidate multi-bar realization carrier; ordered references to one-bar material |
| Song row | arrangement row; its current runtime duration is governed by `feel.patternBars` |
| `feel.patternBars` | persisted Scene timing value; Song-cycle duration input and FEEL setting, not an evolution identity |
| `cycleBarIndex()` / `songBarIndex_` | transport-local coordinate inside the current Song-row duration |
| `phraseBarOrdinal` | proposed explicit bar ordinal inside one realized Phrase, `0..lengthBars-1` |
| `evolutionOrdinal` | proposed deterministic PREPARE coordinate used to derive bar evolution; independent of transport resets and generation retry identity |
| generation attempt ordinal | existing generation/retry identity; must not be overloaded as Phrase evolution |

No two coordinates above may share a name or ownership contract.

### Existing Phrase Core capacity

Measured/compile-time current layout:

- `PhraseCore::kSlotCount = 4`
- `PhraseCore::kMaxBars = 8`
- `PhraseCore::kTrackCount = 3`
- `sizeof(PhraseSlot) = 60 B`
- `sizeof(PhraseBank) = 244 B`

A Phrase reference carrier therefore does not require a resident eight-bar Pattern object.

## Song-row duration semantics

Production `MiniAcid::advanceSongBar_()` calls:

```cpp
nextSongCycleBoundary(songBarIndex_, sceneManager_.currentScene().feel.patternBars)
```

A Song row advances only when that boundary reports `advanceRow`.

Therefore the current semantics are:

**Song row duration = `feel.patternBars` physical bars.**

D2 generated Phrase deliberately materializes a multi-bar result as multiple Song rows with one-bar physical Pattern references and forces `feel.patternBars = 1` for that prepared arrangement. That existing direction is compatible with a Phrase reference carrier; it does not justify expanding `SynthPattern`.

## `feel.patternBars` owner/consumer audit

`feel.patternBars` is persisted Scene state. Its transport meaning is owned by `MiniAcid::advanceSongBar_()` + `src/dsp/song_cycle_boundary.h`; the other sites are persistence, PREPARE, bridge, or UI consumers/producers.

Exact merged-D3 E0 source-audit sites:

| file:line | role |
|---|---|
| `scenes.cpp:911` | streaming parse assignment |
| `scenes.cpp:2508` | Scene serialization |
| `scenes.cpp:2787` | document load/clamp input |
| `scenes.cpp:2789` | document load assignment |
| `scenes.h:339` | persisted field declaration/default |
| `scenes.h:1074` | streaming writer |
| `src/dsp/generated_phrase_song.h:224` | D2 prepared arrangement forces one-bar Song rows |
| `src/dsp/generated_phrase_song.h:237` | generated-Phrase Undo captures previous value |
| `src/dsp/generated_phrase_song.h:286` | generated-Phrase Undo restores previous value |
| `src/dsp/miniacid_engine.cpp:732` | `cycleBarCount()` UI/runtime read |
| `src/dsp/miniacid_engine.cpp:2037` | explanatory D2 activation comment only |
| `src/dsp/miniacid_engine.cpp:3629` | authoritative Song-cycle duration input |
| `src/dsp/phrase_generator.h:446` | legacy/prepared Phrase single-bar-row projection |
| `src/dsp/song_cycle_boundary.h:8-16` | normalization and row-boundary calculation |
| `src/generation/migration/strong_rhythm_live_bridge.cpp:315` | requested Phrase-bar count bridge |
| `src/ui/pages/feel_page.cpp:123-125` | FEEL edit |
| `src/ui/pages/feel_page.cpp:252-253` | FEEL display |
| `src/ui/ui_common.cpp:524` | UI read |
| `src/ui/ui_common.cpp:576` | UI read |

The source audit reports 27 textual hits because several expressions span multiple lines. There is only one transport duration ownership path.

## `songBarIndex_` reset/write audit

`tests/e0_temporal_carrier_source_audit.py` is the reproducible line-number inventory for this exact merged-D3 base.

All direct writes:

| file:line | operation | why it happens | reset transport-local position? | reset Phrase position? | reset evolution identity? |
|---|---|---|---:|---:|---:|
| `src/dsp/miniacid_engine.cpp:415` | `songBarIndex_ = -1` | `MiniAcid::reset()` / engine lifecycle reset | yes | no | no |
| `src/dsp/miniacid_engine.cpp:486` | `songBarIndex_ = -1` | START establishes explicit pre-first-bar phase | yes | no | no |
| `src/dsp/miniacid_engine.cpp:520` | `songBarIndex_ = -1` | STOP terminates active transport cycle | yes | no | no |
| `src/dsp/miniacid_engine.cpp:889` | `songBarIndex_ = -1` | Pattern/Song mode ownership changes | yes | no | no |
| `src/dsp/miniacid_engine.cpp:937` | `songBarIndex_ = -1` | manual Song-row relocation | yes | no | no |
| `src/dsp/miniacid_engine.cpp:975` | `songBarIndex_ = -1` | edit Song slot changes while stopped | yes | no | no |
| `src/dsp/miniacid_engine.cpp:990` | `songBarIndex_ = -1` | runtime Song playback slot changes | yes | no | no |
| `src/dsp/miniacid_engine.cpp:997` | `songBarIndex_ = -1` | Live Mix runtime ownership changes | yes | no | no |
| `src/dsp/miniacid_engine.cpp:2929` | `songBarIndex_ = -1` | persisted Scene transport state is applied | yes | no | no |
| `src/dsp/miniacid_engine.cpp:3630` | `songBarIndex_ = boundary.barIndex` | normal physical-bar advance/wrap inside Song row | yes | no | no |

Other reads/declaration found by the audit:

- `src/dsp/miniacid_engine.h:451` — member declaration, initialized to `-1`;
- `src/dsp/miniacid_engine.cpp:739` — `cycleBarIndex()` presentation read;
- `src/dsp/miniacid_engine.cpp:3629` — input to `nextSongCycleBoundary()`.

Conclusion: `songBarIndex_` is strictly a transport-local coordinate. It must not become `phraseBarOrdinal` and must never be used as deterministic evolution identity.

## Current PREPARE routing gap

Merged-D3 `GeneratedPhraseSong::prepare()` already creates up to eight physical `PhraseBar` values, but Strong Rhythm semantic migration does not receive the Phrase-local bar identity consistently.

Current behavior:

### Atlas path

For each prepared bar it selects the Atlas role variation (`0/1/2`) and passes that value as the migration coordinate. That coordinate describes the Atlas variation class, not `phraseBarOrdinal`.

### Procedural path

It generates one `proceduralBase`, calls current Strong Rhythm migration **once with coordinate `0`**, then derives bars 0..N through `PhraseGenerator::deriveBar()`.

Therefore current procedural 2/4/8-bar PREPARE timing is intentionally cheap but is **not a benchmark of the future semantic per-bar migration call chain**. This is the exact routing hole E0a needs to close.

The fix must not be to feed `songBarIndex_` into migration. PREPARE needs an explicit Phrase-local deterministic coordinate before materialization.

## Memory and slot pressure

Keep these categories separate.

### Exact E0 ABI measurements on merged D3

GitHub Actions run `32594318185`:

| structure/category | measured bytes |
|---|---:|
| `SynthStep` | 7 |
| `SynthPattern` | 112 |
| `DrumStep` | 6 |
| `DrumPattern` | 96 |
| `AutomationNode` | 12 |
| `AutomationLane` | 104 |
| `DrumPatternSet` | 1,192 |
| `PhraseGenerator::PhraseBar` | 1,416 |
| `PhraseCore::PhraseSlot` | 60 |
| `PhraseCore::PhraseBank` | 244 |
| `SongPosition` | 8 |
| `Song` | 1,032 |
| aligned Synth A + Synth B + Drums material per Pattern ID | **1,416** |
| `PreparedPhraseArrangement` | **11,412 transient** |
| `GeneratedPhraseUndoPayload` | **1,048 receipt** |
| canonical `UndoOwner` resident object | 1,552 |
| canonical Undo payload capacity | 1,536 |
| C `PendingGeneration` | 1,480 per slot |
| C `PendingGeneration[2]` payload | 2,960 fixed |
| C publication state including atomics/control | **2,968 fixed** |
| D2 Phrase activation metadata `[2]` | **72 fixed** |
| D3 Song activation metadata `[2]` | **80 fixed** |
| existing C + D2 + D3 generation/activation fixed state | **3,120 fixed** |

These reproduce the accepted D2/D3 measurements on the new authoritative base.

### Pattern storage / slot pressure

Production capacity is 8 Pattern indices per bank, 2 banks per page = **16 global Pattern IDs per page**. D2 aligned Phrase material currently consumes one common global Pattern ID per bar across Synth A, Synth B and Drums.

| Phrase length | worst-case unique Pattern IDs | resident Pattern-bank capacity represented by those IDs |
|---:|---:|---:|
| 1 bar | 1 | 1,416 B |
| 2 bars | 2 | 2,832 B |
| 4 bars | 4 | 5,664 B |
| 8 bars | 8 | 11,328 B |

This is **resident Scene capacity consumption**, not newly allocated BSS.

Reference reuse changes slot pressure, not Phrase duration. For:

`A A A' A' A A'' A'' A`

an 8-bar Phrase needs **3 unique physical Pattern IDs = 4,248 B of represented aligned material**, not 8 IDs / 11,328 B.

The current D2 allocator still requires `bars` contiguous empty slots and therefore does not realize this reuse yet. Dedup/reference reuse belongs in E0b only after carrier freeze.

### Cost categories — do not combine

- **fixed BSS/resident owner state**: C publication/activation metadata, D2/D3 metadata, canonical Undo owner;
- **transient PREPARE**: `PreparedPhraseArrangement` = 11,412 B in the current host ABI;
- **Pattern storage**: already-resident Scene Pattern-bank capacity consumed by unique material;
- **Undo cost**: one generated-Phrase receipt = 1,048 B retained inside the existing 1,536 B canonical payload capacity;
- **C PendingGeneration cost**: 1,480 B × 2 existing publication payloads, independent of Phrase physical slot count.

Do not report their sum as one "Phrase RAM" figure.

## PREPARE performance harness

Reproducible host command:

```bash
bash tests/run_0_9_9_e0_temporal_carrier_audit.sh
```

The benchmark calls the real `GeneratedPhraseSong::prepare()` using the production SDL source set. It runs 32 warmups + 256 measured iterations with fixed seed `0xE0090900`.

### Measured host timings

All values are microseconds on GitHub Actions Ubuntu 24.04 runner; they characterize scaling, not Cardputer deadlines.

| family/path | bars | median total | median/bar | p95 total | max total |
|---|---:|---:|---:|---:|---:|
| HypnoticSparse / Atlas | 1 | 11.838 | 11.838 | 11.908 | 37.034 |
| HypnoticSparse / Atlas | 2 | 23.315 | 11.658 | 24.646 | 37.406 |
| HypnoticSparse / Atlas | 4 | 45.157 | 11.289 | 46.429 | 55.642 |
| HypnoticSparse / Atlas | 8 | **89.593** | 11.199 | 98.325 | 125.676 |
| LoFi / procedural | 1 | 13.420 | 13.420 | 13.460 | 26.789 |
| LoFi / procedural | 2 | 14.442 | 7.221 | 14.541 | 24.116 |
| LoFi / procedural | 4 | 16.494 | 4.123 | 16.585 | 32.108 |
| LoFi / procedural | 8 | **20.781** | 2.598 | 20.861 | 32.508 |
| Rave / procedural | 1 | 13.510 | 13.510 | 13.550 | 23.534 |
| Rave / procedural | 2 | 14.622 | 7.311 | 14.932 | 29.023 |
| Rave / procedural | 4 | 16.634 | 4.159 | 16.705 | 26.629 |
| Rave / procedural | 8 | **20.890** | 2.611 | 20.981 | 32.388 |
| dense DnB / procedural | 1 | 13.400 | 13.400 | 14.422 | 22.243 |
| dense DnB / procedural | 2 | 14.482 | 7.241 | 14.532 | 24.056 |
| dense DnB / procedural | 4 | 16.554 | 4.138 | 18.838 | 28.112 |
| dense DnB / procedural | 8 | **20.791** | 2.599 | 20.891 | 33.639 |

Deterministic checksum: `465408`.

Interpretation:

- Atlas currently performs substantial work for each prepared bar and scales approximately linearly.
- procedural 2/4/8-bar PREPARE does **not** scale linearly because Strong Rhythm migration currently runs only on the base material and `deriveBar()` handles the remaining bars.
- therefore current procedural timings cannot be used to argue that future semantic per-bar materialization is "~21 us for 8 bars"; E0a changes that call chain by design.
- even so, host results show no latency evidence that would justify moving generation into realtime playback or adding a second scheduler.

### Static stack instrumentation

GCC `-fstack-usage -fno-inline` on the host benchmark reports:

| function/frame | static report |
|---|---:|
| `GeneratedPhraseSong::prepare()` | **1,744 B** |
| harness `runPrepare()` | 32 B |
| harness `benchmarkCase()` | **11,648 B**, dynamic bounded |
| harness `main()` | 9,248 B |

`benchmarkCase()` intentionally owns the 11,412 B `PreparedPhraseArrangement`; this explains its large frame. These are per-function static host reports, **not a Cardputer FreeRTOS high-water mark and not a complete callgraph stack sum**.

### Unresolved hardware measurements

**RERUN AFTER E0a coordinate-routing implementation:**

- Cardputer ADV total PREPARE time for 1/2/4/8 bars;
- per-bar timing for the same four family cases;
- AudioTask/UI/control-task deadline impact;
- actual task stack high-water mark while 8-bar PREPARE is live;
- free/largest internal heap before, during and after PREPARE if the staging location changes.

The current host run is sufficient for carrier selection but not for post-E0a hardware acceptance.

## Semantic bar activation

Existing role policies already contain bar-sensitive semantics and already consume `request.barOrdinal`:

- Bass `SparseAnchor`: bar parity can select an empty bar or move the anchor.
- Bass `SustainAndDrop`: `%4`/parity changes empty/drop behavior.
- Chord `DubChordSpace`: bar parity can make the bar empty.
- Melodic `SparseCall`: bar parity changes rest/call behavior.
- Melodic `RestHeavy`: `%4` changes whether the bar is empty.

The source audit also confirms deterministic role selection itself uses `request.barOrdinal` in Bass, Chord and Melodic paths.

This means the system does not need a runtime semantic-tag layer; it needs the existing PREPARE path to provide an unambiguous Phrase-local bar coordinate before materialization.

Minimum conceptual API after carrier freeze:

```cpp
struct TemporalMaterialCoordinate {
  uint8_t phraseBarOrdinal;
  uint32_t evolutionOrdinal;
};
```

This is a contract sketch, **not an implementation in E0**. Prefer adding the two values to the narrowest existing migration/request context instead of creating this as a new lifecycle object if the existing types can carry them cleanly.

PREPARE semantics:

```text
bar 0 -> phraseBarOrdinal 0
bar 1 -> phraseBarOrdinal 1
...
bar N -> phraseBarOrdinal N
```

`evolutionOrdinal` must be deterministic from generation/Phrase identity and must not depend on START/STOP, Song-row relocation, Live Mix, or another transport reset.

Playback role gating is not equivalent. Once semantic rules have been materialized to note/hit topology, the causal semantic topology is no longer recoverable from the Pattern bytes alone.

## Candidate comparison

| | Phrase refs | per-bar regen | expanded Pattern | playback gating |
|---|---|---|---|---|
| RAM | low incremental topology cost; existing one-bar slots; supports reuse | low stored material but repeated transient generation | resident representation grows with max bars and duplicates Phrase topology | low |
| BSS | no new owner required | safe implementation pressures runtime state/scheduling | new resident multi-bar state/format pressure | potentially low |
| latency deadline | PREPARE off audio path; bounded activation stays unchanged | **generation must meet every musical boundary** | PREPARE can stay off audio path but copies larger resident objects | cheap playback operation |
| manual editing | **preserves current one-bar Pattern editor** | later regeneration can overwrite/replace edited material | Pattern editor must learn a multi-bar format | audible truth may diverge from stored edit truth |
| Undo | existing Pattern/Phrase/Song transaction can remain authoritative | future runtime generation complicates receipt identity | larger/new-format receipt pressure | stored receipt may not describe audible gating |
| persistence | reuses existing Pattern/Phrase refs; no new schema required for carrier | requires deterministic replay or persisted generated results | **requires Pattern/storage schema changes** | general solution tends toward persisted semantic tags |
| semantic fidelity | **high: semantic bars materialize before refs freeze** | high if deadlines and deterministic identity hold | high | limited after semantic topology was materialized away |
| realtime risk | **low** | **high** | low-to-medium | low execution cost, semantic ambiguity |
| reuse existing owners | **high** | low | low | medium |

## Ranked recommendation

1. **A — existing one-bar Pattern slots + Phrase refs.** Freeze this as the primary physical carrier. The measured topology object is tiny (`PhraseSlot` 60 B), the Pattern representation remains editable, and no new scheduler/persistence format is required.
2. **D — playback gating only as a narrow fallback** for semantics provably expressible from already-materialized events. Never use it as the general evolution carrier.
3. **B — automatic per-bar regeneration** is rejected as the default. It converts control-side generation into a recurring realtime deadline and pressures the design toward another scheduler/lifecycle owner with harder Undo semantics.
4. **C — expanded resident multi-bar Pattern storage** is rejected. Measurements show no need for it; it duplicates existing Phrase topology and forces Pattern editor/storage/persistence changes.

## E0a — recommended implementation contract

Carrier freeze:

- Pattern remains exactly one editable bar.
- Phrase refs become the multi-bar realization topology.
- Song remains arrangement/reference owner.
- `songBarIndex_` and `cycleBarIndex()` remain transport-only.
- Strong Rhythm PREPARE receives explicit `phraseBarOrdinal` and deterministic `evolutionOrdinal` through existing request/context ownership.
- semantic migration/materialization happens for every realized Phrase bar before COMMIT.
- existing canonical Undo and C/D2/D3 activation owner remain authoritative.
- no reference deduplication requirement is needed to validate E0a itself.

Acceptance focus for E0a:

1. 1/2/4/8-bar PREPARE sends `phraseBarOrdinal = 0..N-1` exactly once per semantic bar;
2. START/STOP/Song navigation does not change evolution identity;
3. the five audited bar-sensitive roles visibly produce their intended multi-bar topology after materialization;
4. one Undo restores the full committed arrangement and one Redo reproduces the same deterministic multi-bar material;
5. no new persistent schema, scheduler, or runtime semantic owner appears;
6. Cardputer timing/stack measurements above are rerun on the new call chain.

## E0b — recommended material reuse contract

After E0a is hardware-safe:

- compare full aligned bar material: Synth A + Synth B + Drums;
- store each unique aligned Pattern material once;
- write Phrase refs for temporal order;
- prove `A A A' A' A A'' A'' A` realizes 8 bars using exactly 3 unique physical Pattern IDs;
- preserve manual edit semantics: editing a shared referenced Pattern intentionally affects all references to that Pattern unless a later explicit copy-on-edit contract is separately approved;
- keep Song as arrangement/reference owner rather than creating a parallel sequencer.

The last manual-edit point is an explicit follow-up contract question: reference reuse creates aliasing. E0b must either accept visible shared-reference editing semantics or use an existing explicit duplication action before edit. It must not silently invent copy-on-write lifecycle state.

## Likely implementation touch set after freeze

Expected, not authorized by this research branch:

- `src/dsp/generated_phrase_song.h`
- `src/dsp/phrase_generator.h`
- `src/phrase/phrase_core.h`
- narrow generation request/context definitions that currently carry role `barOrdinal`
- `src/generation/migration/strong_rhythm_migration.h`
- `src/generation/migration/strong_rhythm_migration.cpp`
- relevant `src/generation/roles/*` call sites only if coordinate routing requires it
- focused D2/E0 host tests

Files that should **not** need architectural expansion for candidate A:

- `SynthPattern` format in `scenes.h`
- Scene persistence schema
- transport scheduler in `MiniAcid`
- C activation publication state machine
- canonical Undo owner

## Freeze verdict

Research gates on the authoritative merged-D3 base are satisfied:

- exact base verified: `78bc8394ede5e6d81464cff5878c29bbf754c555`;
- ABI/slot measurements reproduced;
- all 16 host PREPARE cases PASS;
- `songBarIndex_` reset/write ownership is transport-only;
- `feel.patternBars` duration ownership/consumers are mapped;
- required bar-sensitive semantic roles are confirmed PREPARE-addressable;
- no measurement justifies `SynthPattern[8]` or realtime per-bar generation.

**Recommendation: freeze candidate A and proceed with E0a as a separate implementation branch only after an explicit decision. Do not merge this research PR as production work.**
