# Instrument Interaction Closure — Cardputer ADV Hardware Acceptance

Branch:

    feature/20260923-instrument-interaction-closure

Base:

    dfccfc3435545ca085e08d179645ea8bc1a3addd

Scope is deliberately limited to Cardputer instrument interaction:

- physical arrow de-duplication in Synth NOTE ENTRY;
- audible Synth Pattern Retrig instead of R0 / unsupported RV;
- eight-lane Drum grid readability;
- direct 3..0 mute labels;
- per-hit Drum accent behavior and visibility.

This checkpoint does NOT change Material/Song ownership, Pattern grid resolution,
persistence schema, synth engine selection, or Song/Melody architecture.

## 1. Exact-head preflight

Run from a clean checkout of this branch:

```bash
git branch --show-current
git rev-parse HEAD
git status --short
bash tests/run_instrument_interaction_closure_tests.sh
```

Record:

    TEST_SHA=
    WORKTREE_CLEAN=
    FOCUSED_TESTS=

Do not flash a different SHA than the one recorded here.

## 2. Authoritative Cardputer build

Use the dynamic-FatFs build only:

```bash
export BUILD_PATH="$PWD/build/cardputer-adv-interaction"
export ARDUINO_BUILD_PATH="$BUILD_PATH/.arduino-build"

bash scripts/build_cardputer_dynbuffers.sh
sha256sum "$BUILD_PATH/GroovePuter.ino.elf" \
          "$BUILD_PATH/GroovePuter.ino.bin" \
          "$ARDUINO_BUILD_PATH/GroovePuter.ino.map"
```

The build is acceptable only if:

- FS1B reports candidate-only FatFs;
- static DRAM is <= 191488 B;
- the exact ELF/BIN/MAP hashes are recorded.

Record:

    FS1B=
    STATIC_DRAM=
    STATIC_HEADROOM=
    FLASH_USAGE=
    ELF_SHA256=
    BIN_SHA256=
    MAP_SHA256=

## 3. Flash exact build

Default USB CDC port:

```bash
bash scripts/upload.sh /dev/ttyACM0
```

Because BUILD_PATH remains exported, upload.sh flashes the exact FS1B build above
and verifies the map rather than silently accepting stock FatFs.

After boot, optionally record a serial window:

```bash
bash scripts/monitor.sh 60 /dev/ttyACM0
```

## 4. NOTE ENTRY — physical arrows

Purpose: prove one physical arrow produces one navigation action.

1. Open Synth A NOTES.
2. Select a non-empty Pattern.
3. Enter NOTE ENTRY with N.
4. Enter at least one note so a "last entered note" exists.
5. Put the cursor on an empty step.
6. Press physical Up once.

PASS:

