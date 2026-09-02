# GF2-I1 — Tempo / Generation Corridor Arbitration

## Exact base

```text
base commit  4cb573892c6c7b023866bf564e8f9669a8413165
branch       agent/20260903-01-0.9.10-gf2-i1-tempo-corridor-arbitration
RED commit   fde3a1fc  test(gf2-i1): define single-owner tempo arbitration contract
GREEN commit 04243dbe  fix(gf2-i1): preserve resolved corridor tempo across atlas generation
```

## Problem

Tempo had two runtime owners.

`GenrePage::applyCurrent()` resolves `MATERIALIZE+BPM` once, from the generation
corridor of the requested profile, and hands the resolved value to the
generation request. Atlas-backed material then overwrote it with the source BPM
of the compiled corpus pattern.

## Canonical counterexample

```text
genre / recipe   Reggae / 11 "Minimal Space"
corridor         72 .. 102
suggested        86
Atlas source     116          (REC_DUB_MINIMAL_SPACE)
```

The Atlas value is correct source evidence: the reviewed corpus pattern really
is a 116 BPM pattern. It was never wrong. What was wrong is that production used
it as an unarbitrated tempo writer, so a user asking for 86 heard 116.

The disagreement between the two numbers is the regression fixture and is
preserved. No Atlas file and no corridor value was edited.

## Owner decision

| object | role |
|---|---|
| `GenerationCorridor` (`bpmMin`, `bpmMax`, `suggestedBpm`) | DECLARED genre/recipe tempo policy |
| `GenrePage::applyCurrent()` | RESOLVED — one musical decision, one tempo |
| `PendingGeneration::bpm` | PROPAGATED — transports the resolved tempo, never re-resolves it |
| `SceneManager::setBpm()` | COMMITTED persistent truth |
| `MiniAcid::setBpm()` | EXECUTED transport tempo |
| `AtlasRuntimeMetadata::bpm` | PROVENANCE ONLY |

`AtlasRuntimeMetadata::swingPercent` is unchanged: reviewed Atlas swing remains
production material and still reaches both the Scene and the candidate.

## Call-site audit

Every `setBpm` call site was classified. Two writes were duplicate owners.

### Old propagation graph

```text
GenrePage::applyCurrent()
  requestedBpm = profile.corridor.suggestedBpm            RESOLVED   86
    │
    └── regenerateWithQuantizedCommit()            [undo-owner impl = production]
          │
          └── preparePlayingCandidate()
                candidate.bpm = requestedBpm               PROPAGATED 86
                AtlasRuntime::applyRecipe(...)
                candidate.bpm = atlasMetadata.bpm          OVERWRITE  116   ← duplicate owner
                  │
                  ├── STOPPED  commitPreparedGeneration()  →  86 lost, 116 committed
                  └── PLAYING  BAR_START activation        →  86 lost, 116 activated

MiniAcid::regeneratePatternsWithGenre()
  setBpm(atlasMetadata.bpm)                                OVERWRITE  116   ← duplicate owner
```

### New propagation graph

```text
GenrePage::applyCurrent()
  requestedBpm = profile.corridor.suggestedBpm            RESOLVED   86
    │
    └── regenerateWithQuantizedCommit()
          │
          └── preparePlayingCandidate()
                candidate.bpm = requestedBpm               PROPAGATED 86
                AtlasRuntime::applyRecipe(...)
                candidate.swingPct = atlasMetadata.swing   MATERIAL
                atlasMetadata.bpm                          PROVENANCE (logged, not applied)
                  │
                  ├── STOPPED  commitPreparedGeneration()  →  86 committed and executed
                  └── PLAYING  BAR_START activation        →  86 activated on the bar line

MiniAcid::regeneratePatternsWithGenre()
  material + swing only; no tempo write
```

### Corrections to the pre-I1 trace

Three findings differ from the working hypothesis and are recorded because they
change where the fix belongs:

1. `quantized_generation_commit.h` renames the older implementation to
   `legacy*`. The live entry point is `regenerateWithQuantizedCommit()` in
   `quantized_generation_undo_owner_impl.h`. It has no separate immediate
   `regeneratePatternsWithGenre()` branch: **the stopped and playing paths share
   `preparePlayingCandidate()`**, so one write explained both observed failures.

