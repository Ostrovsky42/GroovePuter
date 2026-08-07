# Genre ownership removal — Cardputer ADV acceptance

## Purpose

Verify that persisted genre settings are owned only by `Scene`, that the old `GenreManager` API is gone, and that the GENERATE workflow exposes only `GENRE` and `FEEL`. Genre catalog data, recipes, deterministic generation and existing musical behavior must remain unchanged.

## Hardware

- M5Stack Cardputer ADV
- USB-C data cable
- Headphones or powered speaker using the standard ES8311 audio path
- Optional Yamaha SEQTRAK for MIDI-only regression checking

## Wiring

No external wiring is required. PORT.A is unused by this test. Do not change the Cardputer ADV PORT.A I2C pins: GPIO2 SDA / GPIO1 SCL.

## Build / Flash

```bash
git fetch origin
git switch agent/remove-genre-manager
git pull --ff-only
bash tests/run_host_tests.sh
bash scripts/build_sdl.sh
bash scripts/build.sh --warnings all
bash scripts/build_seqtrak_midi_only.sh
```

Flash the Cardputer ADV build with the normal upload procedure from `README.md`.

## Expected behavior

### Screen

- GENERATE contains exactly `GENRE` and `FEEL`.
- `[` / `]` within GENERATE cycles `GENRE ↔ FEEL`; there is no third `GENERATION` screen.
- Historical page id `8` (TEXTURE) resolves to FEEL.
- Historical page id `11` (GENERATION) also resolves to FEEL and is never exposed by normal navigation.
- GENRE keeps the same genre and variant names.
- Opening GENRE or FEEL does not mutate Scene revision or sound.
- Pattern/song materialization remains available through the existing Song/Phrase production paths and explicit GENRE apply modes; removing the standalone page does not remove generation code.

### Audio

- An existing Scene sounds unchanged before any explicit genre apply/materialization.
- Acid, Rave, Techno and current Minimal retain their baseline character.
- Loading a project restores its persisted synth TYPE and parameters without an automatic genre timbre projection over the patch.
- A fixed Scene and seed produce the same generated result as the accepted baseline.

### Serial

- Boot completes without assertion, reset loop or scene migration error.
- Loading an existing Scene does not report invalid genre/recipe/morph state.
- No watchdog, heap-growth or audio-underrun regression appears while navigating GENRE/FEEL.

## Troubleshooting

- **A third GENERATION screen still appears:** verify `WorkflowMode::Generate` and `SessionWorkflow::Generate` both report two pages and that page id 11 normalizes to FEEL.
- **An old session opens page 11 directly:** verify `normalizeLegacyUiPage()` handles both page 8 and page 11 before page creation/navigation.
- **Old Scene opens with the wrong genre:** inspect `Scene::genre.generativeMode`, `recipe`, `morphTarget` and `morphAmount`; do not recreate a second mutable genre owner.
- **Saved synth TYPE changes after Load:** normal Scene Load must not call genre timbre projection after restoring the saved patch.
- **Generated result changed:** compare recipe, seed and `GenreCatalog::compiledGenerativeParams()` against the accepted `dev_0.9` baseline.
- **GENRE creates two dirty revisions:** one explicit apply must call `GroovePuterState::markSceneMutated()` exactly once.

## Acceptance checklist

- [ ] Production sources contain no `class GenreManager`, `using GenreManager`, `genreManager()` or `genreManager_`.
- [ ] Host tests pass.
- [ ] Four-axis UI contract passes with exactly two GENERATE pages.
- [ ] Phrase Core tests pass.
- [ ] SDL build passes.
- [ ] Cardputer ADV build passes.
- [ ] Fixed DRAM budget passes.
- [ ] SEQTRAK MIDI-only build passes.
- [ ] GENERATE navigation is exactly GENRE ↔ FEEL.
- [ ] Legacy page id 8 resolves to FEEL.
- [ ] Legacy page id 11 resolves to FEEL.
- [ ] Existing Scene loads with unchanged genre/variant/morph values.
- [ ] Existing Scene sounds unchanged before explicit apply/materialization.
- [ ] Persisted synth TYPE and parameters survive Save/reboot/Load unchanged.
- [ ] Deterministic generation is unchanged for a fixed Scene and seed.
- [ ] Acid, Rave, Techno and current Minimal match the accepted baseline.
- [ ] No user-facing TEXTURE or GENERATION page is reachable.
