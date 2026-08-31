# 0.9.9-E0a Phrase Temporal Coordinates

Status: **PRODUCTION CONTINUATION / DRAFT PR — DO NOT MERGE FROM THIS CHECKPOINT**

Authoritative production base: `dev_0.9.9 @ 78bc8394ede5e6d81464cff5878c29bbf754c555`.

Research evidence only: PR #342, `agent/20260822-01-0.9.9-e0-temporal-material-carrier-research @ 8d8a5548059320023b5633abe678d64a10567ed6`.

## Purpose

E0a closes the PREPARE routing hole identified by E0 without adding a scheduler, transport owner, lifecycle engine, Pattern format, or new evolution vocabulary.

Frozen ownership remains:

- `Pattern` = one editable physical bar (`SynthPattern::kSteps == 16`, `DrumPattern::kSteps == 16`);
- Phrase references = physical multi-bar realization carrier (`PhraseCore::PhraseSlot::patternRefs[8][3]`);
- `Song` = arrangement/reference owner;
- `songBarIndex_` = transport-local coordinate only;
- existing PREPARE -> COMMIT -> ACTIVATE lifecycle = publication owner.

E0a adds two explicit PREPARE input coordinates to the existing Strong Rhythm migration context:

- `phraseBarOrdinal` — physical bar ordinal inside the prepared Phrase;
- `evolutionOrdinal` — deterministic four-bar evolution-segment ordinal.

`generationAttemptOrdinal` remains a separate reroll/retry coordinate and is not overloaded.

## Exact before data flow

At exact base `78bc8394ede5e6d81464cff5878c29bbf754c555`, `GeneratedPhraseSong::prepare()` constructed:

```text
GeneratedPhraseSong::prepare(engine, bars, songStart, prepared)
  |
  +-- capture PhraseRequest / Song target / revision / generation seed
  +-- find contiguous one-bar Pattern slots
  +-- for barIndex = 0..bars-1
       |
       +-- Atlas path
       |    +-- roleForBar(bars, barIndex)
       |    +-- atlasVariationForRole(role) -> variation 0/1/2
       |    +-- AtlasRuntime::applyRecipe(...)
       |    +-- applyCurrentMigration(scene, genre, variation, bar)
       |         +-- migrationContextFor(scene, variation)
       |         +-- migrateStrongRhythmMaterial(...)
       |
       +-- procedural path
            +-- first bar only:
            |    +-- generate raw proceduralBase
            |    +-- applyCurrentMigration(scene, genre, 0, proceduralBase)
            +-- every requested bar:
                 +-- PhraseGenerator::deriveBar(
                         proceduralBase, role, seed, barIndex, bar)
```

The old `StrongRhythmMigrationContext` had `patternAddress`, `generationAttemptOrdinal`, feel, and tonal context but no Phrase-local temporal coordinate.

The old `semanticBarOrdinal(const GenreSettings&, int16_t patternAddress)` returned `patternAddress & 0xFF` only for LoFi/HipHop/FunkSoul and `0` otherwise. Therefore:

- Atlas migration received an Atlas variation coordinate, not the physical Phrase ordinal;
- procedural migration ran only once, with coordinate `0`, before all derived bars;
- physical 2/4/8-bar Phrase material existed, but Strong Rhythm migration did not consistently receive semantic Phrase-local bar identity for each prepared bar.

COMMIT and ACTIVATE were already separate from that routing gap and are not changed by E0a.

## Exact after data flow

E0a keeps one raw procedural generation and moves only semantic migration to the existing per-bar PREPARE loop:

```text
GeneratedPhraseSong::prepare(engine, bars, songStart, prepared)
  |
  +-- capture existing request / target / revision / deterministic seed
  +-- find existing one-bar Pattern slots
  +-- generate raw proceduralBase once when procedural path is used
  +-- for each physical barIndex
       |
       +-- phraseBarOrdinal = barIndex
       +-- evolutionOrdinal = phraseBarOrdinal / 4
       +-- vocabularyLocal = phraseBarOrdinal % 4
       |
       +-- Atlas path
       |    +-- apply Atlas role variation
       |    +-- applyCurrentMigration(
       |           scene, genre, variation,
       |           phraseBarOrdinal, bar)
       |
       +-- procedural path
            +-- migratedBase = proceduralBase
            +-- applyCurrentMigration(
            |       scene, genre, 0,
            |       phraseBarOrdinal, migratedBase)
            +-- PhraseGenerator::deriveBar(
                    migratedBase, role, seed, barIndex, bar)
  |
  v
PreparedPhraseArrangement.material[]
  |
  v
existing applyPreparedPersistent()
  +-- one physical Pattern slot per prepared bar
  +-- Song row references to those Pattern IDs
  |
  v
existing armPhraseActivation / commitPrepared / completePhraseActivation
```

