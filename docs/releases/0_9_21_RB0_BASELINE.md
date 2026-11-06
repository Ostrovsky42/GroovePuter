# Rhythm Browser RB-0 baseline evidence

Baseline source: `release/0.9.20-ours` / `v0.9.20` local tag → `85b53e6b77bd167c23cf23fd78e0832ba9e6a0ef`. The product owner confirmed the branch as the accepted 0.9.20 starting point. Historical release reports for other SHAs are not evidence of a build at this SHA. A matching accepted binary or flashed-image provenance has not been independently established.

The feature worktree was created from that SHA clean, before these documentation edits. The branch has no added UMG candidate commits; `af4d510` is nevertheless already an ancestor of the accepted baseline. The separate archived UMG candidate HEAD `319e6dcc` is not an ancestor of it. The frozen experiment's documentation and private archive are in the adjacent `umg-archive-worktree` and `umg-freeze-20261106`; neither is a product dependency.

## Cardputer build and memory

On 2026-11-06, `bash scripts/build.sh` completed exit 0 on this source tree (only PLAN and goal documentation differed from the clean commit). FQBN: `m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`; loop stack 32768 bytes. Sketch 1,651,378 / 3,145,728 bytes; globals 185,788 / 327,680 bytes. App BIN SHA-256 `11d123bf4541578cbda88909c081eff2dfdde092547e843c32edf0d8434e327e`; ELF `c3b5eb97bf2c0ff2af33e27513dfccc1d7d1fde38ded2e2d8900abdc85ad257f`. Build log is in the local preservation package `rb0-cardputer-build.log`. This is a build witness, not a reproducibility, flash or hardware acceptance claim.

Toolchain inventory: Arduino CLI 1.4.0, installed `m5stack:esp32` 3.2.2, host GCC 13.3.0 (Ubuntu 24.04). This inventory is not a lockfile or proof of a bit-for-bit reproducible build.

`bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf` exited 0: 185,788 / 191,488 bytes, `.dram0.data` 43,340 and `.dram0.bss` 142,448; arithmetic margin 5,700 bytes. The checker explicitly calls the exception provisional with policy items 5–7 pending. No target runtime stack, heap, CPU or timing measurements were obtained. Build-log SHA-256 `ddba87933f2922cccffaaaca70c72e2f66dc6d4a5daaf99a46117fce15cb9ba3`; DRAM-log SHA-256 `0bf71e46ca5c492687d9fec993ab3334435c151754e0cd44a47b7e85e7e6bae6`.

`bash scripts/report_cardputer_memory_baseline.sh build/cardputer-adv-current/GroovePuter.ino.elf` exited 0. It reports active Scene and load scratch at 26,376 bytes each; detailed symbol data is in `rb0-memory-report.log`, SHA-256 `d6bc89c4407537434255ca476dc386308147b1cb5b3f2c9e72af047f874a2c58`. The report explicitly says the provisional gate is not safety-derived; it also shows the historical 122,880-byte reference is exceeded by 62,908 bytes.

## Initial input and ownership observations

The baseline `src/ui/docs/keys.md` assigns DRUMS Tab to sequencer/automation, Enter to toggle a hit, G to drum generation and Fn+Tab globally to next workflow. Space is transport unless a page consumes it. These are documented mappings, not a physical key-matrix or collision audit; no browser shortcut is selected. `src/dsp/miniacid_engine.cpp` around line 2812 contains an existing musical-bar boundary, `src/generation/migration/live_song_arrangement_activation.h` contains an existing BAR_START publication owner, and `src/state/undo_owner.h` has bounded Undo payload capacity 1,536 bytes. These call sites do not establish that read-only drum substitution is possible; RB-301 must trace and prove it before implementation.

The DRUMS page entry point wraps legacy multipage handling in `src/ui/pages/drum_sequencer_page.cpp` (`DrumSequencerPage::handleEvent`); `drum_sequencer_page_legacy.h` dispatches through child controls and `MultiPage`. A modal implementation needs a complete focus and modifier audit before intercepting these handlers.

`MiniAcidDisplay::handleEvent` handles meta/Fn+Tab as global workflow navigation around line 654, before dispatching to `currentPage->handleEvent` around line 789. The browser must preserve that global route while owning its unmodified local navigation. This is an initial source audit, not confirmation of all keyboard normalizations or modifier collisions.

The sandboxed `arduino-cli board list --format json` returned an empty list with serial-discovery `operation not permitted`; this does not establish whether a device is physically attached. No Cardputer interaction or listening acceptance was performed.

## Host and core checks

The first `bash scripts/ci/run_core.sh` in the sandbox exited 1 at Rhythm Stage 2 because LeakSanitizer could not inspect threads in that environment (log SHA-256 `0326c998973aa75fdbfc1ad90948d5128d2c583cff2d4f49c92da5d7c03d4c73`). The separate sandboxed `bash tests/run_host_tests.sh` reached its final S1 synth-switch gate but likewise exited 1 with a LeakSanitizer fatal error (log SHA-256 `943711f70bfacc72f7b7f78d2226150f7e3f58be37d0e9b6b8b1c0d24428cb25`). The already-built S1 binary was run outside the sandbox and exited 0, including 600 rapid switches; this diagnoses that focused environmental failure but does not make the full suite pass.

The full host rerun outside the sandbox, `bash tests/run_host_tests.sh`, exited 0. Its final gate reported `S1: a Synth page restores the MORE tab before its components exist: PASS`; log `rb0-host-unsandboxed.log` SHA-256 `15d3fa04af8f4db829c93aee3b5ab1f4881ce63267644d1afdf2ef26d341afe5`. The full core rerun outside the sandbox is still running; its final outcome must be recorded from the completed log before RB-0 can be closed. The independent core checkout is `/tmp/rb0-core-baseline` at the same SHA; temporary checkout and running processes are not durable evidence.
