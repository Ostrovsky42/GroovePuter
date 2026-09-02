# Song Generation Materialization

## 1. Purpose

Verify that Song Page generation creates audible pattern content instead of only assigning an existing pattern reference.

This stage changes the user contract as follows:

* `Q..I` still assigns an existing pattern without regenerating it.
* `G` generates Synth A, Synth B, or Drums into a safe free destination slot and assigns the selected Song cell only after generation succeeds.
* double-tap `G` prepares and commits all editable tracks in the current row atomically.
* generation is copy-on-write and never overwrites a slot referenced by another Song cell.
* successful single-cell and row generation each advance Scene revision exactly once.
* allocation/generation failure leaves patterns, Song references, Song length, and dirty revision unchanged.

### Reconnaissance baseline

Reconnaissance was performed against `dev` before implementation.

```text
dev SHA: 024f41db2c680b1038725da971d367d92f75a49e
```

#### Runtime ownership

* Song data is owned by `Scene::songs[2]`; `Scene::activeSongSlot` selects editable Song A or B.
* Current-page pattern banks are owned by `Scene::synthABanks[2]`, `Scene::synthBBanks[2]`, and `Scene::drumBanks[2]`.
* `SceneManager` points at the static processing Scene `g_mainScene`; `MiniAcid` owns the `SceneManager` and the production generation managers.
* `GrooveboxModeManager::generationSeed_` owns the deterministic production seed. Without an explicit seed, `ensureGenerationSeed()` captures the existing boot-seeded libc stream once; Cardputer boot seeds that stream from `esp_random()`.
* Scene dirty state is owned by the fixed 8-byte `GroovePuterState::SceneRevisionState` returned by `sceneRevisionState()`.

#### Generation paths before this stage

* `SongPage::generateCurrentCellPattern()` called `SmartPatternGenerator`/`rand()` to choose a numeric pattern slot and assigned only the Song reference.
* `SongPage::generateEntireRow()` repeated reference selection for the row; it did not materialize Synth A, Synth B, or Drums.
* `SmartPatternGenerator` owns a private `DeterministicRng`, but generates only a slot index. It is not a musical pattern materializer.
* `GrooveboxModeManager` is the production musical generator. It already has independent deterministic domains for Synth A, Synth B, Drums, and individual drum voices, and consumes the current mode, flavor, genre parameters, and behavior.
* `PhraseGenerator::generateBarsToSong()` already demonstrates destination preflight, materialization, deterministic bar roles, commit, and rollback. It assumes contiguous multi-bar Phrase slots and therefore was not connected directly to Song Page single-cell semantics.

#### Song pattern reference format

No new pattern ID format is introduced. A Song cell stores the existing global pattern ID:

```text
global ID = page * 16 + bank * 8 + slot
```

The UI formats the same ID as labels such as `1A4` and `1B2` through the existing Song pattern helpers.

#### Existing tests before this stage

* `tests/test_generation_rng_source_regressions.py`
* `tests/test_pattern_generator_rng.cpp`
* `tests/test_song_playhead_source_regressions.py`
* Song copy/paste/selection/source-regression coverage in `tests/run_host_tests.sh`

There was no behavioral host test that proved Song `G` created non-empty pattern bytes, preserved shared source patterns, or rolled back a failed row transaction.

#### Selectable synth engines on baseline `dev`

The runtime catalog returned by `MiniAcid::getAvailableSynthEngines()` is:

```text
TB303
SID
AY
SH101
SN76489
WAVEMORPH
```

The legacy OPL2 enum/value is decode-only and normalizes to TB303. It is not selectable.

#### Documentation mismatches on baseline `dev`

* `README.md` and `MANUAL.md` advertised OPL2 as selectable and omitted SH101, SN76489, and WaveMorph.
* PERFORMANCE TOOLS were implemented but not documented as shipped capabilities.
* MIDI Player physical-track mute/inspection panels, waveform overlay, Player/HUB MIDI navigation, and per-track routing were incomplete or absent from the overview docs.
* Song documentation described `Q..I` assignment but did not distinguish reference assignment from real material generation.
* Theme/session documentation did not accurately separate the public `CARBON ↔ CYBER` cycle from legacy AMBER compatibility.

## 2. Hardware

* M5Stack Cardputer ADV.
* Built-in 240×135 display.
* Built-in keyboard.
* Built-in ES8311 audio codec and speaker/headphone output.
* ESP32-S3 with PSRAM disabled.
* USB data cable for flash and Serial only.
* No external MIDI device is required.
* SEQTRAK is not required; all acceptance steps must pass without it.

## 3. Wiring

No external wiring is required.

Connect the Cardputer ADV USB-C port to the development computer with a data-capable cable. Use the built-in display, keyboard, audio codec, and speaker/headphones.

## 4. Build / Flash

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

The required CI/build matrix also includes:

