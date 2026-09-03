# GF2-I3 — Phrase Law Execution

## Exact base

```text
base commit    f3a288b9   (GF2 line head: E0 -> M0 -> I1 -> I2 -> tooling -> I2A)
branch         agent/20260903-05-0.9.10-gf2-i3-phrase-law-execution
RED commit     f2bc6b1d   test(gf2-i3): prove the phrase law never reaches the bars
GREEN commit   a015c193   feat(gf2-i3): execute the declared phrase law across the bars
```

## Problem, measured

`PhraseEvolutionLawId` had exactly two references in `src/`: its own definition
and one line in `phrase_length_request.cpp` that unpacks it from the profile
candidate. `phrase_execution.cpp`, `generated_phrase_p1r_materializer.h` and
`generated_phrase_song.h` mentioned neither a law nor phrase evolution.

The consequence, on Breaks / UK Garage, which declares DevelopReturn over four
bars and draws a phrase-enabled archetype:

```text
bar 1 vs bar 2: 0 differing onsets
bar 1 vs bar 3: 0 differing onsets
bar 1 vs bar 4: 0 differing onsets
```

Every bar of the phrase carried an identical drum pattern. The drum plan was
realized once per bar with `phraseBars = 1` and no trajectory, so only the
bass, chord and melodic roles varied by bar ordinal. This is the measured form
of "after half a minute you already know everything".

## The axis was not missing — it was already there

The design gate corrected the checkpoint's own premise. A `BarTrajectory` **is**
a bar-function programme:

```cpp
struct BarTrajectory { TrajectoryId id; uint8_t barCount; BarFunction bars[4]; };
```

and `BarFunction` already names Statement, Repeat, RepeatWithGhosts, Response,
Reduction, Build, Turnaround, Break and Return. The shipped catalogue contains
the shapes the laws describe:

| id | bars | programme | levels |
|---|---|---|---|
| 1 | 1 | Statement ×4 | all |
| 2 | 2 | Statement · Repeat | all |
| 3 | 2 | Statement · RepeatWithGhosts | P2/P3 |
| 5 | 4 | Statement · Response · Repeat · Return | all |
| 6 | 4 | Statement · Repeat · Reduction · Return | P2/P3 |
| 7 | 4 | Statement · Build · RepeatWithGhosts · Turnaround | P3 |
| 8 | 4 | Statement · RepeatWithGhosts · Break · Return | P3 |

So a law does not need a parallel axis. It needs to resolve to one of these.

## The mapping

```text
Loop           kNoTrajectoryId   the neutral: one statement, repeated
RepeatReply    5
DevelopReturn  P3Transformation ? 7 : 6
SparseDrift    P3Transformation ? 8 : 3
```

The stronger development shapes are P3-only in the shipped vocabulary, and the
default level is P2Variation. Mapping by level was the musician's decision: the
law works out of the box, and P3 gives its fuller form. No `kFeel*`-style
retuning happened here either — no trajectory, weight, level mask or
`stage12PhraseEnabledId()` entry was changed.

A law only takes effect where the archetype is admitted to phrase evolution and
the mapped trajectory is eligible at the current level. Anything else leaves
`phraseTrajectory` unset and the phrase keeps its established per-bar
realization.

## Ownership — the boundary held

The first implementation put the evolution inside `migrateStrongRhythmDrums`,
and a Stage 12 source guard rejected it:

```text
normal strong migration gained Stage 12 multi-bar ownership
```

That guard was right. The shared migration is a one-bar surface, and the
physical materializer enforces it too: `validPlanShape()` rejects any plan whose
`trajectoryId` is set or whose bar function is not `Statement`.

The implementation moved to the boundary that already existed:

```text
phrase_execution.cpp        resolves the law, realizes the whole programme once
  (the phrase owner)        at PREPARE, stores one RhythmPhrasePlan
        │
        │  StrongRhythmPhraseExecutionOverride::barPlan
        │  (the seam already used for harmonic rhythm and progression source)
        ▼
strong_rhythm_migration.cpp materializes the bar it is handed, keeps
  (the shared one-bar path)  request.phraseBars = 1, owns no multi-bar vocabulary
```

The evolved bar's function tag is normalized to `Statement` before
materialization, exactly as `materializeEvolvedDrumBar()` in the audition path
already does — BarEvolution has applied the function to the masks, so the tag
carries no further information the materializer needs.

