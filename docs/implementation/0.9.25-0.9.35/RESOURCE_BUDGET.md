# Resource budget at the 0.9.24 baseline

Baseline SHA: `28918d9b78e9a527ce2b5ce201a465af5126f76a`  
Evidence: [GitHub Actions run 43541264032](https://github.com/Ostrovsky42/GroovePuter/actions/runs/43541264032), completed successfully on the exact baseline SHA, plus local FS1B build log `build/adv-fs1b-baseline.log`.

## Firmware and DRAM

| Configuration | Program storage | Global DRAM | Fixed DRAM gate | Headroom | `.dram0.data` | `.dram0.bss` |
|---|---:|---:|---:|---:|---:|---:|
| ADV default product job (stock FatFs; FQBN `...CDCOnBoot=cdc...`) | 1,717,622 B / 3,145,728 B (54%) | 187,604 B / 327,680 B (57%) | 187,604 B / 191,488 B | 3,884 B | 44,732 B | 142,872 B |
| ADV FS1B job (dynamic FatFs; FQBN `...CDCOnBoot=cdc...`) | 1,742,082 B / 3,145,728 B (55%) | 187,612 B / 327,680 B (57%) | 187,612 B / 191,488 B | 3,876 B | 44,732 B | 142,880 B |

The local exact-baseline FS1B rebuild reported 1,742,066 B program storage and the same 187,612 B globals / 3,876 B gate headroom. Its app-only BIN is 1,742,224 B; SHA-256 `b03642cac4122be59c4062f8bb142625d384a0621d25b69a56ee7d21d5b6d18e`. Local ELF SHA-256: `893f748d60534d71a3a6c3ece8e0a4c1a537ff410e9e4337e159ee286ac12982`. The 16-byte program-storage delta from CI is recorded rather than normalized away. Both builds use `CDCOnBoot=cdc`.

Both profiles use the pinned M5Stack ESP32 core 3.2.2 and ESP-IDF libraries `idf-release_v5.4-858a988d-v1` in that CI run. The exact FQBN recorded for the default product job is `m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`. The FS1B job uses `scripts/build_cardputer_dynbuffers.sh`, which inherits the same default FQBN unless CI overrides it.

The release workflow's artifact profile instead sets `CDCOnBoot=default` (CDC off at boot). Its FS1B image is not the same build profile as the PR Cardputer job above. Record a separate exact-SHA size/fingerprint baseline for that release FQBN before claiming a byte-for-byte release build or comparing future release artifacts.

The Arduino CLI's remaining dynamic-memory number (140,076 B product; 140,068 B FS1B) is not the project's fixed global DRAM allowance. Use the fixed 191,488-byte gate when evaluating static growth. CI reports `policy: provisional exception; threshold-rule items 5-7 pending` for both configurations. Treat the small gate headroom and provisional policy as an explicit constraint for all milestones; do not infer that all reported free dynamic memory is available to static globals or runtime heap.

## Artifact fingerprints

The exact-head CI FS1B verification job reported:

- ELF SHA-256: `3e06e67a2d62ff674870795e031cc8ed5007ccc81e37d204d130ff0b31e278fd`
- BIN SHA-256: `182ddbd6ff3f60d96a7e2d95660c6f17654a361a8d941323c9fcd22761b4e943`

These are CI artifact fingerprints for the baseline, not release artifacts. The CI artifact archive size is not the firmware image size.

The local exact-head rebuild produced these additional fingerprints:

- App-only BIN: 1,742,224 B; SHA-256 `b03642cac4122be59c4062f8bb142625d384a0621d25b69a56ee7d21d5b6d18e`
- ELF: 37,384,516 B; SHA-256 `893f748d60534d71a3a6c3ece8e0a4c1a537ff410e9e4337e159ee286ac12982`
- Merged BIN: 4,194,304 B; SHA-256 `f2ec2ddec848d66e619bd64d3c590c27e8c26445aa9ec5c0437e78c3bd506424`

## Measurement status

- Exact-head CI Cardputer ADV product and FS1B build jobs: **PASS**.
- Exact-head CI DRAM checks: **PASS**, with the provisional policy note above.
- The default product job links stock FatFs. Repository upload code documents that this image reboot-loops when an SD card is inserted and refuses to flash it unless explicitly overridden. It is compile evidence, not the firmware candidate to hand off. The candidate must use the FS1B dynamic-FatFs build and verify the linker map contains only the candidate archive.
- Local exact-head FS1B rebuild, link-map proof and DRAM gate: **PASS**. It reproduced global DRAM exactly; reported program storage differed from the cited CI run by 16 B. Local artifact hashes are recorded above.
- Hardware measurements: **NOT TESTED** and intentionally deferred until 0.9.35 owner acceptance.

For milestone deltas, compare like-for-like configuration and toolchain, preserve the linker map/ELF report, report both absolute DRAM use and headroom against 191,488 B, and explain any change. No new numeric ceiling is introduced here; the baseline gate is the currently enforced project value.

## 0.9.28 macro engine

The 0.9.28 transformer is a fixed-capacity header-only host-tested domain function and is not yet referenced by the live application. Therefore it adds no firmware object, static DRAM or binary bytes at this milestone. No target-memory claim is made about the future 0.9.29 playback/UI integration; measure that integration against the FS1B baseline then.

## 0.9.29 live macro integration

**Candidate base:** `efa5434562fa70303a75c5097a44f7d7e3911702` (0.9.28). The current implementation changes were built from the dirty 0.9.29 worktree before its local milestone commit.

| Build | Program storage report | Static DRAM | Gate headroom | Result |
|---|---:|---:|---:|---|
| `bash scripts/build.sh --warnings all` | 1,720,738 B | 187,604 B | 3,884 B | PASS; default FatFs profile, not the SD-safe candidate |
| `env TMPDIR=build/tmp-host-suite bash scripts/build_cardputer_dynbuffers.sh --warnings all` | 1,745,210 B | 187,612 B | 3,876 B | PASS; FS1B candidate-only FatFs link proof |

The FS1B global DRAM is unchanged from the exact local baseline: 187,612 B / 191,488 B. The macro state is packed into one 32-bit atomic member of the existing `g_miniAcidInstance`; no full Phrase buffer or new static event queue was added. The current linked symbol `_ZL18g_miniAcidInstance` is 15,952 B. Stack usage was not measured.

On the same local FS1B build path, the exact-baseline program-storage report was 1,742,066 B and the app-only BIN was 1,742,224 B. The current increases are +3,144 B and +3,136 B respectively. Current FS1B artifacts:

- App-only BIN: 1,745,360 B; SHA-256 `e561dcfe3d52e70725a0c93863e908f4b86f322ed4d8da8a6d357f07a1545845`.
- ELF: 37,431,640 B; SHA-256 `bcbd9735bdbb96225af3d23cb7f834da1d730e594cc731b2f194b6f3e69750c8`.
- Link map: `build/cardputer-adv-dynbuffers/.arduino-build/GroovePuter.ino.map`; candidate FatFs archive present and stock FatFs archive absent.

The linker map and compiler report are software evidence only. They do not measure runtime free heap, stack high-water mark, audio callback time or physical-device behavior. The provisional 191,488-byte gate still has less than 4 KiB headroom and remains an explicit constraint.

Target ELF disassembly check: `MiniAcid::setPerformanceMacroState()` publishes the packed word with one aligned `s32i`, and `MiniAcid::performanceMacroState()` reads it with one aligned `l32i`; each is bracketed by Xtensa `memw`. These two accessors do not call `__atomic_*` or FreeRTOS critical-section helpers in the linked ESP32-S3 firmware. This is compiler/linker evidence for coherent 32-bit publication on the target build, not a hardware timing measurement. The firmware was built from the 0.9.29 worktree before its milestone commit.
