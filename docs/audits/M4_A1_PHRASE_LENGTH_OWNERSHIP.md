# 0.9.9-M4-A1 — Phrase Length 1/2/4/8 Ownership Audit

Status: **RESEARCH / CHARACTERIZATION ONLY**

Branch:

`research/20260826-09-0.9.9-m4-a1-phrase-length-policy`

Frozen hardware-accepted M1 base:

`5ad44bb9400ea38d349b7f815f84f833fb18ce6a`

This audit changes no production musical semantics. `src/` semantic delta is **ZERO**.

## 1. Exact ancestry and frozen inputs

The research branch was created directly at `5ad44bb9400ea38d349b7f815f84f833fb18ce6a`, which is also the current head of `agent/20260826-06-0.9.9-m1l-multibar-melodic-listening` at audit start.

No `dev` rebase and no parallel-branch merge are part of M4-A1.

M1 and E0a are inputs to this audit, not subjects for reinterpretation. In particular, M4-A1 accepts the frozen facts that:

- one logical `phraseGenerationIdentity` may span multiple physical destination bars;
- physical `patternAddress` is not phrase identity;
- `phraseBarOrdinal` is the global semantic bar coordinate;
- `evolutionOrdinal` is existing 4+4 context, derived as `phraseBarOrdinal / 4`;
- the Groove Vocabulary projection is `phraseBarOrdinal % 4`;
- the ordinary one-bar generation path remains a compatibility path and must not be changed by M4.

## 2. Finding

The repository has enough semantic coordinates and enough physical storage for 1/2/4/8 bars, but it does **not** yet have one unambiguous authoritative phrase-length owner suitable for a generalized generation request.

There are currently two different 1/2/4/8 concepts with different responsibilities:

1. composition profile data encodes weighted `(PhraseEvolutionLawId, bars)` choices and `resolveGenerationComposition()` selects one from `GenreSettings`;
2. `Scene::feel.patternBars` is persisted FEEL/transport state and drives the playback cycle length.

Those are not interchangeable.

`GENRE != FEEL != GENERATION REQUEST`.

Therefore `scene.feel.patternBars` must not be exposed as composition ownership, and the physical pattern address must not select phrase length.

**Decision: B — OWNER AMBIGUITY.**

This is narrower than C or D:

- not C: existing Pattern/Song/PhraseCore storage can represent eight bars;
- not D: the repository already represents all four values 1/2/4/8 in composition/evolution vocabulary, although individual genre profile sets intentionally expose subsets;
- not A: the current length decision is coupled to genre profile selection while a separate FEEL value controls transport, so an explicit generation-request owner is still missing.

## 3. Owner / consumer table

