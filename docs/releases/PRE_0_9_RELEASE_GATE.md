# GroovePuter 0.9 — Pre-release Gate

## Purpose

Define one reproducible path from the current `dev` branch to a release candidate. This document separates release blockers, hardware-only acceptance, and work deliberately deferred beyond 0.9.

A change is not considered release-ready merely because its PR is mergeable or contains source tests. The relevant host/build jobs and Cardputer ADV acceptance must be completed against the exact candidate commit.

## Current baseline

Baseline when this gate was created:

```text
dev: dd63e73fec46f043b09be93a6612cd93fddc399e
```

The source-regression assertion broken by the held-control acceleration change was repaired in PR #111. The repair was test-only. GitHub Actions did not run for the connector-authored commit, so the complete current `dev` gate still needs a fresh run.

## Hardware list

- M5Stack Cardputer ADV.
- USB-C data cable with data support.
- Headphones; built-in speaker may be used for basic checks.
- microSD card containing known-good scene, pattern-page, and MIDI fixtures.
- Optional Yamaha SEQTRAK for USB-MIDI, clock, transport, and recording acceptance.

## Wiring

No GPIO wiring is required.

For MIDI acceptance, use the same powered USB-host/data connection already validated for Cardputer ADV to SEQTRAK. Do not change MIDI topology during a comparison run.

## Build and flash

```bash
git fetch origin
git switch dev
git reset --hard origin/dev
rm -rf build .pio .pioenvs .piolibdeps
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
```

Required automated jobs for the exact release-candidate SHA:

- Core host regressions;
- SDL build;
- Cardputer ADV build;
- fixed-DRAM budget check;
- Cardputer ADV SEQTRAK MIDI-only build;
- Phrase Core regressions where enabled.

Flash using the repository's normal Cardputer ADV upload command and monitor serial at `115200` baud.

## Release blockers

### P0-A — synth correctness and persistence

The detailed evidence is in:

- `docs/reviews/SYNTH_ENGINE_AUDIT_0_9.md`;
- `docs/reviews/SYNTH_ENGINE_AUDIT_0_9_CURRENT_STATUS.md`.

Before release, fix or remove from the selectable surface:

- TB303 note lifecycle and missing amplitude-envelope behavior;
- live-note clamp versus NoteOff mismatch;
- scene/load logic that can replace the selected synth TYPE;
- persistence loss for the sixth generic synth parameter.

The following P1 items also require focused tests and listening acceptance before calling the synth surface stable:

- AY and SN76489 pitch collapse;
- non-TB303 defaults that can start with maximum noise;
- TB303/SID DC before per-voice effects;
- realtime allocation and filter-stability risks;
- cross-engine loudness mismatch;
- distortion enable/drive restoration;
- truthful, usable parameter ranges.

Held-arrow acceleration from PR #107 only partially mitigates range-insensitive knob stepping; it does not close the range-design issue.

### P0-B — project-scoped pattern storage

PR #102 is the implementation path.

Do not merge until all of these are verified on its current head:

- host and build jobs pass;
- project A and project B keep independent page files;
- Save As copies page files into the new namespace;
- New starts blank without damaging the previous project;
- Clear removes `.gpp`, `.tmp`, and `.bak` files only for the selected project;
- legacy root-level pages migrate once and remain readable;
- corrupt pages and backup recovery remain isolated;
- the status chrome shows the real page/bank/slot address;
- reboot and autosave recovery do not restore cleared material.

Use `docs/tests/PROJECT_PATTERN_STORAGE_CARDPUTER_ADV.md` for the hardware sequence.

### P0-C — genre/variant truthfulness

PR #110 is the implementation path.

Do not merge until all of these are verified on its current head:

- a genre exposes only its compatible variants;
- BASE remains procedural and does not silently select Atlas material;
- Atlas P1/P2/P3 roles can be selected and materialized explicitly;
- PROFILE ONLY preserves pattern contents;
- MATERIALIZE changes only the current pattern set;
- MATERIALIZE+BPM applies the displayed tempo;
- Acid, Rave, Techno, and Synthwave BASE remain behaviorally unchanged for the same seed;
- sparse Dub/Trip-Hop output is intentional, bounded, and not treated as a failed generation;
- direct Song-cell generation retains its documented atomic non-empty rule;
- no allocation failure or retry loop appears on serial.

Use `docs/stages/GENRE_VARIANT_CORRECTNESS.md` for the hardware sequence.

## Merged changes still awaiting hardware acceptance

### Held-value acceleration and BPM chrome — PR #107

Validate:

- tap changes one normal step;
- hold ramps `x1 -> x2 -> x4 -> x5`;
- release and direction reversal reset the ramp;
- discrete selectors do not accelerate;
- BPM updates while transport state is otherwise unchanged;
- the status line does not clip in both themes.

### MIDI/SMF lifecycle

Run the final candidate through:

- GP MASTER and FILE/PROJECT tempo modes;
- Start, Stop, Continue, and position changes;
- live keyboard, arpeggiator, chord, strum, ratchet, Euclidean, and imported SMF notes;
- mutes during playback;
- page switching under dense MIDI;
- all-notes-off cleanup after stop, route change, mute, and target change;
- 32-bar SEQTRAK recording with no end-of-bar stall or growing drift.

