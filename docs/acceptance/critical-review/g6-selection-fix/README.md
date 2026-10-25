# G6 project-selection rollback — software fix

Status: host regression PASS; SDL/ADV builds pending for final source; hardware G6 OPEN.

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
- Hardware Save/Reset/Load, SD failure, G1/G3/G6 and runtime memory tests are NOT TESTED on this repair image.

No push, merge, or flash performed.
