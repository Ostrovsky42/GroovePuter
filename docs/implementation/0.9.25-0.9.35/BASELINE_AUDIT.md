# GroovePuter 0.9.25–0.9.35 baseline audit

Audit date: 2026-11-14 (Europe/Belgrade)
Audit branch: `integration/20261114-midi-first-0925-0935`

## Source of truth

- Baseline branch: `feature/0.9.24`.
- Baseline HEAD: `28918d9b78e9a527ce2b5ce201a465af5126f76a` (`test(midi): route binding check follows the drum event copy`).
- GitHub branch API returned the same SHA. GitHub `main` and the locally cached `origin/main` both point to `ec0c8e25c55c9bbcdd6d4c280d9490964459e615`.
- The feature baseline contains 91 commits not in that `main` SHA and no commits behind it; the diff is 240 files, +22,916 / -1,081 lines.
- `f5f06c72c7c46a773e92c8e5f4fbde75bf987bab` is already an ancestor, merged through `6203a578e`. Do not cherry-pick it again.
- GitHub PR #498 (`feature/0.9.24` → `main`) is OPEN and MERGEABLE. `mergeStateStatus=UNSTABLE` because the host job is still pending. Its head SHA exactly matches this baseline. Exact-head CI evidence is split across workflows: [Cardputer ADV product + FS1B](https://github.com/Ostrovsky42/GroovePuter/actions/runs/43541264032) PASS; [SEQTRAK MIDI targets](https://github.com/Ostrovsky42/GroovePuter/actions/runs/43541264096) PASS; [core/SDL/workflow surface](https://github.com/Ostrovsky42/GroovePuter/actions/runs/43541264017) has SDL, smart-play and workflow-surface PASS but host pending; [baseline variants/contracts](https://github.com/Ostrovsky42/GroovePuter/actions/runs/43541264115) PASS. Each run reports the exact baseline SHA.

The original `feature/0.9.24` branch reference remains at the baseline SHA. Work is isolated on this integration branch in the pre-existing clean linked worktree `gp-0924`; the unrelated dirty checkout on `feature/20260929-m0-musical-play-baseline` has not been touched.

## Product state documented at 0.9.24

The 0.9.24 release note reports genre-specific forms and sound presets, including six 32-bar forms, SEQTRAK preset sending, and UI changes. It explicitly says device acceptance was not done. H1 (LIVELY levels) is blocked: the attempted CALM/NORMAL/LIVELY mapping was reverted after measured event counts did not satisfy the intended ordering. H10 (listening kit for forms) was skipped as dependent on H1. These statuses are preserved; no listening or hardware result is inferred.

The repository already has substantial related functionality: Pattern/Scene/Song/Phrase and FORM paths; editable materialization; performance keyboard; USB and UART MIDI transports; MIDI input parsing/dispatch and external note queue; MIDI ownership table; transport clock source/follower/tracker; device capabilities/profile runtime; SMF parser/stream/player and route-profile code. The MIDI-first roadmap should extend these owners and contracts rather than create parallel engines. Exact runtime contracts and full golden-path integration are still under audit.

### Roadmap feature inventory

- **Routing and note lifetime:** role projections, startup profile routing, USB/UART tee and fixed-capacity note ownership already exist. The profile list has no explicit `Internal` profile, and the startup route snapshot does not provide a safe live rebind transaction. The detailed gaps are in `MIDI_CONTRACTS.md`.
- **Profiles:** SEQTRAK, General MIDI, Generic MIDI and Custom are represented. Custom Performance routing is incomplete and intentionally fails closed. This is not yet the complete user-facing Internal/Generic/SEQTRAK experience in the roadmap.
- **Musical macros:** the composition docs discuss bounded intent controls and rhythm has mutation primitives, but no runtime implementation of the requested four `TENSION / ENERGY / SPACE / MUTATION` performance macros was found. Do not count rhythm mutation or the independent Energy FORM feature as those macros.
- **Harmony:** a substantial harmonic subsystem already exists (`harmonic_source`, `harmonic_route`, Song harmonic state/codec and phrase-graph route builder). This is a foundation for the requested follow behavior, not proof that every 0.9.30 invariant or no-harmony fallback is satisfied. Inspect and extend these owners.
- **Arrangement:** Song, Phrase, FORM, materialization and section-transition mechanisms exist. The seven roadmap roles are not seven storage types; later work must map them onto existing Song/FORM semantics and retain transactional placement.
- **MIDI file export:** the current product docs explicitly say there is no user-facing WAV/MIDI file export of the current Song. The SMF parser, streaming player and MIDI import paths are read/play/import facilities; `loopToMelody` exports a captured loop into internal Melody data, not an SMF file. Version 0.9.33 remains a real product gap.
- **Storage:** scene JSON uses versioned nested records and legacy parsing; `.gpp` pages are written as v6 and source accepts v3/v4/v5. Existing host coverage explicitly constructs v3 and v4 pages; a v5 legacy fixture was not found in the reviewed tests. Add/verify it before changing this format.

