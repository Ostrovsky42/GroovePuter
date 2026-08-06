# Genre Variant Correctness

## Purpose

Make the GENRE page truthful and bounded without changing the accepted Acid, Rave, Techno, or Synthwave generation algorithms.

This stage:

- shows only variants owned by the selected genre family;
- keeps `BASE` as procedural generation without Atlas tables;
- exposes Atlas `P1 / P2 / P3` roles for manual materialization;
- renames the visible `Minimal` profile to `Synthwave` while preserving its persisted enum and sound;
- renames the Atlas preview `Deep Chord` to `Deep Stab`;
- permits sparse Dub/Trip-Hop Synth B material, including an empty `Minimal Space P3` lane on the explicit GENRE/GENERATION materialization paths;
- does not apply FEEL or TEXTURE automatically.

The direct Song-cell `G` path retains its existing rule that every requested track must contain material. `SynthPattern` currently has no persisted metadata bit that can distinguish an intentional empty phrase from generator failure. This stage does not encode that distinction in an unused step field or weaken the rollback gate globally.

## Hardware list

- M5Stack Cardputer or Cardputer ADV.
- USB-C data cable.
- Headphones or the built-in speaker.
- Optional Yamaha SEQTRAK or another class-compliant USB-MIDI target for external listening.

## Wiring

No external wiring is required.

For USB-MIDI monitoring, connect the Cardputer USB data port to the powered USB host or MIDI target using the same working connection used by the existing GroovePuter MIDI tests.

## Build and flash

```bash
git fetch origin
git switch agent/genre-variant-correctness
git reset --hard origin/agent/genre-variant-correctness
rm -rf build .pio .pioenvs .piolibdeps
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
```

Flash with the repository's normal Cardputer ADV upload command, then open the serial monitor at `115200` baud.

## Expected behavior

1. Open `GENERATE -> GENRE`.
2. The former visible `Minimal` name is now `Synthwave`.
3. Changing GENRE resets VARIANT to `BASE`.
4. VARIANT only cycles through compatible entries:
   - Acid: `BASE`, `Chicago Jack`, `Rolling Acid`;
   - Synthwave: `BASE`;
   - Techno: `BASE`;
   - Rave: `BASE`, `Psytrance`;
   - Dub / Reggae: `BASE`, `Dub Techno`, `Deep Stab`, `Minimal Space`;
   - Breaks: `BASE`, `UK Garage`, `Drum&Bass`, `Footwork`, `Classic 2-Step`, `Dark Skippy`.
5. Atlas variants expose `ROLE P1/P2/P3` and their function label.
6. `PROFILE ONLY` changes the selected profile but preserves current pattern contents.
7. `MATERIALIZE` writes only the current Synth A, Synth B, and Drum patterns.
8. `MATERIALIZE+BPM` performs the same write and applies the profile or Atlas BPM.
9. Atlas variants show `MORPH N/A (TABLE)`; procedural BASE variants retain morph behavior.
10. `Minimal Space P3` may produce an empty Synth B pattern through GENRE materialization. This is valid musical space, not a generation failure.
11. GENERATION-to-Song applies the same sparse repair before committing its generated row.
12. Direct Song-cell `G` still blocks a fully empty requested Synth B track rather than silently weakening its atomic materialization contract.

## Troubleshooting

### An old scene shows an incompatible active genre/variant

The page selects the compatible `BASE` view and shows the active state in the warning color. Press `Enter` to apply the compatible selection. The persisted recipe IDs are not renumbered.

### Atlas role does not change

Only Atlas variants have P1/P2/P3. `BASE` and legacy procedural recipes show `ROLE PROCEDURAL`.

### Dub or Trip-Hop sounds too empty

Check the selected role first. `Minimal Space P3` intentionally allows zero Synth B events on explicit genre/phrase materialization paths. Compare P1 and P2 before changing FEEL or TEXTURE.

### Song-cell G rejects an empty Synth B

This is the retained safety behavior. Use GENRE `MATERIALIZE` for the selected P3 role or the GENERATION page. A later Phrase metadata change must introduce an explicit intentional-empty flag before Song-cell generation can safely accept the same result.

### Acid, Rave, Techno, or Synthwave changed unexpectedly

Do not accept the stage. These procedural generator paths are intentionally untouched; capture the selected genre, variant, BPM, seed, and the first 16 steps for Synth A/B and drums.

### Build fails on a new header

Run the clean commands above and confirm the branch includes:

```text
src/dsp/genre_variant_catalog.h
src/dsp/genre_sparse_repair.h
src/dsp/genre_materializer.h
```

## Acceptance checklist

- [ ] Host tests pass.
- [ ] Cardputer ADV build passes without new warnings.
- [ ] Acid BASE sounds the same as `dev` for the same seed and controls.
- [ ] Rave BASE sounds the same as `dev` for the same seed and controls.
- [ ] Techno BASE sounds the same as `dev` for the same seed and controls.
- [ ] Synthwave BASE sounds the same as the previous visible Minimal profile.
- [ ] No incompatible Atlas variant appears under Acid, Synthwave, Techno, Rave, or Dub / Reggae.
- [ ] P1, P2, and P3 can each be materialized manually for an Atlas variant.
- [ ] `PROFILE ONLY` does not change pattern steps.
- [ ] `MATERIALIZE` changes the current pattern set only.
- [ ] `MATERIALIZE+BPM` applies the displayed BPM.
- [ ] `Deep Stab` is shown; `Deep Chord` is not shown to the user.
- [ ] Trip-Hop Synth B contains no more than three notes after genre materialization.
- [ ] `Minimal Space P3` permits an empty Synth B lane without a failure toast on the GENRE path.
- [ ] GENERATION-to-Song keeps Dub/Trip-Hop Synth B within the sparse contract.
- [ ] Direct Song-cell `G` still rejects a fully empty requested track.
- [ ] Serial output contains no crash, allocation failure, or generation retry loop.
