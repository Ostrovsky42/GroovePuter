# Stage 7A Atlas Pass 2 Curation Manifest

Status: temporary five-candidate listening/falsification set.

## Inputs

Runtime base:

```text
PR #185
agent/20260809-04-groove-vocabulary-stage6-1-hardening
ac911c74ded53d5f2fa6b7ad63c8c0f97fb9a395
```

Frozen evidence gate:

```text
PR #192
agent/20260809-09-atlas-pass2-computational-extraction
2f314cac6cc65f5664dc3254ece140bb68fb5390
Atlas v2.6 SHA-256:
5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd
```

Stage 7A intentionally uses #185 as its Git base and pins #192 as an evidence dependency. Offline Atlas tooling is not copied into the firmware audition branch.

## Five candidates

| Slot | Audition grammar | Pass 2 aggregate | Evidence status | Evidence directions | Purpose |
|---|---|---|---|---|---|
| 1 | `staggered_machine` | `HARD_02` | multi-provenance review | Electro / Go-Go / Hip-Hop / Miami Bass | strongest machine/electro evidence candidate |
| 2 | `cross_cycle` | `HARD_05` | multi-provenance review, mapping-sensitive | Afro-Cuban / Bossa Nova | test whether the unusual cross-cycle remains musical after conservative cymbal mapping |
| 3 | `break_halfstep` | `HARD_04` | single-root challenger | Breaks / Drum & Bass / Funk-Soul | compact breakbeat boundary challenge |
| 4 | `rock_push` | `HARD_09` | single-root challenger | Rock | compact backbeat/push boundary challenge |
| 5 | `halfback_control` | `HARD_03` | single-root control | Basic / Breaks / EDM / Pop | positive control for collapse into an existing grammar |

The set is deliberately not five production candidates. It contains two evidence-backed review candidates, two boundary challengers and one control.

## Rights / reconstruction boundary

No Atlas source pattern, pattern ID, structural-group ID, source locator, artifact hash or literal source mask is copied into firmware.

The temporary grammars are curated abstractions:

```text
smaller identity core
+ wider preferred/optional legal corridor
+ bounded density
+ generic role relationship hypotheses
```

A common source skeleton is not copied wholesale into `canonicalAnchors`.

## Runtime boundary

Stage 7A is one-bar, drums-only and temporary.

Allowed roles:

```text
Kick
Backbeat
ClosedHat
OpenHat
Percussion
```

Explicitly absent:

```text
BassRhythm
BassPitch
ChordRhythm
MelodicRhythm
Motif
Mood
new Feel law
BarEvolution production wiring
Genre production routing
ReferenceVocabulary production admission
Scene persistence of audition candidate IDs
```

Synth A/B patterns are cleared only while audition is active and restored exactly on exit.

## Listening decisions

Each slot must end in one of:

```text
ACCEPT_FOR_STAGE7_CURATION
MERGE_WITH_EXISTING
TUNE_EXISTING
REVISE_AND_REAUDITION
REJECT
```

`ACCEPT_FOR_STAGE7_CURATION` still does not mean immediate production routing. A surviving grammar must also pass generated-runtime effective-variation/separation tests before entering `ReferenceVocabulary`.