| Surface | Current behavior | Classification | M4 ownership conclusion |
|---|---|---|---|
| `GenerationProfileView::phraseLaws` and `kPhraseDrive` / `kPhraseBroken` / `kPhraseSlow` / `kPhraseCompact` in `src/generation/composition/generation_profile.cpp` | Stores weighted combined phrase-law + bar-count candidates. The sets collectively contain 1, 2, 4 and 8. | **POLICY OWNER** (current profile policy) | This is the current source of composition `phraseBars`, but it is selected through `GenreSettings`; it cannot also stand in for an explicit generalized generation request. |
| `resolveGenerationComposition(const GenreSettings&, const GenerationContext&)` | Selects one `GenerationDomain::PhraseLawSelection` candidate, decodes upper bits to `phraseLaw` and low nibble to `phraseBars`. | **POLICY OWNER** (current resolver) | Current resolver owns the decision today. M4 should not silently reinterpret FEEL or storage to override it. A future explicit phrase-length request needs a separate input to this boundary. |
| `GenerationCompositionResult::phraseBars` | Carries the decoded selected bar count. Header explicitly calls it planning metadata. | **METADATA** | Resolved value, not an independent owner. It should remain the downstream composition metadata after future request resolution. |
| `StrongRhythmMigrationResult::phraseBars` | Copies planning phrase length through migration. | **METADATA** | Consumer/carrier only; must not select length. |
| `PhraseEvolutionRequest::phraseBars` | Accepts only 1/2/4/8 in `evolveMultiBarPhrase()`. | **METADATA** | An already-resolved request parameter to evolution, not composition ownership. |
| `PhraseEvolutionResult::barCount` | Reports the requested physical/semantic span after orchestration. | **METADATA** | Result metadata only. |
| `PhraseRhythmIdentity::phraseBars` | Records Core-v1 phrase identity span for the bounded rhythm realization. | **METADATA** | Identity metadata inside the 4-bar vocabulary core; not a global M4 owner. |
| `kMaxPhraseBars = 4` in `rhythm_types.h` | Bounds Groove Vocabulary Core v1, `BarTrajectory`, masks and `PhraseRhythmIdentity` arrays to four bars. | **CAPABILITY LIMIT** | Four is a vocabulary implementation bound, not the composition phrase-length policy. |
| `kGrooveVocabularyPhraseBars = 4` in `strong_rhythm_migration.h` | Defines the existing 4-bar vocabulary projection and E0a 4+4 evolution boundary. | **CAPABILITY LIMIT** | Must stay a projection/capability boundary. It does not mean all compositions are four bars. |
| `PhraseEvolution::evolveMultiBarPhrase()` | 1/2/4 use one Core call. 8 uses two deterministic 4-bar segments, increments the second segment generation `phraseOrdinal`, and reuses the first `PhraseRhythmIdentity`. | **CAPABILITY LIMIT** | Existing 4+4 adapter proves eight bars do not require a new semantic coordinate or an 8-bar Core-v1 array. |
| `PhraseCore::kMaxBars = 8`, `PhraseMetadata::lengthBars`, `PhraseSlot::patternRefs[8][3]` | PhraseCore validates exactly 1/2/4/8 and stores up to eight bar references for Synth A, Synth B and Drums. | **CAPABILITY LIMIT** | Storage/metadata capacity, not musical policy. |
| `Scene::feel.patternBars` (`FeelSettings`) | Persisted FEEL field with documented values 1/2/4/8. `MiniAcid::cycleBarCount()` and `advanceSongBar_()` consume it as the playback-cycle span. | **UI REQUEST** | This is current user/scene FEEL state and transport request. It is explicitly **not** the M4 composition owner. |
| `normalizedSongPatternBars()` in `src/dsp/song_cycle_boundary.h` | Accepts 1/2/4/8 and falls back to 1 for transport cycle calculation. | **COMPATIBILITY NORMALIZER** | Transport sanitization only. It must not become generation policy. |
| `normalizedPhraseBars()` in `strong_rhythm_live_bridge.cpp` | Audition-only helper accepts 1/2/4/8 and falls back to 1. It normalizes both `scene.feel.patternBars` for CurrentWired audition and composition metadata reported by selection. | **COMPATIBILITY NORMALIZER** | Audition compatibility helper only; no authority to decide phrase length. |
| `regeneratePhraseAuditionWithProbe()` `result.requestedBars` | M1 listening cases force the frozen 4-bar test span; CurrentWired uses normalized `scene.feel.patternBars`. | **UI REQUEST** | Test/listening request surface. M1's hard-coded four is frozen test scope, not future production policy. |
| Bank/pattern selection in `regeneratePhraseAuditionWithProbe()` | Writes each requested bar to Bank B pattern index equal to `bar`, on Drums/A/B. | **TRANSPORT/ALLOCATION CONSUMER** | Allocation consumes a resolved length. `patternAddress` is produced from page/bank/bar; it must never feed back into phrase-length identity. |
| Song B publication in `regeneratePhraseAuditionWithProbe()` | Publishes rows `0..requestedBars-1`, sets Song length, loop range and Song B playback. | **TRANSPORT/ALLOCATION CONSUMER** | Transport consumes length after semantic/materialization decisions. It does not own length. |
| `Song::kMaxPositions = 128` | Fixed Song position capacity. | **CAPABILITY LIMIT** | Far above M4's eight-bar maximum. |
| `Bank<T>::kPatterns = 8` for two banks | Each Drum/Synth A/Synth B bank has eight physical pattern slots. | **CAPABILITY LIMIT** | Exactly enough for an eight-bar one-slot-per-bar phrase in a single bank. No allocator is required merely for 1/2/4/8 capacity. |

### Note on the requested `normalizedPhraseBars()` audit item

At the frozen M1 base the symbol with that exact name is local to the phrase audition bridge, not composition policy. Normal Song transport has the separately named `normalizedSongPatternBars()`. Both are compatibility normalizers; neither is an authoritative musical owner.

## 4. Current composition flow

The current composition path is:

`GenreSettings`

→ `generationProfileFor(settings)`

→ profile-specific weighted `phraseLaws`

→ `resolveGenerationComposition()` / `GenerationDomain::PhraseLawSelection`

→ decode combined phrase candidate

→ `GenerationCompositionResult::{phraseLaw, phraseBars}`

→ migration/evolution/materialization consumers.

The profile vocabulary currently contains these length sets:

- `kPhraseDrive`: 2, 4
- `kPhraseBroken`: 2, 4
- `kPhraseSlow`: 4, 8
- `kPhraseCompact`: 1, 2, 4

