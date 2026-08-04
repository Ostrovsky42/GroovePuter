# Stage 1B — SMF Structural Inspector

## Purpose

Add a bounded, measurement-first structural view to MIDI Player. The inspector explains the first nine audible physical SMF tracks with grid, note density, active density, proven loop periodicity, swing, register, motion, form and register overlap. Genre remains a secondary `RESEMBLES` hint.

Analysis is accumulated during the existing full SMF load/index pass. It does not reopen the file, add a second SD traversal, alter the scheduler, or change note ownership.

SMF playback is bounded to the first 32 physical tracks. The indexer still validates and skips through every declared `MTrk` chunk. Files with more than 32 tracks load and play the first 32, while the mute table shows an explicit warning such as `TRACKS 41 / 32 PLAYED`.

Physical track identity, channel and first Program Change are retained for those 32 tracks. Arbitrary TrackName strings are not kept in fixed internal DRAM; the table derives a compact GM label such as `Grand Piano`, `Finger Bass`, `Lead` or a program-family fallback. A track without Program Change uses its physical track number.

## Hardware list

- M5Stack Cardputer-Adv, ESP32-S3.
- microSD card with Standard MIDI Files Type 0 or Type 1.
- Optional Yamaha SEQTRAK or another USB MIDI target.
- USB-C data cable for external MIDI playback.

## Wiring

No GPIO wiring is required.

- Insert the microSD card into Cardputer-Adv.
- Connect Cardputer-Adv USB-C to the MIDI target or host when required.
- PORT.A I2C, audio wiring and the existing single TinyUSB writer are unchanged.

## Build / Flash

```bash
git fetch origin
git checkout agent/smf-structural-inspector
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

The normal Cardputer-Adv profile is:

```text
m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc
```

The normal and SEQTRAK MIDI-only builds both use the same fixed-DRAM ceiling: `122880` bytes for `.dram0.data + .dram0.bss`.

Flash the generated firmware with the repository's normal M5Launcher or serial workflow.

## Controls

- `1–9`: toggle the first nine audible physical SMF tracks.
- `U`: open the mute table.
- Arrows + `Enter`: select and toggle a mute-table row.
- `A`: restore all loaded SMF tracks.
- `S`: open or close the structural inspector.
- `Up/Down` in the inspector: select an analyzed layer.
- `I`: channel inspector.
- `D`: performance diagnostics.
- `B`: return to files/player.

Known hardware limitation: physical Cardputer `1–9` has not passed hardware acceptance. `U` + arrows + `Enter` remains the validated mute path. `K` is not a MIDI mute command.

## Metrics

- `GRID`: smallest 1/8, 1/16 or 1/32 grid explaining at least 75% of attacks. A delayed offbeat in the swing window still counts toward its candidate grid.
- `SWING`: delayed offbeat phase against the inferred grid. Straight material reports 50%; triplet-like 1/16 swing reports about 66%.
- `NOTES/B`: Note On attacks per bar in the layer's own first-to-last active span.
- `ACTIVE`: percentage of that same span where at least one note is held.
- `LOOP`: only a proven 1-, 2- or 4-bar repetition. Unproven periodicity displays `--`.
- `MOTION`: change in timing/pitch signatures. Velocity changes alone do not alter it.
- `NOTE REGISTER`: MIDI note range, not an audio-frequency claim.
- `FORM`: four 16-bar activity bins covering the first 64 file bars.
- `OVERLAP`: note-register overlap with inferred chords/pad and lead layers.
- `RESEMBLES`: low-priority interpretation derived from measured values.

Only the first 64 bars are analyzed. Longer files show `PARTIAL 64` and continue to play normally.

## Expected behavior

1. A normal file with 32 or fewer physical tracks loads without a track-limit notice.
2. Tempo/conductor tracks do not consume audible hotkey positions.
3. `U` shows hotkey position and physical SMF track number separately.
4. Program Change produces a bounded GM instrument/family label.
5. Muting with `Enter` leaves other tracks playing and uses the existing ownership-safe Note Off cleanup.
6. `S` shows `GRID`, `LOOP`, `SWING`, `MOTION`, `NOTES/B`, `ACTIVE`, `NOTE REGISTER`, `FORM`, `OVERLAP` and `RESEMBLES`.
7. Straight 1/16 material reports about `SWING 50%`.
8. Swinged 1/16 material near 66% remains `GRID 1/16` and reports about `SWING 65–67%`.
9. A file with 41 physical tracks loads, retains the first 32 and shows `TRACKS 41 / 32 PLAYED` in the `U` table.
10. A file longer than 64 bars shows `PARTIAL 64` without a second load pass.

## Troubleshooting

- `NO STRUCTURAL DATA`: wait for the existing load/index pass to finish.
- `MIDI SLOT N EMPTY`: fewer than N audible loaded tracks were found.
- `TRACKS N / 32 PLAYED`: the file declared more than 32 physical tracks. The first 32 are loaded; later tracks are validated but intentionally not streamed.
- A custom TrackName is absent: labels are derived from Program Change to avoid fixed global storage for arbitrary strings.
- Physical `1–9` does nothing: use `U`, arrows and `Enter` and record the hardware-input result separately.
- Wrong role suggestion: validate register, polyphony, density and activity; role remains heuristic.
- External target is silent: verify USB data cable, target channels and `RAW`/`SEQTRAK` routing.
- DRAM gate says `Firmware ELF not found`: use `build/cardputer-adv-current/GroovePuter.ino.elf`, not the obsolete `build/cardputer_adv` path.

## Acceptance checklist

- [ ] `bash tests/run_host_tests.sh` passes.
- [ ] SDL desktop build passes.
- [ ] Cardputer-Adv build passes with `bash scripts/build.sh --warnings all`.
- [ ] CI checks `build/cardputer-adv-current/GroovePuter.ino.elf`.
- [ ] Normal and MIDI-only profiles share the `122880` fixed-DRAM ceiling.
- [ ] Straight 1/16 reports `SWING 50%`.
- [ ] Swinged 1/16 at 66% reports `GRID 1/16` and `SWING 65–67%`.
- [ ] Random free timing remains `GRID FREE`.
- [ ] Velocity changes do not alter `LOOP` or `MOTION`.
- [ ] Late-entering layers use their own `NOTES/B` and `ACTIVE` span.
- [ ] High-PPQN held notes do not overflow `ACTIVE`.
- [ ] Unproven periodicity displays `LOOP --`.
- [ ] `SmfTrackInspectorState` remains at or below 136 bytes of fixed storage.
- [ ] A 41-track host fixture indexes successfully and retains exactly 32 tracks.
- [ ] A 33+ track hardware file shows `TRACKS N / 32 PLAYED`.
- [ ] The shared 4096-byte SMF cache pool is unchanged.
- [ ] No second file open, index pass, scheduler path or TinyUSB writer is introduced.
- [ ] `U` + arrows + `Enter` works without stuck notes.
- [ ] Physical `1–9` is reported separately and does not invalidate the `U` path.
