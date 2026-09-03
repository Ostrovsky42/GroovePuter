# GF2-I2 — Profile FEEL Contract

## Exact base

```text
base commit    8c9ad2574e18f77d26cd606c95d85eccddc4100b   (GF2-I1 closed)
branch         agent/20260903-02-0.9.10-gf2-i2-profile-feel-contract
RED commit     9534bee1  test(gf2-i2): define profile feel ownership contract
GREEN commit   fcc60963  feat(gf2-i2): resolve auto feel through generation profile
swing commit   3b86643d  fix(gf2-i2): keep atlas swing as provenance
```

## Problem

The generation profile already selects a FEEL prior — profiles carry weighted
FEEL vocabularies and `resolveGenerationComposition()` picks one into
`GenerationCompositionResult::suggestedFeel`. But production materialization
read `scene.feel.timingProfile`, and `suggestedFeel` was only copied into result
metadata. The profile could claim "this vocabulary prefers LAID BACK" forever
while production stayed STRAIGHT.

Measured on the exact base, Reggae / recipe 11 "Minimal Space", FEEL amount 100:

```text
scene profile=STRAIGHT      suggestedFeel=LAID BACK   displaced events=0
scene profile=SWING COMPAT  suggestedFeel=LAID BACK   displaced events=4
scene profile=LAID BACK     suggestedFeel=LAID BACK   displaced events=12
scene profile=PUSH/PULL     suggestedFeel=LAID BACK   displaced events=11
```

The FEEL mechanism worked. The profile prior simply never reached it.

Minimal Space uses `kFeelDubPocket`, whose candidates are SwingCompatible /
LaidBack / PushPull — **no Straight candidate**. AUTO can therefore never
resolve to Straight there, which makes it the honest A/B fixture.

## Owner graph — BEFORE

```text
GenerationProfile FEEL weights
        ↓
resolveGenerationComposition()
        ↓
composition.suggestedFeel ──────────► result metadata only (inert)

scene.feel.timingProfile ───────────► context.feelProfile ──► every role's
                                                              event timing

scene.generatorParams.microTimingAmount ──► context.feelAmount (intensity)

scene.feel.swingPct ──► live offbeat swing
        ▲
        └── atlasMetadata.swingPercent overwrote it on Atlas-backed recipes
```

## Owner graph — AFTER

```text
                    GenerationProfile FEEL weights
                                 ↓
                    composition.suggestedFeel
                                 ↓
Scene PROFILE ────────► resolveFeelProfile(requested, suggested)
 (concrete or AUTO)              ↓
                    frozenSelection.resolvedFeel   ← arbitrated once
                                 ↓
                    drums · bass · chord · melodic
                                 ↓
                    SynthStep.timing / DrumStep.timing

scene.generatorParams.microTimingAmount ──► profile intensity (musician)
scene.feel.swingPct ─────────────────────► live offbeat swing (musician)
atlasMetadata.bpm / swingPercent ────────► provenance only
```

## FeelProfileId — persisted IDs

| profile | id | kind |
|---|---|---|
| Straight | 0 | concrete timing character |
| SwingCompatible | 1 | concrete timing character |
| LaidBack | 2 | concrete timing character |
| PushPullControlled | 3 | concrete timing character |
| **Auto** | **4** | **selection mode (appended)** |
| Count | 5 | — |

Append-only. IDs 0–3 keep their meaning, `FeelSettings` still defaults to
Straight, a document with no `profile` field still decodes as Straight, and an
out-of-range value still falls back to Straight. AUTO is opt-in and never
appears in a Scene the musician did not put it there.

## AUTO semantics

AUTO is a **selection mode**, not a fifth timing curve. It has no coefficients
of its own:

```text
requestedFeel == Auto  ->  resolvedFeel = composition.suggestedFeel
requestedFeel concrete ->  resolvedFeel = requestedFeel
```

`resolvedFeel` is generation-derived state. It is **never** written back into
`scene.feel.timingProfile`: AUTO is the musician's persistent decision, and the
concrete profile it resolved to this time is not.

`isValidFeelProfile()` now means *concrete*, so an unresolved AUTO reaching
`interpretFeelPhrase()` returns `InvalidProfile` — a hard resolution failure,
never a silent Straight. The same predicate guards the profile tables, so a
genre vocabulary cannot offer AUTO as a weighted candidate.
`isSelectableFeelProfile()` covers what a Scene may hold and the FEEL page may
select.

