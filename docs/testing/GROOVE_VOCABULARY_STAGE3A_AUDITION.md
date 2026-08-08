# Groove Vocabulary Stage 3A — Hardware Audition

## Purpose

Listen to the Stage 2 `RhythmPhraseRealizer` on real Cardputer ADV audio before the production generator is migrated.

Stage 3A is a temporary listening harness, not a production migration stage. It materializes one-bar role-level rhythm plans into the current in-memory patterns only while audition mode is armed.

It intentionally does **not** test:

- Genre selection or Genre mapping;
- Song/PhraseCore ownership;
- BarEvolution;
- generated bass pitch/motifs;
- chord/lead pitch generation;
- Scene persistence;
- automatic BPM changes;
- TB303 slide/accent generation from gate intent.

## Hardware list

- M5Stack Cardputer ADV;
- USB-C data cable;
- built-in speaker or normal audio output path.

No PORT.A devices, SEQTRAK, external MIDI device, external display, or SD content is required for the core listening pass.

## Wiring

USB-C only.

Stage 3A does not change GPIO, PORT.A I2C, ES8311/I2S ownership, USB MIDI routing, or external display initialization.

## Build / flash

```bash
git fetch origin
git switch agent/20260808-05-groove-vocabulary-stage3a-audition
git reset --hard origin/agent/20260808-05-groove-vocabulary-stage3a-audition

bash tests/run_rhythm_stage1_tests.sh
bash tests/run_rhythm_stage2_tests.sh
bash tests/run_rhythm_stage3a_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Use the actual serial port if it differs from `/dev/ttyACM0`.

## Safety / ownership contract

Audition mode is ephemeral.

On activation it copies the current:

```text
DrumPatternSet
Synth A pattern
Synth B pattern
```

into fixed-capacity RAM backup fields, then replaces only those current in-memory patterns with the audition material.

On `Ctrl+Alt+A` exit, the exact backed-up patterns are restored.

Do **not** Save the project while audition mode is active. Stage 3A deliberately does not add persistence semantics for temporary audition material.

Audition activation is rejected while Song mode is active. Exit Song mode first so current-pattern playback and the audition material refer to the same storage target.

## Controls

`Ctrl+Alt+A` is the only global activation/deactivation chord. This avoids stealing the existing Pattern Edit `Alt+A` accent command.

While audition mode is **OFF**, all normal `Alt` shortcuts keep their existing behavior.

While audition mode is **ON**:

| Key | Action |
|---|---|
| `Ctrl+Alt+A` | Exit audition and restore original current patterns |
| `Alt+1` | `straight_drive` |
| `Alt+2` | `rolling_acid` |
| `Alt+3` | `classic_2step` |
| `Alt+4` | `two_step_roll` |
| `Alt+5` | `sparse_skank` |
| `Alt+P` | Cycle `P1 -> P2 -> P3 -> P1` |
| `Alt+[` | Previous seed |
| `Alt+]` | Next seed |
| `Alt+B` | Fixed-root BassRhythm OFF/ON |
| `Alt+R` | Reapply the same deterministic realization |
| `Space` / transport button | Normal transport control |

The screen toast and Serial output identify the active state, for example:

```text
AUD classic_2step S7 P2 BOFF
[RHYTHM-AUDITION] AUD classic_2step S7 P2 BOFF bpm~132
```

The `bpm~` value is a listening suggestion only. Stage 3A does not change project BPM.

## Reference archetypes

| Key | Archetype | Listening role | Suggested BPM |
|---|---|---|---:|
| `Alt+1` | `straight_drive` | Four-floor / Techno control | 128 |
| `Alt+2` | `rolling_acid` | Acid rhythmic control | 126 |
| `Alt+3` | `classic_2step` | UK Garage | 132 |
| `Alt+4` | `two_step_roll` | D&B / fast break control | 174 |
| `Alt+5` | `sparse_skank` | Dub / space control | 118 |

These are curated audition grammars, not the final Stage 3 19–20-archetype reference vocabulary.

## Listening procedure

### Pass A — drums only

1. Ensure Song mode is OFF.
2. Choose a simple drum engine you know well; do not change it during one comparison block.
3. Start transport.
4. Press `Ctrl+Alt+A`.
5. Leave BassRhythm `OFF`.
6. For each archetype `Alt+1..5`, listen to seeds `1..8`.
7. For each seed listen to `P1`, `P2`, `P3` for at least four loops each.
8. Do not change synth sound, FX, drum engine, or BPM inside a direct P1/P2/P3 comparison.

Record one label per level:

```text
GREAT     memorable / would keep
GOOD      musical and usable
DEAD      legal but boring or too empty
RANDOM    roles feel independently randomized
BUSY      excessive activity / weak space
COLLAPSE  archetype identity collapses toward generic rhythm
```

Example:

```text
classic_2step  seed 03  P1 GOOD  P2 GREAT  P3 GOOD
classic_2step  seed 04  P1 GOOD  P2 RANDOM P3 COLLAPSE
```

### Pass B — fixed-root bass rhythm

1. Stay on the same archetype/seed/P-level used in Pass A.
2. Press `Alt+B` to enable BassRhythm.
3. Synth A plays only fixed MIDI note 36 (C2) on BassRhythm onsets.
4. No generated pitch contour, slide, or accent is introduced by Stage 3A.
5. Compare Bass OFF vs ON.

Listen for:

- bass rhythm supporting/responding to drums;
- preserved intentional gaps;
- UK 2-Step remaining broken rather than becoming four-floor;
- Dub retaining space;
- D&B feeling rolling rather than merely dense;
- Acid rhythm becoming more propulsive without relying on pitch tricks.

### Pass C — determinism / restore

1. Note an archetype, seed and P-level.
2. Press `Alt+R` several times. The pattern must remain identical.
3. Move to the next seed and back. The original seed must return to the same realization.
4. Exit with `Ctrl+Alt+A`.
5. Verify the original Drum/Synth A/Synth B current patterns are restored.
6. Re-enter audition; the project patterns must not have been replaced permanently.

## Expected behavior

Screen / toast:

- activation shows `AUD <name> S<seed> P<level> B<ON/OFF>`;
- each audition command updates the toast;
- exit shows `AUDITION OFF / RESTORED`;
- Song mode activation attempt shows `AUD: EXIT SONG MODE`.

Serial:

- every successful state reports `[RHYTHM-AUDITION] ...`;
- invalid realization reports `AUDITION INVALID` while preserving the previous audition pattern;
- exit reports restoration of original current patterns.

Audio:

- P1/P2/P3 of one seed share one phrase identity;
- P2/P3 may add legal variation but do not reroll the structural core;
- changing seed may establish a new identity within the same archetype;
- changing archetype deliberately changes rhythmic organization;
- drums-only and fixed-root-bass passes remain directly comparable.

## Troubleshooting

### `Ctrl+Alt+A` does nothing

Verify this exact Stage 3A branch is flashed. Normal `Alt+A` remains the existing accent shortcut and does not activate audition.

### `AUD: EXIT SONG MODE`

Disable Song mode first. Stage 3A intentionally refuses ambiguous Song/current-pattern ownership.

### `AUDITION INVALID`

Record archetype, seed and P-level from Serial. Do not keep changing controls. The previous valid audition pattern should remain audible. Treat the case as a real realizer/catalog falsification seed.

### Pattern changed after leaving audition

This is a release-blocking Stage 3A defect. Exit without saving and report the exact sequence of audition commands. Backup/restore host tests must also be rerun.

### Bass sounds melodically primitive

Expected in this stage. Bass uses fixed C2 by design. Stage 3A evaluates BassRhythm placement, not Bass Generator v2 pitch quality.

### P2/P3 sound almost identical

Record the archetype/seed. This may indicate insufficient legal variation capacity rather than a runtime bug and is useful input for the Stage 3 curated vocabulary.

## Acceptance checklist

- [ ] Stage 1 host tests pass;
- [ ] Stage 2 full host matrix passes;
- [ ] Stage 3A GCC/Clang/ASan+UBSan matrix passes;
- [ ] five audition grammars validate;
- [ ] each reference archetype has multiple P1 identities across the fixed seed corpus;
- [ ] P2/P3 reuse P1 identity;
- [ ] materializer invents zero physical onsets;
- [ ] materializer drops zero realized role onsets for bound audition roles;
- [ ] fixed-root BassRhythm creates no pitch motion, slides, or accents;
- [ ] inactive normal `Alt` shortcuts are preserved;
- [ ] `Ctrl+Alt+A` activation does not collide with Pattern Edit `Alt+A` accent;
- [ ] activation is rejected in Song mode;
- [ ] exit restores the exact pre-audition current patterns;
- [ ] audition path performs no Save/Scene persistence/Genre regeneration;
- [ ] SDL build passes;
- [ ] Cardputer ADV normal build passes;
- [ ] fixed DRAM gate passes;
- [ ] SEQTRAK MIDI-only build passes;
- [ ] no unexpected audio underruns/crackling during at least 10 minutes of audition switching;
- [ ] at least seeds `1..8` are rated for all five archetypes in drums-only mode;
- [ ] the same seed subset is checked with fixed-root BassRhythm;
- [ ] any technical finding resets the three-review counter to `0/3`.