Known limitation to confirm explicitly: direct MIDI Player mute hotkeys `1–9` were previously reported as not working reliably without opening the mute page.

### Memory and runtime stability

The fixed-DRAM threshold is not a substitute for runtime telemetry. Record for both normal and SEQTRAK MIDI-only profiles:

- free heap at boot;
- minimum free heap during the run;
- largest free block;
- PSRAM allocation failures;
- loop/audio task stack high-water marks;
- watchdogs, audio underruns, queue overflows, and dense-MIDI crashes.

Required soak:

1. load and save projects repeatedly;
2. switch pages and themes during playback;
3. generate and materialize multiple genres;
4. play dense SMF while sending MIDI to SEQTRAK;
5. run for at least 30 minutes without a monotonic memory loss or crash.

## Deferred beyond 0.9

These directions are intentionally excluded from the release candidate:

- PR #101 Song-generation UX prototype — closed; rebuild cleanly without temporary apply scripts after the release blockers;
- PR #90 Phrase Arranger Stage 2 — closed; rebuild only the unique arranger layer on fresh `dev` after 0.9;
- new genre families such as true Minimal Techno, Boom Bap, and Lo-Fi;
- new Atlas corpus material;
- framework-level navigation or architecture rewrites;
- broad deletion of audit candidates without focused reachability and persistence proof.

Branches may remain for forensic reference, but closed experimental PRs are not release inputs.

## Expected behavior

A valid release candidate must:

- boot to a usable page without a black screen or recovery loop;
- show the current BPM and an accurate pattern address where applicable;
- preserve project, scene, pattern, synth TYPE, and parameter state across reboot;
- generate musically appropriate sparse and dense material without silence being confused with failure;
- play every advertised synth without stuck notes, gross pitch collapse, unintended maximum noise, or destructive level jumps;
- maintain stable audio while navigating, generating, saving, and sending dense MIDI;
- stop cleanly with no hanging internal or external notes.

## Troubleshooting

### Core regressions fail on synth MORE navigation

Confirm the branch includes PR #111. The regression must inspect the `more_tab_` branch semantically and must not require a complete switch case to remain on one source line.

### A PR is mergeable but has no Actions runs

Treat it as unverified. Connector-authored commits may not start workflows. Run the documented commands locally or trigger a repository workflow against the exact head before hardware acceptance.

### A project appears to share pages with another project

Stop testing other features. Capture both project names, encoded folder names, the current address, the filesystem listing, and the reboot result. This is a release-blocking storage failure.

### A sparse genre produces silence

Identify the selected genre, variant, role, destination path, seed, and track. `Minimal Space P3` may intentionally leave Synth B empty on explicit materialization paths; direct Song-cell generation has a different non-empty atomic contract.

### A synth hisses, sticks, or changes TYPE after reload

Capture engine, note, parameter values, scene name, save/reload steps, and serial output. Do not accept the candidate until the relevant synth-audit blocker is resolved.

## Final acceptance checklist

### Automated

- [ ] Exact release-candidate SHA is recorded.
- [ ] Core host regressions pass.
- [ ] SDL build passes.
- [ ] Cardputer ADV build passes without new warnings.
- [ ] fixed-DRAM gate passes for normal firmware.
- [ ] SEQTRAK MIDI-only build and fixed-DRAM gate pass.
- [ ] Phrase Core regressions pass.

### Storage and persistence

- [ ] #102 acceptance is complete on hardware.
- [ ] Save, Save As, New, Clear, reboot, and recovery are correct.
- [ ] Projects do not share pattern pages.
- [ ] Pattern address is correct on Synth A, Synth B, and Drums.
- [ ] Synth TYPE and every advertised parameter survive reload.

### Audio and generation

- [ ] All synth P0 blockers are fixed or the affected engine/control is removed from the release surface.
- [ ] AY and SN76489 pitch checks pass.
- [ ] No engine starts with unintended maximum noise.
- [ ] No stuck notes, destructive clicks, silence regressions, or gross loudness jumps occur.
- [ ] #110 genre/variant acceptance is complete.
- [ ] Acid, Rave, Techno, and Synthwave accepted behavior is unchanged.

### UI

- [ ] #107 tap/hold/reset behavior is accepted.
- [ ] BPM and pattern address remain visible and unclipped.
- [ ] Every visible page has a reliable exit path.
- [ ] Global and local key bindings do not collide.

### MIDI and stability

- [ ] Start/Stop/Continue and tempo-source switching are accepted.
- [ ] Dense SMF playback does not stall at bar boundaries.
- [ ] Internal and external NoteOff cleanup is complete.
- [ ] MIDI Player mute behavior is either fixed and accepted or explicitly removed from release documentation.
- [ ] 30-minute runtime soak passes without crash, watchdog, allocation failure, or monotonic memory loss.

Only after every applicable item is checked should `dev` be tagged as the 0.9 release candidate.