## Resolution happens once

Arbitration lives in the two frozen-selection resolvers
(`resolveStrongRhythmFrozenSelection` and `…ForPhraseBars`), where the
composition identity is already known and before any materialization. The
frozen selection carries `resolvedFeel`, so all roles of one musical decision —
and all bars of one phrase — hear the same FEEL. `migrateStrongRhythmMaterial`
no longer reads `context.feelProfile` at all.

`StrongRhythmMigrationResult` reports both values, so *suggested* and *resolved*
stay distinguishable:

```text
AUTO:            suggested = LAID BACK   resolved = LAID BACK
manual STRAIGHT: suggested = LAID BACK   resolved = STRAIGHT
```

## Reachability — the central I2 proof

Same fixture, same generation identity, FEEL amount 100, after I2:

```text
scene profile=STRAIGHT      suggested=LAID BACK  resolved=STRAIGHT      displaced=0
scene profile=SWING COMPAT  suggested=LAID BACK  resolved=SWING COMPAT  displaced=4
scene profile=LAID BACK     suggested=LAID BACK  resolved=LAID BACK     displaced=12
scene profile=PUSH/PULL     suggested=LAID BACK  resolved=PUSH/PULL     displaced=11
scene profile=AUTO          suggested=LAID BACK  resolved=LAID BACK     displaced=12
```

```text
DECLARED                 YES   profile FEEL weights
RESOLVED                 YES   composition.suggestedFeel
SELECTED                 YES   AUTO / manual arbitration
PROPAGATED               YES   frozenSelection.resolvedFeel
CONSUMED                 YES   applyFeelToMaterializedPattern / interpretFeelPhrase
MATERIALIZATION-CAPABLE  YES   12 displaced events vs 0 for STRAIGHT
EXECUTION-CAPABLE        YES   SynthStep.timing / DrumStep.timing
```

## Atlas swing audit

GF2-I1 established Atlas BPM as provenance. The same audit for
`atlasMetadata.swingPercent` found a live duplicate owner on both production
paths. RED, with the musician's swing at 50%:

```text
STOPPED  Minimal Space (source 51%)   50% -> 51%
PLAY     Dark Skippy   (source 68%)   50% -> 68%
STOPPED  Minimal Space, user at 62%   62% -> 51%
```

Both writes removed:

```text
preparePlayingCandidate()      candidate.swingPct = atlasMetadata.swingPercent
regeneratePatternsWithGenre()  scene.feel.swingPct = atlasMetadata.swingPercent
```

`AtlasRuntimeMetadata` keeps its `bpm` and `swingPercent` intact for
diagnostics, corpus provenance and editorial review. `FeelSettings::swingPct`
is now the single runtime swing owner, independent of the FEEL profile as the
interpreter's `SwingCompatible` design intends.

## What stayed where it was

| owner | unchanged |
|---|---|
| FEEL AMOUNT | `scene.generatorParams.microTimingAmount` — amount 0 is byte-identical to Straight |
| SWING OFFBEAT | `scene.feel.swingPct` — separate musical decision |
| VELOCITY VAR | `scene.generatorParams.velocityRange` — outside the I2 contract |
| profile weights | no `kFeel*` table retuned |
| rhythm topology | identical onsets under every profile |
| tempo | GF2-I1 arbitration untouched |
| FEEL page | no redesign; PROFILE already cycles to `Count`, so AUTO appears; TIGHT/HUMAN/LOOSE presets still select concrete profiles |

## Tests

`tests/run_gf2_i2_tests.sh` (wired into the GF2 HOST gate) runs three binaries
plus the source contracts:

- `test_gf2_i2_profile_feel_contract.cpp` — real profile table and real
  migration pipeline; append-only IDs, AUTO as selection mode, concrete-only
  genre vocabularies, AUTO resolution, manual override, AUTO rejected by the
  interpreter, same-topology/different-timing, amount-0 neutrality, one
  resolution shared by every role.
- `test_gf2_i2_atlas_swing_ownership.cpp` — real Atlas corpus through the real
  production quantized path, stopped and PLAY/BAR_START.
- `test_gf2_i2_feel_persistence.cpp` — Scene round-trips for every selectable
  profile, legacy documents, missing field, out-of-range fallback.

