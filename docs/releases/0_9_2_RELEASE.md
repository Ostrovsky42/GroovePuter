# GroovePuter 0.9.2 — Release Record

Status: **RELEASE READY**

## Runtime freeze

- Branch: `dev_0.9.2`
- Final runtime PR: `#262`
- Hardware-tested PR head: `c3a06347bf6b07d19b038d678661c1e28c4c8e82`
- Runtime merge: `3b1187d74a1456ffc52ac9e940ba6dcfcfa0271b`
- Production tree: `a99d9303924dcd9ab79be8f95146ed9de8869265`

The runtime merge and the hardware-tested PR head point to the same tree. No untested production content was added by the merge.

## Release scope

0.9.2 is a hardening release, not a feature expansion. Its release-critical change is the Song generated-pattern lifecycle fix:

- uniquely owned generated cells reroll in place;
- unreferenced generated orphans are reclaimable;
- shared generated material remains copy-on-write;
- manual/imported non-empty patterns remain protected;
- Song deletion stays logical so immediate Undo is preserved;
- genuine resident-page exhaustion uses the existing explicit pattern-page navigation rather than unsafe SD scanning in a realtime path.

The canonical 0.9.2 key map also documents Song cell clearing and resident-page recovery.

## Automated acceptance

On `c3a06347bf6b07d19b038d678661c1e28c4c8e82` all 12 release workflows completed SUCCESS, including:

- Core regressions;
- Cardputer ADV normal build;
- fixed DRAM gate;
- Cardputer ADV SEQTRAK MIDI-only build;
- SDL build;
- Synth persistence;
- Tonal Projector;
- Generation Stage 15B / 15C;
- tonal scale, baseline, materializer, integration, register sweep and final acceptance.

The Core host suite explicitly executes `test_song_pattern_materializer`, which passed.

## Hardware acceptance

Cardputer ADV acceptance is complete on the hardware-tested PR head:

- repeated reroll of one generated Song cell does not consume a new slot each time;
- cleared generated cells yield reusable generated capacity;
- shared generated patterns preserve copy-on-write behavior;
- manual patterns are not overwritten;
- pattern-page switching continues generation on another page;
- playback across page boundaries works;
- `Save -> reboot -> Load` preserves the lifecycle behavior.

## Hardware assumptions

- M5Stack Cardputer ADV / ESP32-S3.
- PSRAM disabled in the normal release profile.
- Existing SD-card project/pattern storage.
- No new GPIO, I2C, SPI or voltage requirement.
- Yamaha SEQTRAK remains optional.

## Build / flash

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

## Expected behavior

The existing 0.9.1 workflow remains intact. 0.9.2 specifically removes the need to clear the whole project merely because Song-generated orphan patterns accumulated in the resident page.

If a resident page is genuinely full of referenced or manual/imported material, use `Alt+[` / `Alt+]` to change pattern page.

## Troubleshooting

A release correctness defect includes any of the following:

- repeated reroll still consumes fresh slots indefinitely;
- Song generation overwrites non-empty manual/imported material;
- a shared generated source mutates another Song reference unexpectedly;
- `Save -> reboot -> Load` breaks the accepted Song lifecycle;
- page switching breaks playback or global pattern identity;
- Cardputer ADV normal/MIDI-only build or DRAM gate regresses.

## Known deferred

Not part of 0.9.2:

- sampler recovery;
- Harmony Atlas expansion;
- broader Phrase Arranger work;
- automatic cross-page Song allocation requiring new storage-I/O behavior;
- BLE MIDI;
- broad DSP redesign or oversampling/wavetable experiments;
- cosmetic cleanup unrelated to release correctness.

## Publication

After the docs-only release closure is merged, create tag `v0.9.2` on that final `dev_0.9.2` commit and publish GitHub Release **GroovePuter v0.9.2** using this file as the release-note source.

No production-code change is allowed between the accepted runtime freeze and the tag. If production code changes, acceptance must be reopened.
