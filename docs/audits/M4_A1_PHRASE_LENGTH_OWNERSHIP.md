# 0.9.9-M4-A1 — Phrase Length 1/2/4/8 Ownership Audit

Status: **RESEARCH / CHARACTERIZATION ONLY**  
Decision: **B — OWNER / REPRESENTATION BOUNDARY GAP**  
Frozen M1 base: `5ad44bb9400ea38d349b7f815f84f833fb18ce6a`  
M4-A1 branch: `research/20260826-09-0.9.9-m4-a1-phrase-length-policy`  
Production `src/` delta: **ZERO**

## Purpose

Freeze who currently owns phrase length, characterize the existing 1/2/4/8 coordinate contract, and prevent a future multi-bar production checkpoint from silently conflating:

- GENRE/profile phrase planning;
- FEEL/transport `patternBars`;
- explicit generation-request phrase length;
- physical Pattern/Song allocation.

M4-A1 does not start M4 production and does not reopen frozen M1 behavior.

## Exact ancestry

The research branch was created directly from the hardware-accepted M1 head:

`agent/20260826-06-0.9.9-m1l-multibar-melodic-listening @ 5ad44bb9400ea38d349b7f815f84f833fb18ce6a`

The original docs-only M4-A1 audit head was:

`6a37ea18c0e64fd9dc739c17fccb579742494cae`

Executable characterization is added on that same branch. No `dev` rebase and no parallel-branch merge are part of this checkpoint.

## Finding

The repository already contains:

- phrase-length policy vocabulary covering 1/2/4/8 collectively;
- global semantic phrase bar coordinates sufficient for 1/2/4/8;
- deterministic 8-bar 4+4 projection using the existing E0a coordinate contract;
- physical Pattern, Song and PhraseCore capacity for eight bars.

What is missing is one explicit generalized generation-request owner for requested phrase length.

There are currently two distinct 1/2/4/8 concepts:

1. `resolveGenerationComposition()` selects a packed phrase-law/bar-count candidate from the active generation profile;
2. `Scene::feel.patternBars` is persisted FEEL/transport state and controls the physical playback cycle.

They are not the same contract:

`GENRE != FEEL != GENERATION REQUEST`.

Therefore the frozen M4-A1 decision remains:

**B — OWNER / REPRESENTATION BOUNDARY GAP.**

This is not an implementation-capacity gap and not a phrase-length vocabulary gap.

## Current owner / consumer classification

| Surface | Classification | Current contract |
|---|---|---|
| `GenerationProfileView::phraseLaws` | **POLICY OWNER** | Stores weighted `(PhraseEvolutionLawId, bars)` candidates. |
| `resolveGenerationComposition(...)` | **POLICY OWNER** | Selects `PhraseLawSelection` and decodes `GenerationCompositionResult::{phraseLaw, phraseBars}`. |
| `GenerationCompositionResult::phraseBars` | **METADATA** | Resolved planning metadata; not an independent owner. |
| `StrongRhythmMigrationResult::phraseBars` | **METADATA** | Copies composition planning metadata. |
| ordinary `RhythmRealizationRequest::phraseBars = 1` | **COMPATIBILITY CONSUMER** | Frozen ordinary strong-rhythm realization remains one bar. |
| ordinary `ChordProgressionRequest::phraseBars = 1` | **COMPATIBILITY CONSUMER** | Frozen ordinary harmonic realization remains one bar. |
| `Scene::feel.patternBars` | **UI / TRANSPORT REQUEST** | Persisted FEEL state consumed by transport cycle logic. |
| `normalizedSongPatternBars()` | **COMPATIBILITY NORMALIZER** | Sanitizes transport span to 1/2/4/8. |
| audition-local `normalizedPhraseBars()` | **COMPATIBILITY NORMALIZER** | Sanitizes explicit phrase-audition request/reporting values. |
| `PhraseEvolutionRequest::phraseBars` | **METADATA / SEMANTIC CONSUMER** | Receives an already requested/resolved length. |
| `kGrooveVocabularyPhraseBars = 4` | **CAPABILITY LIMIT** | Four-bar vocabulary projection bound, not global phrase policy. |
| `kMaxPhraseBars = 4` in rhythm Core | **CAPABILITY LIMIT** | Core-v1 phrase arrays/trajectories are four bars. |
| `PhraseCore::kMaxBars = 8` | **CAPABILITY LIMIT** | Physical/reference storage supports up to eight bars. |
| `Bank<T>::kPatterns = 8` | **CAPABILITY LIMIT** | One bank can store eight one-bar pattern slots per track. |
| `Song::kMaxPositions = 128` | **CAPABILITY LIMIT** | Song capacity is not an M4 blocker. |
| `patternAddress` | **TRANSPORT / ALLOCATION COORDINATE** | Physical destination/compatibility coordinate; not phrase-length policy. |