- cursor/navigation moves exactly once according to NOTE ENTRY navigation;
- no note is inserted by the punctuation shadow;
- there is no second command caused by `;'.

Repeat with Left, Down and Right.

Also verify normal letter note-entry keys still enter notes.

Record:

    NOTE_UP=
    NOTE_LEFT=
    NOTE_DOWN=
    NOTE_RIGHT=
    NOTE_ENTRY_LETTERS=

## 5. Synth Pattern Retrig

Purpose: prove F represents an audible operation.

Use a simple Pattern with one clearly audible note on the selected step.
A patch with a clear transient/decay is preferable.

Test Synth A first:

1. Start transport.
2. Focus the sounding step.
3. Press F once.

PASS:

- marker becomes R2, never R0 or RV;
- the selected onset is audibly retriggered.

4. Press Alt+Up repeatedly.

PASS:

- marker increases up to R8;
- retrigger density audibly increases;
- value never exceeds R8.

5. Press Alt+Down repeatedly.

PASS:

- value stops at R1;
- it never reaches R0 while Retrig is active.

6. Press F again.

PASS:

- Retrig switches off;
- R marker disappears;
- ordinary single-trigger playback returns.

Repeat the basic F ON/OFF test on Synth B.

Recommended listening engines:

- TB303;
- SH101;
- one of SID / AY / SN76489 / WAVEMORPH.

The purpose is not to certify identical articulation across engines. It is to
prove that the generic Pattern retrigger reaches the active synth voice.

Record:

    SYNTH_A_R2=
    SYNTH_A_R1_R8=
    SYNTH_A_OFF=
    SYNTH_B_R2=
    ENGINE_2_R2=
    RV_VISIBLE=NO

## 6. Drum grid layout and mute mapping

Open DRUMS on the 240x135 device.

PASS:

- all eight logical lane rows fit on-screen;
- the bottom lane is visible;
- normal engines show:

      3KIK
      4SNR
      5HH1
      6HH2
      7PR1
      8PR2
      9RIM
      0CLP

- TR-606 keeps its engine-specific final-lane meaning while retaining the 9/0
  mute-number mapping.

While transport is running, press 3 through 0 and verify the visible label makes
the muted voice predictable without counting rows.

Record:

    ALL_8_LANES_VISIBLE=
    BOTTOM_LANE_VISIBLE=
    MUTE_3_TO_0_MAPPING=
    TR606_LABELS=

## 7. Drum accent — selected hit only

Create two hits on the SAME step, for example Kick and Snare.

1. Focus Kick on that step.
2. Press A.

PASS:

- only the Kick hit receives the accent marker;
- Snare on the same column does not change;
- Kick accent is audibly stronger/different according to the selected drum engine.

3. Press A again.

PASS:

- Kick accent is removed;
- Snare remains unchanged.

4. Move to an empty cell and press A.

PASS:

- no invisible accent state is created;
- toast says `ACCENT: ADD HIT`.

Repeat once on Snare or another voice.

Record:

    ACCENT_SELECTED_HIT_ONLY=
    ACCENT_AUDIBLE=
    ACCENT_MARKER_VISIBLE=
    ACCENT_EMPTY_FAIL_CLOSED=

## 8. Theme/layout check

Repeat the Drum page visual inspection in both public UI styles.

PASS:

- bottom lane remains visible;
- numbered labels do not collide with the first step column;
- cursor, selected cell and accent marker remain distinguishable;
- footer/HUD does not cover the eighth lane.

Record:

    THEME_1_LAYOUT=
    THEME_2_LAYOUT=

## 9. Persistence smoke test

No storage format changed, but the existing persisted fields should still round-trip.

1. Set one Synth step to Retrig R3.
2. Set one Drum hit to Accent.
3. Save project.
4. Power off physically.
5. Cold boot and load the project.

PASS:

- Synth step returns as Retrig R3 and remains audible;
- Drum accent returns on the exact hit;
- no unrelated Pattern or Drum cells changed.

Record:

    RETRIG_PERSISTENCE=
    DRUM_ACCENT_PERSISTENCE=

## 10. Runtime smoke / regression

Run a dense normal workload for at least several minutes:

- Synth A + Synth B;
- full drums;
- Retrig active on a few synth steps;
- several drum accents;
- SD mounted;
- normal UI navigation.

Watch the existing PERF log.

PASS:

- no reset/watchdog;
- no growing audio underrun count attributable to these changes;
- no stuck notes;
- no runaway key repeat;
- no accidental note insertion from arrow navigation.

Record:

    AUDIO_UNDERRUN_DELTA=
    RESET_OR_WATCHDOG=
    STUCK_NOTES=
    INPUT_DOUBLE_DISPATCH=

## 11. Grid-resolution boundary

This branch deliberately does NOT add 1/8 / 1/16 / 1/32 controls to the fixed
16-step Pattern/Drum editor.

Verify ordinary Pattern and Drum editing still uses 16 physical step positions.

Melody/FEEL grid-resolution work is a separate semantic surface and must not be
claimed by this checkpoint.

Record:

    FIXED_16_STEP_PATTERN_UNCHANGED=

## Final verdict

Use exactly one:

    INSTRUMENT_INTERACTION_HARDWARE_GREEN

    INSTRUMENT_INTERACTION_HARDWARE_RED

If RED, record:

    FIRST_FAILED_CASE=
    OBSERVED=
    EXPECTED=
    TEST_SHA=
