# Cardputer fixed DRAM Scene storage

## Purpose

Keep the active project Scene and the shared transaction scratch Scene out of the ESP32-S3 internal fixed DRAM budget without weakening the `122880 B` gate.

## Design

- Both `Scene` objects remain static and deterministic.
- Cardputer firmware places them in external PSRAM BSS with `EXT_RAM_BSS_ATTR`.
- Host and SDL builds retain ordinary static storage.
- Cardputer builds fail at compile time when the PSRAM profile is not enabled.
- No MIDI, SMF, audio queue, ownership, transport, persistence, or file-format behavior changes.
- No runtime heap allocation is introduced.

The two objects are each approximately 25.8 KB, so moving both removes approximately 51.6 KB from `.dram0.bss`.

## Validation

```bash
./tests/run_host_tests.sh
./scripts/build.sh --warnings all
./scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
./scripts/build_seqtrak_midi_only.sh --warnings all
```

Expected fixed internal DRAM remains at or below `122880 B` for both Cardputer profiles.

## Hardware acceptance

- Boot the normal Cardputer ADV profile.
- Load, save, and reload a project.
- Switch pattern pages and verify page save/load rollback behavior.
- Boot the SEQTRAK MIDI-only profile and repeat project load plus MIDI playback.
- Confirm no reset, allocation failure, corrupted scene, or stuck MIDI note.
