# Dev Function/Input/UX Audit — Current Status

## Purpose

Record the release disposition of the original static audit against `dev_0.9`. The original audit remains a source snapshot; this file does not claim that untested reachability or hardware behavior has been proven.

## Baseline

```text
release branch: dev_0.9
release base SHA: b28c63801660c9d024e4aad57716d534744fa324
stabilization PR: #131
hardware acceptance: PENDING
```

## Integrated and preserved

- **PR #102:** merged into `dev_0.9`; project-scoped pages, Save As copy, New/Clear isolation, migration/recovery, canonical `page + bank + slot` identity and `.gpp` v3 are release inputs.
- **PR #125:** merged into `dev_0.9`; AY/SN pitch policy and logical NoteOff identity are release inputs.
- **PR #107:** held-value acceleration and BPM chrome are merged; Cardputer ADV acceptance is still required.

## Rejected or deferred

- **PR #110:** rejected experiment. It is not the release path and none of its genre/Atlas behavior should be documented as current firmware.
- **PR #101:** Song-generation UX prototype deferred until after 0.9.
- **PR #90:** Phrase Arranger Stage 2 deferred until after 0.9.
- Tape/Sampler page reachability remains a separate product decision. Persisted compatibility fields do not prove a reachable supported page.
- TEXTURE/GENERATION ownership and page cleanup are deferred; do not redesign navigation in the stabilization PR.
- Static dead-code candidates remain candidates until linker and reachability proof exists.

## Revision and ownership status

The repository has a Scene revision service and existing commit boundaries for several user actions. The remaining release proof must distinguish:

- browse/preview: no dirty revision;
- successful Apply/commit: exactly one revision;
- failed transaction: no revision;
- Save: dirty clears;
- recovery autosave: restores the last committed state;
- Genre PROFILE ONLY: no pattern materialization;
- Genre materialization and materialization+BPM: one documented mutation each.

The synth persistence and normal-load ownership blockers are tracked in `docs/reviews/SYNTH_ENGINE_AUDIT_0_9_CURRENT_STATUS.md`.

## Hardware assumptions

- M5Stack Cardputer ADV;
- built-in keyboard and display;
- headphones or built-in speaker;
- FAT32 microSD for project lifecycle checks;
- optional Yamaha SEQTRAK for MIDI acceptance;
- no external GPIO wiring required.

## Build and flash

```bash
git fetch origin
git switch release/0.9-final-stabilization
git reset --hard origin/release/0.9-final-stabilization
rm -rf build .pio .pioenvs .piolibdeps
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
```

Flash with the normal upload command and monitor serial at `115200` baud.

## Release cleanup rule

Do not remove or remap a function solely because the static audit marks it unused. First prove:

1. no active page constructs or references it;
2. no persisted page ID, Scene field, shortcut or migration path depends on it;
3. host, SDL, Cardputer ADV and MIDI acceptance still pass;
4. help and documentation are updated in the same focused change.

## Acceptance checklist

- [ ] every advertised page is reachable and has a reliable exit;
- [ ] global shortcuts do not collide with local editor controls;
- [ ] continuous controls are precise on tap and accelerate on hold;
- [ ] discrete selectors advance one item per event;
- [ ] Save, Save As, New, Clear, reboot and recovery preserve project ownership;
- [ ] FEEL/TEXTURE/GENRE revision and preview contracts pass focused tests;
- [ ] synth TYPE and visible parameters survive persistence without genre overwrite;
- [ ] MIDI Player, MIDI Hub and live performance clean up NoteOff correctly;
- [ ] release documentation does not promise rejected #110 behavior, inaccessible Tape UI or unverified direct mute hotkeys;
- [ ] serial contains no crash, watchdog, allocation failure or repeated recovery loop.

The remaining audit candidates are deferred or release-blocking findings, not permission for broad cleanup in PR #131.
