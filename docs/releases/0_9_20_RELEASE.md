# GroovePuter 0.9.20 — Release Candidate Record

## Decision

**DEFER — not a release approval.** This record is for the candidate now under local
readiness work. Do not tag or publish it. The latest published release remains v0.9.19.

## Candidate identity

- Local readiness branch: `release/0.9.20-readiness`
- Exact source SHA and tree state: recorded in the generated `provenance.txt`; a
  release image is valid only when the provenance commit equals the reviewed
  candidate SHA and `worktree_dirty=false`.
- Product: M5Stack Cardputer ADV / ESP32-S3, PSRAM disabled
- Release build: `bash scripts/build_cardputer_dynbuffers.sh --warnings all`
- Release workflow FQBN: `m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc`
- Build script default FQBN differs (`CDCOnBoot=cdc`); the successful release-profile build explicitly overrode `FQBN` to match the workflow.
- Filesystem: dynamic FatFs FS1B, verified by the build script's linker-map proof
- Candidate BIN: `build/cardputer-adv-dynbuffers/GroovePuter.ino.bin`; exact size and
  SHA-256 are recorded in the generated `SHA256SUMS.txt` after the final candidate
  commit is rebuilt.
- The earlier `ef3b9465…` dirty-tree image and draft ZIPs are superseded. They are
  not release artifacts and must not be installed or shared as the 0.9.20 candidate.
- Memory report: 186,236 bytes globals against provisional 191,488-byte budget; only 5,252 bytes margin. Build script reports provisional exception, threshold-rule items 5–7 pending.

The candidate branch has no exact-SHA Actions run, no PR, and no published v0.9.20
release. The current release workflow has no push trigger for this branch. The first
local build used the script default (`CDCOnBoot=cdc`) and is superseded for release
profile evidence by the explicit workflow-profile build (`CDCOnBoot=default`).

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
- Exact workflow-profile firmware build: PASS using `bash scripts/build_cardputer_dynbuffers.sh --warnings all` with the workflow FQBN override. FS1B link-map proof passed. Latest build emitted 1,001 warning occurrences: 978 are six Arduino core macro redefinitions repeated across 163 translation units; the remaining 23 application warnings are format-truncation diagnostics reviewed against bounded source values in the R0 audit. Two real UI truncation risks were fixed. All other application warnings from the preceding build were resolved, including dead locals, debug-only metric work, incomplete aggregate initializers, initializer ordering, a deprecated RGB API and volatile increment syntax. Log: `/tmp/grooveputer-0.9.20-warning-cleanup-final.log`.
- Release-image rebuild check: two clean builds at the same fixed release `BUILD_PATH`
  produced identical BINs. A separate-path experiment produced a different image:
  Arduino linked library objects directly in one build and through archives in the
  other. Therefore exact-byte reproducibility is established only for a repeated
  build in the same environment and absolute build path; cross-path reproducibility
  remains UNVERIFIED. The release workflow uses one fixed path for its check and
  artifact build.
- `bash scripts/ci/run_midi_targets.sh` — PASS; MIDI profile, route, pattern binding, performance binding, profile UI, convergence, and C9 repair targets passed.
- `git diff --check` — PASS.
- Full local `run_core.sh` after warning cleanup: PASS, exit 0. All 46 groups closed and the final S1 page-restore check recorded PASS. Log: `/tmp/grooveputer-0.9.20-run-core-after-warning-cleanup.log`. The focused release identity, Smart Play, and S1 engine-switch checks also passed after cleanup. This is local validation, not an exact-SHA remote CI run.
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
