# Cardputer fixed DRAM Scene storage

## Purpose

Restore the mandatory `122880 B` fixed internal-DRAM gate on the real Cardputer ADV without assuming external PSRAM or hiding the failure by raising the limit.

## Hardware constraint

Cardputer ADV has no usable external PSRAM. `PSRAM=disabled` remains the build profile for both the normal CDC+MIDI image and the SEQTRAK MIDI-only image.

## Design

- Fixed `.dram0.bss` retains only the active `Scene` pointer.
- Setup allocates the active `Scene` once from byte-addressable internal heap immediately after `M5Cardputer.begin()` and before I2S, tasks, SD, SMF, USB MIDI, engine initialization, or UI allocation.
- Scene parsing and pattern-page validation use `SceneScratchLease`: one temporary internal-heap `Scene` that is released automatically on success and every error return.
- No allocation occurs in the audio callback, MIDI dispatcher, SMF scheduler, or other realtime path.
- Failure to allocate the active Scene is fatal and is shown directly on the display, including in the MIDI-only profile where CDC output is absent.
- Host and SDL retain deterministic static active Scene storage and bounded transaction allocation.
- Scene codec, page format, and commit-after-validation semantics are unchanged.

This removes one permanent 25,800-byte transaction Scene from the runtime footprint and both 25,800-byte Scene objects from fixed `.dram0.bss`. Peak memory during an explicit load transaction remains bounded and temporary.

## Validation

```bash
./tests/run_host_tests.sh
./scripts/build.sh --warnings all
./scripts/check_cardputer_dram_budget.sh   build/cardputer-adv-current/GroovePuter.ino.elf
./scripts/build_seqtrak_midi_only.sh --warnings all
./scripts/check_cardputer_dram_budget.sh   build/cardputer-adv-seqtrak-midi-only/GroovePuter.ino.elf
```

## Hardware acceptance

- Boot normal Cardputer ADV and confirm the UI appears.
- Load, save, and reload a project.
- Switch pattern pages and verify save/load rollback.
- Exercise playback while changing patterns and song position.
- Boot the SEQTRAK MIDI-only profile and repeat project load plus MIDI playback.
- Confirm no reset, allocation failure, corrupted scene, audio underrun regression, or stuck MIDI note.
