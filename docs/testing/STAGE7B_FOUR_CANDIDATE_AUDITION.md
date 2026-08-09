# Stage 7B Four-Candidate Cardputer ADV Audition

## Purpose

Listen to the four remaining recurring hardened Atlas Pass #2 rhythm clusters. All four are single-root challengers; this test should reject or merge weak ideas rather than force new archetypes.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable for power/flash/Serial
- headphones or speaker on normal GroovePuter audio output

SEQTRAK is optional.

## Wiring

No external I2C/SPI unit is required.

```text
Cardputer ADV <-USB-C-> host
Cardputer audio output -> headphones / speaker
```

PORT.A is unused.

## Build / flash

Use the final reviewed SHA from PR #194:

```bash
git fetch origin
git checkout agent/20260810-01-stage7b-remaining-four-audition
git reset --hard <FINAL_SHA_FROM_PR_194>
bash scripts/build.sh --warnings all
```

Flash with the normal Cardputer ADV workflow.

## Controls

Open `GENERATE -> GENRE`, keep Song mode OFF, then:

| Control | Action |
|---|---|
| `Ctrl+Alt+A` | audition ON/OFF; OFF restores originals |
| `Ctrl+1` | `stacked_quarters` / HARD_01 |
| `Ctrl+2` | `electro_backskip` / HARD_06 |
| `Ctrl+3` | `funk_house_bridge` / HARD_07 |
| `Ctrl+4` | `electro_gap_push` / HARD_08 |
| `Ctrl+P` | P1 -> P2 -> P3 -> P1 |
| `Ctrl+Left` | previous seed |
| `Ctrl+Right` | next seed |
| `Ctrl+R` | deterministic rerender |
| `Space` | transport; retains SEQ MASTER guard |

Seed arrows use `UIInput::navCode()`; bracket characters are not part of the contract.

## Expected behavior

- only temporary drums are heard; Synth A/B are cleared during audition;
- active audition is modal and global page/workspace/paging shortcuts do not escape it;
- `Ctrl+Alt+Backspace` restores/closes audition before normal PROJECT RESET;
- P2/P3 retain the P1 `PhraseRhythmIdentity` but audibly change at least some realizations;
- `Ctrl+R` reproduces the exact candidate/seed/P-level;
- exit restores exact pre-audition drums + Synth A/B.

## Listening protocol

For each slot, listen to P1 seeds 1..8. On seeds 1, 3, 5 and 8 also hear P2 and P3. Test `Ctrl+R` twice on at least two seeds and return to a previous seed to confirm determinism.

Rate each candidate:

```text
GREAT
GOOD
DEAD
RANDOM
BUSY
SAME-AS-EXISTING
```

For `SAME-AS-EXISTING`, record the closest existing archetype. Specifically compare slots 2 and 4; if they are one musical identity, record `MERGE 2+4`.

## Troubleshooting

If `Ctrl+Left/Right` does not change the seed, capture the Serial `[KEY]` line including `ctrl`, `key`, and scancode. If `S7B INVALID` appears, record candidate/seed/P-level and stop rating that realization. If original material is not restored, or a freeze/reset/new crackle occurs, treat it as a blocker and do not Save the project.

## Acceptance checklist

- [ ] exact final PR #194 SHA flashed
- [ ] normal boot; Song mode OFF
- [ ] `Ctrl+Alt+A` enters/exits audition
- [ ] `Ctrl+1..4` selects four distinct candidates
- [ ] `Ctrl+Left/Right` changes and reverses seed
- [ ] P1 seeds 1..8 checked on all four
- [ ] P2/P3 checked on seeds 1/3/5/8
- [ ] `Ctrl+R` deterministic
- [ ] modal navigation cannot escape temporary material
- [ ] no Bass/Synth material during audition
- [ ] no freeze/reset/new crackle
- [ ] each candidate rated
- [ ] exit restores exact drums + Synth A/B

No hardware rating alone changes production `ReferenceVocabulary`.
