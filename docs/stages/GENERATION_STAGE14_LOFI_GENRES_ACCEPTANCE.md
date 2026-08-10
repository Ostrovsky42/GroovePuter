# Generation Stage 14 — Lo-Fi and Genre Expansion Acceptance

## Purpose

Stage 14 extends the Stage 13 composition matrix with slower and less skeleton-dominated directions without adding a second generator framework.

Production additions:

- top-level Genre directions: `House`, `Techno`, `Hip-Hop`, `Funk/Soul`, `UK Garage`, `Drum&Bass`, `Lo-Fi`;
- Lo-Fi variants: `Classic Chill`, `Drunken Groove`, `Lo-Fi House`, `Minimal Sleep`;
- Hip-Hop variants: `Golden Era`, `Dusty Jazz`;
- one physical Synth B can use chord topology with sparse melodic fill on otherwise free cells.

The production rhythm vocabulary remains **24 identities**. Stage 14 does not promote pending Stage 7A candidates merely to fill a new Genre pool.

### Hardware finding incorporated after `ec22b46`

The first Cardputer ADV audition rejected the previous automated candidate for three concrete reasons:

1. plain `DRUMS -> G` still called the legacy whole-drum randomizer directly, bypassing selected `RHYTHM AUTO/MANUAL` and semantic FEEL materialization;
2. `FeelSettings::patternBars` was already persisted and consumed as a `1/2/4/8` transport-cycle value, but the FEEL page no longer exposed a control for it;
3. `Funk/Soul` used the sparse/hybrid semantic palette and deterministic bar coordinate while its empty-bar policy did not admit the same sparse semantic result, allowing a full materialization transaction to fail for some generated combinations.

The corrected contract is:

- plain `DRUMS -> G` performs legacy drum generation as a rollback source, then applies `migrateStrongRhythmDrums()` with the active Genre/Rhythm/FEEL context;
- `Ctrl+G` remains voice-local legacy randomization and `Alt+G` remains explicit chaos randomization;
- DRUMS-page generation never regenerates Synth A/B;
- FEEL exposes `REPEATS = 1/2/4/8 BARS`, backed by the existing `scene.feel.patternBars` field;
- `REPEATS` is a transport-cycle setting and is **not** Stage 12 phrase evolution;
- Funk/Soul admits the same bounded sparse semantic-bar policy required by its Stage 14 hybrid profile.

### Evidence boundary

The following remain `HARDWARE_PENDING` until a repository hardware verdict exists:

- HARD_02 `staggered_machine` / 701
- HARD_04 `break_halfstep` / 703
- HARD_05 `cross_cycle` / 702
- HARD_09 `rock_push`
- HARD_03 `halfback_control`

`Latin` is therefore deferred: the proposed `cross_cycle` topology is not production-admitted yet, and Stage 14 will not publish an empty or invented Latin compatibility profile.

Atlas rows `lofi_sparse_stable_backbeat` and `boombap_syncopated_kick_backbeat` remain `REVIEW`. Jungle sampler-break topology remains out of scope until sampler ownership is explicit.

### Lo-Fi expression contract

Lo-Fi uses existing production rhythm topology plus independently selected FEEL, BassRhythm, ChordRhythm, MelodicRhythm/Motif, and phrase-planning metadata.

The physical output remains:

```text
Synth A = Bass
Synth B = Chord primary + sparse melodic fill
```

Synth B is still monophonic. Chord onsets and continuations have priority. Melodic events are admitted only on cells not occupied by the chord plan. The Lo-Fi melodic palette is restricted to identities with at most three onsets per bar before blocking; zero-event bars are valid when the selected sparse identity allows them.

This is not a third synth voice and not simultaneous chord/melody timbre ownership.

### Stage 12 boundary

`phraseLaw` / `phraseBars` remain transient planning metadata. Stage 14 does **not** claim production 2/4/8-bar BarEvolution execution.

The production rhythm-realization bridge deliberately keeps:

```cpp
request.phraseBars = 1;
```

until the normative Stage 6.1 physical hardware gate authorizes a production multi-bar caller. A displayed or logged `phraseBars=4/8` is therefore planning information, not proof that four/eight bars were materialized by Stage 12.

The FEEL `REPEATS 1/2/4/8` control is separate: it edits the already-existing transport-cycle value `scene.feel.patternBars` and does not widen Stage 12 ownership.

### Stage 9 boundary

Stage 14 does not widen Stage 9 into pitch contour or articulation ownership. Stage 9 established BassRhythm topology while the legacy generation path remains the source of pitch, velocity, and articulation during migration. Lo-Fi quality work in this stage stays inside that contract.

## Hardware list

- M5Stack Cardputer ADV (ESP32-S3FN8)
- USB-C data cable
- headphones or built-in speaker
- optional Yamaha SEQTRAK or another supported USB-MIDI receiver

