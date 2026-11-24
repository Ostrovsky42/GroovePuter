# Build manifest and reproduction

Research source `e3f16d57898182a8b68946ac7828d487b0973c5b`, clean at start. CI parent `8bfc5b8143fe816d2b42972d625206f277dc4a45`; the only delta is research README. Parent stock CDC and MIDI-only identities and result log links are in BASELINE.md. The ESP32 build has not been rerun locally. The bundled CLI is 1.4.0, `rsync` is missing, Arduino package directory has no M5Stack platform or ESP32 toolchain, and `scripts/install_arduino_deps.sh` failed DNS/network access to downloads.arduino.cc and static-cdn.m5stack.com. CI artifact download was attempted via connector; its temporary download URL returned HTTP 503. No ELF or MAP available locally. Preserve original CI artifact checksums rather than inventing local ones:

- CDC stock product artifact 14490608938, SHA-256 `6313c564d7a28d960b8b57fced58e342a80753db57ecbd62edb516bee989de24`.
- MIDI-only stock product artifact 14491421123, SHA-256 `d50e231c56cc00bc9eec3c206a0fe2c6b67608a5ff9e5a38f930c49118acff2a`.
- FS1B CDC artifact 14491168529, SHA-256 `ce755f7462b682a478fb1f1be35a0fe62c2cca5885b9fa708f440d1621c139f9`.

These are ZIP digests from Actions API; archive contents and actual ELF/MAP availability were not verified. Artifacts expire 2027-02-17 according to API. Use run links from BASELINE.md to recover them.

Pinned by `scripts/install_arduino_deps.sh`: M5Stack core 3.2.2, M5Cardputer 1.1.0, M5Unified 0.2.8, M5GFX 0.2.10. FS1B script references prebuilt IDF libs `idf-release_v5.4-858a988d-v1`; the actual compiler version and sdkconfig, instruction/data cache sizes, region map and linker scripts still need verification from installed package. Compile profiles disable PSRAM and use `huge_app`; that is a software configuration, not a physical chip inspection. CDC normal uses `CDCOnBoot=cdc`; MIDI-only/SEQTRAK uses `CDCOnBoot=default`; both use `USBMode=default,UploadMode=cdc`. `scripts/build.sh` sets loop stack 32768 and `GROOVEPUTER_FELUCCA_ANALOG=1`; FS1B substitutes the FatFs archive in a temporary build and verifies the map.

Reproduce with dependencies installed and network available, from the exact clean research source (record `git status --short`, full SHA, CLI/core/library versions and SOURCE_DATE_EPOCH):

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build_cardputer_memory_baseline.sh normal product --warnings all
bash scripts/build_cardputer_memory_baseline.sh midi-only product --warnings all
MEMORY_BASELINE_FS1=1 BUILD_PATH="$PWD/build/mao0-fs1b-cdc" bash scripts/build_cardputer_memory_baseline.sh normal product --warnings all
MEMORY_BASELINE_FS1=1 BUILD_PATH="$PWD/build/mao0-fs1b-midi" bash scripts/build_cardputer_memory_baseline.sh midi-only product --warnings all
bash scripts/build_seqtrak_midi_only.sh --warnings all
bash tests/run_host_tests.sh
make -C platform_sdl clean all CXX=g++
```

For each build retain ELF, full MAP, command log, SHA-256, generated build properties, precompiled `sdkconfig.h`, linker scripts and tool versions. Resolve exact tool paths in the installed package; execute `xtensa-esp32s3-elf-size -A`, `xtensa-esp32s3-elf-nm -S --size-sort --radix=d`, and `xtensa-esp32s3-elf-objdump -h` and `-t` against each ELF. Use actual paths, avoiding assumptions about `PATH`. Archive with checksums as CI artifacts, preserving source/build identity; do not commit generated SDK or large binaries. Reconcile section totals against non-overlapping symbol address intervals, COMMON and map fill; enumerate at least 30 top IRAM functions plus 30 mutable symbols with source, lifetime, mutability, alignment and access constraints.

`idf.py size-files` / `size-components` are not established for this Arduino CLI build. No change to SDK config or memory limit is authorized here.
