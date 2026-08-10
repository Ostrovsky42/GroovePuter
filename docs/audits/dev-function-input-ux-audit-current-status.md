# Dev Function/Input/UX Audit — Current Status

## Baseline

```text
release branch: dev_0.9
base SHA: 538ae24a1c88253eb0cfc1a9a671e16091e449bf
stabilization PR: #131
hardware acceptance: PENDING
```

## Integrated release inputs

- #102: project-scoped pattern pages, Save As copy, New/Clear isolation, recovery and canonical `page + bank + slot` identity.
- #125: AY/SN pitch policy and logical NoteOff identity.
- #130: user-facing TEXTURE page removed; persisted legacy page ID 8 resolves to FEEL; Scene sound fields remain compatibility data.
- #107: held-value acceleration and BPM chrome; hardware acceptance remains pending.

## Rejected or deferred

- #110 is a rejected genre experiment and is not a release input.
- #101 Song-generation UX and #90 Phrase Arranger Stage 2 are deferred.
- Tape/Sampler reachability is not promised by 0.9 documentation.
- TEXTURE must not be restored by stabilization work.
- static dead-code candidates remain candidates until linker and reachability proof exists.

## Revision ownership in PR #131

- FEEL selector browsing is preview-only.
- a real FEEL value or preset change increments Scene revision once.
- Genre/variant/morph browsing is preview-only.
- Apply-mode selection is persistent and increments once.
- Genre PROFILE ONLY increments only when committed state changes.
- Genre materialization and materialization+BPM count as one logical mutation.
- repeated no-op Apply does not create a revision.

Still open: successful explicit Save and successful Load must call the revision success hooks; failed Save/Load and recovery autosave must not incorrectly clear dirty state.

## Runtime workflow

The user-facing Generate workflow is:

```text
GENRE 1/3 → FEEL 2/3 → GENERATION 3/3
```

Legacy page IDs are compatibility inputs, not proof of reachable pages.

## Release cleanup rule

Do not remove or remap a function only because the static audit marks it unused. Prove navigation, persistence, migration and linker ownership first, then update tests and documentation in the same focused change.

## Acceptance checklist

- [ ] every advertised page is reachable and has a reliable exit;
- [ ] TEXTURE page remains absent and legacy ID 8 resolves to FEEL;
- [ ] global shortcuts do not collide with editor controls;
- [ ] continuous controls are precise on tap and accelerate on hold;
- [ ] discrete selectors advance one item per event;
- [ ] Save/Save As/New/Clear/reboot/recovery preserve project ownership;
- [ ] FEEL/Genre preview and commit revision contracts pass;
- [ ] successful Save/Load clean dirty state and failed operations do not;
- [ ] synth TYPE and visible parameters survive persistence without genre overwrite;
- [ ] MIDI lifecycle cleans up NoteOff after Stop, mute and route changes;
- [ ] documentation does not promise #110 behavior, Tape UI, TEXTURE UI or unverified direct mute hotkeys.
