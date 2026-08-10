# Generation Stage 14 — Lo-Fi and Genre Expansion Acceptance

## Purpose

Stage 14 extends the Stage 13 composition matrix with slower and less skeleton-dominated directions without adding a second generator framework.

Production additions:

- top-level Genre directions: `House`, `Techno`, `Hip-Hop`, `Funk/Soul`, `UK Garage`, `Drum&Bass`, `Lo-Fi`;
- Lo-Fi variants: `Classic Chill`, `Drunken Groove`, `Lo-Fi House`, `Minimal Sleep`;
- Hip-Hop variants: `Golden Era`, `Dusty Jazz`;
- one physical Synth B can use chord topology with sparse melodic fill on otherwise free cells.

The production rhythm vocabulary remains **24 identities**. Stage 14 does not promote pending Stage 7A candidates merely to fill a new Genre pool.

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

The production bridge deliberately keeps:

```cpp
request.phraseBars = 1;
```

until the normative Stage 6.1 physical hardware gate authorizes a production multi-bar caller. A displayed or logged `phraseBars=4/8` is therefore planning information, not proof that four/eight bars were materialized by Stage 12.

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
- the corridor line shows the selected Variant BPM range, not one fixed top-level Lo-Fi BPM.

Generation:

- HARD_02/HARD_04/HARD_05 do not resolve through `ReferenceVocabulary` or Genre compatibility pools;
- Lo-Fi AUTO reaches several already-approved production identities across a pattern-address sweep;
- Lo-Fi Synth B materializes chord topology first and adds only sparse melodic cells that do not collide with chord onsets/holds;
- Lo-Fi melodic fill never exceeds three onsets per bar before additional chord blocking;
- `Minimal Sleep` suggested BPM is 54 and corridor is 42–66;
- `Lo-Fi House` suggested BPM is 106 and corridor is 92–118;
- normal Stage 14 generation still materializes one RhythmArchetype bar at a time until the Stage 12 hardware gate is cleared.

Serial:

- no assertion/reset/watchdog loop during generation;
- no continuously growing audio underrun count during normal audition;
- Save/reboot/Load retains Genre/Variant and existing RHYTHM AUTO/MANUAL intent.

## Troubleshooting

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
