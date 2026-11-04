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
- Candidate code fix: `fbbf0105a8105a2869f12af691da1522fb4d5338` corrects undefined kick-phase conversion in the 808/909 voices. A fresh workflow-profile build from the clean evidence snapshot `83ea592ed503221d78cbafc3bf4c8b3865bb250f` passed; two fixed-path/fixed-epoch builds matched:
  - BIN 1,726,784 bytes, SHA-256 `4ab1bfafad54d7601a0b68b1d7e2c2472b29614d1b9db3db02c8170f256ad6fe`.
  - ELF SHA-256 `695ad678afc43f917f06320d04b6b0fb2993ea895bd9fcf61c3c099d128c6a66`.
  - Merged BIN SHA-256 `393e97e178be0c81cc4fc09c4330c501396d77529f3604622b32fae0745f7972`.
  - FS1B candidate-only linker proof passed. Static globals remain 186,236/191,488 bytes; the 5,252-byte DRAM margin and threshold policy remain provisional.
- The existing review-only bundle predates the kick-phase fix and is superseded. Regenerate it from the final candidate before any installation or distribution.
- Review-only bundle: `build/grooveputer-0.9.20-candidate-NOT-A-RELEASE.zip`.
  It contains the app and merged images, build proof, provenance, checksums and
  first-user documentation; `STATUS.txt` marks the decision DEFER.
- The earlier `ef3b9465…` and dirty-tree draft ZIPs are superseded. They are not
  release artifacts and must not be installed or shared as the 0.9.20 candidate.
- Memory report: 186,236 bytes globals against provisional 191,488-byte budget; only 5,252 bytes margin. Build script reports provisional exception, threshold-rule items 5–7 pending.

The readiness branch's currently running workflow (`41882445936`)
is pinned to the superseded pre-fix commit `93541d89c1f733d661cbaf0bf12ad9d3e8201e0c`.
It does not provide CI evidence for the refreshed candidate; a new manual workflow run must cover the final full-tree SHA. No v0.9.20 release is published. The first local build used the script default
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
- The earlier pre-fix workflow-profile build (1,726,816 bytes) and its logs are superseded. The refreshed post-fix build and same-path repeat are recorded above; repeat log: `/tmp/gp-0.9.20-83ea-second-build.log`.
- Release-image rebuild check: two clean builds at the same fixed release `BUILD_PATH`
  produced identical BINs. A separate-path experiment produced a different image:
  Arduino linked library objects directly in one build and through archives in the
  other. Therefore exact-byte reproducibility is established only for a repeated
  build in the same environment and absolute build path; cross-path reproducibility
  remains UNVERIFIED. The release workflow uses one fixed path for its check and
  artifact build.
- `bash scripts/ci/run_midi_targets.sh` — PASS; MIDI profile, route, pattern binding, performance binding, profile UI, convergence, and C9 repair targets passed.
- `test_drum_phase_wrap` — the sanitizer test reproduced out-of-range float-to-`uint32_t` conversion for both 808 and 909 on the pre-fix candidate; it passes both voices after commit `fbbf0105` with `undefined,float-cast-overflow` enabled. A refreshed full-suite and exact-SHA run is still required.
- Refreshed exact workflow-profile firmware build: PASS from clean snapshot `83ea592e`; same-path repeat produced identical ELF/BIN/merged-BIN hashes listed above. FS1B link proof passed. The final review bundle and exact-SHA CI still need refresh.
- `git diff --check` — PASS.
- Full local `bash scripts/ci/run_core.sh`: all 46 groups closed, including UI navigation (`Pattern pair view (L): PASS`, `PAIR PLAY PASS`) and final S1 page restore. Log: `/tmp/gp-0.9.20-540a-run-core-final.log`. This run predates the kick-phase fix and is not evidence for the refreshed source. The sanitizer regression and workflow-profile firmware build do cover the fix.
- The only remote workflow currently running is pinned to the superseded pre-fix SHA; exact-SHA CI and Cardputer hardware acceptance for the refreshed candidate remain UNVERIFIED.

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
