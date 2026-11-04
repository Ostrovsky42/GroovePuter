# GroovePuter 0.9.20 — Release Candidate Record

## Decision

**DEFER — not a release approval.** This record is for the candidate now under local
readiness work. Do not tag or publish it. The latest published release remains v0.9.19.

## Candidate identity

- Local readiness branch: `release/0.9.20-minimal-ui-fix`
- Exact source SHA and tree state: carried in the generated review bundle's
  `provenance.txt`; it must match the build source and report `worktree_dirty=false`.
- Product: M5Stack Cardputer ADV / ESP32-S3, PSRAM disabled
- Release build: `bash scripts/build_cardputer_dynbuffers.sh --warnings all`
- Release workflow FQBN: `m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc`
- Build script default FQBN differs (`CDCOnBoot=cdc`); the successful release-profile build explicitly overrode `FQBN` to match the workflow.
- Filesystem: dynamic FatFs FS1B, verified by the build script's linker-map proof
- Candidate BIN: `build/cardputer-adv-dynbuffers/GroovePuter.ino.bin`, 1,726,816
  bytes, SHA-256 `e5d8b4e927ad64a662a69facea021bb49b163c06bba07cf3c84971232c71fa08`.
  ELF SHA-256 `46a5052b529efb7c663a16f2f4b36f9a996b8d78b6d1ec90d08c3d4e3fd3475a`;
  merged BIN SHA-256 `a117fa567aee83f3f51344d39989610713661ef26bbf104f324b3cedc802341e`.
- Review-only bundle: `build/grooveputer-0.9.20-candidate-NOT-A-RELEASE.zip`.
  It contains the app and merged images, build proof, provenance, checksums and
  first-user documentation; `STATUS.txt` marks the decision DEFER.
- The earlier `ef3b9465…` and dirty-tree draft ZIPs are superseded. They are not
  release artifacts and must not be installed or shared as the 0.9.20 candidate.
- Memory report: 186,236 bytes globals against provisional 191,488-byte budget; only 5,252 bytes margin. Build script reports provisional exception, threshold-rule items 5–7 pending.

This candidate branch exists locally only; it has no exact-SHA Actions run or PR,
and no v0.9.20 release is published. The first local build used the script default
(`CDCOnBoot=cdc`) and is superseded by the exact workflow-profile build recorded above.

## Evidence collected

- `bash tests/run_smart_play_tests.sh` — PASS at baseline; Partner phase, note-range,
  and related focused host suites pass. Existing compiler warnings are recorded in
  [the R0 audit](0_9_20_R0_AUDIT.md).
- `bash tests/run_step_settings_tests.sh` — PASS at baseline.
- `bash tests/run_melody_slot_navigation_tests.sh`, `bash tests/run_ui_navigation_tests.sh`, `bash tests/run_s1_synth_engine_switch_tests.sh`, and `bash tests/run_s1_synth_page_restore_tests.sh` — PASS, exit 0. Detailed scope is recorded in the [R0 audit](0_9_20_R0_AUDIT.md).
- Eight source-regression checks listed in the R0 audit were refreshed to current
  implementation ownership/names. All eight now pass on the candidate source.
- SDL 2.30.0 / SDL2_gfx 1.0.2 smoke on the exact baseline — app launched, basic
  generation, workspace navigation, MATERIAL TAKE, Synth view, and Partner settings
  were visually inspected. Dummy audio cannot verify sound, MIDI hardware, persistence,
  or physical Cardputer behavior. Exact captures and limitations are in the R0 audit.
- Exact workflow-profile firmware build: PASS using `bash scripts/build_cardputer_dynbuffers.sh --warnings all`, FQBN override `CDCOnBoot=default`, fixed release `BUILD_PATH`, and fixed `SOURCE_DATE_EPOCH` from the remote base commit. FS1B candidate-only linker-map proof passed. Two clean builds at the same path produced matching ELF/BIN/merged-BIN hashes above. Image size: 1,726,816 bytes; globals: 186,236/191,488 bytes, with DRAM policy provisional and items 5–7 open. Logs: `/tmp/gp-0.9.20-f5cd730a-finalpath-build1.log` and `build2.log`.
- Release-image rebuild check: two clean builds at the same fixed release `BUILD_PATH`
  produced identical BINs. A separate-path experiment produced a different image:
  Arduino linked library objects directly in one build and through archives in the
  other. Therefore exact-byte reproducibility is established only for a repeated
  build in the same environment and absolute build path; cross-path reproducibility
  remains UNVERIFIED. The release workflow uses one fixed path for its check and
  artifact build.
- `bash scripts/ci/run_midi_targets.sh` — PASS; MIDI profile, route, pattern binding, performance binding, profile UI, convergence, and C9 repair targets passed.
- `git diff --check` — PASS.
- Full local `bash scripts/ci/run_core.sh`: all 46 groups closed, including UI navigation (`Pattern pair view (L): PASS`, `PAIR PLAY PASS`) and final S1 page restore. Log: `/tmp/gp-0.9.20-540a-run-core-final.log`. It ran before the source commit against the same tested source contents. Local validation is not exact-SHA remote CI.
- Exact-SHA remote checks and Cardputer hardware acceptance remain UNVERIFIED.

## User journey and known gaps

- Discover / understand: README still points to v0.9.19 as latest published release;
  it now identifies 0.9.20 as an unaccepted candidate.
- Install: no published image exists. The local NOT A RELEASE bundle includes
  matching images and a draft M5Launcher SD install/recovery path; neither step
  has been verified on Cardputer.
- First session: candidate Quickstart now says to choose a genre other than Acid/House
  before MATERIAL `D`; repeating a TAKE in those unsupported genres does not fix the
  refusal.
- Edit / save / reload: documented; not yet physically verified against a final image.
- Export / share: the candidate has no user-facing MIDI or WAV file export workflow.
  WAV writer implementations are present, but Cardputer recorder setup is disabled
  and the UI has no recording start/stop action. The MIDI Player imports files;
  project Save is not export. External realtime MIDI recording in a DAW is a
  separate handoff and needs physical route verification.
- Support / recovery: on-device help and manual exist; the local bundle contains
  a draft factory-restore path, which remains unverified on the candidate device.

## Physical acceptance: UNVERIFIED

The available owner report is not tied here to a final image hash and has not been
re-run. On the exact final BIN, record device identity, profile, BIN SHA-256, test
results and logs for:

- cold boot, project Save / power cycle / Load / continued edit;
- Partner ANSWER and LOOP reply, note release, and silence/panic behavior;
- drum step settings and pattern copy (`Ctrl+A`) boundaries;
- C#5 / E5 note entry and high-note behavior;
- USB Device MIDI routing and USB Host role/reboot behavior;
- recovery after failed or interrupted installation.

## Go / No-Go gate

Remain **DEFER** until the exact final SHA has a clean release-profile build with
checksums/provenance, required software gates, a usable install and recovery path,
closed or explicitly accepted product gaps (including export expectations), and
physical acceptance. Missing evidence is UNVERIFIED. No release tag or publication
is authorized by this record.