Collectively, the repository has the required 1/2/4/8 vocabulary. However, this is a **genre/profile default policy**, not a representation of a caller saying “generate exactly N bars”.

That distinction is the M4-A1 ownership gap.

## 5. `scene.feel.patternBars` is not composition policy

`FeelSettings::patternBars` is stored in `Scene` next to grid/timebase/swing/timing-profile controls.

Playback consumes it directly:

- `MiniAcid::cycleBarCount()` reads `currentScene().feel.patternBars` and accepts 1/2/4/8;
- `MiniAcid::advanceSongBar_()` passes it to `nextSongCycleBoundary()`;
- `normalizedSongPatternBars()` falls back to one bar for invalid transport state.

That makes it a persisted FEEL/UI request feeding transport behavior.

It does **not** appear in `resolveGenerationComposition()`, whose phrase choice comes from the active generative-mode/recipe profile.

M4 must therefore preserve this boundary:

- FEEL may request/control physical playback span;
- GENERATION must own an explicit requested semantic composition span when the generalized API is introduced;
- GENRE may provide weighted musical defaults/admissibility for phrase law;
- allocation and transport consume the resolved result.

No implicit `scene.feel.patternBars -> GenerationCompositionResult::phraseBars` assignment should be introduced.

## 6. Semantic coordinates for 1/2/4/8

M4 does not need a new bar-coordinate system. Use the frozen E0a contract.

| Phrase length | Global `phraseBarOrdinal` | Vocabulary ordinal (`% 4`) | `evolutionOrdinal` (`/ 4`) |
|---:|---|---|---|
| 1 | `0` | `0` | `0` |
| 2 | `0,1` | `0,1` | `0,0` |
| 4 | `0,1,2,3` | `0,1,2,3` | `0,0,0,0` |
| 8 | `0,1,2,3,4,5,6,7` | `0,1,2,3,0,1,2,3` | `0,0,0,0,1,1,1,1` |

The authoritative semantic coordinate for eight bars is therefore **global ordinal 0..7**.

`vocabulary ordinal 0..3 + evolutionOrdinal 0/1` is a derived projection/context pair for existing 4+4 consumers, not a replacement identity coordinate.

This matches the existing implementation:

- `phraseTemporalCoordinatesForBar(bar)` preserves the global ordinal and derives `evolutionOrdinal = bar / 4`;
- `phraseVocabularyBarOrdinal(bar)` derives `bar % 4`;
- `evolveMultiBarPhrase(8)` runs two four-bar segments while reusing one phrase rhythm identity rather than creating a new eight-bar Core-v1 coordinate space.

## 7. Physical capacity audit

### Pattern storage

`Scene` owns two physical banks for each material family:

- `drumBanks[2]`
- `synthABanks[2]`
- `synthBBanks[2]`

Each `Bank<T>` contains `kPatterns = 8` patterns.

Therefore a single bank provides these one-pattern-per-bar capacities:

| Track | 1 bar | 2 bars | 4 bars | 8 bars |
|---|---:|---:|---:|---:|
| Drums | YES | YES | YES | YES |
| Synth A | YES | YES | YES | YES |
| Synth B | YES | YES | YES | YES |

An eight-bar phrase consumes all eight local pattern slots for each participating track in that bank. That is a capacity edge, but not a blocker.

### Existing M1 Bank B path

M1's audition bridge already demonstrates the required allocation shape for four bars:

- reserved bank index `1` (Bank B);
- `bar` becomes physical pattern index only **after** semantic selection;
- Drums, Synth A and Synth B use the same local Bank B slot for each row;
- Song A and Bank A remain untouched by the audition publication.

The loop is already structurally bounded by `result.requestedBars`, while the bank itself has eight slots. Extending the generalized production contract to 8 does not require a new storage primitive or a new allocator merely to fit the bars.

M4-A1 does **not** change the frozen M1 four-bar audition path.

### Song storage / transport

`Song::kMaxPositions = 128`, and `Scene` contains `songs[2]`.

The existing audition path uses Song slot `1` (Song B), clears its rows, publishes `requestedBars` rows, then sets:

- Song length = `requestedBars`;
- position = 0;
- loop range = `0..requestedBars-1`;
- Song B as playback slot.

Thus Song B has ample capacity for 1/2/4/8.

### PhraseCore storage

`PhraseCore` has:

- `kSlotCount = 4`;
- `kMaxBars = 8`;
- `kTrackCount = 3`;
- `patternRefs[kMaxBars][kTrackCount]`;
- `isValidLength()` accepting exactly 1/2/4/8.

