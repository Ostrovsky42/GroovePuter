# Generation Stage 15 — ChordProgression acceptance

## Purpose

Stage 15 owns **which harmonic identity is active at each harmonic event**. It emits scale degrees plus chord quality; it never owns event timing and never emits absolute MIDI pitches.

Implementation base at branch creation: current Stage 12 PR #204 head `0b8caff7a7e71f599eeac2f7e473f9f41f45e482`.

The briefing's earlier `476e2338474b7b19664d21071e1bf4b99bdfc8ba` was no longer PR #204 head when Stage 15 started, so the branch was created from the then-current head instead of knowingly starting stale.

## Ownership

Stage 15 owns:

- `ProgressionId` selection from profile data;
- deterministic progression grammar selection;
- `HarmonicEvent.degree` in `0..6`;
- `HarmonicEvent.quality`;
- `rootOffsetSemitones` only in `[-2,+2]`, and only for `ParallelShift` / `BorrowedLift`.

Stage 15 does not own:

- harmonic event timing/count — `ChordRhythm` owns that;
- scale intervals, root pitch class, register or MIDI pitch;
- voicing, inversions or voice leading;
- Scene, Song or PhraseCore state;
- bass or melodic rhythm.

The production bridge passes `popcount(chord.plan.onsets)` into `ChordProgressionRequest.harmonicEventCount`. Stage 15 does not choose that count.

## Tonal Projector boundary

The Stage 15 base does not contain `src/generation/tonal/tonal_projector.h`. Stage 15 therefore has **no include or link dependency** on the parallel Tonal Projector work.

The semantic progression plan is production-reachable and validated before legacy chord pitch materialization. The current compatibility renderer, `projectLegacyPitchPattern`, remains the owner of absolute pitch until Tonal Projector is merged/rebased.

Stage 15 intentionally does **not** translate `degree` to semitones locally. Doing so would add a fourth scale-interval mapping and violate the axis contract. Tonal Projector integration is separate follow-up work after PR #201 is available on the Stage 15 ancestry.

Consequently host acceptance proves the harmonic-plan contract. Audible degree-to-MIDI projection remains `HARDWARE_PENDING` together with the Tonal Projector integration; this PR does not claim a hardware musical verdict.

## Reachability

State: **A — Production-reachable**.

Proof is two-part:

1. `resolveGenerationComposition()` selects a concrete non-`Auto` `ProgressionId` from profile data using the existing `GenerationDomain::ChordPitch` domain.
2. `migrateStrongRhythmMaterial()` invokes `realizeChordProgression()` after `ChordRhythm` has produced onsets and before chord pitch materialization.

`tests/test_generation_stage15_reachability.cpp` enumerates every exact `(GenerativeMode, recipe)` profile in the current profile table and 1024 deterministic generation ordinals per profile. Every concrete `ProgressionId` must be observed as an actual selected composition result, not merely present in a candidate pool.

The current repository has `GenerativeMode::Outrun`, not a separate `Synthwave` enum. Stage 15 does not invent a new genre to satisfy wording. The data-only reuse criterion is pinned to three distinct genre identities for every demonstrated primitive:

- `StaticModal`: Acid, Techno, Rave;
- `TwoFiveOne`: LoFi, TripHop, FunkSoul;
- `PopCycle`: House, Outrun, FunkSoul.

Adding another genre that reuses these primitives requires only profile data, not edits to `chord_progression.cpp`.

## Evidence class

Atlas is a rhythm corpus and contains no harmonic observations used by Stage 15. Every progression grammar and every genre progression pool is therefore **`EDITORIAL_CURATED`**.

| Grammar | Evidence class |
| --- | --- |
| `StaticModal` | `EDITORIAL_CURATED` |
| `PedalDrone` | `EDITORIAL_CURATED` |
| `PopCycle` | `EDITORIAL_CURATED` |
| `TwoFiveOne` | `EDITORIAL_CURATED` |
| `ParallelShift` | `EDITORIAL_CURATED` |
| `MinorFall` | `EDITORIAL_CURATED` |
| `BorrowedLift` | `EDITORIAL_CURATED` |