### Same topology / different timing

The central musical regression. Same Genre, recipe, rhythm identity, seed,
level, pattern address and attempt; only the FEEL selection differs:

```text
A/B: same rhythm identity                     OK
A/B: same onset topology                      OK
A/B: at least one event is placed differently OK
```

Structure is compared through an onset signature that deliberately ignores
every expressive timing value, so a passing A/B cannot be explained by a
different pattern being generated.

### One resolution for every role

Materializing with AUTO and materializing with the profile AUTO resolved to are
byte-identical. If any role re-resolved independently, they could not be.

### RED evidence

```text
$ bash tests/run_gf2_i2_tests.sh        # at 8c9ad257 + tests only
19 compile errors: FeelProfileId::Auto, isSelectableFeelProfile and
StrongRhythmMigrationResult::resolvedFeel do not exist.

$ build/host-tests/gf2-i2/atlas_swing_ownership
STOPPED: Atlas does not rewrite the user's swing            FAIL expected 50%, got 51%
PLAY: persistent swing after COMMIT is the user's swing     FAIL expected 50%, got 68%
PLAY: committed swing is still the user's swing             FAIL expected 50%, got 68%
CUSTOM SWING: the user's 62% survives Atlas materialization FAIL expected 62%, got 51%
```

### GREEN evidence

```text
$ bash tests/run_gf2_i2_tests.sh
GF2-I2 profile feel contract: PASS
GF2-I2 atlas swing ownership: PASS
GF2-I2 feel persistence: PASS
source regressions: OK
```

## Target matrix

```text
$ bash scripts/validate_gf2_targets.sh --all
GF2 COMMIT            3b86643d55ce90c8645c20caaaf33fe5b6aeb3bf
HOST                   PASS      run_host_tests + 0.9.9-C/I0R + C2-V0R + I1 + I2
SDL                    PASS
CARDPUTER_ADV          PASS      flash 1 314 250 B (41%)
FIXED_DRAM             PASS      DRAM globals 183 976 B (budget 191 488)
SEQTRAK_MIDI_ONLY      PASS
GF2 TARGET STATUS     GREEN
```

Additional directly affected suites, all PASS: `run_feel_stage8_tests.sh`,
`run_stage15_tonal_integration_tests.sh`, `run_generation_stage13_tests.sh`,
`run_0_9_9_phrase_pmb_p1_tests.sh`, `run_0_9_9_phw_p1_tests.sh`,
`run_phrase_stage12_tests.sh`, `run_0_9_9_e0a_tests.sh`,
`run_generation_0_9_9_c_tests.sh`, `run_gf2_c2_v0r_tests.sh`,
`run_gf2_i1_tests.sh`, `run_host_tests.sh`.

## Hardware A/B

Isolate timing, not timbre. Fix BPM for every case; do not judge by patch
quality.

```text
Genre / Recipe   Minimal Space (AUTO cannot resolve STRAIGHT here)
BPM              same value for every case
SWING            50 / neutral
FEEL AMOUNT      100 for the diagnostic A/B
VELOCITY VAR     unchanged
RHYTHM           fixed MANUAL archetype if practical
```

| case | change | expected |
|---|---|---|
| A | PROFILE = STRAIGHT | reference placement; listen to backbeat, hats, bass vs kick, chord, melody |
| B | PROFILE = AUTO only | resolves LAID BACK; events sit audibly behind/around the grid |
| C | PROFILE = STRAIGHT, then LAID BACK / PUSH/PULL | the manual choice wins regardless of the genre prior |
| D | PROFILE = AUTO, FEEL AMOUNT = 0 | no profile displacement at all |
| E | SWING 50 vs SWING > 50, PROFILE unchanged | profile character remains; offbeat swing moves independently; no Atlas swing reset |

Record observed values, not only the subjective impression.

## Semantic delta

```text
production semantic delta   YES — profile FEEL resolution and swing ownership
rhythm topology delta       NONE
tempo semantic delta        NONE (GF2-I1 arbitration untouched)
velocity semantic delta     NONE
profile weight delta        NONE
Atlas generated file delta  NONE
```

## What I2 does not close

```text
GF2-I3   phrase-law execution
GF2-I4   other corridor field consumers
GF2-I5   synth-role DEPTH
GF2-C2-V1
```
