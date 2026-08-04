# Cardputer fixed DRAM Scene storage

## Purpose

Keep the active project `Scene` and shared transaction scratch `Scene` out of the ESP32-S3 fixed internal DRAM budget without weakening the `122880 B` gate or changing the established Cardputer audio-memory profile.

## Why external BSS was rejected

The pinned M5Stack Arduino profile accepts `EXT_RAM_BSS_ATTR`, but its linker configuration does not enable external `.bss` placement. ELF diagnostics showed both 25,800-byte `Scene` objects still in `.dram0.bss`. The implementation therefore does not rely on that attribute.

## Design

- Cardputer setup enables PSRAM and allocates one `SceneStorageBlock` with `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`.
- The block owns the active scene and the transaction scratch scene.
- Allocation occurs once, immediately after `M5Cardputer.begin()`, before I2S, tasks, SD, SMF, USB MIDI, engine initialization, or UI allocation.
- Failure is fatal and visible through the boot-stage diagnostic; there is no fallback into internal RAM.
- Host and SDL builds keep deterministic static storage.
- Cardputer continues to use its constrained looper, delay, and sample-pool profile; PSRAM enablement in this stage is not permission for unrelated implicit large-buffer growth.
- MIDI, SMF, audio queues, scene codec, paging format, and transaction semantics are unchanged.

The two scenes occupy approximately 51.6 KB. Replacing them with one fixed pointer in internal BSS should return the real ELF below the existing gate; the measured ELF remains the source of truth.

## Validation

```bash
./tests/run_host_tests.sh
./scripts/build.sh --warnings all
./scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
./scripts/build_seqtrak_midi_only.sh --warnings all
./scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-seqtrak-midi-only/GroovePuter.ino.elf
```

## Hardware acceptance

- Boot normal Cardputer ADV and confirm the Scene PSRAM boot stage completes.
- Load, save, and reload a project.
- Switch pattern pages and verify save/load rollback.
- Exercise playback while changing patterns and song position; confirm no audio underrun regression.
- Boot the SEQTRAK MIDI-only profile and repeat project load plus MIDI playback.
- Confirm no reset, allocation failure, corrupted scene, or stuck MIDI note.
