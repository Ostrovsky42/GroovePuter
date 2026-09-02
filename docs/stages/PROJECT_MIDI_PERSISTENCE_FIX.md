# Project MIDI Import and Persistence Fix

## Purpose

Validate the Project page workflow after fixing blank project creation, scene/song persistence, MIDI import routing, and MIDI browser navigation.

## Hardware

- M5Stack Cardputer ADV
- microSD card formatted as FAT32
- USB-C cable for flashing and serial monitor
- one or more Standard MIDI Files (`.mid`)

## Wiring

No external wiring is required. Insert the microSD card before boot. Copy MIDI files to `/midi` or any subdirectory below `/midi`.

## Build and flash

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
./scripts/flash.sh
./scripts/monitor.sh
```

## Expected behavior

1. **Project → New** creates a blank project. No Mario melody, demo drums, or eight-row demo song is inserted.
2. **Project → Save As** creates `/scenes/<name>.json` and persists both patterns and both song slots.
3. MIDI auto-routing treats channel 10 as GM drums. A melodic single-channel file without channel 10 routes to Synth A unless its track name explicitly identifies percussion.
4. Import populates only tracks selected in the MIDI Matrix and saves the updated project after a successful import.
5. In `/midi/<subdirectory>`, `Esc` moves to the parent directory. At `/midi`, `Esc` closes the browser. In MIDI Matrix, `Esc` returns to the browser.

## Troubleshooting

- `Project save failed`: verify the SD card is mounted, writable, and has free space.
- `No compatible routed notes`: verify at least one MIDI channel is assigned to A, B, or D and that drum notes use supported GM mappings.
- `Read or save error`: inspect serial output for page save/restore or malformed MIDI diagnostics.
- A saved name does not appear in Load: verify that `/scenes/<name>.json` exists; names without a backing scene file are intentionally hidden.

## Acceptance checklist

- [ ] New project opens with empty synth patterns, empty drum patterns, and an empty first song row.
- [ ] Save As reports `Project and songs saved`.
- [ ] Reboot and Load restore edited patterns and song rows.
- [ ] Single-channel melodic MIDI does not auto-route to Drums.
- [ ] A-only import does not populate Synth B or Drums song lanes.
- [ ] Imported song rows remain after reopening the project.
- [ ] `Esc` moves up one MIDI directory and closes only at `/midi`.
- [ ] Serial shows `MIDI import result=0 saved=1` for a successful persisted import.
