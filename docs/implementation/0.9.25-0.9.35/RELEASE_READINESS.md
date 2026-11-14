# Release readiness — 0.9.25–0.9.35

Status: `.34` is `HOST-VALIDATED / HARDWARE-UNVERIFIED`; its canonical host suite
passed at `fb300ff5`. `.35` integration remains in progress and is not ready for
owner hardware acceptance.
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
  addresses derived from the installed core's flash recipe;
- manifest, artifact sizes and SHA-256 values, resource report, ZIP and ZIP
  checksum.

The generator checks image presence, partition/recipe offsets, component overlap,
app/merged sizes, configured static DRAM gate, ELF build identity, and—when
requested—the exact clean source SHA and tree. Final-candidate packaging requires
a rollback archive. The packager checks its outer SHA, accepted FQBN, recorded
owner acceptance and internal image checksums, then bundles restore instructions.
Direct Arduino IDE builds retain a fallback `0.0.0-dev / unknown` identity;
the scripted build replaces it with the exact version and commit in its staged
source tree.
The current `.34` dry-run was generated from an earlier dirty build and is
engineering evidence only. It was not opened in M5Burner, installed through
M5Launcher, published or flashed to hardware. The legacy M5Burner schema/device
label and newer Web Burner compatibility remain unverified.

## Remaining `.34` / `.35` package work

- Rebuild the release FQBN (`CDCOnBoot=default`) with FS1B from the exact final
  `.35` SHA and preserve its linker map, build log, ELF and binaries. An
  exploratory release-profile build exists, but it predates current packaging
  changes and was made from a dirty source tree.
- Rebuild the artifact from the clean `.35` SHA and prove the embedded version
  and source SHA match its manifest.
- Run the clean-source package gate on that exact SHA, include the verified
  rollback bundle, and compare repeated final archives byte-for-byte.
- Run the full host/SDL/MIDI/storage/resource gates on the exact
  artifact-producing SHA; the previous full host-suite result is `.33` evidence.
- Publish no GitHub Release and submit nothing to M5Burner/M5Launcher before
  separate owner approval after hardware acceptance.

## Install formats and evidence boundary

The installed M5Stack core's merge recipe and generated partition table identify
the current component offsets: bootloader `0x1000`, partition table `0x8000`,
`boot_app0` `0xe000`, and app `0x10000`; the merged image is for offset `0x0`.
The package records the platform flash size and generated layout rather than
inferring compatibility from a filename. The current build profile describes a
4 MiB image/partition layout even though the Cardputer ADV board specification
lists 8 MiB flash; do not change partitioning or claim the image fills the
physical flash without an explicit, separately reviewed decision.

The app-only image requires the matching partition layout. The merged image is a
full-layout flash artifact. The M5Launcher app payload is only a proposed SD
installation artifact until the owner checks it on the device. The dry-run
M5Burner package validates the legacy directory shape and component addresses;
it does not prove current M5Burner or Web Burner acceptance.

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
  Manual / key-map regression contract. The final `.35` SHA still needs all lanes.
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
- No physical firmware, keyboard, internal audio, SD, USB MIDI or SEQTRAK test
  has been performed for this development line.
- SDL preflight at integration HEAD `a11b601b` exercised Melody creation and
  length editing, a bar-2 note, refusal to shrink across existing notes,
  STEPS/Melody switching, Song generation, FORM entry and full-slot refusal
  without partial Song mutation. SDL used dummy audio. This is a partial
  software workflow check, not final `.35` SDL acceptance.
- A focused host trace added in the current uncommitted `.35` worktree covers
  event-router/output/tee behavior across USB loss with DIN active, shared note
  ownership, captured-route stability, DIN release and USB cleanup before
  post-reconnect traffic. Its direct compile/run passes; the complete host
  suite and candidate gates have not yet run on this worktree state.

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
record. The `.35` handoff must regenerate the package from its exact clean SHA,
bundle these rollback files and instructions, then verify their hashes inside
the final archive.

## `.35` handoff gate

Do not set `READY_FOR_OWNER_HARDWARE_TEST` until the exact artifact-producing SHA
has a complete green host suite, SDL workflow evidence, mock-MIDI ownership and
reconnect traces, independent SMF semantic parsing, old/new project fixture
coverage, release-profile firmware build, resource report, deterministic package,
rollback instructions and the hardware checklist. The resulting status is
`HARDWARE_UNVERIFIED`; only the owner can record hardware acceptance.
