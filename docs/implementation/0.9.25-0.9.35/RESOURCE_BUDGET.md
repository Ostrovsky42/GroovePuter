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

## 0.9.31 FORM arrangement and role presets

**Base commit:** `885ce13507e7d7f93740ee84eb5fd15ee1e64c81` (0.9.30). The implementation was built from this integration worktree after the curve validation and transient FORM role presets; no storage schema changed.

| Build | Program storage report | Static DRAM | Gate headroom | Result |
|---|---:|---:|---:|---|
| `bash scripts/build.sh --warnings all` | 1,721,130 B | 187,604 B | 3,884 B | PASS; default FatFs, not SD-safe |
| `env TMPDIR=/tmp/grooveputer-0931-host-tmp bash scripts/build_cardputer_dynbuffers.sh --warnings all` | 1,745,602 B | 187,612 B | 3,876 B | PASS; candidate-only FatFs link proof |

Compared with the 0.9.29 records, program storage grew by 392 B in both
profiles. The FS1B app-only BIN grew by 400 B to 1,745,760 B. Static DRAM did
not change: default `.dram0.data`/`.bss` are 44,732 B / 142,872 B; FS1B are
44,732 B / 142,880 B. The code adds no global event buffers. Stack high-water
mark and runtime heap remain unmeasured.

The current FS1B artifact was compiled with
`m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`.
It is the candidate-only FatFs software build; the separate release workflow
profile with `CDCOnBoot=default` has not been built here. The linker map
contains the candidate FatFs archive and excludes the stock archive.

Default-profile artifacts (not the SD-safe candidate):

- App-only BIN: 1,721,280 B; SHA-256 `879a84cbb91082793db43b9a13da65d3b50cb024108acb1cc617e06485809427`.
- Merged BIN: 4,194,304 B; SHA-256 `2f9e854ae8643f5826e0cd303f4b32e0a3efdbe909db30d7cb346a6388033dd7`.
- ELF: 37,492,424 B; SHA-256 `4b217434eff94f6256c7d7c22b1f09cff6ff3bd8efb0dc50d320a8d56cf46bcf`.

FS1B candidate artifacts:

- App-only BIN: 1,745,760 B; SHA-256 `e772647df8313c47e01cca01d7f2184f5fe40139870e5f9000465e76a71fb61e`.
- Merged BIN: 4,194,304 B; SHA-256 `65b1b7ecc95ad578205373265f438d0851842b4ef21b290fa5908202e09478d3`.
- ELF: 37,438,616 B; SHA-256 `74b26e8668b713bd18f595a998c407b4d185b43b4a30da3f1c45cb993f64e4b8`.
- Link map: `build/cardputer-adv-dynbuffers/.arduino-build/GroovePuter.ino.map`; candidate FatFs present, stock FatFs absent.

Both build and link-map checks are host toolchain evidence. No image was
flashed, and no physical Cardputer behavior is claimed.

## 0.9.32 expressive MIDI

The dispatcher adds a two-byte `g_cutoffAttemptedMask[2]` so each synth can
distinguish a first controller attempt from a wrap-safe retry deadline. The
review fixes add an endpoint controller-state epoch and primary mount tracker
to the existing Tee/USB output owners, plus a 16-byte local source-mode snapshot
inside `EnergyCurve::apply()`. These do not add a MIDI queue or saved material.

| Build | Program storage report | App-only BIN | Static DRAM | Gate headroom | Result |
|---|---:|---:|---:|---:|---|
| `bash scripts/build.sh --warnings all` | 1,722,646 B | 1,722,800 B | 187,628 B | 3,860 B | PASS; default FatFs profile, not SD-safe |
| `env TMPDIR=/tmp bash scripts/build_cardputer_dynbuffers.sh --warnings all` | 1,747,110 B | 1,747,264 B | 187,636 B | 3,852 B | PASS; candidate-only FatFs map |