No grammar is labelled measured/evidence-backed and no external progression table is imported as a preset dump.

## RNG and fixed-capacity contracts

- Existing RNG domain: `GenerationDomain::ChordPitch`.
- No new `GenerationDomain` value.
- Profile selection while the requested progression is unresolved uses the stable `static_cast<uint8_t>(ProgressionId::Auto)` salt; it does not incorporate `ChordRhythm` identity or another profile-axis selection into that salt.
- Concrete grammar variant salt: `static_cast<uint8_t>(progressionId)`.
- `ProgressionId` is append-only; `Auto == 0` and `Count` is last.
- `kMaxHarmonicEvents == 8`.
- `ChordProgressionPlan` is trivially copyable and `<= 32 B`.
- `ChordProgressionRequest` is trivially copyable.
- No heap allocation, VLA, floating point, Scene/Song/PhraseCore reference or scale interval table in the module.

## ABI budget

`GenerationCompositionResult`:

- before Stage 15: `24 B` on the repository host C++17 ABI;
- Stage 15: `26 B` (pinned by a compile-time assertion in the Stage 15 host test);
- remaining budget under the existing `<= 32 B` production ceiling: `6 B`.

The ceiling is not raised.

## Host build and test

No hardware is required for host acceptance.

```bash
bash tests/run_generation_stage15_tests.sh
bash tests/run_generation_stage13_tests.sh
bash tests/run_host_tests.sh
```

Stage 15 is chained into the existing Stage 13/14 generation runner so the normal Core regressions workflow exercises it before the aggregate core host runner.

The Stage 15 runner executes:

- source ownership regressions;
- exact editorial grammar-catalog checks;
- full selected-progression reachability over all exact profiles;
- GCC C++17 build/run;
- Clang C++17 build/run when available;
- ASan + UBSan build/run;
- `-Wvla -Werror`;
- `-fstack-usage` with `realizeChordProgression()` ceiling `192 B`.

## Assertion mutation ledger

Each assertion is non-tautological; the listed production mutation must make it fail.

| Assertion | Production mutation that must fail it |
| --- | --- |
| append-only enum values | renumber or insert before an existing `ProgressionId` |
| plan/result ABI size | grow `ChordProgressionPlan` or `GenerationCompositionResult` beyond the pinned layout/budget |
| profile-table reachability | remove/zero the last profile path for a concrete progression |
| selected production reachability | alter selector/pool data so a concrete ID is never actually selected across the enumerated profile/ordinal matrix |
| three-genre data reuse | remove the demonstrated primitive from any required third distinct genre |
| exact editorial grammar | change a valid-but-wrong degree/quality/offset, e.g. `PopCycle` V to iii |
| determinism | introduce mutable/random state or change same-input seed/salt semantics |
| degree bound | emit degree `7+` |
| root-offset bound | emit offset outside `[-2,+2]` |
| root-offset allowlist | emit non-zero offset from any ID except `ParallelShift` / `BorrowedLift` |
| event capacity | accept/emit more than eight harmonic events |
| prefix/cycle semantics | choose a different grammar for the same request when only requested event count changes, or stop deterministic cycling |
| invalid request handling | accept invalid enum/family, event count `>8`, or unsupported phrase-bar count |
| profile-only genre ownership | add `GenerativeMode` knowledge to `roles/chord_progression.*` |
| no tonal ownership | add `ScaleType`, interval table, MIDI note array or Tonal Projector include to the Stage 15 axis |
| existing RNG domain | replace `GenerationDomain::ChordPitch` or add a new RNG domain |
| profile selector salt orthogonality | replace the stable `ProgressionId::Auto` salt with a chord-dependent or profile-axis-dependent salt |
| event-count ownership | stop deriving `harmonicEventCount` from `chord.plan.onsets` in the bridge |
| bridge ordering | move `realizeChordProgression()` after chord pitch materialization |
| one-bar hardware guard | change either production `phraseBars = 1` guard |
| no dynamic allocation | add `new`, `malloc`, `std::vector` or `std::string` to the module |
| integer-only axis | add `float` or `double` to the module |
| stack ceiling | increase `realizeChordProgression()` measured stack above `192 B` |

