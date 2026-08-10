# GroovePuter 0.9 — Pre-release Gate

## Purpose

Track the exact `dev_0.9` release candidate, distinguish code completion from automated and Cardputer ADV evidence, and prevent rejected experiments from entering 0.9.

## Current baseline

```text
release branch: dev_0.9
base SHA: 538ae24a1c88253eb0cfc1a9a671e16091e449bf
stabilization PR: #131
stabilization branch: release/0.9-final-stabilization
```

The base includes:

- #102 project-scoped pattern storage;
- #125 AY/SN pitch and logical NoteOff lifecycle;
- #130 removal of the user-facing TEXTURE page while preserving legacy Scene compatibility and redirecting persisted page ID 8 to FEEL.

PR #110 is a rejected experiment and is not a release input. PRs #101 and #90 remain deferred.

## Code complete in PR #131

- TB303 has a separate amplitude ADSR-style lifecycle from its filter envelope.
- NoteOff starts a bounded release and reaches silence.
- active legato slide does not fully retrigger the amplitude envelope.
- TB303 `Volume` controls output exactly once.
- the optional TB303 sub component is mixed exactly once.
- distortion enable restores a safe audible drive only when the stored drive is below the working threshold.
- FEEL persistent changes mark Scene dirty once; preset browsing remains preview-only.
- Genre browsing remains preview-only; Apply/materialization marks one logical Scene mutation.
- the three-page Generate workflow remains `GENRE 1/3 → FEEL 2/3 → GENERATION 3/3`.

## Release blockers still open

### P0-1 — versioned synth persistence

Scene persistence still stores only five generic synth fields. The release needs a backward-compatible engine-aware payload containing independent Synth A/B TYPE and normalized parameters `0..5`, plus deterministic legacy defaults and malformed-version rollback.

### P0-2 — loaded patch ownership

Normal Scene Load must guarantee:

```text
stored TYPE + stored parameters win
```

Genre timbre must not overwrite a restored patch as hidden post-load work.

### P1-1 — neutral engine defaults

AY, SH101, SN76489 and WAVEMORPH must not inherit TB303 raw defaults such as maximum normalized noise. This must be implemented together with versioned legacy decode so existing user patches are not overwritten.

### P1-3 — Save/Load revision wiring

FEEL and Genre mutation boundaries are covered in PR #131. The engine still must call revision success hooks only after successful explicit Save and successful Load; failed operations and recovery autosave must not clear dirty state incorrectly.

PR #131 remains draft while these blockers are open.

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
- TB303 trigger, accent, slide, sustain, NoteOff, release, Volume, sub and Panic;
- #125 AY/SN pitch and logical NoteOff checklist;
- neutral defaults and DST listening;
- Acid, Rave, Techno and current Minimal regression;
- MIDI/SEQTRAK Start/Stop/Continue/live/SMF/mute/route/Panic smoke;
- 30-minute runtime and memory soak.

Hardware checkboxes must remain unchecked until the owner records the exact flashed SHA and result.

## Explicit boundary

Do not add or restore in this PR:

- TEXTURE, Tape or Sampler UI;
- Song/Generation redesign;
- Phrase Arranger Stage 2;
- new genres or Atlas material;
- behavior from rejected PR #110;
- navigation framework rewrites;
- BLE MIDI, oversampling, wavetable mipmaps or broad loudness work;
- pattern `.gpp` format changes;
- broad dead-code cleanup without linker/reachability proof.

## Acceptance checklist

### Code and automated

- [ ] exact final PR head recorded;
- [ ] P0-1 versioned synth persistence complete and backward-compatible;
- [ ] P0-2 loaded patch ownership complete;
- [ ] P1-1 engine-aware neutral defaults complete;
- [ ] explicit Save/Load revision wiring complete;
- [ ] all required host, SDL, Cardputer, DRAM, Generate and Phrase jobs pass.

### Hardware

- [ ] storage smoke passes;
- [ ] synth persistence passes for all engines on A and B;
- [ ] TB303 lifecycle/output passes;
- [ ] AY/SN pitch and NoteOff pass;
- [ ] accepted genres remain unchanged;
- [ ] MIDI/SEQTRAK smoke passes;
- [ ] 30-minute soak passes.

Do not merge PR #131 or tag 0.9 while any release blocker remains.
