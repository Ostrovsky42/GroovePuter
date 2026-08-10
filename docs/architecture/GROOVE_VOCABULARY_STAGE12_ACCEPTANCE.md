# Generation Stage 12 — Multi-Bar Phrase Evolution Acceptance

## Purpose

Provide deterministic 1/2/4/8-bar phrase orchestration over the existing
`BarEvolution` owner without creating a second transform engine or a second
phrase-length owner.

## Architecture

- 1/2/4 bars use one `BarEvolution` call.
- 8 bars use two 4-bar calls and reuse the first `PhraseRhythmIdentity` for the
  second segment.
- Bass, Chord, Melodic and Motif identities are carried unchanged.
- The result is transient and contains no Scene, Song, page, bank or PhraseCore
  destination.
- `Scene::feel.patternBars` remains the user-visible 1/2/4/8 length owner.

## Acceptance

- 1/2/4/8 lengths are deterministic fixed-capacity values;
- P1/P2/P3 preserve the same rhythm identity;
- 8-bar output records two trajectory segments and variation history;
- failure exposes no partial bars or role identities;
- GCC, Clang, ASan/UBSan and `-Wvla -Werror` pass;
- no production caller exists yet.

## Blocking hardware gate

Production wiring is intentionally blocked by the normative Stage 6.1 gate.
Before `GENRE -> MATERIALIZE` or Phrase/Song code may call this module on
Cardputer ADV, one physical ESP32-S3 run must record task stack high-water,
largest internal heap block and worst-case 4-bar execution duration. Until then:

```text
HOST_CORE = PASS
PRODUCTION_REACHABILITY = BLOCKED_BY_STAGE_6_1_HARDWARE_GATE
HARDWARE_MUSICAL = HARDWARE_PENDING
```
