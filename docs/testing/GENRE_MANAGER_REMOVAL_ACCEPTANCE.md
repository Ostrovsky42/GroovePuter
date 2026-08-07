# GenreManager ownership removal — acceptance

## Purpose

Verify that genre settings are owned only by the persisted `Scene`, while genre names, mappings, recipe compilation and behavior lookup remain stateless. The test must not change existing genre output, deterministic seeds, scene compatibility or texture behavior.

## Hardware list

- M5Stack Cardputer-Adv
- USB-C data cable
- Headphones or powered speaker for the built-in audio output
- Optional: Yamaha SEQTRAK for MIDI-only regression checking

## Wiring

No external wiring is required.

For optional SEQTRAK validation, connect Cardputer-Adv USB-C to the existing USB-MIDI host/device setup used by the project. This PR does not change MIDI routing or the Cardputer-Adv PORT.A I2C bus.

## Build and flash

```bash
git fetch origin
git switch agent/remove-genre-manager
git pull --ff-only
bash tests/run_host_tests.sh
bash scripts/build_sdl.sh
bash scripts/build.sh --warnings all
bash scripts/build_seqtrak_midi_only.sh
```

Flash the Cardputer-Adv build with the normal project upload command documented in `README.md`.

## Expected behavior

### Screen

- GENERATE contains exactly `GENRE`, `FEEL`, and `GENERATION`.
- GENRE still shows the same genre and variant names.
- Changing GENRE or VARIANT and pressing Enter updates the active selection.
- PROFILE ONLY does not rewrite pattern notes.
- MATERIALIZE regenerates through the existing production path.
- Historical page id `8` resolves to FEEL; no TEXTURE page opens.

### Audio

- An existing scene loaded before this PR sounds the same before any explicit materialization.
- Acid, Rave, Techno and Minimal retain their existing observable character.
- Repeated loads do not accumulate filter bias or delay changes.
- The same scene and generation seed produce the same generated pattern.

### Serial

- Boot completes without assertion, reset loop or scene migration error.
- Loading an existing scene does not report an invalid genre, recipe or morph value.
- No new audio underrun burst appears while opening GENRE or applying PROFILE ONLY.

## Troubleshooting

- **Old scene opens with the wrong genre:** inspect `Scene::genre.generativeMode`, `recipe`, `morphTarget` and `morphAmount`; do not add a second runtime copy.
- **Generated result changed:** compare the selected recipe, generation seed and `GenreCatalog::compiledGenerativeParams()` output against `dev_0.9`.
- **Sound changes after every load:** verify texture bias baseline tracking; the delta must be applied once, not accumulated.
- **GENRE creates two dirty revisions:** `GenrePage::applyCurrent()` must call `GroovePuterState::markSceneMutated()` exactly once.

## Acceptance checklist

- [ ] Host tests pass.
- [ ] Four-axis UI contract passes.
- [ ] SDL build passes.
- [ ] Cardputer-Adv build passes.
- [ ] Fixed DRAM budget passes.
- [ ] SEQTRAK MIDI-only build passes.
- [ ] Existing scene loads with unchanged genre, variant, morph and texture values.
- [ ] Existing scene sounds unchanged before explicit materialization.
- [ ] Deterministic generation is unchanged for a fixed scene and seed.
- [ ] GENRE PROFILE ONLY changes metadata only.
- [ ] GENRE MATERIALIZE uses the existing production generation path.
- [ ] No user-facing TEXTURE page is reachable.