`docs/architecture/GENERATION_COMPOSITION_MODEL.md` defines Scene as the source of truth after materialization, keeps rhythm identity separate, restricts downstream generation to upstream legal choices, and forbids implicit mutation of materialized user edits. This is a relevant existing architecture contract.

Reference artifacts exist under `docs/testing/0.9.24-base/` (MIDI/TSV fixtures, feature snapshot, manifest and checksums); `tools/gen_probe/` contains MIDI inspection and comparison utilities. Some manifest paths refer to external `/home/gg/pro/...` artifacts, so fixture reproducibility/provenance must be checked before treating the full set as portable golden evidence.

## Parallel work and integration boundaries

Open PRs are not a single linear continuation of `feature/0.9.24`. PR #498 is the baseline candidate itself. PR #493 (`feature/20261011-0921-energy-transitions` → `dev_0.9.13`) is a separate line that touches Performance, MIDI input and FORM. PR #494 (long-melody/HGS/Energy) → #495 (memory prune) → #496 (DSP-A) is a dependent branch chain based on `experiment/performance-hgs-diagnostics`, not on `feature/0.9.24`; these areas are relevant to later arrangement, resource and sound milestones. Their latest check runs are not clean: #494 host failed at `Run current core contracts`; #495 host failed at the same step and product dependency installation failed before firmware build; #496 host failed at the same step. The Actions logs are not available through the current CLI query, so the underlying assertions are not yet known. Their PR descriptions include hardware observations for different SHAs; those do not establish hardware acceptance for this baseline or this goal. Review the diffs, recover actionable failure detail, and rerun relevant gates before considering local integration. PRs #489 (drum phase overflow), #490 (GRAB/audio profiling) and #492 (LoFi shared drum bus) are also open on the older `dev_0.9.13` line. No parallel PR has been merged or cherry-picked into this baseline worktree.

The local repository contains additional MIDI, Performance, harmony, memory and Song work branches/worktrees. The source baseline remains isolated in `integration/20261114-midi-first-0925-0935`; the pre-existing dirty checkout on `feature/20260929-m0-musical-play-baseline` is untouched. Branch names and PR mergeability alone are not evidence that a change is compatible with this baseline.

## Confirmed commands and build environment

- Host suite: `bash tests/run_host_tests.sh` (full local run passed with exit 0 when `TMPDIR` was directed to the worktree build directory; evidence below).
- SDL app: `make -C platform_sdl`; SDL2 2.30.0 and SDL2_gfx 1.0.2 are installed. There is no `scripts/build_sdl.sh` in this baseline.
- Cardputer ADV CI command: `bash scripts/build.sh --warnings all`, then `bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf`.
- `scripts/build.sh` defaults to M5Stack Cardputer, PSRAM disabled, `huge_app`, USB default/CDC; it sets a 32 KiB Arduino loop stack. The CI also builds a separate dynbuffers/FS1B candidate using pinned ESP-IDF.
- Local `arduino-cli` is 1.4.0; installed cores are `esp32:esp32` 3.3.5 and `m5stack:esp32` 3.2.2. CI installs dependencies using `scripts/install_arduino_deps.sh`; reproducibility requires using/checking those pins rather than assuming local installed versions match.

## Test and build evidence