`semanticBarOrdinal(settings, context)` now prefers explicit `context.phraseBarOrdinal` and maps it through the existing four-bar vocabulary bound. If `phraseBarOrdinal` is unspecified, the pre-E0a `patternAddress` compatibility behavior remains unchanged.

`evolutionOrdinal` is explicit context only in E0a. It is deliberately **not** mixed into RNG and does not force output variation. Existing generation vocabulary may change material only where the existing vocabulary already consumes the local bar coordinate.

## Owner table

| Concern | Authoritative owner after E0a | E0a change |
|---|---|---|
| Editable musical bar | one-bar `SynthPattern` / `DrumPattern` | none |
| Physical multi-bar realization | Phrase ordered Pattern references | none |
| Arrangement / Song rows | `Song` | none |
| PREPARE staging | `GeneratedPhraseSong::PreparedPhraseArrangement` | no temporal lifecycle state added |
| Phrase-local physical coordinate | existing PREPARE loop | explicit input routed |
| Four-bar evolution segment coordinate | existing PREPARE loop | explicit input routed |
| Generation reroll/retry identity | `generationAttemptOrdinal` | none |
| Groove Vocabulary identity bound | four bars | none |
| COMMIT | canonical `UndoOwner::commitPrepared` path | none |
| Live activation | existing C/D2/D3 phrase activation owner | none |
| Transport-local row/bar clock | existing MiniAcid transport / `songBarIndex_` | none |
| Scheduler | existing transport scheduler only | **no new scheduler** |

## 1 / 2 / 4 / 8 coordinate examples

`P` = `phraseBarOrdinal`, `E` = `evolutionOrdinal`, `V` = four-bar vocabulary-local ordinal.

```text
1 bar
P  0
E  0
V  0

2 bars
P  0 1
E  0 0
V  0 1

4 bars
P  0 1 2 3
E  0 0 0 0
V  0 1 2 3

8 bars
P  0 1 2 3 4 5 6 7
E  0 0 0 0 1 1 1 1
V  0 1 2 3 0 1 2 3
```

The 8-bar physical carrier is therefore **4+4 semantic/evolution structure**, not one eight-bar Groove Vocabulary trajectory. `PhraseCore::kMaxBars` remains 8 and the Groove Vocabulary bound remains 4.

## Determinism

Required contract:

```text
same generation request
same semantic Phrase length
same generation seed/context
same phraseBarOrdinal
same evolutionOrdinal
=> byte-identical physical Phrase bar material
```

Focused runtime characterization runs `GeneratedPhraseSong::prepare()` twice without COMMIT for 1/2/4/8 bars and compares the physical `PhraseBar` bytes.

The focused run confirmed byte-identical reruns for 1/2/4/8 bars. Additional characterization keeps all migration inputs fixed while changing only `evolutionOrdinal`; material remains identical in E0a because that coordinate is not an RNG salt or a new trajectory vocabulary.

Non-Phrase compatibility is characterized with an unspecified `phraseBarOrdinal`. The pre-E0a address-sensitive LoFi/HipHop/FunkSoul behavior remains the fallback, while other legacy callers retain the old zero semantic ordinal convention.

## Performance

Harness: `bash tests/run_0_9_9_e0a_tests.sh`.

Methodology is intentionally comparable to E0 research but these are fresh E0a measurements, not copied E0 values:

- real `GeneratedPhraseSong::prepare()`;
- production SDL source set;
- fixed seed `0xE0090900`;
- 32 warmup iterations;
- 256 measured iterations;
- 1/2/4/8 bars;
- HypnoticSparse Atlas, LoFi procedural, Rave procedural, DenseDnB procedural.

Fresh E0a median total PREPARE time on the GitHub Actions host from focused workflow run `32650612903`:

