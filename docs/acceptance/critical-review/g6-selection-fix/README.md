# G6 project-selection rollback — software fix

Status: host regression, SDL build, Cardputer ADV build, image upload/hash verification PASS; physical G6 Save/Reset/Load and SD-failure cases OPEN.

Base: accepted #495 source `64ae4b8bcf13fc165a04b2b79a80b7e8b3a1f71f`.

## Defect and repair

Explicit Load used the Save-As/New scene-name setter before validating the target. That setter persisted the boot pointer and selected the project paging namespace. If parsing or storage then failed, rollback reused the failing storage setter, leaving canonical A paired with target B selection.

Load now admits and migrates the target namespace without persisting the pointer. On refusal MiniAcid restores both in-memory names without filesystem calls. Cardputer validates its streaming filters without publishing sample references/output modes, then persists the pointer, then publishes validated filter state and Scene. SDL follows the same pointer-before-output-state order. Boot reads retain the default nonpublishing behavior. Existing SaveAs/New selection remains on the old setter.

SDL pointer writes use a flushed temporary file and staged rename with a recoverable backup. Cardputer's shared temp promotion preserves a backup-only pointer across retries when main is absent. A behavioral injected rename failure proves the sequence: main-to-backup succeeds, temp promotion and backup restore fail, the next attempt also fails, and the old pointer bytes remain recoverable; eventual promotion succeeds without deleting that backup.

Scene JSON/project formats and material identities are unchanged. No persistent runtime owner, queue, second Scene buffer, or allocation was added. Runtime stack/high-water impact remains OPEN pending device measurement.

## Verification

- `tests/test_g6_selection_rollback.cpp`: failed read leaves canonical Scene, output mode, SceneStorage name, paging namespace, and boot pointer at A; pointer-commit refusal publishes none of candidate B; successful B Load publishes B and its saved output mode.
- `tests/test_atomic_file_commit.cpp`: Cardputer backup-only pointer survives repeated failed promotion/restoration and later successful promotion.
- `tests/run_g6_failed_load_tests.sh`: full host persistence regression suite; exact stdout/stderr logs are gzip-compressed in this directory (`host-suite.log.gz`, `sdl-build.log.gz`, `adv-build.log.gz`, `source-contracts.log.gz`).
- Source contracts: output ownership, memory instrumentation applicability, sampler persistence/atomic pointer ordering, scene revision.
- SDL: forced full build with `make -B -C platform_sdl -j2 CXX=g++`.
- Cardputer ADV: clean frozen-FQBN build via `scripts/build_cardputer_dynbuffers.sh`; DRAM limit remains 191488 bytes. Exact final BIN SHA and `.data/.bss/flash` are in `build-evidence.txt`.
- The repair image was flashed with esptool hash verification. Hardware Save/Reset/Load confirmation and SD-failure tests remain OPEN; passive captures below are not workload gates.

No push or merge performed.

## First device observation

BIN SHA matched before upload; esptool wrote the app and verified the device flash hash, then hard-reset. A 60-second serial capture (`hardware-monitor.serial`) recorded 12 PERF samples: underruns remained 0; audio load 6.2–6.7%, peak 7.0–7.7%, free internal heap 19528 B and largest block 10740 B throughout this passive window. MIDI USB mounted. This is a short idle observation only, not the 30-minute runtime gate or paired #495 benchmark. Boot screen and Save→Reset→Load await user confirmation; SD-failure testing remains OPEN.

A second passive serial capture (`hardware-interaction.serial`, 125 seconds / 27 PERF samples) continued the same idle observation: 0 underruns; audio load 6.2–7.0%, peak 7.7–9.7%, free internal heap 19528–19532 B and largest block 10740 B. Serial uptime advanced monotonically from 130009 to 255023 ms, with no reset marker. No Save/Load interaction was visible in this capture, so it does not close G1, G6 or the 30-minute runtime gate.

The nearest short #495 control capture reported free internal heap 19480 B and largest block 10740 B; this candidate's passive captures reported 19528–19532 B and 10740 B. The observed free-heap difference is +48 to +52 B and largest block is unchanged, but capture timing/workload is not paired, so this is supporting context only, not the paired memory/runtime gate.

## Follow-up device monitor

A further 175-second passive capture (`g6-user-check.serial`, 36 PERF samples) reports 0 underruns, free internal heap 19528–19532 B, largest block 10740 B, and uptime progressing 450041→625061 ms. No reset/panic markers were present. No explicit Save/Reset/Load action was confirmed; this does not close G1/G6 or the 30-minute workload gate.

## Refused Load revision regression follow-up

An actual ProjectPage test exposed a separate bug: the generic audio-guard wrapper marked the scene dirty after a refused Load. The Load path now keeps the audio pause guard but does not run the generic mutation-marking wrapper. The test asserts canonical JSON, project name, and both current/persisted revision stay unchanged. The regression failed before the fix and passes after it. See [`review-disposition.md`](review-disposition.md) for the prior independent review finding and rationale.

The follow-up is commit `c93447478f76b6d58d7f80c0695b3654ff425791`; its Cardputer BIN SHA-256 is `844ed89ac755ecc6a84640c96700ed2e51c34936b13791e87fd0cf53183506ff`. Host G6 suite, SDL, and Cardputer ADV rebuilds pass. This exact image was uploaded to `/dev/ttyACM0` and esptool verified the written app hash. An earlier upload of the same source produced BIN `b7d755437ecf51659c9886c4f6c220ef9f610925ab9f7745f809473b2b710ffe` and had a 60-second idle observation (12 PERF samples, 0 underruns). A later clean rebuild produced a different BIN; it is the current uploaded image listed in `bin.sha256` and `manifest.json`. Its 60-second capture is summarized below and exposes a startup scene-load failure. Do not treat the earlier idle capture as evidence for the latest BIN.

A subsequent clean rebuild yielded a different BIN SHA despite identical source SHA, BIN size, `.data`, and `.bss`; the currently listed `bin.sha256` is the later build, which was uploaded and hash-verified. This build nondeterminism remains an artifact-release risk and must be explained before freezing a release BIN.


## Boot scene load finding on the latest image

The post-upload 60-second serial capture is clean for runtime basics (12 PERF records, 0 underruns; free internal heap 19148–21548 B; largest block 10228–11252 B; uptime advanced 5003→60007 ms). It also exposes a persistence acceptance failure: Cardputer reports the selected main scene invalid, attempts `/scenes/test-save.json` backup, then `loadSceneFromStorage()` reports streaming parse failure and loads the default scene. Stage-specific diagnostics localize the rejection to stored material. The project is `test-save`, page0, Synth B voice1 slot5; both A/B publication payloads and the legacy payload are absent. The `out` warning is nonfatal and not the rejection cause. This may be legacy test data on the card; the expected on-screen project has not yet been confirmed. Treat G6 boot restore as FAIL/OPEN and do not overwrite the SD project until the operator confirms what appeared and whether recovery is required.


### Exact missing material record

A second diagnostic BIN `8e1e49e3bab82e940394e9cee38875c0d26bb16a328bb7c4f2a44ee357310357` narrowed both main and backup rejection to the same `test-save` record: page 0, Synth B (voice 1), local slot 5 / global slot 5. None of its dual-publication files or legacy file exists (`v1_g005_a.gpml`, `v1_g005_b.gpml`, `v1_g005.gpml`). Boot therefore falls back to the default Scene. This confirms the fail-closed validator is responding to a dangling Melody descriptor; it does not establish whether that project is disposable test content or user work. The SD card has not been modified. Do not save over this project until the operator confirms what should be present and whether recovery is required.
