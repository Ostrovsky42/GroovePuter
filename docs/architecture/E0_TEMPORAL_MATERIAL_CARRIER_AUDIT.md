# 0.9.9-E0 Temporal Material Carrier Audit

Status: **RESEARCH / CONTRACT — DO NOT MERGE WITHOUT A SEPARATE CARRIER-FREEZE DECISION**

Authoritative base: `dev_0.9.9 @ 78bc8394ede5e6d81464cff5878c29bbf754c555` (merged D3 including #340 Undo and #341 generation-reset fixes).

This branch changes no production semantics. It exists to choose the physical carrier for deterministic multi-bar evolution without introducing a second sequencer, scheduler, Pattern format, persistence schema, runtime semantic-tag system, or lifecycle engine.

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
| `feel.patternBars` | persisted Scene timing value currently consumed by Song-cycle transport and FEEL UI; not an evolution identity |
| `cycleBarIndex()` / `songBarIndex_` | transport-local coordinate inside the current Song-row duration |
| `phraseBarOrdinal` | proposed explicit bar ordinal inside one realized Phrase, `0..lengthBars-1` |
| `evolutionOrdinal` | proposed deterministic PREPARE coordinate used to derive bar evolution; independent of transport resets and generation retry identity |
| generation attempt ordinal | existing generation/retry identity; must not be overloaded as Phrase evolution |

No two coordinates above may share a name or ownership contract.

## Song-row duration semantics

Production `MiniAcid::advanceSongBar_()` calls `nextSongCycleBoundary(songBarIndex_, scene.feel.patternBars)`. A Song row advances only when that boundary reports `advanceRow`.

Therefore the current semantics are:

**Song row duration = `feel.patternBars` physical bars.**

D2 generated Phrase deliberately materializes a multi-bar result as multiple Song rows with one-bar physical Pattern references. That existing direction is compatible with a Phrase reference carrier; it does not justify expanding `SynthPattern`.

## `songBarIndex_` reset/write audit

`tests/e0_temporal_carrier_source_audit.py` is the authoritative reproducible line-number inventory for this base and prints every direct site as `file:line`.

Classification:

| operation/site | why it changes | transport-local position | Phrase position | evolution identity |
|---|---|---:|---:|---:|
| member initialization to `-1` | explicit pre-first-bar state | yes | no | no |
| `MiniAcid::reset()` | engine lifecycle reset | yes | no | no |
| `MiniAcid::start()` | START must begin at bar zero | yes | no | no |
| `MiniAcid::stop()` | STOP terminates the active cycle | yes | no | no |
| `setSongMode()` | changes Pattern/Song transport domain | yes | no | no |
| `setSongPosition()` | manual Song-row relocation | yes | no | no |
| `setActiveSongSlot()` while stopped | edit/play selection changes before transport | yes | no | no |
| `setSongPlaybackSlot()` | runtime Song source changes | yes | no | no |
| `setLiveMixMode()` | runtime playback ownership changes | yes | no | no |
| `applySceneStateFromManager()` | a different persisted transport state is applied | yes | no | no |
| `advanceSongBar_()` | normal sub-row bar advance/wrap | yes | no | no |

Conclusion: `songBarIndex_` is strictly a transport-local coordinate. It must not become `phraseBarOrdinal` and must never be used as deterministic evolution identity.

## Memory and slot pressure

Keep these categories separate.

### Already accepted D2/D3 measurements

- D2 `PreparedPhraseArrangement`: **11,412 B transient PREPARE staging**.
- generated-Phrase Undo receipt: **1,048 B**.
- D2 Phrase pending metadata: **72 B fixed**.
- canonical C `PendingGeneration`: **1,480 B per slot × 2 = 2,960 B fixed payload storage**.
- D3 Song pending metadata: **80 B fixed**.
- canonical Undo owner payload capacity: **1,536 B**, already resident.

E0 re-measures ABI sizes on the merged D3 base through `tests/e0_temporal_carrier_sizes.cpp`; CI output is authoritative if an ABI number differs from the accepted D2/D3 checkpoints above.

### Pattern slot capacity

Production capacity is 8 Pattern indices per bank, 2 banks per page = **16 global Pattern IDs per page**. D2 aligned Phrase material currently consumes one common global Pattern ID per bar across Synth A, Synth B and Drums.

Worst-case slot pressure is therefore:

| Phrase length | unique physical Pattern IDs if every bar differs |
|---:|---:|
| 1 | 1 |
| 2 | 2 |
| 4 | 4 |
| 8 | 8 |

Reference reuse changes capacity pressure, not Phrase duration. For:

`A A A' A' A A'' A'' A`

an 8-bar Phrase needs **3 unique physical Pattern IDs**, not 8, provided equality/deduplication is defined over the full aligned Synth A + Synth B + Drum material for that bar.

### Cost categories

- **fixed BSS**: existing C/D2/D3 publication/activation structures and canonical Undo owner;
- **transient PREPARE**: bounded `PreparedPhraseArrangement`, lifetime only during control-side generation;
- **Pattern storage**: already-resident Scene Pattern-bank capacity consumed by unique material; not new BSS;
- **Undo cost**: one retained canonical before-state receipt, not a second history owner;
- **C PendingGeneration cost**: existing two-slot activation publication storage, independent of Phrase physical slot count.

Do not sum these as one undifferentiated "Phrase RAM" number.

## PREPARE performance harness

Reproducible host command:

```bash
bash tests/run_0_9_9_e0_temporal_carrier_audit.sh
```

The benchmark calls the real `GeneratedPhraseSong::prepare()` using the production SDL source set. It measures 1/2/4/8 bars for:

- HypnoticSparse (`Minimal Space` recipe)
- LoFi
- Rave
- dense Drum&Bass-like material

For each case it reports median total PREPARE time, median per-bar time, p95 total, maximum total, and a deterministic checksum after 32 warmups + 256 measured iterations. GCC `-fstack-usage -fno-inline` is also run where the host compiler can expose static frames.

**Cardputer timing: RERUN AFTER E0 carrier freeze/call-chain implementation.** Host timing characterizes PREPARE scaling now; it is not a substitute for post-E0 Cardputer timing.

## Semantic bar activation

Existing role policies already contain bar-sensitive semantics:

- `SparseAnchor`
- `SustainAndDrop`
- `DubChordSpace`
- `SparseCall`
- `RestHeavy`

The relevant role request paths already reason in `barOrdinal`. This means the system does not need a new runtime semantic-tag layer; it needs the existing PREPARE path to provide an unambiguous Phrase-local bar coordinate before materialization.

Minimum API shape after carrier freeze:

```cpp
struct TemporalMaterialCoordinate {
  uint8_t phraseBarOrdinal;
  uint32_t evolutionOrdinal;
};
```

This is a contract sketch, **not an implementation in E0**. It should normally be represented by fields on the narrowest existing request/context type rather than introducing a new lifecycle owner.

PREPARE semantics:

```text
bar 0 -> phraseBarOrdinal 0
bar 1 -> phraseBarOrdinal 1
...
bar N -> phraseBarOrdinal N
```

`evolutionOrdinal` must be deterministic from generation/Phrase identity and must not depend on START/STOP, Song-row relocation, Live Mix, or another transport reset.

Playback role gating is not equivalent to this. Once semantic rules have been materialized to note/hit topology, the causal semantic topology is no longer recoverable from the Pattern bytes alone.

## Candidate comparison

| | Phrase refs | per-bar regen | expanded Pattern | playback gating |
|---|---|---|---|---|
| RAM | low incremental; uses resident one-bar slots and 60 B-class Phrase refs | low storage but repeated transient generation | high resident growth proportional to max bars | low |
| BSS | no new owner required | likely needs runtime scheduling/state to be safe | new resident multi-bar state | potentially low |
| latency deadline | PREPARE off audio path; activation stays bounded | generation must meet every musical boundary | PREPARE can be off-thread but copies larger resident objects | low at playback |
| manual editing | preserves one-bar Pattern editor | edits can be overwritten/regenerated | editor must learn multi-bar Pattern format | ambiguous; playback differs from stored material |
| Undo | existing Pattern/Phrase/Song transaction can remain authoritative | difficult: generated future bars are runtime mutations | larger receipts/new format pressure | stored truth may not match audible truth |
| persistence | existing Pattern/Phrase reference model | needs deterministic replay contract or persisted generated results | requires Pattern schema/storage changes | may require persisted semantic/runtime tags for fidelity |
| semantic fidelity | high: semantic bars materialize before refs are frozen | high if deadlines are met | high | limited after materialization |
| realtime risk | low | **high** | low-to-medium | low execution cost, high semantic ambiguity |
| reuse existing owners | **high** | low | low | medium |

## Ranked recommendation

1. **A — existing one-bar Pattern slots + Phrase refs.** Freeze this as the primary carrier unless host/Cardputer measurements reveal a concrete blocker.
2. **D — playback gating only as a narrow fallback** for semantics that are provably representable after materialization; never the general evolution carrier.
3. **B — automatic per-bar regeneration** is rejected as the default because it converts control-side generation into a recurring realtime deadline and pressures the architecture toward another scheduler/lifecycle owner.
4. **C — expanded resident multi-bar Pattern storage** is rejected before measurements show a necessity. It duplicates Phrase topology and forces Pattern editor/storage/persistence changes.

## Proposed follow-up split

### E0a — carrier freeze / coordinate routing

- freeze Pattern = one bar;
- freeze Phrase refs = multi-bar realization topology;
- route explicit `phraseBarOrdinal` and deterministic `evolutionOrdinal` through existing PREPARE request/context owners;
- preserve existing C/D2/D3 activation owner and canonical Undo owner;
- no deduplication requirement yet beyond tests proving the reference topology.

### E0b — material reuse

- materialize all semantic bars during PREPARE;
- compare full aligned bar material;
- store each unique aligned Pattern material once;
- write Phrase refs for temporal order;
- prove `A A A' A' A A'' A'' A` realizes 8 bars using 3 unique physical Pattern IDs;
- keep Song as arrangement/reference owner rather than creating a parallel sequencer.

## Likely implementation touch set after freeze

Expected, not authorized by this research branch:

- `src/dsp/generated_phrase_song.h`
- `src/dsp/phrase_generator.h`
- `src/phrase/phrase_core.h`
- narrow generation request/context definitions that currently carry role `barOrdinal`
- `src/generation/migration/strong_rhythm_migration.h/.cpp`
- relevant `src/generation/roles/*` call sites only if coordinate routing requires it
- focused D2/E0 host tests

Files that should **not** need architectural expansion for A:

- `SynthPattern` format in `scenes.h`
- Scene persistence schema
- transport scheduler in `MiniAcid`
- C activation publication state machine
- canonical Undo owner

## Freeze criteria

E0 can move to implementation only when:

1. host size/slot numbers are reproduced on this exact merged-D3 base;
2. PREPARE benchmark passes for all 16 family/length cases;
3. reset/write audit confirms no Phrase/evolution identity is owned by transport state;
4. semantic role audit confirms bar-sensitive policies can be driven at PREPARE time;
5. no measurement justifies expanded `SynthPattern[8]` storage;
6. Cardputer measurements are explicitly deferred to the post-freeze implementation call chain rather than inferred from host timing.