| Family | 1 bar | 2 bars | 4 bars | 8 bars |
|---|---:|---:|---:|---:|
| HypnoticSparse / Atlas | 37.701 us | 76.501 us | 156.602 us | 313.804 us |
| LoFi / procedural | 41.701 us | 78.201 us | 157.002 us | 314.404 us |
| Rave / procedural | 42.001 us | 81.301 us | 161.702 us | 323.204 us |
| DenseDnB / procedural | 45.001 us | 89.701 us | 179.402 us | 357.904 us |

These timings are host measurements only. The roughly bar-count-proportional growth is consistent with E0a performing Strong Rhythm semantic migration for every physical prepared bar; no target-runtime timing claim is made from these host values.

## Stack evidence and caveat

The focused harness ran host GCC with `-fstack-usage -fno-inline`. Relevant static `.su` entries were:

| Function | Host GCC static frame |
|---|---:|
| `runPrepare` | 32 B |
| `benchmarkCase` | 144 B |
| benchmark `main` | 4,253,888 B |

The benchmark `main()` frame contains host benchmark objects and is **not** PREPARE stack usage.

**host GCC `.su` != ESP32-S3 runtime HWM**

No existing Cardputer runtime HWM probe was found on the `GeneratedPhraseSong::prepare()` call path during the E0/E0a audit.

**ESP32-S3 PREPARE runtime HWM: NOT MEASURED.**

## Cardputer ADV fixed DRAM evidence

Focused workflow run `32650612903`, successful Cardputer ADV job:

```text
.dram0.bss   = 20,800 B
.dram0.data  =  9,920 B
external_bss =      0 B
fixed_dram   = 30,720 B
budget       = 65,536 B
headroom     = 34,816 B
```

This is fixed-DRAM build/accounting evidence. It is not ESP32-S3 PREPARE runtime stack HWM.

## Validation

Focused E0a workflow run `32650612903` completed successfully with:

1. focused E0a runtime/source tests — PASS;
2. fresh 32-warmup + 256-measured PREPARE benchmark — PASS;
3. host GCC `-fstack-usage` evidence collection — PASS;
4. full `tests/run_host_tests.sh` — PASS;
5. SDL build — PASS;
6. Cardputer ADV build — PASS;
7. Cardputer ADV fixed DRAM budget — PASS (`30,720 / 65,536 B`, headroom `34,816 B`);
8. Cardputer ADV SEQTRAK MIDI-only build — PASS.

The final bookkeeping freeze reruns the focused workflow after the documentation-only tree update so the PR check, rather than this historical run number, is the authoritative exact-final-HEAD validation record.

## Non-goals

E0a does **not** implement or authorize:

- PatternLease / Phrase Lab P1;
- EVOLVE NEXT UI;
- E1 evolution-owner consolidation;
- new BarEvolution semantics;
- new trajectory vocabulary;
- F08.1 or new harmonic masks/clocks;
- realtime per-bar regeneration;
- a second sequencer or scheduler;
- a new transport owner;
- a multi-bar Pattern or new Pattern layout;
- E0b reference deduplication.

## Acceptance checklist

- [x] branch merge-base is exactly `78bc8394ede5e6d81464cff5878c29bbf754c555`;
- [x] PR #342 head is evidence only and is not branch ancestry;
- [x] `P/E/V` sequences are exact for 1/2/4/8;
- [x] 8 bars remain 4+4 semantics;
- [x] identical PREPARE request reruns are byte-identical;
- [x] unspecified non-Phrase callers retain compatibility behavior;
- [x] changing only `evolutionOrdinal` does not manufacture variation;
- [x] Pattern remains one editable 16-step bar;
- [x] Phrase refs remain the bounded multi-bar carrier;
- [x] Song remains arrangement/reference owner;
- [x] no scheduler or lifecycle state is added;
- [x] existing COMMIT and ACTIVATE calls remain authoritative;
- [x] fresh E0a 1/2/4/8 host timing is recorded;
- [x] host GCC `.su` is reported only as host static-frame evidence;
- [x] ESP32-S3 PREPARE runtime HWM is reported as NOT MEASURED;
- [x] full host regressions pass;
- [x] SDL build passes;
- [x] Cardputer ADV build passes;
- [x] fixed DRAM budget passes;
- [x] SEQTRAK MIDI-only build passes;
- [x] final branch is one focused E0a commit from the exact production base;
- [x] Draft PR #352 is open;
- [x] no merge performed.