Compared with 0.9.31, program storage grew by 1,516 B in the default profile
and 1,508 B in FS1B. The FS1B app-only BIN grew by 1,504 B. Static DRAM grew
by 24 B in each profile, leaving 3,860 B and 3,852 B before the provisional
191,488-byte gate. Relative to the pre-review .32 firmware build, review fixes
added 428 B of program storage, 432 B to each app-only BIN, and 8 B of static
DRAM. Both map reports show `.dram0.data` 44,732 B; `.dram0.bss` is 142,896 B
default and 142,904 B FS1B. Stack high-water and runtime heap were not measured.

Both builds use
`m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`.
Only FS1B is the candidate-only FatFs software build; its map contains
`build/cardputer-adv-dynbuffers/fatfs-dynbuffers/libfatfs.a` and excludes the
stock FatFs archive.

Post-review default firmware log: `/tmp/gp-0932-review-default-firmware.log`.
Post-review FS1B firmware log: `/tmp/gp-0932-review-fs1b-firmware.log`.
Both were built from the same integration worktree. The default linker map is
compile evidence only and uses stock FatFs; use FS1B for any candidate package.

Default-profile artifacts (not the SD-safe candidate):

- App-only BIN: 1,722,800 B; SHA-256 `c9ee14b18d34a52f156854f07cde492739614719b57bbc4b7abc5c4cf58f8ead`.
- Merged BIN: 4,194,304 B; SHA-256 `f6bd479e19e4b284f5231268267f2795e9e35c4e2f0c3a8782b80c94047d69a9`.
- ELF: 37,504,244 B; SHA-256 `a0cdb202b2ae2b87c4ef0cf7ba0d180a4aa83ead1f6af188c05a99f719488a77`.

FS1B candidate artifacts:

- App-only BIN: 1,747,264 B; SHA-256 `a9e481eeffd3fca39faa38116993e53c4e84cf124f76e37ef4a2bb80c41d098d`.
- Merged BIN: 4,194,304 B; SHA-256 `17201791191c09f6f04770236201c110c444768ad28c9b354c90f6739977e15d`.
- ELF: 37,450,436 B; SHA-256 `84ad7ec0ace0c0cfd64e65f61bc9091243824280b57fb2d2138e685d9ed759e9`.
- Link map: `build/cardputer-adv-dynbuffers/.arduino-build/GroovePuter.ino.map`; candidate FatFs present, stock FatFs absent.

Post-review FS1B map check confirmed `build/cardputer-adv-dynbuffers/fatfs-dynbuffers/libfatfs.a` is linked and the toolchain stock `libfatfs.a` is absent.

These are compile/link measurements only. No firmware was flashed; runtime
heap, stack, audio timing, actual SD behavior and device behavior remain
unmeasured.

## 0.9.33 MIDI export

**Source commit:** `3f492197d460a5e59664d0fefb3be0519902c0df`.

| Build | Program storage report | App-only BIN | Static DRAM | Gate headroom | Result |
|---|---:|---:|---:|---:|---|
| `bash scripts/build.sh --warnings all` | 1,735,150 B | 1,735,312 B | 187,628 B | 3,860 B | PASS; default FatFs profile, not SD-safe |
| `env TMPDIR=/tmp bash scripts/build_cardputer_dynbuffers.sh --warnings all` | 1,759,594 B | 1,759,744 B | 187,636 B | 3,852 B | PASS; candidate-only FS1B FatFs map |

Compared with the 0.9.32 report, program storage grew by 12,504 B in the
default profile and 12,484 B in FS1B. App-only BIN growth is 12,512 B and
12,480 B respectively. Static DRAM is unchanged in both profiles. `.dram0.data`
is 44,732 B in both; `.dram0.bss` is 142,896 B default and 142,904 B FS1B.
Runtime heap and task stack high-water were not measured. The provisional
191,488 B gate remains below 4 KiB away.

Both builds use
`m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`.
The default build links stock FatFs and is not the SD-safe candidate. The FS1B
map includes `build/cardputer-adv-dynbuffers/fatfs-dynbuffers/libfatfs.a` and
excludes the toolchain stock FatFs archive.

Default-profile artifacts (compile evidence only; not for the candidate):