## Hardware

Hardware for the later audible acceptance:

- M5Stack Cardputer ADV;
- existing internal audio path or the user's normal SEQTRAK/MIDI monitoring path;
- no new external wiring required by Stage 15.

Wiring: unchanged from the existing GroovePuter setup. Stage 15 introduces no I2C/SPI/GPIO dependency.

### Build / flash for later hardware audition

```bash
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
```

Flash using the repository's normal Cardputer ADV procedure after host CI is green and after the Tonal Projector integration branch is available.

## Expected behavior

Host:

- every exact profile is valid;
- every concrete progression is selected somewhere in the full profile reachability sweep;
- repeated identical requests produce byte-identical plans;
- all events satisfy degree/quality/offset/capacity bounds;
- exact editorial grammar variants remain pinned;
- Stage 5, Stage 7C, Stage 13/14/15, core host, SDL and Cardputer builds remain green.

Hardware after tonal integration:

- harmonic rhythm timing is unchanged;
- only the harmonic root identity changes according to `HarmonicEvent.degree`;
- `quality` and `rootOffsetSemitones` remain semantic plan data until a later explicitly-scoped renderer consumes them;
- no voicing/SATB/multichannel chord implementation appears implicitly.

## Troubleshooting

- Linker error for `isValidProgressionId` or `realizeChordProgression`: a freestanding/build source list is missing `src/generation/roles/chord_progression.cpp`.
- `InvalidRequest` in the production bridge: first check chord onset count (`<= 8`), `ProgressionId`, and the preserved one-bar guard.
- A progression never appears in reachability: inspect profile candidate data and `ChordPitch` selector salt; do not add a genre switch to `chord_progression.cpp`.
- Audible notes do not follow scale degrees on this branch: expected until Tonal Projector integration; do not add local interval tables as a workaround.

## Acceptance checklist

- [ ] PR diff contains only Stage 15 generation/tests/docs/build-list changes.
- [ ] `ProgressionId` append-only contract is pinned.
- [ ] no scale/MIDI interval table exists in `roles/chord_progression.*`.
- [ ] no `GenerativeMode` switch exists in `roles/chord_progression.*`.
- [ ] every exact profile has a non-empty valid progression pool.
- [ ] every concrete progression is actually selected in the reachability matrix.
- [ ] every demonstrated primitive is reused across three distinct genres using only profile data.
- [ ] exact editorial grammar variants are mutation-pinned.
- [ ] profile progression selection uses stable `ProgressionId::Auto` salt and is not coupled to `ChordRhythm` salt.
- [ ] same input produces byte-identical plans.
- [ ] degree, quality, root-offset and capacity bounds pass for every concrete ID.
- [ ] static progressions return `ValidButStatic` for non-empty requests.
- [ ] one-bar production guard is unchanged.
- [ ] Stage 5 and Stage 7C freestanding migration harnesses link Stage 15.
- [ ] Stage 13/14/15 GCC + Clang + sanitizers pass.
- [ ] stack usage is measured and `<= 192 B`.
- [ ] `GenerationCompositionResult == 26 B`, leaving `6 B` under the existing ceiling.
- [ ] `bash tests/run_host_tests.sh` is green on the frozen SHA.
- [ ] SDL build is green on the frozen SHA.
- [ ] Cardputer ADV normal + fixed-DRAM build is green on the frozen SHA.
- [ ] Cardputer ADV SEQTRAK MIDI-only build is green on the frozen SHA.
- [ ] audible scale-degree projection remains explicitly `HARDWARE_PENDING` until Tonal Projector integration.
