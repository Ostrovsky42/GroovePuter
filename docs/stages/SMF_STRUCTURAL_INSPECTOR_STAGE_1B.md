# Stage 1B — SMF Structural Inspector

## Purpose

Add a bounded, measurement-first structural view to MIDI Player. The inspector explains the first nine audible physical SMF tracks with grid, note density, active density, proven loop periodicity, swing, register, motion, form and register overlap. Genre remains a small `RESEMBLES` hint, never the primary result.

The analysis is accumulated during the existing full SMF load/index pass. It does not open the file again, add a second SD traversal, alter the scheduler, or change note ownership.

## Hardware

- M5Stack Cardputer-Adv (ESP32-S3).
- microSD card containing a Standard MIDI File Type 0 or Type 1.
- Optional Yamaha SEQTRAK or another USB MIDI target.
- USB-C data cable when using an external MIDI target.

## Wiring

No GPIO wiring is required for this test.

- Insert the microSD card into Cardputer-Adv.
- Connect Cardputer-Adv USB-C to the MIDI target/host when external playback is required.
- The PR does not change PORT.A I2C, audio wiring, or the single TinyUSB writer established by PR #35.

## Build / Flash

```bash
git fetch origin
git checkout agent/smf-structural-inspector
./tests/run_host_tests.sh
./scripts/build_cardputer_adv.sh
./scripts/check_cardputer_dram_budget.sh build/cardputer_adv
```

Flash the generated Cardputer-Adv firmware using the repository's normal M5Launcher or serial workflow.

## Controls

- `1–9`: toggle the first nine audible physical SMF tracks.
- `U`: open the working mute table; use arrows and `Enter` to toggle the selected track.
- `S`: open/close the structural inspector.
- `Up/Down` in the structural inspector: select an analyzed audible track.
- `I`: channel inspector.
- `D`: performance diagnostics.
- `B`: return to files/player.

Known hardware limitation: the physical Cardputer `1–9` path is not yet accepted on hardware. The hotkey mapping and event ownership are retained for diagnosis, while `U` + arrows + `Enter` remains the validated mute path. `K` is not a MIDI mute command.

## Metrics

- `GRID`: smallest 1/8, 1/16 or 1/32 grid explaining at least 75% of attacks.
- `NOTES/B`: Note On attacks per bar in the layer's own first-to-last active bar span.
- `ACTIVE`: percentage of that same layer span where at least one note is held. This is intentionally separate from note density.
- `SWING`: delayed offbeat phase measured against the inferred grid; straight material reports 50%.
- `LOOP`: only a proven 1-, 2- or 4-bar repetition in the first eight layer-relative bar signatures. Unproven periodicity displays `--`.
- `MOTION`: change in timing/pitch bar signatures. Velocity changes alone do not alter it.
- `NOTE REGISTER`: MIDI note range only; it is not a frequency/spectral claim about the SEQTRAK patch.
- `FORM`: four 16-bar activity bins covering the first 64 file bars.
- `OVERLAP`: note-register overlap with the first inferred chords/pad and lead layers.
- `RESEMBLES`: low-priority interpretation derived from measurable values.

Only the first 64 bars are analyzed. Longer files show `PARTIAL 64` and continue to play normally.

## Expected behavior

1. Load an SMF containing conductor/meta tracks and at least seven musical tracks.
2. Tempo/conductor tracks do not consume hotkey positions.
3. Open `U`: rows show hotkey position and physical SMF track number separately.
4. Toggle a layer with `Enter`. Playback of other layers continues and the existing ownership-safe cleanup remains in charge of Note Off generation.
5. Press `S`: the selected layer shows `GRID`, `LOOP`, `SWING`, `MOTION`, `NOTES/B`, `ACTIVE`, `NOTE REGISTER`, `FORM`, `OVERLAP` and `RESEMBLES`.
6. A sustained pad can show low `NOTES/B` and high `ACTIVE`; this is expected.
7. A loop without enough repeated evidence shows `LOOP --` rather than an invented length.
8. Files longer than 64 bars show `PARTIAL 64` without a second load delay.

## Troubleshooting

- `NO STRUCTURAL DATA`: wait for load completion; the snapshot is published only after the existing full load/index pass finishes.
- `MIDI SLOT N EMPTY`: fewer than N audible physical tracks were found; conductor tracks are intentionally excluded.
- Physical `1–9` does nothing on Cardputer: use `U`, arrows and `Enter`; this remains a documented open hardware-input issue.
- Wrong role suggestion: role is heuristic. Validate register, polyphony, density and activity rather than treating the label as truth.
- `ACTIVE` looks approximate: SMF tracks can contain unmatched or unusual Note Off sequences. The value is a bounded note-on/off balance estimate, not audio analysis.
- External target is silent: confirm USB data cable, target MIDI channels and `RAW`/`SEQTRAK` routing.

## Acceptance checklist

- [ ] `./tests/run_host_tests.sh` passes.
- [ ] Structural analyzer host tests pass with `-Wall -Wextra -Werror`.
- [ ] Straight 1/16 reports `SWING 50%`.
- [ ] Humanized velocity does not change `LOOP` or `MOTION` when timing and pitch are unchanged.
- [ ] Late-entering layers use their own span for `NOTES/B` and `ACTIVE`.
- [ ] High-PPQN held notes do not overflow `ACTIVE`.
- [ ] Unproven periodicity displays `LOOP --`.
- [ ] Source regression proves analysis is observed in `SmfEventStreamMerger` and finalized at the end of the existing first pass.
- [ ] Track metadata freezes after that pass and is not recomputed during playback.
- [ ] No second `SmfFileIndexer::build()` or second file open is introduced.
- [ ] Conductor/meta-only tracks do not become structural layers.
- [ ] First nine audible layers retain physical `trackIndex` identity.
- [ ] Long files are capped at 64 bars and marked partial.
- [ ] `NOTE REGISTER` wording is used; no frequency-band claim is shown.
- [ ] `RESEMBLES` remains secondary.
- [ ] `U` + arrows + `Enter` works and ownership-safe cleanup remains unchanged.
- [ ] Cardputer-Adv ELF passes `check_cardputer_dram_budget.sh` in CI.
- [ ] No changes to Song, project codecs, Pattern/PERFORM ownership, remote CC23/CC24 mute, scheduler or TinyUSB writer.