- App-only BIN: 1,735,312 B; SHA-256 `8b80898be602a219338828c4841236fc344b185d48a21362a0c1951372e8a8ba`.
- Merged BIN: 4,194,304 B; SHA-256 `72f1692a25a622c4c6b5df2cdf27e1d564fde05df2b732a1770144d08d4ce2be`.
- ELF: 37,950,352 B; SHA-256 `c1ef06d0ecab39b4aba3af3b40ceca1150c029f92a7372193fef4dee50e7fc5d`.

FS1B artifacts:

- App-only BIN: 1,759,744 B; SHA-256 `9a96a7d3d00d1303bf464e9e950ddf2897cc91d43ccd6a9167395c6740e6ec78`.
- Merged BIN: 4,194,304 B; SHA-256 `d26359af90cc797c470a244ed5cf1e73f8e353f8f40eee70702f873b3cd84cec`.
- ELF: 37,896,564 B; SHA-256 `60f7e91d87e92aa2269c0fb3a4c0d6992ed77318b4123323e609e6d97cd1098b`.
- Link map: `build/cardputer-adv-dynbuffers/GroovePuter.ino.map`; candidate FatFs present, stock FatFs absent.

Build logs: `/tmp/gp-0933-final-default-build.log` and
`/tmp/gp-0933-final-fs1b-build.log`. These are exact-source compile/link
measurements for the 0.9.33 result commit. No artifact was flashed. Runtime
memory, physical SD behavior, audio timing and MIDI behavior remain unmeasured.

## 0.9.34 productization build identity — exploratory

These two FS1B firmware builds were made from source commit
`0d973ffcae96f056bb1605de2000a8bac787d2f7` with `tree=dirty`. The embedded
identity is `0.9.35-dev`; the packager confirmed that the ELF contains that
version and source SHA. These builds prove the release profile compiles and
that identity stamping adds negligible static state. They are not clean-source
or final-candidate evidence.

| Build profile | Program storage report | App-only BIN | Static DRAM | Gate headroom | `.dram0.data` | `.dram0.bss` |
|---|---:|---:|---:|---:|---:|---:|
| FS1B, release `CDCOnBoot=default` | 1,808,354 B | 1,808,512 B | 188,060 B | 3,428 B | 44,812 B | 143,248 B |
| FS1B, comparison `CDCOnBoot=cdc` | 1,759,710 B | 1,759,872 B | 187,636 B | 3,852 B | 44,732 B | 142,904 B |

The CDC-on comparison is like-for-like with the `.33` FS1B build: +116 B program
storage, +128 B app image and no static-DRAM change. The release profile is a
different compiler target from the `.33` baseline; do not read its larger
footprint as an identity-stamping delta.

Release-profile app-only BIN SHA-256:
`dacc59dd4e64a6faad520992a2266e2e0d303a303b7da0596e7771f159abc95e`; merged
BIN SHA-256:
`d01eb4778112286b6672e1b7c71f557e6bcf7a63584fabfb46cc8142481bc9e8`.
Comparison-profile app-only BIN SHA-256:
`cff61bafdd2e293f972e889e92f9a11db9774793c9ad8b3826c0aad894d9ffa1`; merged
BIN SHA-256:
`845f609914e5af741d53692b8521a26d3191bfdf4060993b7178a5cd066eaa38`.

Build logs are `/tmp/gp-0934-build-identity.log` and
`/tmp/gp-0934-build-identity-cdc.log`. The release-profile package dry-run
includes the verified `.16` rollback bundle; all package checksums passed, and
two independently generated ZIPs were byte-identical. Dry-run ZIP SHA-256:
`b664e80d691d51b6e6cc6ef26928913372b9bb5cb6a0c949023dd4e83e1d9747`.
Its manifest records `working_tree=NOT_CHECKED`; do not treat it as the `.35`
candidate. No firmware was flashed.

## 0.9.34 clean release build and package

**Build source:** `d3dd29f859a22b982d4af33c6d5da42e4b611857`; build identity
`0.9.34`, source tree `clean`.

| Profile | Program storage | App-only BIN | Static DRAM | Gate headroom | `.dram0.data` | `.dram0.bss` |
|---|---:|---:|---:|---:|---:|---:|
| FS1B, `CDCOnBoot=default` | 1,808,354 B | 1,808,512 B | 188,060 B | 3,428 B | 44,812 B | 143,248 B |

