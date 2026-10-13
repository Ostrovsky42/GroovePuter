# Smart Play / HGS execution progress

## SP-0 baseline

- Recovery worktree: `/home/gg/.codex/worktrees/performance-smart-play`
- Branch: `feature/smart-play-return`
- Starting HEAD: `6c7f971c512b7c5fabc37e83006f18b7c082cb8c`
- Base release: `v0.9.19` = `a5f74f73`
- `origin/main`: `a51213c4ab5d14eca3d88e9882d917b021ebd21b`
- HGS v2 source SHA-256: `d3a288a2c57dbdda1e04c8cd295429a699ffb5ec55c0dae4fd8b47e4398ef7c2`
- Existing restored code commits: `42b57ec1`, `6c7f971c`.
- The worktree contained only the authored Smart Play/HGS plan/spec artifacts before this runner; there were no tracked source edits beyond the two restored commits.
- Initial source diff against `v0.9.19`: `src/generation/roles/chord_progression.h`, `src/generation/tonal/tonal_materializer.cpp`, `src/input/performance_harmony_context.h`, `src/input/performance_keyboard.cpp`, `src/input/performance_keyboard.h`, `src/input/performance_smart_play.h`, `tests/run_host_tests.sh`, `tests/test_performance_chord_quality.cpp`, `tests/test_performance_harmony_context.cpp`, `tests/test_performance_smart_arp.cpp`, `tests/test_performance_smart_play.cpp`.

### Existing focused tests

Command: `bash tests/run_smart_play_tests.sh`

Result: PASS (`model quality legacy_context arp keyboard external`). The individual keyboard test reports `PERFORM keyboard external notes: PASS`.

Unknown-target behavior: `bash tests/run_smart_play_tests.sh typo` exits 2 and identifies the invalid target.

### ADV baseline

Command: `scripts/build.sh --warnings all`

FQBN: `m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc`.

Result: exit 0. Flash 1,525,362 bytes (48% of 3,145,728); static/global RAM 183,668 bytes (56% of 327,680), 144,012 bytes left. The warning-enabled build emitted existing compiler warnings from board headers and project dependencies; no new production source changes were part of this baseline.

### Toolchain

GCC 13.3.0 is available. `clang++` is not installed, so cross-compiler coverage cannot run in this environment unless the toolchain becomes available.

### Current branch implementation scope

Restored code at baseline provides pure Smart Play interpretation, legacy P1R harmony context validation, and Smart Arp pool/configuration behavior. It does not yet provide the provider-neutral shared harmonic registry/snapshot, end-to-end Cardputer/MIDI input routing, source/session/cable-owned external note lifecycle, truthful live material publication to Cardputer+SDL, standalone persisted controls, or the genre-aware HGS graph/corpus/planner integration.

### Focused runner

`tests/run_smart_play_tests.sh` supports exact targets `model`, `quality`, `legacy_context`, `arp`, `keyboard`, `external`; no arguments runs all six. It honors `CXX` and `SMART_PLAY_CXXFLAGS` and writes binaries under `build/smart-play-tests/`.

## Worktree list at baseline

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid
HEAD b5cf8e5b86fe37f5cf71c43e20f0d94bc3df5d33
branch refs/heads/feature/20260929-m0-musical-play-baseline

worktree /home/gg/.codex/worktrees/7c5b/miniacid
HEAD d05fd1fbce035292c12f50173fd3554f167a33d0
branch refs/heads/experimental/performance-layer

worktree /home/gg/.codex/worktrees/7c5b/miniacid-live-midi
HEAD d05fd1fbce035292c12f50173fd3554f167a33d0
branch refs/heads/feature/performance-live-midi

worktree /home/gg/.codex/worktrees/7c5b/miniacid-v0919
HEAD a5f74f7332e41f12e05d79136725ad42ed55681b
detached

worktree /home/gg/.codex/worktrees/performance-smart-play
HEAD 6c7f971c512b7c5fabc37e83006f18b7c082cb8c
branch refs/heads/feature/smart-play-return

