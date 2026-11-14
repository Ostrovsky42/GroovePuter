# Release readiness — 0.9.25–0.9.35

Status: `.35` candidate `0.9.35-rc+615ac96` is `READY_FOR_OWNER_HARDWARE_TEST` /
`HARDWARE-UNVERIFIED`. Host, SDL workflow, mock-MIDI, release-profile FS1B build,
resource gate and unpublished-package checks passed at exact artifact source SHA
`615ac9642f1103ce1f9d4fcddcb083710b171db4`. Only the owner can accept physical
behavior. At the owner's request, the app-only image was flashed to the connected
GroovePuter; esptool write hash and independent readback verification passed, and
the ESP32-S3 USB interface re-enumerated. The owner now reports the MIDI-export
version is visible, Play/Stop works and the signal stops. Cold power-cycle,
display readability, SD project persistence, keyboard and MIDI acceptance remain
pending.
Latest published package remains `v0.9.19`.

## Verified repository pipeline

- `.github/workflows/release.yml` builds the FS1B dynamic-FatFs Cardputer image
  with `CDCOnBoot=default`, runs its release reproducibility check, checks the
  DRAM budget, hashes artifacts and records commit/ref/run/FQBN provenance.
- The workflow uploads an Actions artifact. It does not create a public GitHub
  Release or publish firmware to M5Burner/M5Launcher.
- `scripts/build_cardputer_dynbuffers.sh` enforces candidate-only FatFs linkage
  and the repository's static DRAM budget.
- The existing `scripts/release.sh`, `scripts/dev_release.sh` and
  `scripts/upload_launcher.sh` are not the `.35` candidate packager: they use
  older release assumptions or flash/upload paths without the new package
  manifest and evidence checks.

## Local package generator

`scripts/package_hardware_candidate.py` creates a deterministic, unpublished
bundle containing:

- app-only image and merged full-flash image;
- separate M5Launcher SD app payload, explicitly marked
  `HARDWARE_UNVERIFIED`;
- legacy M5Burner directory (`m5burner.json` plus `firmware/` components) with
  addresses derived from the selected board's `boards.txt` properties and
  installed core recipe;
- manifest, artifact sizes and SHA-256 values, resource report, ZIP and ZIP
  checksum.

The generator checks image presence, partition/recipe offsets, component overlap,
app size, merged-image span against the board's physical flash capacity, configured
static DRAM gate, ELF build identity, and—when
requested—the exact clean source SHA and tree. Final-candidate packaging requires
a rollback archive. The packager checks its outer SHA, accepted FQBN, recorded
owner acceptance and internal image checksums, then bundles restore instructions.
Board-specific offsets and flash capacity come from the selected board entry in
`boards.txt`, which overrides generic `platform.txt` defaults.
Direct Arduino IDE builds retain a fallback `0.0.0-dev / unknown` identity;
the scripted build replaces it with the exact version and commit in its staged
source tree.
The `.34` dry-run was generated from an earlier dirty build and is historical
engineering evidence only. The `.35` package is clean-source, tied to the exact
artifact SHA and includes the verified `.16` rollback archive. It has not been
opened in M5Burner, installed through M5Launcher or published. Only the app-only
image was flashed directly; the legacy M5Burner schema/device label and newer Web
Burner compatibility remain unverified.

## `.35` package status

The release-profile FS1B build, linker map, embedded identity, static DRAM gate,
full host suite, SDL Golden Path, independent parser of an actual SDL export,
two-build firmware reproducibility check, repeated package comparison, rollback
verification and package checksums passed at exact source SHA
`615ac9642f1103ce1f9d4fcddcb083710b171db4`. Exact artifacts and sizes are in
[`RESOURCE_BUDGET.md`](RESOURCE_BUDGET.md); test evidence is in
[`VERSION_LOG.md`](VERSION_LOG.md).

The archive remains local and unpublished. Do not create a GitHub Release or
submit the dry-run package to M5Burner/M5Launcher. Runtime and musical checks
remain with the owner.

## Install formats and evidence boundary

The selected Cardputer ADV board entry and generated partition table identify
bootloader `0x0`, partition table `0x8000`, `boot_app0` `0xe000`, and app
`0x10000`. The device reported 8 MiB physical flash; the candidate merged image
spans 4 MiB from offset `0x0`. The app-only image is the update path when the
installed bootloader and partition table match. Both matched on the connected
device, so the app-only update preserved all other flash regions. The merged
image rewrites its full 4 MiB span, including NVS/SPIFFS regions; back up internal
flash data before using it. The M5Launcher payload and M5Burner dry-run remain
unverified for installation.

## Current evidence

- Baseline branch: `feature/0.9.24` at
  `28918d9b78e9a527ce2b5ce201a465af5126f76a`.
- `.33` export implementation: `3f492197d460a5e59664d0fefb3be0519902c0df`.
- `.33` expanded export tests: `7a1b5c6b`.
- `.34` package generator: `db29299fb130ad23bd5be6693f29242044e75e01`.
- `.34` build identity and rollback integration: commit
  `d3dd29f859a22b982d4af33c6d5da42e4b611857`; documentation regression test
  alignment: `fb300ff5c3e221902c3771d7af53973400073e4b`.
- Clean release-profile FS1B build and DRAM gate passed at `d3dd29f8`; firmware
  was reproducible byte-for-byte. The clean-source package gate passed with the
  verified `.16` rollback bundle and all package checksums.