2. `MiniAcid::regeneratePatternsWithGenre()` is no longer on the GENRE path.
   Its callers are:

   | caller | status |
   |---|---|
   | `miniacid_engine.cpp` BAR_START `commitPendingRecipe()` | dead — the hook always returns `false` |
   | `quantized_generation_commit_impl.h` legacy path | reference implementation, not called through the public header |
   | `strong_rhythm_live_bridge.cpp` `regenerateWithStrongRhythmMigration()` | no production caller |
   | `strong_rhythm_live_bridge.cpp` PHRASE audition loop | **live** (`drum_sequencer_page.cpp`) |

3. The engine-side `setBpm(atlasMetadata.bpm)` was therefore an undeclared
   transport write inside **PHRASE audition**: with `MATERIALIZE+BPM` selected,
   auditioning a phrase re-tempoed the transport from Atlas metadata on every
   bar. Same bug class, different consumer. Removing it is part of I1 by
   explicit user decision.

## Atlas provenance vs corridor policy

All six Atlas-backed recipes, before and after:

| recipe | corridor | suggested | Atlas source | old committed BPM | new committed BPM |
|---|---|---|---|---|---|
| 6 Chicago Jack | 118–132 | 124 | 124 | 124 | 124 |
| 7 Rolling Acid | 126–145 | 136 | 128 | 128 | 136 |
| 8 Classic 2-Step | 126–136 | 132 | 134 | 134 | 132 |
| 9 Dark Skippy | 128–140 | 134 | 136 | 136 | 134 |
| 10 Deep Chord | 108–124 | 116 | 120 | 120 | 116 |
| 11 **Minimal Space** | **72–102** | **86** | **116** | **116** | **86** |

Minimal Space is the only recipe whose Atlas source BPM lies outside its
corridor. An Atlas source value outside the corridor is legitimate and must stay
visible to audits; it is simply not a production tempo override.

## Tests

`tests/test_gf2_i1_tempo_corridor_arbitration.cpp` is a behavior test. It drives
the real production entry points against the **real** Atlas corpus
(`src/dsp/atlas_runtime.cpp`) and the **real** profile table
(`src/generation/composition/generation_profile.cpp`); only MiniAcid,
SceneManager and the mode manager are stubbed.

Runner: `tests/run_gf2_i1_tests.sh`, wired into the GF2 HOST gate in
`scripts/validate_gf2_targets.sh`.

### RED evidence

```text
$ bash tests/run_gf2_i1_tests.sh        # at 4cb57389 + tests only
STOPPED: engine tempo is the resolved corridor tempo     FAIL expected 86.0, got 116.0
STOPPED: persistent Scene tempo matches the request      FAIL expected 86.0, got 116.0
PLAY: prepared candidate carries the resolved tempo      FAIL expected 86.0, got 116.0
PLAY: activated tempo is the resolved corridor tempo     FAIL expected 86.0, got 116.0
recipe 7: committed BPM 128.0, expected corridor 136.0
recipe 8: committed BPM 134.0, expected corridor 132.0
recipe 9: committed BPM 136.0, expected corridor 134.0
recipe 10: committed BPM 120.0, expected corridor 116.0
recipe 11: committed BPM 116.0, expected corridor 86.0
GF2-I1 tempo arbitration: 9 failure(s)
```

`tests/test_source_regressions.py` at the same tree:

```text
AssertionError: Atlas materialization must not write the production tempo
AssertionError: Atlas metadata must not overwrite the resolved candidate tempo
```

### GREEN evidence

```text
$ bash tests/run_gf2_i1_tests.sh
STOPPED: request resolves to corridor tempo              OK   (86.0)
STOPPED: engine tempo is the resolved corridor tempo     OK   (86.0)
STOPPED: persistent Scene tempo matches the request      OK   (86.0)
STOPPED: Atlas source metadata stays 116                 OK
PLAY: live tempo is untouched before BAR_START           OK   (120.0)
PLAY: prepared candidate carries the resolved tempo      OK   (86.0)
PLAY: BAR_START activates the candidate                  OK
PLAY: activated tempo is the resolved corridor tempo     OK   (86.0)
PLAY: Atlas source metadata stays 116                    OK
every production corridor is well formed                 OK
  corridors audited: 33
Atlas provenance and corridor policy stay distinct       OK
every Atlas recipe commits its corridor tempo            OK
GF2-I1 tempo corridor arbitration: PASS
source regressions: OK
```

### Negative contracts

