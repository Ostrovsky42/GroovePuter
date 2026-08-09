# Stage 7A Five-Candidate Cardputer ADV Audition

## Purpose

Listen to five evidence-selected generalized rhythm grammars before any production `ReferenceVocabulary` admission.

This is a temporary falsification harness. It tests whether the Atlas-derived candidates sound like stable, reusable musical ideas rather than source-pattern copies or aliases of existing GroovePuter grammars.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable for power/flash/Serial
- headphones/speaker on the normal GroovePuter audio output

SEQTRAK is optional and not required for this test.

## Wiring

No external I2C/SPI unit is required.

```text
Cardputer ADV <-USB-C-> host computer
Cardputer audio output -> headphones / speaker
```

Use normal Cardputer ADV power/USB voltage. PORT.A is unused by this test.

## Build / flash

Use the final reviewed SHA recorded in PR #193. Do not test a moving branch head after that SHA changes.

```bash
git fetch origin
git checkout agent/20260809-10-stage7a-five-candidate-audition
git reset --hard <FINAL_SHA_FROM_PR_193>
bash scripts/build.sh --warnings all
```

Flash with the project's normal Cardputer ADV upload command/workflow.

Before listening, open Serial at the same baud rate used by normal GroovePuter diagnostics.

## Enter audition

1. Boot normally.
2. Make sure Song mode is OFF.
3. Navigate to `GENERATE -> GENRE`.
4. Press `Ctrl+Alt+A`.
5. A toast/Serial line beginning with `S7A` must appear.

While audition is active it is modal on the GENRE page. Current drum and Synth A/B patterns were backed up before activation. Synth A/B are temporarily cleared so only candidate drums are heard.

## Controls

| Control | Action |
|---|---|
| `Ctrl+Alt+A` | enter / exit audition; exit restores original patterns |
| `Ctrl+1` | `staggered_machine` — HARD_02 — evidence-backed machine/electro |
| `Ctrl+2` | `cross_cycle` — HARD_05 — evidence-backed Afro/Bossa, mapping-sensitive |
| `Ctrl+3` | `break_halfstep` — HARD_04 — single-root break/funk challenger |
| `Ctrl+4` | `rock_push` — HARD_09 — single-root rock challenger |
| `Ctrl+5` | `halfback_control` — HARD_03 — control candidate |
| `Ctrl+P` | P1 -> P2 -> P3 -> P1, reusing the same P1 phrase identity |
| `Ctrl+Left` | previous seed |
| `Ctrl+Right` | next seed |
| `Ctrl+R` | rerender the exact same candidate/seed/P-level |
| `Space` | normal transport play/stop while audition is active |

`Ctrl+Left/Right` uses the project's normalized arrow path (`UIInput::navCode`) so Cardputer HID/scancode and WORD paths behave the same. Bracket characters are not used for seed navigation.

Global panic/emergency handling remains outside the audition owner.

## Listening protocol

For each of the five slots:

1. Start at seed 1 and P1.
2. Listen for at least four loops.
3. Cycle P1/P2/P3 with `Ctrl+P`; listen for at least four loops at each level.
4. Test seeds 1 through 8 with `Ctrl+Left/Right`.
5. On at least two seeds press `Ctrl+R` and confirm the rhythm is identical.
6. Move away from a seed and back to it; confirm deterministic return.
7. Record one rating per candidate:

```text
GREAT
GOOD
DEAD
RANDOM
BUSY
COLLAPSE
SAME-AS-EXISTING
```

Also note the closest existing GroovePuter archetype if you choose `SAME-AS-EXISTING`.

## What to listen for

### P1

- recognizable core survives seed changes;
- not a single fixed preset;
- kick/backbeat identity is stable enough to recognize the candidate;
- candidate does not sound like accidental noise.

### P2

- variation remains the same phrase identity;
- added movement is audible but does not replace the groove;
- hats/percussion do not dominate the skeleton.

### P3

- transformation is clearly related to P1;
- no random-fill collapse;
- identity is still recognizable after the stronger variation.

### Candidate-specific checks

`staggered_machine`: should feel distinct from existing straight/machine syncopation without becoming random electro clutter.

`cross_cycle`: specifically check whether the sparse conservative cymbal mapping still leaves a coherent cross-cycle. If it sounds incomplete rather than distinctive, rate `REVISE` in notes even if the kick cycle is interesting.

`break_halfstep`: compare mentally against `two_step_roll`, `ghosted_roll`, `sparse_fast_break` and `halftime_switch`.

`rock_push`: listen for a reusable push/backbeat identity rather than simply "basic rock".

`halfback_control`: this candidate is expected to be capable of failing by sounding equivalent to an existing grammar. `SAME-AS-EXISTING` is a successful falsification result.

## Expected behavior

Screen/Serial status resembles:

```text
S7A staggered_machine S1 P1 EVID
S7A cross_cycle S4 P2 EVID
S7A break_halfstep S2 P3 CHAL
S7A halfback_control S8 P1 CTRL
```

Changing P-level must not change the P1 phrase identity. Changing candidate or seed may establish a new identity.

During audition:

- only temporary drums are materialized;
- Synth A/B are silent;
- normal project navigation/save is blocked by the modal GENRE handler;
- no new genre/recipe is persisted;
- production `ReferenceVocabulary` is untouched.

Exiting with `Ctrl+Alt+A` must restore the exact drum, Synth A and Synth B patterns present before activation.

## Troubleshooting

### `Ctrl+Alt+A` does nothing

Confirm you are on `GENERATE -> GENRE`, not FEEL or another workspace. Confirm Song mode is OFF.

### `Ctrl+Left/Right` does nothing

Confirm audition is active and the key event includes Ctrl. The handler reads normalized arrow scancode/key input rather than `[`/`]` characters. Record the `[KEY]` Serial line if it still fails.

### `S7A INVALID`

Record the candidate, seed and P-level from Serial. Do not continue rating that seed as musical evidence.

### Candidate switches but no sound

Press `Space` to start transport. Verify normal GroovePuter audio works after exiting audition.

### Original pattern did not return

Stop the test and report it as a release-blocking Stage 7A failure. Do not Save the project.

### Crackle / freeze / reset

Record Serial output and the candidate/seed/P-level that triggered it. Treat this as a blocker, not a musical rating.

## Acceptance checklist

- [ ] exact final PR #193 SHA flashed
- [ ] normal boot
- [ ] Song mode OFF
- [ ] `Ctrl+Alt+A` enters only on GENRE page
- [ ] five Ctrl-number slots select five different candidates
- [ ] `Ctrl+Left/Right` changes seed and can return to the previous seed
- [ ] seeds 1..8 tested for each slot
- [ ] P1/P2/P3 tested for each slot
- [ ] `Ctrl+R` deterministic on sampled seeds
- [ ] returning to a previous seed reproduces it
- [ ] no Bass/Synth material audible during audition
- [ ] no freeze/reset/new crackle
- [ ] each candidate rated
- [ ] `Ctrl+Alt+A` exit restores exact original drums + Synth A/B

Do not mark any candidate production-accepted from this hardware test alone. Return the ratings and notes to the Stage 7 curation review.
