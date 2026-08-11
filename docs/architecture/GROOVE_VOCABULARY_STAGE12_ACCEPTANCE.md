# Generation Stage 12 — Multi-Bar Phrase Evolution Acceptance

Status: `AUDITION-REACHABLE / NORMAL-G-ONE-BAR`

Stage 12 remains a deterministic fixed-capacity 1/2/4/8-bar orchestration API
over `BarEvolution`, but its release reachability changed after the original
API-only checkpoint: the explicit Cardputer ADV `Ctrl+Alt+G` audition/probe path
has now passed its hardware gate and may use the wider phrase-evolution catalog.
Normal production `G` remains one-bar.

## Purpose

Provide deterministic 1/2/4/8-bar phrase orchestration without creating a
second transform engine, a second phrase-length owner, or implicit Song/Scene
ownership.

## Architecture

- 1/2/4 bars use one `BarEvolution` call.
- 8 bars use two 4-bar calls and reuse the first `PhraseRhythmIdentity` for the
  second segment.
- Bass, Chord, Melodic and Motif identities are carried unchanged.
- `PhraseEvolutionResult` is transient and owns no Scene, Song, page, bank or
  PhraseCore destination.
- `Scene::feel.patternBars` remains the user-visible 1/2/4/8 length owner.
- normal `strong_rhythm_migration.cpp` remains pinned to `request.phraseBars = 1`.
- the only release-linked multi-bar owner is the explicit
  `regeneratePhraseAuditionWithProbe()` path in `strong_rhythm_live_bridge.cpp`.

## Acceptance

- 1/2/4/8 lengths are deterministic fixed-capacity values in the dedicated
  Stage 12 candidate catalog;
- P1/P2/P3 preserve the selected rhythm identity;
- 8-bar output records two trajectory segments and variation history;
- failure exposes no partial bars or role identities;
- GCC, Clang, ASan/UBSan and `-Wvla -Werror` pass;
- normal full/drums production `G` never calls `evolveMultiBarPhrase()`;
- explicit `Ctrl+Alt+G` may call `evolveMultiBarPhrase()` after locking one
  admitted rhythm identity;
- one-bar-only identities use the deterministic variation fallback rather than
  being mislabeled as structured evolution.

## Normal production vocabulary

The normal `ReferenceVocabulary` remains one-bar:

```text
catalog trajectories       = 1
trajectory barCount        = 1 (Statement)
allowedPhraseBars          = phraseBarsBit(1) for every archetype
```

This is intentional. Stage 12 did not silently widen the ordinary GENRE/DRUMS
`G` materialization path.

## Audition candidate vocabulary

`ReferenceVocabulary::phraseEvolutionCatalog()` is a separate bounded candidate
surface used by the explicit audition/probe owner. It contains only identities
that have admitted multi-bar capability; unsupported identities remain on the
strong one-bar variation fallback.

The source gates must enforce the distinction:

```text
strong_rhythm_migration.cpp
    phraseBars = 1
    no phraseEvolutionCatalog
    no evolveMultiBarPhrase

strong_rhythm_live_bridge.cpp
    runSubtractiveRuntimeProbe              may use candidate catalog/evolution
    regeneratePhraseAuditionWithProbe       may use candidate catalog/evolution
    normal regenerateWithStrongRhythmMigration       no multi-bar evolution
    normal regenerateDrumsWithStrongRhythmMigration  no multi-bar evolution
```

## Hardware gate result

The original Stage 6.1 requirement was not removed; it was satisfied by the
explicit Cardputer ADV audition/probe work before release routing was accepted.
The probe records:

- task stack high-water;
- internal free heap;
- largest internal heap block;
- command duration;
- worst 4-bar Reduction duration;
- worst 4-bar Break duration.

The hardware-tested P-level source head carried the resulting audition path:

```text
6800e5a2c8f22a641feb816bdb26e70e892647f5
```

That production tree was squash-transferred into the #226 routing branch as:

```text
68e0cccb790cf500ceca1ab1c14b56de7994c9ff
```

Subsequent #226 corrections to this acceptance boundary are tests/docs only and
do not alter that hardware-tested production implementation.

Current gate state:

```text
HOST_STAGE12 = PASS_REQUIRED
NORMAL_PRODUCTION_G = ONE_BAR
AUDITION_PROBE_REACHABILITY = HARDWARE_ACCEPTED
AUDITION_LENGTHS = 1_2_4_8
P_LEVEL_REQUEST = P1_P2_P3
FINAL_0_9_1_RC_HARDWARE = REQUIRED_AFTER_STACK_MERGE
```

## Release boundary

Stage 12 multi-bar evolution is not a general automatic Song/Phrase engine in
0.9.1. The accepted release surface is deliberately narrow:

```text
DRUMS Ctrl+Alt+G -> explicit audition/probe -> Bank B + Song B
```

Normal GENRE `G` and DRUMS `G` remain the ordinary one-bar production commands.
Future automatic phrase evolution, Song arrangement ownership, or broader
multi-bar generation belongs after the 0.9.1 release boundary.