FQBN:
`m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc`.
The map proves candidate FS1B is linked and the stock FatFs archive is absent.
`bash tests/test_release_firmware_reproducibility.sh` built the same app image
twice; both SHA-256 values were
`2c2ed42e32f7067d27cbd6c7df3bfe4c86c7e7969a3ac1195584c331870b6f49`.

Artifact SHA-256 values:

- App-only BIN: `2c2ed42e32f7067d27cbd6c7df3bfe4c86c7e7969a3ac1195584c331870b6f49`.
- Merged BIN: `247ad9967f55c9321b69ed5253b0f4857ba3badf5e972622c63bdcca29baf282`.
- ELF: `965f24ab5435e9a64c33b014a748db1550ff9d9382a6e8a5c7111a4ef89b7140`.
- Clean-source package ZIP (includes verified `.16` rollback):
  `69ef4b383a1ab04be589636f337503088214591de3840825e1f9808ac3cab337`.

The manifest records the matching full source SHA and `working_tree=CLEAN`;
every bundled file passed `sha256sum -c`. This is `.34` engineering evidence,
not a `.35` hardware candidate. No image was flashed.

## 0.9.35 release-profile hardware candidate

Artifact-producing source SHA: `615ac9642f1103ce1f9d4fcddcb083710b171db4`;
source tree was clean. Firmware build identity is `0.9.35-rc+615ac96`.
FQBN:
`m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc`.
The FS1B build log is `/tmp/gp-0935-fs1b-release-615ac964.log`; the linker map
contains the candidate dynamic-buffer FatFs archive and excludes stock FatFs.
The release build and project DRAM budget check passed.

| Program storage | App-only BIN | Merged image | Static DRAM | Gate headroom | `.dram0.data` | `.dram0.bss` |
|---:|---:|---:|---:|---:|---:|---:|
| 1,808,354 B | 1,808,512 B | 4,194,304 B | 188,060 B | 3,428 B | 44,812 B | 143,248 B |

Static DRAM matches `.34` release profile (delta 0 B). Runtime heap and task
stack high-water remain unmeasured. The fixed DRAM gate is provisional.

The selected Cardputer ADV profile has 8 MiB physical flash and bootloader
address `0x0`; the partition table, `boot_app0` and app are at `0x8000`,
`0xe000` and `0x10000`. The candidate merged artifact spans 4 MiB from `0x0`.
Its 8 MiB target capacity and 4 MiB artifact span are recorded separately in
`manifest.json`.

| Artifact | Size | SHA-256 |
|---|---:|---|
| `GroovePuter.ino.bin` (app-only) | 1,808,512 B | `6a6c920b3cab4c241464b02209680962743211d4d592af6c6adfd5a315339567` |
| `GroovePuter.ino.merged.bin` | 4,194,304 B | `2d3d78a10baf44af9e966b42fd8d72326412d01dcef2cadd40daf360058d5c19` |
| `GroovePuter.ino.elf` | 37,897,036 B | `4038a6671dc840e94677abe10dbd910151040cf861c3944e020cfacded27c003` |

Two same-source, same-profile builds produced the same app-only BIN hash;
log: `/tmp/gp-0935-fw-repro-615ac964.log`. The connected device's bootloader,
partition table and `boot_app0` readbacks matched the candidate. At the owner's
request, the app-only image was written at `0x10000`; esptool write verification
and independent `verify_flash` passed, followed by USB re-enumeration. Display,
audio, SD and MIDI were not checked. An earlier 3 MiB readback attempt for an
extra app-partition backup ended with a serial CRC error before any write.

The corrected clean-source package is built twice with the verified `.16`
rollback archive and compared byte-for-byte. Candidate archive:
`build/release-candidate/0.9.35-HARDWARE-CANDIDATE-owner-handoff.zip`; ZIP SHA-256
`b9810a1f98fa20a6d5f98ece295c52eaae0209eed79a1c050a7311bf09675731`. Both ZIPs
were byte-identical; ZIP integrity and every bundled checksum passed. The
archive remains unpublished; M5Launcher and M5Burner installation compatibility
remain unverified.
