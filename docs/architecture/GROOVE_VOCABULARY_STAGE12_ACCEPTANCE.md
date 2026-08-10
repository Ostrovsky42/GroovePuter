# Generation Stage 12 — Multi-Bar Phrase Evolution Acceptance

Status: `API-ONLY` — 1/2/4/8 bars are fixture-tested, but the shipped
`ReferenceVocabulary` reaches only one bar.

## Purpose

Provide a deterministic fixed-capacity 1/2/4/8-bar orchestration API over the
existing `BarEvolution` owner without creating a second transform engine or a
second phrase-length owner.

## Architecture

- 1/2/4 bars use one `BarEvolution` call.
- 8 bars use two 4-bar calls and reuse the first `PhraseRhythmIdentity` for the
  second segment.
- Bass, Chord, Melodic and Motif identities are carried unchanged.
- The result is transient and contains no Scene, Song, page, bank or PhraseCore
  destination.
- `Scene::feel.patternBars` remains the user-visible 1/2/4/8 length owner.

## Acceptance

- 1/2/4/8 lengths are deterministic fixed-capacity values in the dedicated
  Stage 12 fixture catalog;
- P1/P2/P3 preserve the same rhythm identity;
- 8-bar output records two trajectory segments and variation history;
- failure exposes no partial bars or role identities;
- GCC, Clang, ASan/UBSan and `-Wvla -Werror` pass;
- no production caller exists yet.

## Shipped vocabulary reachability

The production `ReferenceVocabulary` was not expanded by Stage 12:

```text
catalog trajectories       = 1
trajectory barCount        = 1 (Statement)
allowedPhraseBars          = phraseBarsBit(1) for every archetype
maxDrops                   = 0 at P1/P2/P3
allowedIntents             = 0 at P1/P2/P3
```

Consequently `evolveMultiBarPhrase` succeeds for one bar and fails through the
core for 2/4/8 bars when called with shipped catalog data. The host matrix now
pins that limitation explicitly so a future catalog expansion must also update
this status and its acceptance evidence.

## Blocking hardware gate

Production wiring is intentionally blocked by the normative Stage 6.1 gate.
Before `GENRE -> MATERIALIZE` or Phrase/Song code may call this module on
Cardputer ADV, one physical ESP32-S3 run must record task stack high-water,
largest internal heap block and worst-case 4-bar execution duration. Until then:

```text
HOST_CORE = PASS
SHIPPED_VOCABULARY_1_BAR = REACHABLE
SHIPPED_VOCABULARY_2_4_8_BAR = UNREACHABLE
PRODUCTION_REACHABILITY = BLOCKED_BY_STAGE_6_1_HARDWARE_GATE
HARDWARE_MUSICAL = HARDWARE_PENDING
```

Stage 12 may be reported only as `API-ONLY` until both conditions are met:

1. `ReferenceVocabulary` contains validated 1/2/4-bar trajectories,
   `allowedPhraseBars` coverage and non-zero bounded mutation intent where
   musically required;
2. the Stage 6.1 physical ESP32-S3 task high-water/heap/runtime gate permits the
   first production caller.