```bash
python3 tests/test_generation_rng_source_regressions.py
python3 tests/test_song_playhead_source_regressions.py
bash tests/run_host_tests.sh

cd platform_sdl
make clean all CXX=g++
cd ..

bash scripts/build.sh --warnings all
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Both Cardputer builds must pass the repository fixed-DRAM gate. The gate is a regression budget and does not replace the runtime checks below.

## 5. Controls

| Control | Result |
|---|---|
| `Arrows` | Select Song row and track |
| `Ctrl+1..8` | Select pattern page |
| `B` | Select bank A/B for assignment context |
| `Q..I` | Assign an existing pattern; do not generate content |
| `G` | Materialize one selected Synth A, Synth B, or Drums cell |
| double-tap `G` | Atomically materialize Synth A, Synth B, and Drums for the current row |
| `Space` | Start/stop playback |
| Project Save | Persist generated patterns and Song references |

## 6. Expected behavior

### Single cell

1. Select an editable Song cell.
2. Press `G` once.
3. The firmware finds a strictly empty, unreferenced slot on the current pattern page.
4. The current production mode/genre generator prepares real events in fixed local storage.
5. Pattern content and the Song reference are committed together.
6. The toast identifies the generated track and destination, for example:

```text
GEN A -> 1A4
GEN B -> 1B2
GEN DR -> 1A6
```

The destination pattern must be audible. Synth generation must not modify the other synth or drum banks.

### Copy-on-write

If another Song cell references the old pattern, generating the selected cell assigns a new destination. The old pattern remains byte-identical and the other Song cell keeps its old reference.

### Current row

Double-tap `G` generates all editable tracks in the current row. All required slots are preflighted and all content is prepared before commit. Success shows:

```text
GENERATED ROW 12
```

The operation advances Scene revision once, not once per track.

### Errors

When no safe destination exists:

```text
NO EMPTY PATTERN SLOTS
```

When a production generator cannot produce valid non-empty material:

```text
GENERATION FAILED
```

Either error leaves the full Scene and revision state as they were before the action. A failed double-tap also rolls back the provisional first-tap cell generation.

### Determinism

For the same explicit seed, byte-identical initial Scene, row, page, mode, and action:

* generated pattern bytes are identical;
* destination allocation is identical;
* generating Synth A does not advance Synth B or Drums RNG state;
* running A → B → Drums or Drums → B → A produces the same per-track results.

## 7. Serial expectations

Song generation does not require a new continuous Serial stream. Existing boot, Scene, audio, and `[PERF]` diagnostics remain authoritative.

Expected observations:

* no reboot or exception during `G` or double-tap `G`;
* no new TinyUSB, SMF scheduler, or MIDI ownership messages caused by Song generation;
* `[PERF] underruns` does not continuously increase during ordinary generation/playback;
* Save/Load logs complete normally after generated material is persisted;
* no repeated allocation failure messages after returning to a page with a genuinely free slot.

## 8. Troubleshooting

### `NO EMPTY PATTERN SLOTS`

The current page has no slot that is both strictly empty and unreferenced by Song A or Song B for the selected track. Clear an unused pattern or switch pattern page/bank. The error is non-destructive.

### A reference appears but playback is silent

This is a failure. Confirm that the build contains `SongPatternMaterializer`, then inspect the destination pattern directly in Pattern/Drum pages. `G` must never report success for a strictly empty destination.

### Double-tap changes only one track

Confirm the second press occurs within the current 300 ms gesture window. On success the toast must read `GENERATED ROW N`. A row failure must restore the first-tap result and dirty revision to the pre-gesture state.

### Dirty `*` remains after Save

Confirm Save completed successfully. Successful generation adds one mutation; successful Save sets the persisted revision to the current revision and clears `*`.

### Generated material disappears after reboot

Confirm the Scene was saved after generation and the same Scene was loaded after reboot. UI session persistence and Scene persistence are separate stores.

### Audio or MIDI regression

Stop playback and check for stuck notes. Re-run the full host suite, both Cardputer builds, fixed-DRAM gate, and the MIDI Player/HUB navigation checks. Song materialization must not modify transport or MIDI owners.

## 9. Acceptance checklist

### Hardware

- [ ] Open Song Page.
- [ ] Select an empty Synth A cell.
- [ ] Press `G`.
- [ ] A new pattern reference appears in the cell.
- [ ] During playback Synth A contains real audible material.
- [ ] Repeat for Synth B and Drums.
- [ ] Previously assigned patterns remain unchanged.
- [ ] Double-tap `G` creates audible material for the entire current row.
- [ ] Fill all available destinations and verify `NO EMPTY PATTERN SLOTS`.
- [ ] After the error, Song references and pattern banks contain no partial changes.
- [ ] After successful generation, status chrome shows dirty `*`.
- [ ] After Save, dirty `*` clears.
- [ ] After reboot/load, generated patterns and Song references remain present.
- [ ] No stuck notes.
- [ ] No audio hangs.
- [ ] No continuous growth of `[PERF] underruns`.
- [ ] Song A and Song B are not corrupted.
- [ ] MIDI Player and HUB MIDI behavior is unchanged.

### Automated and build

- [ ] `python3 tests/test_generation_rng_source_regressions.py`
- [ ] `python3 tests/test_song_playhead_source_regressions.py`
- [ ] `bash tests/run_host_tests.sh`
- [ ] `cd platform_sdl && make clean all CXX=g++`
- [ ] `bash scripts/build.sh --warnings all`
- [ ] `bash scripts/build_seqtrak_midi_only.sh --warnings all`
- [ ] fixed-DRAM gate passes for normal and MIDI-only ELF files.