## Wiring

No external wiring is required.

Use USB-C for flashing, Serial, and USB MIDI. Stage 14 adds no GPIO, I2C, or SPI ownership.

If PORT.A hardware is attached for an otherwise existing setup, its Cardputer ADV I2C contract remains unchanged: GPIO2 = SDA, GPIO1 = SCL, shared `Wire`, 3.3 V logic. Stage 14 neither configures nor accesses that bus and introduces no new power or voltage requirement.

## Build / Flash

From the repository root:

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_generation_stage13_tests.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Optional MIDI-only gate:

```bash
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

## Hardware audition

Open `GENERATE -> GENRE`.

### Hardware-regression retest: DRUMS G + FEEL

Before the longer musical matrix, verify the exact defects found on the first Cardputer pass:

1. Select a Stage 14 Genre and keep `RHYTHM = AUTO`.
2. Open `GENERATE -> FEEL` and confirm that `PROFILE` changes among the named timing profiles.
3. Confirm that the new `REPEATS` row cycles exactly `1 BAR -> 2 BARS -> 4 BARS -> 8 BARS` with Left/Right.
4. Set `PROFILE = STRAIGHT`, open DRUMS and press plain `G` several times.
5. Return to FEEL, select a visibly different timing profile such as `LAID BACK` or `PUSH/PULL`, then use plain DRUMS `G` again. The new drum material must use the selected FEEL profile rather than the old legacy-only randomizer path.
6. Set a named MANUAL RHYTHM and press plain DRUMS `G` across several pattern addresses. The selected rhythm identity must stay fixed while its deterministic realization may vary.
7. Confirm that plain DRUMS `G` does not change Synth A or Synth B patterns.
8. Quick-check `Ctrl+G` still randomizes only the selected drum voice and `Alt+G` still performs explicit chaos randomization.
9. Quick-check `Funk/Soul` with plain `G` over several pattern addresses; generation must never silently fail or produce an all-empty drum pattern.

### Lo-Fi matrix

Select `GENRE = Lo-Fi`, keep `RHYTHM = AUTO`, and audition:

1. `BASE`
2. `Classic Chill`
3. `Drunken Groove`
4. `Lo-Fi House`
5. `Minimal Sleep`

Materialize at least 12 different pattern addresses per variant.

Then choose named compatible RHYTHM values. Manual RHYTHM must keep the rhythm identity fixed while FEEL, bass, chord, sparse melodic realization, and other downstream choices may vary with the deterministic generation coordinate.

Suggested listening targets:

- `Classic Chill`: restrained pocket, whole/held chord space, sparse answers;
- `Drunken Groove`: noticeably less rigid pocket without random kick/backbeat placement;
- `Lo-Fi House`: recognizable house pulse with looser harmony/melodic behavior;
- `Minimal Sleep`: useful at 42–66 BPM; sparse or empty melodic bars are valid;
- all variants: recognizable family resemblance without repeating the complete pattern every time.

### New directions

Quick-check these base genres with `RHYTHM = AUTO`:

- House
- Techno
- Hip-Hop
- Funk/Soul
- UK Garage
- Drum&Bass

For Hip-Hop also audition `Golden Era` and `Dusty Jazz`.

`Latin` must not appear as a Stage 14 production Genre while HARD_05 remains `HARDWARE_PENDING`.

## Expected behavior

Screen:

- GENRE cycles through 16 top-level modes; persisted IDs 0..8 keep their original numeric meaning;
- Lo-Fi exposes exactly `BASE`, `Classic Chill`, `Drunken Groove`, `Lo-Fi House`, `Minimal Sleep`;
- Hip-Hop exposes exactly `BASE`, `Golden Era`, `Dusty Jazz`;
- RHYTHM AUTO/MANUAL lists only identities already present in the 24-entry production vocabulary;
- incompatible manual rhythm selection resets to AUTO;
- the corridor line shows the selected Variant BPM range, not one fixed top-level Lo-Fi BPM;
- FEEL exposes an editable `PROFILE` and an editable `REPEATS` row with only `1/2/4/8 BARS`.

Generation:

- HARD_02/HARD_04/HARD_05 do not resolve through `ReferenceVocabulary` or Genre compatibility pools;
- Lo-Fi AUTO reaches several already-approved production identities across a pattern-address sweep;
- plain DRUMS `G` uses the active Genre/Rhythm/FEEL context but changes drums only;
- a failed or unsupported strong drum route retains freshly generated legacy drums instead of clearing the pattern;
- host coverage sweeps all Stage 14 base directions and Lo-Fi/Hip-Hop variants across 64 pattern addresses and requires `Applied` plus at least one drum hit;
- Lo-Fi Synth B materializes chord topology first and adds only sparse melodic cells that do not collide with chord onsets/holds;
- Lo-Fi melodic fill never exceeds three onsets per bar before additional chord blocking;
- `Minimal Sleep` suggested BPM is 54 and corridor is 42–66;
- `Lo-Fi House` suggested BPM is 106 and corridor is 92–118;
- normal Stage 14 generation still materializes one RhythmArchetype bar at a time until the Stage 12 hardware gate is cleared.

Serial:

- no assertion/reset/watchdog loop during generation;
- no continuously growing audio underrun count during normal audition;
- Save/reboot/Load retains Genre/Variant, RHYTHM AUTO/MANUAL intent, FEEL profile and FEEL repeat count.

## Troubleshooting

### DRUMS G sounds unrelated to selected Genre/Rhythm

Fail the test for plain `G`. Plain `G` is now the whole-pattern generation command and must pass through the strong drum bridge after legacy fallback generation. `Ctrl+G` and `Alt+G` intentionally remain local/chaos editor tools and are not expected to preserve the full relational topology.

### FEEL PROFILE changes on screen but sounds unchanged

PROFILE owns bounded semantic timing for newly materialized material; it does not rewrite note topology in-place. Compare newly generated material with plain DRUMS `G` or GENRE `MATERIALIZE` after changing PROFILE. Swing remains the separate runtime offbeat timing control.

### FEEL REPEATS shows 1/2/4/8 but Stage 12 still materializes one bar

Expected. `REPEATS` is the existing transport-cycle control (`scene.feel.patternBars`). It must not be interpreted as Stage 12 multi-bar phrase generation.

### Lo-Fi repeats one complete beat

Keep `RHYTHM = AUTO` and materialize different pattern addresses. Resolution is deterministic for an identical generation coordinate; variability is expected across coordinates, not from repeated evaluation of the same coordinate.

### Manual rhythm resets after Genre/Variant change

Expected if the selected identity is outside the new compatibility set. The UI must show `RHYTHM RESET TO AUTO` rather than silently substitute another named identity.

### Minimal Sleep sounds unusually empty

Sparse bass/melody bars are intentional. Reject it only if a multi-pattern sample is musically unusable or violates topology/ownership contracts, not merely because one bar contains large rests.

### Chord disappears when melody appears

Fail the test. Hybrid Synth B is chord-first. Sparse melody may fill free cells but must never overwrite a chord onset or continuation.

### Stage 14 reports an 8-bar phrase but only one bar is materialized

Expected before the Stage 6.1 hardware gate. `phraseBars` is planning metadata; Stage 12 production execution remains deliberately unwired.

## Acceptance checklist

- [ ] `bash tests/run_generation_stage13_tests.sh` passes GCC, Clang when available, and ASan/UBSan.
- [ ] `bash tests/run_host_tests.sh` exits 0.
- [ ] Cardputer ADV normal build passes with warnings enabled.
- [ ] DRAM budget gate passes.
- [ ] MIDI-only build passes.
- [ ] Production `ReferenceVocabulary` remains 24 identities.
- [ ] IDs 701/702/703 are absent from production vocabulary and compatibility routing.
- [ ] House, Techno, Hip-Hop, Funk/Soul, UK Garage, Drum&Bass, Lo-Fi materialize without legacy fallback.
- [ ] Stage 14 host sweep materializes every new base/variant profile across 64 addresses without an empty drum result.
- [ ] Plain DRUMS `G` follows selected Genre/Rhythm/FEEL and leaves Synth A/B untouched.
- [ ] `Ctrl+G` remains selected-voice randomize; `Alt+G` remains chaos randomize.
- [ ] FEEL PROFILE affects newly materialized semantic timing.
- [ ] FEEL REPEATS cycles exactly 1/2/4/8 and persists through Save/reboot/Load.
- [ ] Latin is not exposed while HARD_05 lacks repository hardware evidence.
- [ ] Lo-Fi BASE/Classic Chill/Drunken Groove/Lo-Fi House/Minimal Sleep are audibly distinguishable.
- [ ] Minimal Sleep remains useful below 60 BPM.
- [ ] Lo-Fi chord material remains present while sparse melodic fills occupy only free Synth B cells.
- [ ] Sparse melodic fill is 0–3 onsets per bar.
- [ ] Manual RHYTHM remains fixed while downstream executed dimensions still vary.
- [ ] Hip-Hop Golden Era and Dusty Jazz are audibly distinguishable.
- [ ] Atlas Lo-Fi/Boom-Bap research rows remain REVIEW.
- [ ] Stage 12 is not described or tested as production multi-bar execution before its physical gate.
- [ ] 30-minute runtime soak shows no sustained underrun growth, reset, or navigation hang.
- [ ] Three consecutive clean reviews are completed on one unchanged final SHA; any finding or head movement resets the counter to 0/3.
