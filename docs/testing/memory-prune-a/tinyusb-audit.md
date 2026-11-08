# MEMORY-PRUNE-A: TinyUSB class archive audit

Status: **OPEN, zero demonstrated recovered bytes**. No firmware was flashed,
no installed SDK was changed, and no production source was edited. This isolated
experiment demonstrates why simply deleting unused class archive members cannot
produce valid firmware. A matching SDK/core rebuild remains necessary.

## Existing ELF evidence

Audited `build/long-melody-integrated-runtime/GroovePuter.ino.elf`, SHA256
`435e8d95b7d3b1ceec5086e654eb8b07e8f2dcb6c45619eddb6ea4ef691a25bb`.
This is the existing runtime-instrumented image, not a newly built product ELF.

| Class | Buffer symbol | Buffer bytes | Other class BSS | Total named BSS |
|---|---|---:|---:|---:|
| MSC | `_mscd_epbuf` | 4096 | `_mscd_itf`: 64 | 4160 |
| DFU mode | `_dfu_epbuf` | 4096 | `_dfu_ctx`: 20 | 4116 |
| NCM | `ncm_epbuf` | 6416 | `ncm_interface`: 56 | 6472 |
| Total | | 14608 | 140 | 14748 |

These are current allocations/potential candidates, not recoverable-byte claims.
Padding, changed core structures, and additional retained references require a
candidate ELF before any net static-memory saving can be stated.

The map's archive inclusion records (lines 1139–1148) attribute all three to
`libarduino_tinyusb.a(usbd.c.obj)` references (`mscd_init`, `dfu_moded_reset`,
`netd_init`). Allocated BSS records at lines 65426–65443 attribute each buffer to
`msc_device.c.obj`, `dfu_device.c.obj`, and `ncm_device.c.obj` respectively.
Thus absence of class descriptors in the sketch does not prevent linking these
drivers: the SDK dispatch object still refers to them.

Installed package: M5Stack ESP32 3.2.2; SDK
`m5stack/tools/esp32-arduino-libs/idf-release_v5.4-858a988d-v1/esp32s3`.
Its `qio_qspi/include/sdkconfig.h` enables MSC, DFU, DFU runtime, NCM, MIDI,
and CDC; MSC/DFU buffers are each configured as 4096. The installed
`include/arduino_tinyusb/include/tusb_config.h` maps `CFG_TUD_MSC`,
`CFG_TUD_DFU`, and `CFG_TUD_NCM` to those SDK configuration values.
`ARDUINO_USB_MSC_ON_BOOT`/`ARDUINO_USB_DFU_ON_BOOT` govern Arduino startup
behavior; they do not recompile the already-built TinyUSB archive. Sketch C/C++
extra flags likewise cannot change definitions inside that archive. Overriding
header macros alone risks a mixed-config core and is not a valid removal proof.

## Reproducible isolated experiment

Run from the repository root with a fresh output directory:

```sh
python3 scripts/experiments/audit_tinyusb_archive.py --elf build/long-melody-integrated-runtime/GroovePuter.ino.elf --output build/memory-prune-a-tinyusb-archive-audit
```

Executed 2026-10-23: exit 0. The script copies the installed archive, removes
only MSC/DFU-mode/NCM objects from that copy, checks their absence, verifies
CDC/MIDI/DFU-runtime/dispatch members are byte-identical, and computes unresolved
class dispatch references against remaining archive definitions. It also reads
the six BSS symbols from the existing ELF and verifies the installed archive's
hash remains unchanged. Artifacts live under the output directory:
`candidate-no-msc-dfu-ncm.a`, `usbd.c.obj`, and `report.json`.

Installed archive SHA256 before/after:
`b417f095c68bf92eaf69ae664a8a8e8bd708f64a62e51b3bb4e50a4df2977186`.
Candidate archive SHA256:
`ba841271c88eef61f12b535407822ed5ac32ada04d9391139cff998ebfb4a643`.
Sixteen unresolved dispatch references remain: five `mscd_*`, five
`dfu_moded_*`, and six `netd_*`. This is a symbol closure check, not a full
firmware linker invocation. The candidate is intentionally **not usable firmware**.
No stubs or binary edits hide the missing driver references.

Additional inspection commands executed:

```sh
rg -n -B2 -A2 'ncm_epbuf|_mscd_epbuf|_dfu_epbuf|_transfer_buf' build/long-melody-integrated-runtime/*.map
~/.arduino15/packages/m5stack/tools/esp-x32/2411/bin/xtensa-esp32s3-elf-nm -S --size-sort build/long-melody-integrated-runtime/GroovePuter.ino.elf
rg 'CONFIG_TINYUSB_(MSC|DFU|NCM|CDC|MIDI)' ~/.arduino15/packages/m5stack/tools/esp32-arduino-libs/idf-release_v5.4-858a988d-v1/esp32s3/qio_qspi/include/sdkconfig.h
```

## Required next experiment and recovery constraints

Matching TinyUSB implementation sources (`usbd.c`, class implementation files)
are absent from the installed package. A genuine class-disable build must obtain
the exact SDK's pinned sources/build configuration and rebuild TinyUSB dispatch
and affected Arduino core code in a separate SDK/package tree. Disable
MSC, DFU **mode**, and NCM there; retain CDC, MIDI, DFU runtime, USB controller
and USB persistence/restart support. Build both control and candidate from the
same source snapshot/configuration and compare their ELFs, maps and hashes.

Removing DFU mode must not silently be equated with preserving firmware recovery.
The Arduino core contains separate bootloader/DFU restart and USB persistence
paths. The isolated archive experiment preserves the runtime-DFU member, but does
not prove recovery works. Required hardware checks remain OPEN: CDC enumeration,
CDC upload/restart, MIDI enumeration and traffic, normal boot, and the supported
bootloader/manual recovery procedure. No candidate is approved for flashing or
production use until those checks and an exact candidate ELF comparison pass.