## Current composition ownership

The current phrase planning flow is:

`GenreSettings`

→ `generationProfileFor(settings)`

→ weighted `phraseLaws`

→ `resolveGenerationComposition()`

→ `GenerationDomain::PhraseLawSelection`

→ packed candidate decode

→ `GenerationCompositionResult::{phraseLaw, phraseBars}`.

Current profile subsets are:

- `kPhraseDrive`: 2, 4;
- `kPhraseBroken`: 2, 4;
- `kPhraseSlow`: 4, 8;
- `kPhraseCompact`: 1, 2, 4.

Collectively the policy vocabulary covers 1/2/4/8. Individual profiles intentionally expose subsets.

The source-contract guard freezes that `resolveGenerationComposition()` remains the current profile phrase-length policy owner. It also freezes that neither `patternBars` nor `patternAddress` is a direct composition-policy input.

## Plain G remains one-bar

On the frozen M1 production path, profile `composition.phraseBars` is copied into migration result metadata, but ordinary semantic materialization still explicitly sets:

- `RhythmRealizationRequest::phraseBars = 1`;
- `ChordProgressionRequest::phraseBars = 1`.

The normal GENRE G and DRUMS G live-bridge entry points do not read `scene.feel.patternBars` as semantic phrase length.

The explicit phrase audition is intentionally separate and its header states that it never replaces normal G.

This distinction is now guarded in CI so a future refactor cannot silently turn FEEL transport state into normal-G phrase policy without breaking M4-A1 characterization.

## Current live disposition: `patternBars != phraseBars`

**A mismatch is possible today in Phrase Audition. It is not classified as a current normal-generation length bug.**

For `PhraseAuditionListeningCase::CurrentWired`:

- `result.requestedBars` comes from normalized `scene.feel.patternBars`;
- `result.profileBars` comes independently from normalized `selection.phraseBars`.

Those values can differ because they are produced by different owners.

That mismatch is observable audition metadata and is useful evidence of the ownership split.

It is not currently two competing length owners inside plain G because plain G remains one-bar and does not consume FEEL `patternBars` as semantic phrase length.

Therefore current disposition is:

**LATENT MULTI-BAR INTEGRATION RISK — NOT A DEMONSTRATED NORMAL-G PRODUCTION LENGTH BUG.**

If a future production checkpoint makes generalized multi-bar semantic realization reachable, it must explicitly publish/commit the resolved semantic span to the downstream physical transport contract instead of inheriting either current field implicitly.

## Physical `patternAddress` does not own phrase length

M4-A1 freezes the ownership direction:

semantic request / profile resolution

→ stable logical phrase identity

→ global `phraseBarOrdinal`

→ semantic materialization

→ physical Bank/pattern slot

→ `patternAddress`

→ Song/transport publication.

The source guard checks that the composition policy sources do not contain `patternAddress` and that phrase length is decoded from `PhraseLawSelection` inside `resolveGenerationComposition()`.

This deliberately does **not** claim that legacy compatibility seeding can never depend on physical address. The narrower frozen statement is the ownership statement required by M4:

**physical `patternAddress` is not the phrase-length policy selector.**

## Deterministic phrase coordinates

M4 does not introduce a new temporal coordinate.

The existing E0a/M1 contract is:

`phraseBarOrdinal = global bar ordinal`

`vocabularyPhraseBarOrdinal = phraseBarOrdinal % 4`

`evolutionOrdinal = phraseBarOrdinal / 4`.