- Canonical `bash tests/run_host_tests.sh` exited 0 on clean HEAD
  `fb300ff5c3e221902c3771d7af53973400073e4b`; log:
  `/tmp/gp-0934-host-suite-fb300ff5.log`. This includes the current Quickstart /
  Manual / key-map regression contract; this is historical `.34` evidence.
- Two package dry-runs built from the existing `0.9.35-dev` FS1B artifacts were
  byte-identical (ZIP SHA-256
  `b664e80d691d51b6e6cc6ef26928913372b9bb5cb6a0c949023dd4e83e1d9747`) and
  include the accepted `.16` rollback images. Their source tree was
  `NOT_CHECKED`, so neither manifest nor firmware is a clean candidate. Every
  file checksum passed.
- The `.33` default and FS1B builds passed with `--warnings all`; exact sizes,
  hashes and static-DRAM headroom are in [`RESOURCE_BUDGET.md`](RESOURCE_BUDGET.md).
- Focused exporter/parser, identity, version resolver and package-generator
  checks passed. A prior complete suite also exited 0 at `.33` HEAD
  `db29299fb130ad23bd5be6693f29242044e75e01`; its log is
  `/tmp/gp-0933-stable-host-suite.log`.
- A limited physical update was performed at the owner's request: the board
  returned ESP32-S3 QFN56 rev 0.2 with 8 MiB GD flash; its bootloader at `0x0`,
  partition table at `0x8000`, and `boot_app0` at `0xe000` matched candidate
  hashes. App-only image `6a6c920b...` was written at `0x10000`; esptool's write
  hash and independent `verify_flash` passed, followed by USB re-enumeration.
  The owner reports the version with MIDI Export visible, Play/Stop working and
  the signal stopping. The exact displayed version string, cold power-cycle,
  display readability and SD application save/load are still unconfirmed. No
  keyboard, USB MIDI, DIN or SEQTRAK behavior has been tested.
- At exact source SHA `615ac9642f1103ce1f9d4fcddcb083710b171db4`,
  `env TMPDIR=/tmp bash tests/run_host_tests.sh` exited 0; log:
  `/tmp/gp-0935-host-suite-615ac964.log`. The release-profile FS1B firmware
  passed its exact-source identity and 191,488 B static DRAM gate; log:
  `/tmp/gp-0935-fs1b-release-615ac964.log`.
- SDL was built cleanly and exercised in Xephyr with dummy audio. The path
  covered Melody creation/edit and safe length rejection, phrase generation in
  STOP and PLAY, Material TAKE, FORM insertion after source material, project
  save/load, Project output-mode selection/restoration and MIDI export. The
  exported 12,737 B SMF passed the independent parser. Screenshots:
  `/tmp/gp-0935-sdl-615ac964/xephyr-run/`.
- Two same-source/profile firmware builds produced app-only BIN SHA-256
  `6a6c920b3cab4c241464b02209680962743211d4d592af6c6adfd5a315339567`.
  The corrected candidate package includes board-specific flash addresses and
  the hardware checklist; final ZIP hash is recorded in
  [`RESOURCE_BUDGET.md`](RESOURCE_BUDGET.md). It contains the checksum-verified
  rollback bundle.
- Candidate static DRAM is 188,060 B, leaving 3,428 B under the provisional
  gate. Runtime heap and stack high-water were not measured; compiler warnings
  and physical unknowns are recorded in `VERSION_LOG.md`.
- A duplicate full-suite rerun after changing packaging-only code was stopped
  during a sanitizer compilation after 12 minutes; it is not counted. The full
  host suite at the unchanged firmware source SHA `615ac964...` passed above,
  and all 12 focused candidate-packaging tests pass after the board-address fix.
- Physical UI startup, keyboard, internal audio, SD, USB MIDI, DIN and SEQTRAK
  checks remain owner acceptance work.

## Rollback source identified

The public `v0.9.16` acceptance archive is available and its downloaded
sidecar checksum matches:

- Archive: `0.9.16-acceptance-fad8dfde.zip`
- Archive SHA-256: `494169c35db9bd276c9f873f5020f8bf5404f5f2efe62f1a1f57087e3c81508c`
- Accepted source SHA: `fad8dfde01d7fa60fe82405446db572484a52c23`
- Product app-only image: 1,532,144 B, SHA-256
  `44915f28853c1f915114be48a27cb915a803e216ce9be0928010a9152c0e681e`
- Product merged image: 4,194,304 B, SHA-256
  `53f49d6ad92a386a6a0034f754db22d43e15107fe7f8d0e05e2960a2c62ec39f`

The archive records owner PASS for its USB diagnostic image. It also records
that the same-source, same-FQBN product image (diagnostics disabled) was
subsequently flashed to that Cardputer, its upload hash was checked and the
board re-enumerated. Per-step telemetry was not supplied. Use this product
image as the rollback candidate and preserve the archive's flash instructions;
do not describe the product image as having its own separate per-step acceptance
record. The `.35` package bundles these rollback files and instructions; their
hashes and the outer archive checksum were verified.

## `.35` handoff gate

The software handoff gate is satisfied at exact artifact-producing SHA
`615ac9642f1103ce1f9d4fcddcb083710b171db4`: complete host suite, SDL workflow,
mock-MIDI ownership/reconnect trace, independent SMF parsing, project fixtures,
release-profile firmware and DRAM report, corrected reproducible package and
verified rollback are recorded above. The app-only write/readback is the only
physical operation completed. Candidate status remains
`READY_FOR_OWNER_HARDWARE_TEST` / `HARDWARE-UNVERIFIED`; only the owner can
record hardware acceptance.