worktree /home/gg/.local/share/kilo/worktree-pool/miniacid-87e25a96b9a6/marshy-painter
HEAD a51213c4ab5d14eca3d88e9882d917b021ebd21b
detached

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/GroovePuter-e1
HEAD 9323e14f1dc0f5b7e29228c00713f7746363638b
branch refs/heads/agent/20260823-01-0.9.9-f08-listen-surface

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/GroovePuter-p1d
HEAD 69474f39dc429122f4cc28d8f0dffd3ace19dbd9
branch refs/heads/agent/20260824-02-0.9.9-p1d-song-audition-lease

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/GroovePuter-p2-86c689cb-20260912
HEAD 86c689cb3035e2df9f1ff2a0a07e4571e21d75e5
detached

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-build-exact-cardputer-adv
HEAD 6bae461aa1593ef3ef27b537dac7c482976d125c
branch refs/heads/feature/20260911-01-g4-cc1a-p1-acid-articulation-authority

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-genre-calibration
HEAD fcd0d77da5ed6ef38419547477ab26e77ec6ff26
branch refs/heads/feature/20260909-genre-calibration

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-gf2-i1
HEAD 8c9ad2574e18f77d26cd606c95d85eccddc4100b
branch refs/heads/agent/20260903-01-0.9.10-gf2-i1-tempo-corridor-arbitration

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-gf2-i2
HEAD 95426b6dcc7997dbf9aa3b7e20ffb016b99f6910
branch refs/heads/agent/20260903-02-0.9.10-gf2-i2-profile-feel-contract

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-gf2-i2a
HEAD f3a288b931c51242720a1192cbdee83fb067ad63
branch refs/heads/agent/20260903-04-0.9.10-gf2-i2a-feel-amplitude

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-gf2-i3
HEAD 3e23ef5e521845ae8a96d98779b8b617b481929b
branch refs/heads/agent/20260903-05-0.9.10-gf2-i3-phrase-law-execution

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-gf2-tools
HEAD d471b82558cdbc0193705d97659c8f87a4d4fb7d
branch refs/heads/agent/20260903-03-0.9.10-gf2-analysis-tooling-on-line

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-i1-freeze
HEAD fb30bbb93f739d8b3e4514ae931cbc73fc8eeb09
branch refs/heads/agent/20260828-05-0.9.9-phrase-i1-end-to-end-synth-phrase

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-ie1
HEAD 0992f3d8b7af317d91ff166b641b893f806a921c
branch refs/heads/ie1-local

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-iui1
HEAD 947cc6f90e89d9aef71694159192e3b4051c31bc
branch refs/heads/integration/20260824-03-0.9.9-ui-picker-lease

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-p4i-base
HEAD 8f0910381d1c73e23642a2003379ff676d15ae46
detached

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-p4i-build-probe
HEAD 8f0910381d1c73e23642a2003379ff676d15ae46
detached

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-p4i-head
HEAD fec2df5c97ca43edee79a842fadd2238912fe491
detached

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-p4i-head-build-probe
HEAD 84badd3b7f2f8d171948126d94ec17e1fa7e8463
branch refs/heads/agent/20260824-07-0.9.9-p4i-song-alt-routing-closure

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-phrase-mem-audit
HEAD 03f633d2e431c75ec9b2f4402507b9253f785a74
branch refs/heads/research/20260830-01-0.9.9-phrase-memory-budget-audit

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-phw-p1
HEAD db4e0f4993eeaeeb4b965c4f07333139216eb4fe
branch refs/heads/agent/20260831-03-0.9.9-phw-p1-phrase-product-workflow

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-pmb-a2
HEAD 3e3c997fbde003ead9efc80b31ae2c92abc59859
branch refs/heads/research/20260831-01-0.9.9-pmb-a2-legacy-replay-preflight

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-pmb-p1
HEAD df426a5d596888580b88499b0b1aad03bc9006e4
branch refs/heads/agent/20260831-02-0.9.9-pmb-p1-bounded-phrase-prepare

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-pr-sync
HEAD 8b5ce4da4255ec4641a700749a185aca7cf27603
branch refs/heads/pr441-sync

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-ui-final
HEAD 31f42ac45183eb7bc65254a3195adf980cae328e
branch refs/heads/agent/20260828-09-0.9.9-ui-final-product-pass

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/gp-ui-test
HEAD 2e7e7d0e78f75fc6be1d7d67e1794bc75b0518ca
branch refs/heads/feature/20260907-01-u4b6-make-phrase-entry

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/mini_ui_refactor
HEAD 7c86345b76df17e593023dd7800b7427b267ab8a
branch refs/heads/mini_ui_refactor

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-0917-combined
HEAD 9e07aaa9a1618d72f8d97e8b56f894e2aadb66a1
branch refs/heads/feature/20261007-0917-combined-poly

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-0917-develop
HEAD e43cd96c80b14efd09b6a57f6423555285581e31
branch refs/heads/feature/20261006-0917-develop-ready-take

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-0917-midi-sync-ux
HEAD 12cdc2e33b25cff50ebbfd062b0c6a47901d3822
branch refs/heads/feature/20261007-0917-melody-poly

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-0918
HEAD c3031d28f6655ec78271d03d4a5e8d8224273d3e
branch refs/heads/feature/20261008-0918-sound-and-oled-theme

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-0918-flash-ae1ed296
HEAD 4be17e3cc90a07011a9313606b1d89de1a4f1061
branch refs/heads/feature/20261009-0918-grab-midi

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-0918-rc
HEAD 4222ac0e177c95c7ba476f6da8834bd80b3455ad
branch refs/heads/feature/20261010-0918-release-candidate

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-0919
HEAD 6f9c696f502a3764850346402ba891dbdd8e9d7d
branch refs/heads/feature/20261010-0919-ui-frame-stages

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-0919-p1
HEAD bf312f1cbb4e250b001c0ef4840c060c5569e3da
branch refs/heads/feature/20261010-0919-dsp-p1

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-0919-s1
HEAD 52bf79a211c24c031ecfb77271c448884aa2c6f4
branch refs/heads/feature/20261010-0919-s1-engine-alloc

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-0920
HEAD bb98220fc6fc08b049725b62c137f5eedca60b36
branch refs/heads/feature/20261011-0920-roadmap

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-acceptance-84a0076f
HEAD 84a0076ffbf858a0d44e759de3a40803d06e10ad
detached

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-acceptance-e29a2a86
HEAD e29a2a8669a17aab2c7308bbdf2aea7f82b8a850
detached

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-acceptance-fad8dfde
HEAD fad8dfde01d7fa60fe82405446db572484a52c23
detached

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-chords-research
HEAD e43cd96c80b14efd09b6a57f6423555285581e31
detached

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-cr-p0
HEAD f92933df48626fb551cbba9189ad976b6d29764b
branch refs/heads/feature/20261009-0918-audio-cores

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-dsp-p0
HEAD 3be1f9cd21bc96e5921a739fa0a7e39963ea396b
branch refs/heads/fix/20261011-0919-dsp-phase

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-dsp-polish
HEAD b534051842b3d83dcdd6e653e3751ac4bb0a005f
branch refs/heads/feature/20261009-internal-dsp-correctness

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-energy
HEAD 6d107e8b6d95d263ef90a873d05aa3089e057990
branch refs/heads/feature/20261011-0921-energy-transitions

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-h0-r1
HEAD 27ec8474ae2a85dcf55c7de02bfbb0ab6d80b9e0
branch refs/heads/feature/20261006-h0-r1-harmonic-rhythm

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-h0r1-base
HEAD 4f1304916389c5af315faa24127555c062a32fa9
detached

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-h0r1-review
HEAD 4f1304916389c5af315faa24127555c062a32fa9
detached

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-lofi
HEAD 415d32a0c723cc82077922c4093ce91414894e2e
branch refs/heads/fix/20261011-0919-lofi-drum-bus

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-memory-r1
HEAD 3b2bd280e9c39d8d5cbdf425db6ef18126e650be
branch refs/heads/feature/20260908-0.9.10-memory-r1-dram-recovery

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-memory-r1-wt1
HEAD 9fa4bb12e3c4826720f16ace865a2c4de8e4cc0c
branch refs/heads/feature/20260908-0.9.10-memory-r1-wt1

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-midi-clock-fix
HEAD 5774c5b3071d74868130d0cefef38233c5d0e496
branch refs/heads/fix/20261005-midi-clock-boundary

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-midi-clock-probe
HEAD 4f1304916389c5af315faa24127555c062a32fa9
branch refs/heads/research/20261005-midi-clock-probe

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-midi-import-final
HEAD f67aa0ab0f044f04384df47f6f3c24391fd24dbb
branch refs/heads/feature/midi-workspace-import

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-midi-import-integration
HEAD 3fda75e2da053c8d5e318a86cb310a665edb80c0
branch refs/heads/integration/midi-import-candidate-20261004

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-midi-io-nanokey2
HEAD 02bfe444647fc6755e0f21f58626ee654c311d0f
branch refs/heads/feature/20260907-midi-io-nanokey2

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-pr487
HEAD a0699be169ac4f7ac676f90b577a1ed6512a6e89
branch refs/heads/fix/20261011-0919-manual-hub-midi

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-wt-0914-15
HEAD fad8dfde01d7fa60fe82405446db572484a52c23
branch refs/heads/feature/20261001-0914-15-candidate

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-wt-sd-residency
HEAD c408376a82d9e2ca07004e987726cac59bf1bf4f
branch refs/heads/feature/20260906-05-0.9.10-sd-runtime-residency

worktree /home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid-wt-v099
HEAD 0a2a6211ef00dcf2214dfd4704b6c34b424b1c9d
detached