Expected coordinates are:

| Length | Global ordinal | Vocabulary ordinal | Evolution ordinal |
|---:|---|---|---|
| 1 | `0` | `0` | `0` |
| 2 | `0,1` | `0,1` | `0,0` |
| 4 | `0,1,2,3` | `0,1,2,3` | `0,0,0,0` |
| 8 | `0,1,2,3,4,5,6,7` | `0,1,2,3,0,1,2,3` | `0,0,0,0,1,1,1,1` |

For eight bars, the authoritative semantic coordinate is therefore global:

**`phraseBarOrdinal = 0..7`.**

The `%4` vocabulary ordinal plus `/4` evolution ordinal is derived compatibility context, not a replacement for the global coordinate.

`tests/test_0_9_9_m4_a1_phrase_coordinates.cpp` calls the real `phraseTemporalCoordinatesForBar()` helper for 1/2/4/8 and verifies this mapping deterministically.

## Physical capacity

### Pattern banks

Each Drum/Synth A/Synth B bank contains eight pattern slots.

| Track | 1 bar | 2 bars | 4 bars | 8 bars |
|---|---:|---:|---:|---:|
| Drums | YES | YES | YES | YES |
| Synth A | YES | YES | YES | YES |
| Synth B | YES | YES | YES | YES |

Eight bars consume the full local bank for a participating track, but no new allocator is required merely for M4 length capacity.

### Song

`Song::kMaxPositions = 128`, so 1/2/4/8 publication is within existing physical capacity.

### PhraseCore

PhraseCore has:

- `kMaxBars = 8`;
- `kTrackCount = 3`;
- `patternRefs[kMaxBars][kTrackCount]`;
- valid lengths 1/2/4/8.

PhraseCore physical/reference capacity is therefore sufficient.

## M3 cross-check

M4-A1 strengthens the input contract for the future:

`0.9.9-M3-T1 — BOUNDED PHRASE HARMONIC TIMELINE CONTRACT`.

M3-T1 must support, from its first representation contract:

- requested phrase lengths `{1,2,4,8}`;
- global `phraseBarOrdinal = 0..phraseBars-1`;
- therefore global ordinals through `0..7` for an eight-bar phrase.

It must not build a first representation structurally limited to bar ordinals `0..3` and defer 8-bar coordinate support to a later M4 pass.

Accepted F08.1 gives the capacity question a bounded worst-case coordinate envelope:

`8 bars × up to 4 harmonic events/bar = 32 phrase harmonic event positions`.

That is a **phrase harmonic timeline representation capacity requirement**.

It does **not** imply that the existing `ChordProgressionPlan` itself must grow to 32 entries. M3-T1 may use another bounded representation, indexing scheme, source reuse, or equivalent encoding as long as it proves the 32-position semantic envelope without physical-address dependence.

Do not infer:

`32 timeline positions => ChordProgressionPlan::events[32]`.

That design decision belongs to M3-T1, not M4-A1.

## M2 cross-check

M4-A1 remains consistent with the M2 ownership direction:

semantic identity / coordinates

→ materialization

→ physical pattern lifecycle.

A physical PatternPlayer/transport owner may decide when already materialized physical state advances, but it must not infer or redefine logical phrase length from `patternAddress`.

No cross-audit ownership conflict was found.

## Next-policy default direction

The next M4 production contract must receive an explicit requested phrase length in `{1,2,4,8}` at the generation/composition boundary.

If the active profile has no admissible phrase-law candidate at exactly that requested length, the default must be:

**REJECT.**

No silent nearest-length substitution is allowed.

Forbidden examples:

- requested 8 -> silently generate 4;
- requested 4 -> silently generate 2;
- choose nearest profile-supported length;
- use FEEL `patternBars` as an implicit replacement length.

A reviewed fallback law that preserves the exact requested bar count remains a separate future policy decision. M4-A1 does not implement or approve such a fallback.

## Executable characterization

### Files

- `tests/test_0_9_9_m4_a1_source_contract.py`
- `tests/test_0_9_9_m4_a1_phrase_coordinates.cpp`
- `tests/run_generation_stage13_tests.sh` — existing Core-regression entrypoint, minimally extended to execute both tests.

