# GF2-I2A — FEEL Amplitude

## Exact base

```text
base commit    b32da0c1   (GF2-I2 closed + magnitude contract)
branch         agent/20260903-04-0.9.10-gf2-i2a-feel-amplitude
tool commit    fdc89c78   tools(gf2-i2a): measure feel amplitude
RED commit     307ea5e7   test(gf2-i2a): pin the feel magnitude contract
GREEN commit   ade4a698   feat(gf2-i2a): raise the shipped feel amplitude
```

## Problem

GF2-I2 made the profile FEEL prior causal and was inaudible on hardware. This
checkpoint decides how loud the declared policy should be, and turns the answer
into an invariant rather than a preference.

It is an editorial decision, not a defect: nothing in the I2 chain is broken.

## Measurement first

`tools/gf2/gf2_i2a_feel_amplitude_dump.cpp` materializes every production
genre/recipe against every FEEL profile across an amount sweep and reports what
moved as well as how much. Artifact:
`docs/research/GF2_I2A_FEEL_AMPLITUDE_CENSUS.tsv` — 1815 rows, 33 genre/recipe
pairs, 5 profile selections, 11 amounts. All 33 materialize.

### The profiles are not on one scale

Share of the 33 recipes where the profile differs from STRAIGHT at all:

| amount | SWING COMPAT | LAID BACK | PUSH/PULL | all three mutually distinct |
|---|---|---|---|---|
| 12% | 0/33 | 33/33 | 0/33 | 0/33 |
| **20%** (old default) | **0/33** | **33/33** | **0/33** | **0/33** |
| 22% (LOOSE preset) | 0/33 | 33/33 | 0/33 | 0/33 |
| 30% | 0/33 | 33/33 | 33/33 | 33/33 |
| 50% | 0/33 | 33/33 | 33/33 | 33/33 |
| 60% | 18/33 | 33/33 | 33/33 | 33/33 |
| 80% | 33/33 | 33/33 | 33/33 | 33/33 |

At the old default two of three profiles were **byte-identical** to STRAIGHT —
not quiet, identical — and the third moved two events of nineteen by 5 ms.

### Magnitude per profile (median over the corpus)

| amount | LAID BACK | PUSH/PULL | SWING COMPAT |
|---|---|---|---|
| 20% | 2 events / 5.1 ms | 0 | 0 |
| 30% | 6 / 10.2 ms | 5 / 5.1 ms | 0 |
| 50% | 10 / 14.6 ms | 7 / 10.2 ms | 0 |
| 100% | 14 / 29.2 ms | 14 / 19.5 ms | 5 / 9.7 ms |

### The anchor

`LAID BACK never moves the kick, at any amount, anywhere in the corpus.` That is
the profile's shape, not a bug: everything else drags behind a steady anchor.
`PUSH/PULL` moves the anchor on 100% of recipes from 30% upward, which is why it
is the most immediately audible of the three.

This also explains the GF2-I2 hardware failure more precisely than "the amount
was low". Minimal Space holds exactly **one kick per bar**, so under LAID BACK
the steady reference was too sparse to function. The fixture was as much at
fault as the amplitude.

## Decisions

| decision | old | new | evidence |
|---|---|---|---|
| default `microTimingAmount` | 0.2 | **0.5** | both moving profiles clear the threshold with margin; 0.8 would be needed to register SwingCompatible and drives LAID BACK to 24 ms as a shipped default |
| FEEL AMOUNT step | 0.01 | **0.05** | the useful range is roughly 0.15–1.0; crossing it took about eighty presses |
| TIGHT preset | 0.02 | **0.15** | deliberately the weak case, below the knee |
| HUMAN preset | 0.12 | **0.50** | matches the new default |
| LOOSE preset | 0.22 | **0.80** | above the knee, where all three profiles register |
| SwingCompatible | undecided | **expected zero by design** | it is defined as compatible with the independently applied `swingPct` and adds no displacement of its own |
| 17 inert-AUTO recipes | undiagnosed | **recorded as correct** | acid, techno, house, drum & bass and rave are played straight |

No `kFeel*` weight, profile coefficient or vocabulary was retuned. Rescaling
SwingCompatible to match the other two was considered and rejected: it would
change the sound of existing projects to fix a profile that is doing what it was
designed to do.

## The magnitude contract, applied

First use of `docs/gf2/GF2_MAGNITUDE_CONTRACT.md`. The test reads
`GeneratorParams().microTimingAmount` directly, so it tracks the shipped value
rather than a copy.

```text
At the shipped default, on every production recipe, a manually selected
LAID BACK or PUSH/PULL must displace >= 3 events by >= 2 ticks, and
STRAIGHT / LAID BACK / PUSH/PULL must stay mutually distinct.
```

Thresholds come from the census: at 50% the weakest case is LAID BACK with 5
events and PUSH/PULL with 3, both at 2 ticks; at the old 20% default PUSH/PULL
displaced nothing anywhere.

### The named corpus

Classified by what the vocabulary actually draws for AUTO, not by a guessed
list, with the counts pinned so a vocabulary edit has to be acknowledged:

| class | count | contract |
|---|---|---|
| AUTO draws LaidBack or PushPull | 16 | must reach the threshold |
| AUTO draws Straight | 11 | must stay identical to STRAIGHT |
| AUTO draws SwingCompatible | 6 | must stay an expected zero |

Manual selection is held to the threshold on **all 33** recipes — what AUTO
draws is a statement about the genre, but a musician who picks a profile by hand
must hear it everywhere.

### Evidence