| contract | proof |
|---|---|
| PROFILE ONLY does not generate or re-tempo | source contract on `GenrePage::applyCurrent()`: both generation entries stay behind `doRegenerate`, and Genre Apply contains no `setBpm` |
| MATERIALIZE without BPM keeps the manual tempo | behavior test, stopped and playing |
| failed generation does not mutate tempo | behavior test (forced migration failure) |
| Busy does not mutate tempo | behavior test (both quantized slots held) |
| TargetChanged does not mutate tempo | behavior test (target moved during migration) |
| synth-only reroll keeps the live tempo | behavior test |
| AttemptUnavailable does not mutate tempo | unreachable with a valid genre/level tuple; the return precedes every tempo write by construction |
| Atlas metadata preserved | behavior test re-reads `AtlasRuntimeMetadata::bpm` after each commit |

### Corridor validity contract

Every production generation profile is enumerated through
`availableRecipeCount()` / `availableRecipeAt()` and audited for
`bpmMin > 0`, `bpmMax >= bpmMin`, `bpmMin <= suggestedBpm <= bpmMax`, plus
`isValidGenerationProfile()`. 33 corridors audited. Atlas source BPM is
deliberately **not** required to lie inside the corridor.

## Semantic delta

```text
production semantic delta   YES — tempo arbitration only
timbral delta               NONE
FEEL semantic delta         NONE
PHRASE semantic delta       NONE for phrase material and phrase law;
                            PHRASE audition no longer re-tempos the transport
                            from Atlas metadata (tempo only)
Atlas generated file delta  NONE
corridor value delta        NONE
```

## Musical reading

```text
MATERIALIZE+BPM   make this genre/recipe and adopt its tempo
MATERIALIZE       make this genre/recipe but keep my tempo
PROFILE ONLY      select the language without rewriting material or tempo
```

The rejected model — "choose 86, generate, then let Atlas decide 116" — is gone.
Atlas 116 remains true evidence about the source pattern.

## Target matrix

```text
$ bash scripts/validate_gf2_targets.sh --all
GF2 COMMIT            04243dbe14562e64c034793791de4a2959f687f0
HOST                   PASS      run_host_tests + 0.9.9-C/I0R + C2-V0R + I1
SDL                    PASS
CARDPUTER_ADV          PASS      flash 1303926 B (41%)
FIXED_DRAM             PASS      DRAM globals 186904 B (budget 191488)
SEQTRAK_MIDI_ONLY      PASS
HARDWARE               NOT TESTED
GF2 TARGET STATUS     GREEN
```

Additional directly affected suites, all PASS:
`tests/run_host_tests.sh`, `tests/run_generation_0_9_9_c_tests.sh`,
`tests/run_gf2_c2_v0r_tests.sh`, `tests/run_stage15_tonal_integration_tests.sh`,
`tests/run_gf2_i1_tests.sh`.

## Hardware listening check — RUN, PASS

Flashed and verified on the user's physical Cardputer ADV.

```text
flashed SHA   a6868297c6308b2dc2f379f5969d05aaf805be3a
              (GF2-I1 merged with origin/main @ 6694876e)
remote CI     41/41 checks pass on that exact SHA
image         1 314 192 B written, hash verified
DRAM globals  183 976 B (budget 191 488)
port          /dev/ttyACM0
```

The user executed the A/B checklist below on the device and reported it working.
Per-case BPM readings were not transcribed; the acceptance recorded here is the
user's direct observation, not a measured log.

Checklist executed:

| case | setup | expected |
|---|---|---|
| A | BPM 120, select Minimal Space, `MATERIALIZE+BPM`, PLAY | live BPM becomes 86; large audible tempo drop |
| B | BPM set manually, `MATERIALIZE` | BPM stays exactly the manual value; material changes |
| C | PLAY at 120, request `MATERIALIZE+BPM` | no mid-bar jump; tempo changes on the BAR_START activation |
| D | `PROFILE ONLY` | transport tempo untouched |
| E | Deep Chord, `MATERIALIZE+BPM` | 120 → 116; the smallest of the six shifts, checked for musical sanity |

The same checklist remains the reference for any future re-run; recording the
observed BPM values, not only the subjective impression, keeps a re-run
comparable to this one.

## Status

```text
GF2-I1   PASS
```

Closed on: RED/GREEN host evidence, negative contracts, corridor audit over 33
profiles, GF2 target matrix GREEN, remote CI 41/41, and hardware A/B confirmed
on the device. This does not close GF2-I2 FEEL, I3 PHRASE LAW, I4 corridor
consumers or I5 DEPTH.