### Source-contract guard proves

- `scene.feel.patternBars` is absent from composition ownership;
- composition sources do not use `patternAddress` as phrase-length policy;
- `resolveGenerationComposition()` remains the current `phraseBars` policy owner;
- ordinary strong-rhythm materialization remains one-bar;
- normal GENRE/DRUMS G does not use `patternBars` as semantic phrase length;
- CurrentWired `requestedBars` and selected `profileBars` are assigned independently;
- M4-A1 has no `src/` delta.

Expected source-guard output:

```text
M4-A1 source contract: PASS
  composition_owner=resolveGenerationComposition
  plain_G_phraseBars=1
  patternBars_owner=FEEL_transport_not_composition
  patternAddress_phrase_length_owner=NO
  audition_requestedBars_vs_profileBars=INDEPENDENT
```

### Coordinate test proves

Expected output:

```text
M4-A1 coordinates: PASS
  lengths=1,2,4,8 global=0..N-1
  length8_global=0,1,2,3,4,5,6,7
  length8_vocabulary=0,1,2,3,0,1,2,3
  length8_evolution=0,0,0,0,1,1,1,1
```

## Hardware list

None. M4-A1 is host/source characterization only.

Frozen M1 Cardputer ADV hardware acceptance remains closed and is not rerun or reinterpreted by this checkpoint.

## Wiring

None.

## Build / run

Focused source guard:

```bash
python3 tests/test_0_9_9_m4_a1_source_contract.py
```

Focused coordinate test:

```bash
mkdir -p build/host-tests
g++ -std=c++17 -Wall -Wextra -Werror -Wvla -Wno-c++20-extensions -I. \
  tests/test_0_9_9_m4_a1_phrase_coordinates.cpp \
  -o build/host-tests/test_0_9_9_m4_a1_phrase_coordinates
build/host-tests/test_0_9_9_m4_a1_phrase_coordinates
```

Full existing generation matrix including both new M4-A1 checks:

```bash
bash tests/run_generation_stage13_tests.sh
```

GitHub `Core regressions` already invokes that runner for pull requests, so no new workflow owner is introduced.

## Expected behavior

Both focused tests print PASS and return zero. Existing Generation Stage 13/14/15 regressions then continue unchanged.

No firmware behavior, UI, transport, persistence, allocator or musical output changes in M4-A1.

## Troubleshooting

If the source guard fails:

- inspect whether a refactor moved `patternBars` or `patternAddress` into composition ownership;
- inspect whether ordinary G stopped being one-bar;
- inspect whether CurrentWired audition collapsed `requestedBars` and `profileBars` into one owner;
- do not weaken the assertion until ownership is re-audited.

If the coordinate test fails:

- inspect `phraseTemporalCoordinatesForBar()` and `kGrooveVocabularyPhraseBars`;
- do not add a new M4 temporal coordinate as a test fix;
- any change to the frozen `0..7 / %4 / /4` mapping requires an explicit E0a/M1 contract review.

## Acceptance checklist

- [ ] source-contract guard PASS;
- [ ] deterministic 1/2/4/8 coordinate test PASS;
- [ ] Core regressions CI executes both tests and is green;
- [ ] changed files contain no `src/` path;
- [ ] CurrentWired mismatch is documented as possible but not a current normal-G length bug;
- [ ] M3-T1 dependency explicitly includes 1/2/4/8 and global ordinals through 7;
- [ ] 32 harmonic event positions are documented as representation capacity, not a mandated `ChordProgressionPlan` resize;
- [ ] unsupported explicit requested length default direction is REJECT;
- [ ] no silent nearest-length fallback;
- [ ] Decision B remains the frozen outcome after executable evidence is green.

## Decision

**B — OWNER / REPRESENTATION BOUNDARY GAP.**

The representation/capability pieces are sufficient to state a future explicit 1/2/4/8 request contract, but no single generalized request owner exists today.

M4-A1 stops after executable characterization. It does not implement the owner.

## Hard stop

After the source guard, deterministic coordinate characterization and existing CI are green, freeze Decision B and stop.

Do not start M4 production from this checkpoint.
