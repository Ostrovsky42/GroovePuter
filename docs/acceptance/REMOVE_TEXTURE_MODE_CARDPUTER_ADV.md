# TextureMode removal — Cardputer ADV acceptance

## Purpose
Confirm the runtime TextureMode migration on physical Cardputer ADV without changing musical generation behavior.

## Hardware
- M5Stack Cardputer ADV (ESP32-S3FN8)
- USB cable for flash + serial
- Optional headphones / SEQTRAK for A/B listening

## Wiring
No external wiring is required. Use the normal Cardputer ADV USB connection.

## Build / flash
```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/upload.sh /dev/ttyACM0
```

## Expected behavior
- GENERATE exposes exactly GENRE, FEEL, GENERATION.
- Historical page id 8 opens FEEL and never appears in normal navigation.
- Loading an older Scene with TEXTURE metadata succeeds.
- Existing synth/Tape/delay/distortion settings are not overwritten by a texture preset.
- Saving the migrated Scene does not write legacy texture metadata.

## Troubleshooting
- If an old project sounds different, compare its persisted synth/Tape/delay/distortion values before changing any generator settings.
- If page 8 is visible in normal navigation, the PR is not acceptable.
- If load fails on an unknown legacy texture value, capture the Scene JSON and serial log.

## Acceptance checklist
- [ ] Boot succeeds with no reset loop.
- [ ] GENRE -> FEEL -> GENERATION navigation works in both directions.
- [ ] Legacy page id 8 redirects to FEEL.
- [ ] Old Scene loads without error.
- [ ] Old Scene sounds materially the same in an A/B check.
- [ ] Save/reload keeps concrete synth/FX values.
- [ ] Serial shows no Scene parse/load errors.
- [ ] Normal and SEQTRAK MIDI-only firmware smoke tests pass.