The exact baseline SHA has multiple successful GitHub Actions runs: [Cardputer ADV product + FS1B](https://github.com/Ostrovsky42/GroovePuter/actions/runs/43541264032), [SEQTRAK MIDI targets](https://github.com/Ostrovsky42/GroovePuter/actions/runs/43541264096), and [baseline variants/contracts](https://github.com/Ostrovsky42/GroovePuter/actions/runs/43541264115). The [core workflow](https://github.com/Ostrovsky42/GroovePuter/actions/runs/43541264017) reports SDL, smart-play and workflow-surface PASS; refresh its host status before closing the audit.

The first local full host run exited 2 with `unexpected EOF while looking for matching '"'` at line 962 while the runner script was being edited; a later attempt hit transient `ENOSPC` during SDL-linked compilation. A stable rerun with `TMPDIR` directed to `build/tmp-host-suite` completed with exit 0 (`build/host-suite-home-tmp-rerun.log`). It finished through the final S1 synth-engine-switch tests (600 rapid switches), synth page restore, perform key header and UI continuity tests. Keep earlier failures in the evidence trail; this final run is the canonical result.

Local SDL build `make -C platform_sdl -j2 CXX=g++` completed with exit 0. On `DISPLAY=:1`, the 480x270 `MiniAcid` window navigated Genre → Feel with `]`; Space toggled Play/Stop. Screenshots are in `evidence/sdl-genre.png`, `sdl-feel.png`, `sdl-playing.png` and `sdl-stopped.png`. This used `SDL_AUDIODRIVER=dummy`; it is not audio-quality validation. Attempts to send Alt+H and Alt+V through xdotool did not confirm those modifier shortcuts; they remain unverified by this simulation.

Local exact-baseline FS1B build `env TMPDIR=build/tmp-host-suite bash scripts/build_cardputer_dynbuffers.sh --warnings all` completed with exit 0. Arduino reported 1,742,066 B program storage and 187,612 B globals; fixed DRAM gate passed at 187,612/191,488 B (3,876 B headroom). Link-map verification found the candidate dynamic-FatFs archive and confirmed the stock archive absent. Local app BIN is 1,742,224 B (SHA-256 `b03642cac4122be59c4062f8bb142625d384a0621d25b69a56ee7d21d5b6d18e`); ELF is 37,384,516 B (SHA-256 `893f748d60534d71a3a6c3ece8e0a4c1a537ff410e9e4337e159ee286ac12982`). This used `CDCOnBoot=cdc`, not the release profile. No firmware was flashed.

The live host command emits existing warnings including format-truncation, unused parameters and C++20 bit-field initializer extensions in C++17 host builds. One GCC `-Warray-bounds` diagnostic occurs inside libstdc++ `std::sort` for an eight-note chord buffer; the range and caller require separate confirmation before labeling it a product defect.

Locally passed for this baseline: full host suite, SDL build and limited keyboard-driven UI scenario, and Cardputer ADV FS1B build/link-map/DRAM gate. Storage schema and release/package inventory have been source-reviewed; fixture coverage and full real-time allocation boundaries remain open. No device test has been run.

The standard scene file is JSON with versioned nested state rather than one evident global scene schema. The reader retains legacy layouts and loads into a staged Scene before publishing; the Cardputer writer verifies a temporary file and commits it with backup recovery. Pattern page `.gpp` files are explicitly versioned: current v6, with v3/v4/v5 readers in source; direct legacy fixtures found in the reviewed host tests cover v3 and v4, but not v5. MIDI companion settings accept schema v1 and v2; harmonic state and SMF route profile have independent schemas. This is a useful compatibility foundation, but new fields still need explicit defaults and fixture coverage in their owning codec; there is no single project-wide migration switch.

Packaging is split across multiple paths. `.github/workflows/release.yml` is triggered by `v*` tags, `release/**`, or manual dispatch; it performs reproducibility and FS1B checks and uploads a workflow artifact, but does not itself create a public GitHub Release. Local `scripts/release.sh`/`dev_release.sh` produce app and merged images; `scripts/upload_launcher.sh` flashes a supplied image and is not a packager. The 0.9.20 M5Launcher installation guide is explicitly an unverified draft. App-only versus merged-image addresses and Launcher packaging remain release-readiness work, not verified baseline behavior.

## Known limitations and open risks

- H1 LIVELY and H10 remain unresolved/deferred as documented above.
- The 0.9.24 release note overstated H2 after a deliberate source correction: commit `47795ec1` restored salt selection for UK Garage because press selection changed 92/204 golden bars; `test_genre_press_alternation.cpp` checks that behavior. Acid and Techno remain press-selected. The release note/checklist has now been corrected on this integration branch to match the implementation and golden evidence; no H2 code change is warranted.
- Existing fixture manifests contain machine-local absolute paths.
- Broadly changed baseline (91 commits over cached/current `main`) and several open parallel PRs create integration overlap risk. Review PR #498 and relevant MIDI/generation PRs before merging or replacing branch state.
- Root README and docs navigation still describe v0.9.19, while `MANUAL.md` identifies 0.9.22; the active 0.9.24 release note is more recent. User-facing docs are not yet one consistent description of the candidate.
- The default product firmware CI job links stock FatFs. `scripts/upload.sh` documents that this build reboot-loops with SD inserted and blocks flashing it by default. Use the separate FS1B dynamic-FatFs path for any candidate artifact; do not treat the default product job's successful compile as sufficient.
- MIDI profile/runtime code, MIDI ownership lifecycle on disconnect/route change, and single clock ownership exist in multiple layers; the detailed call graph and failure guarantees are not yet proven by source presence alone.
- Baseline firmware and DRAM metrics are captured from exact-head CI and a local FS1B rebuild in `RESOURCE_BUDGET.md`; the fixed DRAM budget has only 3,884 bytes of headroom on the stock-FatFs product build and the policy labels the exception provisional.

## Baseline decision

**0.9.25: GO.** Baseline identity and clean isolation are verified; local full host, SDL build/UI smoke scenario, FS1B firmware build/link-map and DRAM checks passed. Storage, release paths and core MIDI contracts were source-reviewed. The separate remote Core host job and PR #498 remain in progress/UNSTABLE at the latest check; the equivalent local full host suite passed. The provisional DRAM exception is a continuing resource risk. Hardware status: **NOT TESTED**; hardware testing is intentionally deferred until the 0.9.35 candidate.