The change is confined to callers that name a phrase bar. Ordinary one-bar GENRE
generation keeps its established realization byte for byte.

## GREEN evidence

Breaks / UK Garage, DevelopReturn at P2 → trajectory 6
(Statement · Repeat · Reduction · Return):

```text
bar 1 vs bar 2: 0 differing onsets              the repeat is the statement
bar 1 vs bar 3: 3 differing onsets (salient)    the reduction, and it moves the anchor
bar 1 vs bar 4: 0 differing onsets              the return is the statement
```

The numbers match the declared programme bar for bar. The salience flag is the
magnitude contract's anchor rule: a development bar that leaves the kick and the
backbeat untouched is not a development bar.

## Memory

```text
DRAM globals   183 976 bytes, unchanged   (budget 191 488)
per-bar heap   none
```

One `RhythmPhrasePlan` lives in the caller-owned `PreparedPhraseExecution`; each
bar is a pointer into it. Evolution runs once per phrase at PREPARE and never on
the audio thread.

## Target matrix

```text
$ bash scripts/validate_gf2_targets.sh --all
GF2 COMMIT            a015c1931341f938469792ce820455539e0a7c57
HOST / SDL / CARDPUTER_ADV / FIXED_DRAM / SEQTRAK_MIDI_ONLY   all PASS
GF2 TARGET STATUS     GREEN
```

## A finding outside this checkpoint

`tests/run_0_9_9_phrase_p1r_tests.sh` fails, and **it was already failing before
I3** — verified on the I2A head. Its source guard diffs `src/generation` against
a hard-coded 0.9.9-era SHA and asserts the changed-file set equals a frozen
owner list, so it goes red on any later work in that directory; GF2-I1 and I2
made it red and nobody ran it. Its workflow only triggers on the P1R branch and
on PRs targeting the H2R branch, so CI never ran it either.

This is a stale checkpoint-scoped guard, not a regression. It is deliberately
left untouched — editing a frozen guard to make a later run green would destroy
what it recorded. It belongs on the release-freeze disposition list, together
with the M0 lifecycle residual.

## Semantic delta

```text
phrase law execution      YES - the declared programme now plays
rhythm vocabulary         unchanged
trajectory weights/masks  unchanged
stage12PhraseEnabledId    unchanged
one-bar GENRE generation  unchanged, byte for byte
timing / FEEL             unchanged (GF2-I2)
tempo                     unchanged (GF2-I1)
```

## Hardware A/B — RUN, ACCEPTED

```text
flashed SHA   a81e3f7968bfb67ec6b41a2823aebd05100a22ff
remote CI     59/59 checks pass on that exact SHA
image         1 315 008 B written, hash verified
DRAM globals  183 976 B, unchanged (budget 191 488)
port          /dev/ttyACM0
```

Fixture: Breaks / UK Garage, DevelopReturn at P2, trajectory 6. The four bars
as materialized:

```text
bar 1   KICK   x..x......x...x.     SNARE  ....x.......x...   C-HAT  ..x...x...xx..x.
bar 2   identical to bar 1
bar 3   KICK   x..x............     SNARE  ....x.......x...   C-HAT  ..x...x...x...x.
bar 4   identical to bar 1
```

Bar 3 is the Reduction: two of the four kicks and one hat drop out, so the
second half of the bar loses its low end, and bar 4 restores it. At 132 BPM the
phrase breathes on a roughly seven-second cycle.

The user played it on the device and accepted the checkpoint. Per-case
observations were not transcribed; the acceptance recorded here is the user's
direct judgement, not a measured listening log.

Unlike GF2-I2A, whose acceptance failed on amplitude, this effect is structural
and at phrase scale — the difference is a missing kick, not a displaced one.

## Status

```text
GF2-I3   PASS
```

Closed on: RED/GREEN host evidence with the bar-by-bar numbers matching the
declared programme, the Stage 12 ownership boundary intact, unchanged DRAM and
byte-identical one-bar GENRE generation, GF2 target matrix GREEN, remote CI
59/59, and hardware accepted on the device.

## What I3 does not close

```text
GF2-I4   corridor field consumers
GF2-I5   secondary-role depth
GF2-C2-V1 / C2 Gate B / G1
```

Recorded for Gate B: multi-bar admissibility is a property of the **drawn**
archetype, not of the recipe. The same recipe draws a phrase-enabled archetype
at one generation ordinal and not at another, so any corpus statement about
phrase coverage has to be distributional, not a single sample.
