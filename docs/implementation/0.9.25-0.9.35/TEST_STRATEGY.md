# Verification strategy for 0.9.25–0.9.35

Baseline SHA: `28918d9b78e9a527ce2b5ce201a465af5126f76a`. Test results are recorded only after the relevant process exits successfully; CI and local evidence are labeled separately.

## Existing baseline commands

| Layer | Command | Baseline state at audit |
|---|---|---|
| Host suite | `env TMPDIR=build/tmp-host-suite bash tests/run_host_tests.sh` | Local full suite exit 0; log: `build/host-suite-home-tmp-rerun.log`. Earlier attempts failed (shell EOF while runner was concurrently edited; later transient `ENOSPC` compiling an SDL-linked target). |
| SDL application | `make -C platform_sdl -j2 CXX=g++` | Local build PASS. `DISPLAY=:1`; 480x270 window opened; `]` changed Genre → Feel and Space toggled Play/Stop. Screenshots: `evidence/sdl-{genre,feel,playing,stopped}.png`. Dummy audio only. Alt+H/Alt+V were not confirmed through xdotool. |
| ADV default build | `bash scripts/build.sh --warnings all` then `bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf` | Exact-head CI compile/DRAM PASS, but this uses stock FatFs and is unsafe with SD inserted. Not a candidate image. |
| ADV FS1B image | `env TMPDIR=build/tmp-host-suite bash scripts/build_cardputer_dynbuffers.sh --warnings all` | Local exact-head build, candidate-only FatFs link-map proof and DRAM gate PASS. 1,742,066 B program storage; 187,612 B globals; 3,876 B gate headroom. No device flashing. Use this path for the candidate. |
| MIDI targets | Existing workflow / test scripts | Exact-head SEQTRAK target CI PASS; mock transport tests run inside host suite. No physical MIDI evidence. |

`scripts/build_sdl.sh` is absent from the baseline; use the Makefile command. The release workflow's `CDCOnBoot=default` FQBN differs from the Cardputer PR workflow's `CDCOnBoot=cdc`; compare artifacts only with a matching profile.

## Baseline coverage already present

- MIDI ownership, USB output and pending NoteOff release; tee transport and UART queue behavior.
- Mock external MIDI clock follower, timing skew and jitter.
- Device profile projections, MIDI input dispatch, SMF import/parser/player and route behavior.
- Genre H2 behavior (Acid/Techno press alternation; UK Garage salt selection), preset projections and source-level UI contracts.
- Generation stages through Stage 15, rhythm browser, deterministic/golden MIDI probes and musical invariants.
- Pattern page round-trip, corruption refusal, backup recovery, project isolation and selected legacy `.gpp` formats.
- Scene staged-load refusal and atomic file promotion tests in focused scripts; verify they are included in the canonical suite before relying on them for a broad claim.

## Required additions by milestone

- **0.9.26:** fake transport traces for overlapping same-pitch owners, route changes while held, Stop/project/Song transitions, endpoint disconnect/rejoin and integrated queue overflow.
- **0.9.27:** profile serialization/migration, Generic fallback, incomplete Custom refusal and fake SEQTRAK ordering.
- **0.9.28:** `bash tests/run_performance_macro_engine_tests.sh` checks state bounds, no-op/reversibility, seeded determinism, anchor/onset preservation, harmony fallback, transactional rejection, legal output and zero heap allocations. The full host suite includes this runner; the engine is not yet wired to live UI/playback (0.9.29).
- **0.9.29:** SDL action sequence through Performance, macro selection/value display/reset, Play/Stop, section changes, page return and persistence.
- **0.9.30–0.9.31:** harmonic fallback and chord transitions, motif identity, transactional Song form generation, capacity refusal, Undo and round-trip fixtures.
- **0.9.32:** CC ranges/order/rate limit/deduplication, Stop reset and disconnect cleanup.
- **0.9.33:** parse exported SMF with an independent parser and verify tempo, channels, delta times, track endings, deterministic round-trip, overlaps and failed-write cleanup.
- **0.9.34–0.9.35:** reproducible release-profile FS1B artifact, checksums/provenance, old/new project fixtures, full SDL Golden Path and final resource delta.

## Evidence rules

- Hardware status before 0.9.35 remains **NOT TESTED**.
- A passing unit test proves its function, not the whole Golden Path.
- A firmware compile does not prove runtime or memory safety on hardware.
- A remote CI PASS is not a local reproducibility result.
- Keep failed tests, warnings with potential safety impact, and unavailable evidence in the milestone log; do not silently rerun or overwrite their history.
