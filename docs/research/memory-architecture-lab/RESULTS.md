# MAO-0 results and outstanding work

Status: **BLOCKED / PARTIAL**, 2026-11-24. This is an evidence checkpoint, not a claim of MAO-0 completion or approval to begin optimization. Files in this directory record CI-derived parent-build totals, a 40-symbol subset, source-based lifetime map, candidate hypotheses, reproduction and future hardware protocol. See BASELINE.md for provenance and limitations.

Local commands attempted:

- `ARDUINO_CLI="$PWD/platform_sdl/bin/arduino-cli" bash scripts/install_arduino_deps.sh`: exit 1, Arduino/M5Stack indexes inaccessible due to restricted network; core and toolchain absent.
- `ARDUINO_CLI="$PWD/platform_sdl/bin/arduino-cli" bash scripts/build_cardputer_memory_baseline.sh normal product`: exit 127, `rsync` missing; no ELF generated. Other target profiles require the same unavailable dependencies and were not run.
- `bash tests/run_host_tests.sh`: exit 1 during Rhythm Browser engine test, missing `sdl2-config`, SDL2_gfx and `SDL.h`. Earlier test output is not a full-suite pass.
- `make -C platform_sdl clean all CXX=g++`: exit 2, missing SDL2 development headers/configuration. CI Core success at the parent SHA is separate historical evidence.
- GitHub Actions artifact metadata and CI logs were recovered, but temporary artifact download returned 503. The archive contents, including presence of ELF and MAP, remain unverified.

Remaining to complete MAO-0 offline: install the exact pinned dependencies; rerun four product profiles from the exact research source; save ELF/MAP, SDK config/linker scripts and full tool output with hashes; inventory at least 30 largest IRAM functions and 30 mutable symbols with owner, actual alignment, ISR/DMA constraints, concurrency and source; reconcile section/COMMON/fill accounting and module attribution; verify actual cache settings and linker regions. Expand dynamic allocation and lifecycle inventory to libraries and failure paths. Re-run host, SDL and supported trace tests; resolve any actual failures without weakening tests. Reassess candidates from exact-head evidence. Hardware runtime measurements remain separately BLOCKED until the operator can run WORKLOADS.md; do not fabricate PASS.