Therefore PhraseCore can already represent all four required lengths across Synth A, Synth B and Drums.

**Capacity conclusion: no Pattern, Song, or PhraseCore storage blocker for 1/2/4/8. No new allocator is justified by M4 length capacity.**

## 8. Pattern address must remain post-semantic

The physical address flow in the accepted M1 audition path is intentionally one-way:

resolved logical selection / phrase identity

→ global `phraseBarOrdinal`

→ materialize bar

→ choose Bank B + local pattern index

→ derive physical `patternAddress`

→ publish Song B row.

M4 must not reverse this relationship.

Forbidden ownership forms include:

- `patternIndex == 7` implies eight-bar phrase;
- Bank B implies four/eight bars;
- Song row count selects a new musical phrase identity;
- `patternAddress` participates in choosing requested phrase length.

Physical addresses are allocation/transport coordinates only.

## 9. Ordinary one-bar compatibility

The future generalized M4 API must be opt-in and must not reinterpret plain `G`.

Compatibility contract:

1. Keep the existing ordinary one-bar generation entry path unchanged.
2. Do not make it read `scene.feel.patternBars` as composition ownership.
3. Do not make it allocate additional bars because the profile's planning metadata happens to report 2/4/8.
4. Existing non-Phrase migration callers continue to use the frozen unspecified phrase semantic context.
5. A generalized one-bar request uses `phraseBarOrdinal = 0`; it does not require a different one-bar identity model.
6. Physical `patternAddress` remains the destination coordinate after semantic resolution.

This permits a new 1/2/4/8 API without changing plain one-bar G behavior.

## 10. Exact recommended next production contract

M4-A1 recommends one narrow production boundary, not a UI or allocator change.

### Authoritative owner

**An explicit generation request must own requested phrase length.**

It must be resolved at the composition boundary before semantic realization and before any physical allocation.

Do not use `Scene::feel.patternBars` as that field and do not infer it from `patternAddress`, Bank, Song row count, or PhraseCore storage.

### Minimal API shape

Preserve the current two-argument `resolveGenerationComposition(settings, generation)` compatibility path exactly for existing callers.

Add a separate generalized phrase-resolution entry point that receives a strictly validated requested bar count in `{1,2,4,8}` together with the existing genre/settings and generation context. The resolver then:

1. treats the requested length as authoritative;
2. uses the selected genre/profile only to choose an admissible/weighted `PhraseEvolutionLawId` for that requested length;
3. returns that same resolved length in `GenerationCompositionResult::phraseBars`;
4. passes the value unchanged to semantic/evolution/materialization consumers;
5. allocates physical bars only after the logical phrase identity and all per-bar semantic coordinates are known.

The implementation may retain the current packed `(law << 4) | bars` catalog representation internally. M4 does not require a new persistent schema or a new semantic coordinate.

### Admission behavior that must be frozen in the next checkpoint

Current profile sets do not each contain all four lengths. Therefore the production checkpoint must explicitly choose one behavior when an explicit requested length has no candidate in the active profile:

- reject as unsupported for that profile; or
- define a reviewed genre-independent law fallback for the already-requested length.

It must **not** silently choose a different bar count. Length remains request-owned.

This is a follow-on policy/admission decision, not a storage problem and not permission to make FEEL own composition length.

## 11. Decision

**B — OWNER AMBIGUITY**

Evidence summary:

- semantic coordinates: sufficient;
- 8-bar 4+4 projection: already defined;
- PhraseCore: supports 1/2/4/8;
- Bank capacity: 8 slots per bank per Drums/A/B;
- Song capacity: 128 positions;
- current composition phrase length: selected from genre/profile weighted phrase choices;
- current transport length: `scene.feel.patternBars`;
- explicit generation-request phrase-length owner: absent.

## 12. Next checkpoint

Next production checkpoint should freeze only the **generation-request phrase-length resolution contract**:

`explicit requested 1/2/4/8`

→ composition admission / phrase-law selection

→ `GenerationCompositionResult::phraseBars`

→ existing E0a global bar coordinates

→ existing semantic realization

→ existing physical allocation / Song transport consumers.

It should include focused tests for:

- explicit 1, 2, 4, 8 requests;
- eight-bar global ordinals `0..7` with vocabulary projection `0..3,0..3` and evolution ordinals `0,0,0,0,1,1,1,1`;
- unsupported profile/length admission behavior;
- plain one-bar G byte/behavior compatibility;
- proof that changing physical `patternAddress` does not change requested length or logical phrase identity.

No M2, M3, allocator, UI, persistence, or M1 semantic work belongs in that checkpoint.