```text
RED   at the 0.2 default: 82 failures. LAID BACK managed 1-2 events at one
      tick, PUSH/PULL zero, and no recipe kept its three profiles distinct.

GREEN shipped FEEL AMOUNT: 50%  (threshold: >=3 events, >=2 ticks)
      corpus: 16 active, 11 draw STRAIGHT, 6 draw SWING COMPAT
      every active recipe is audible at the default              OK
      FEEL amount zero is neutral for every profile              OK
      TIGHT / HUMAN / LOOSE stay recognisably different          OK (1 < 9 < 12)
      GF2-I2A feel magnitude: PASS
```

## Invariants held

```text
amount zero exactly neutral        PASS   every profile, every recipe
STRAIGHT exactly zero              PASS   at the shipped default
topology unchanged                 PASS   only step.timing differs
determinism                        PASS   census artifact is byte-reproducible
persistence                        PASS   a stored amount survives the new
                                          default; only a document without the
                                          field inherits it
budget                             PASS   fixed DRAM gate unchanged
```

## Target matrix

```text
$ bash scripts/validate_gf2_targets.sh --all
GF2 COMMIT            ade4a69889a60aa1931882ece0c1a4cdb1f08242
HOST                   PASS      run_host_tests + 0.9.9-C/I0R + C2-V0R + I1 + I2 + I2A
SDL                    PASS
CARDPUTER_ADV          PASS
FIXED_DRAM             PASS
SEQTRAK_MIDI_ONLY      PASS
GF2 TARGET STATUS     GREEN
```

## Hardware A/B

Minimal Space is deliberately **not** the primary fixture this time.

```text
Genre / Recipe   Classic 2-Step   (AUTO draws PUSH/PULL; anchor moves)
BPM              fixed, identical across every case
SWING            50 / neutral
FEEL AMOUNT      left at the new default for case C
```

| case | change | expected |
|---|---|---|
| A | PROFILE = STRAIGHT | reference placement |
| B | PROFILE = PUSH/PULL | kick and snare separate audibly |
| C | **at the default, without touching FEEL AMOUNT** | B is audible as shipped — the acceptance test |
| D | FEEL AMOUNT = 0 | identical to STRAIGHT |
| E | Minimal Space, same settings | control case: LAID BACK, anchor stationary by design |

Case C is the checkpoint. If the profile is only audible after the musician
hunts for the amount control, I2A has not delivered.

### Result — case C FAILED

Flashed at `22923001` with remote CI green on the same SHA. Switching PROFILE
was still not distinguishable by ear at the shipped default, or at maximum
amount.

The same listener **does** hear the SWING control. That is a calibration, not a
failure report:

| control | displacement at 132 BPM | audible |
|---|---|---|
| SWING 75% | 12 ticks / 56.8 ms | yes |
| LAID BACK at the default | 3 ticks / 14.2 ms | no |
| PUSH/PULL at the default | 2 ticks / 9.4 ms | no |

The perceptual threshold therefore lies above 2 ticks and at or below 12. The
provisional contract threshold of 2 ticks sits below it.

The ceiling matters more than the default here. At FEEL AMOUNT 100 the profiles
top out at 6 ticks (LAID BACK), 4 ticks (PUSH/PULL) and 2 ticks
(SwingCompatible) — roughly half the swing range. If the calibrated threshold
lands above 6 ticks, the amount control cannot reach it at any setting and the
profile coefficients themselves are the binding constraint.

## Calibration pending

The next step is not a code change. It is one measurement: find the smallest
SWING setting that a listener still distinguishes from 50%, which converts
directly to a threshold in ticks (`round((pct - 50) * 24 / 50)`).

| outcome | reading | consequence |
|---|---|---|
| lost at 54–56% (2–3 ticks) | the profiles are already large enough | the problem is distribution, not size: swing moves every odd step uniformly, the profile moves a scattered subset in mixed directions |
| lost at 58–65% (4–7 ticks) | the profile ceiling sits at or below the threshold | reopen the profile coefficients — the decision I2A deliberately deferred |
| only audible from 70% (10+ ticks) | micro-timing at this scale is not a musically available axis here | record it as a limitation rather than engineering against it |

Deferred by the musician pending a calibration session with a drummer. This is
the right call: the remaining decision is perceptual, and guessing the threshold
would put a number in the contract that no listener validated.

Until then `kMinDisplacedEvents` / `kMinOffsetTicks` in
`tests/test_gf2_i2a_feel_magnitude.cpp` are marked PROVISIONAL in source. The
contract currently proves the effect is present, deterministic and mutually
distinct. It does **not** prove it is audible, and the checkpoint does not claim
that.

## Semantic delta

```text
production semantic delta   amplitude only
profile weights             unchanged
vocabularies                unchanged
rhythm topology             unchanged
tempo arbitration           unchanged (GF2-I1)
FEEL resolution             unchanged (GF2-I2)
velocity                    unchanged
```

## Status

```text
GF2-I2A   contract PASS, acceptance BLOCKED on perceptual calibration
```

The amplitude change ships: at 0.2 two of three profiles were byte-identical to
STRAIGHT, which was indefensible regardless of where the perceptual threshold
turns out to be. It is necessary and, on this evidence, not yet sufficient.

## What I2A does not close

```text
GF2-I3   phrase-law execution
GF2-I4   corridor field consumers
GF2-I5   secondary-role depth
GF2-C2-V1 / C2 / G1
```

Recorded for Gate B: **17 of 33 recipes have an inert AUTO** — 11 drawing
STRAIGHT and 6 drawing SwingCompatible. That is an accepted editorial state, not
a defect, but it is exactly the kind of fact Gate B exists to weigh when it
classifies real against declared capacity.
