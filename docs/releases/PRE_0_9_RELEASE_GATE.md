# GroovePuter 0.9 — Pre-release Gate

## Purpose

Track the exact 0.9 release candidate after the final correctness fixes, distinguish code completion from hardware evidence, and keep non-critical synth/refactor work out of the RC path.

## Current baseline

```text
release branch: dev_0.9
base SHA: 0afd24fe5f4b0b2d549214abe2ed1a7eb7f3448c
stabilization PR: #131
stabilization branch: release/0.9-final-stabilization
```

The base includes:

- #102 project-scoped pattern storage;
- #125 AY/SN pitch and logical NoteOff lifecycle;
- #130 removal of the user-facing TEXTURE page and legacy page-id redirect;
- #143 versioned synth persistence, normal Scene Load ownership, engine-native missing-patch defaults and Save/Load revision result semantics.

PR #110 is a rejected experiment and is not a release input.

## Code complete for the RC

### Persistence and Scene ownership — merged in #143

- Scene `synthState` schema is versioned.
- Synth A/B persist independent stable TYPE plus normalized parameter slots `0..5`.
- legacy TB303-shaped `synthParams` remain decode-only input and are not reserialized.
- malformed/unknown versioned synth state fails transactionally; the current Scene is not half-loaded.
- normal Load restores stored TYPE/patch and does not call Genre timbre/texture projection over it.
- AY/SH101/SN76489/WAVEMORPH missing legacy patch data uses each engine's own runtime defaults.
- Save snapshot follows the pending swappable voice during the engine crossfade, so a quick Save cannot combine the new TYPE with the old engine's parameters.
- successful explicit Save/Load establishes the correct revision baseline; failed operations preserve dirty/current state; recovery autosave does not impersonate explicit Save.

### Final stabilization — PR #131

- TB303 has a separate amplitude ADSR-style lifecycle from its filter envelope.
- NoteOff starts a bounded release and reaches silence.
- active legato slide does not fully retrigger the amplitude envelope.
- TB303 `Volume` controls output exactly once.
- the optional TB303 sub component is mixed exactly once.
- distortion enable restores a safe audible drive only when stored drive is below the working threshold.
- FEEL persistent changes mark Scene dirty once; preset browsing remains preview-only.
- Genre browsing remains preview-only; Apply/materialization marks one logical Scene mutation.
- the three-page Generate workflow remains `GENRE 1/3 → FEEL 2/3 → GENERATION 3/3`.

There are no remaining known P0/P1 code blockers owned by #131.

## Critical-path exclusions

- #132 GenreManager ownership refactor is post-0.9 work.
- #142 AY articulation work is not required for the 0.9 RC.
- #139 SID articulation is optional and may enter the RC only after a clean Cardputer ADV SID listening smoke; otherwise it remains post-0.9.
- #134 TextureMode runtime removal is accepted before RC only if rebasing/integrating it onto the #143 persistence baseline is conflict-light and the combined legacy Scene regression stays green. Any ambiguous Scene migration or load-ownership interaction defers #134 until after 0.9.

## Persistence formats

- pattern `.gpp` remains version 3;
- project-scoped namespaces from #102 remain unchanged;
- Scene synth state is version 1 and normalized; legacy synth fields are decode-only compatibility input.

## Automated gate

Run against the exact final PR head:

```bash
git fetch origin
git switch release/0.9-final-stabilization
git reset --hard origin/release/0.9-final-stabilization
rm -rf build .pio .pioenvs .piolibdeps
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Required jobs:

- Release 0.9 focused TB303/DST contracts;
- synth persistence/load ownership focused contracts;
- FEEL/Genre revision ownership;
- Core host regressions;
- SDL build;
- Cardputer ADV build and fixed-DRAM gate;
- SEQTRAK MIDI-only build and fixed-DRAM gate;
- three-page Generate contract;
- Phrase Core.

No assertion may be deleted or weakened merely to obtain a green result.

## Hardware pending

- #102 project namespace/Save As/Clear/reboot smoke;
- all six synth engines on Synth A and B: TYPE plus every visible parameter after Save/reboot/Load;
- normal Load preserves the saved synth patch before any explicit Genre Apply;
- TB303 trigger, accent, slide, sustain, NoteOff, release, Volume, sub and Panic;
- #125 AY/SN pitch and logical NoteOff checklist;
- neutral defaults and DST listening;
- Acid, Rave, Techno and current Minimal regression;
- MIDI/SEQTRAK Start/Stop/Continue/live/SMF/mute/route/Panic smoke;
- 30-minute runtime and memory soak.

Hardware checkboxes remain unchecked until the exact flashed SHA and result are recorded.

## Explicit boundary

Do not add to PR #131:

- GenreManager ownership rewrite;
- AY articulation redesign;
- unvalidated SID articulation;
- risky TextureMode/Scene migration;
- Song/Generation redesign;
- Phrase Arranger Stage 2;
- new genres or Atlas material;
- behavior from rejected PR #110;
- navigation framework rewrites;
- BLE MIDI, oversampling, wavetable mipmaps or broad loudness work;
- pattern `.gpp` format changes;
- broad dead-code cleanup.

## Acceptance checklist

### Code and automated

- [x] P0-1 versioned synth persistence implemented and merged via #143;
- [x] P0-2 loaded patch ownership implemented and merged via #143;
- [x] P1-1 engine-aware neutral defaults implemented and merged via #143;
- [x] explicit Save/Load revision result semantics implemented and merged via #143;
- [x] TB303 lifecycle/output and DST changes implemented in #131;
- [ ] exact final #131 head recorded after release-scope decision;
- [ ] all required host, SDL, Cardputer, DRAM, Generate, persistence and Phrase jobs pass on that final head.

### Hardware

- [ ] storage smoke passes;
- [ ] synth persistence passes for all engines on A and B;
- [ ] TB303 lifecycle/output passes;
- [ ] AY/SN pitch and NoteOff pass;
- [ ] accepted genres remain unchanged;
- [ ] MIDI/SEQTRAK smoke passes;
- [ ] 30-minute soak passes.

Do not merge PR #131 or tag 0.9 until the final exact-head automated gate and required hardware acceptance are recorded.
