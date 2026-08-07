# Remove TextureMode runtime model

## Purpose

Remove `TextureMode` as a runtime genre axis after the user-facing TEXTURE page was removed in PR #130. New projects must no longer expose, edit, apply, or persist a separate texture selection.

## Compatibility contract

Older scene JSON may still contain the historical genre fields `tex`, `amt`, `curated`, and `snd` (plus long-form aliases). The loader accepts those keys without failing, but they are decode-only compatibility input and do not recreate editable runtime texture state. Unknown historical texture values are ignored.

The migration removes the historical texture projection from reset and scene-load paths. In particular, old texture metadata no longer rewrites Tape, delay, FEEL tape enablement, or TB303 cutoff/resonance bias after the concrete scene state has been decoded.

This PR does not redefine the separate pre-existing GENRE timbre/load ownership contract. Genre note/drum generation, recipe selection, and `applyGenreTimbre()` behavior are intentionally unchanged. The hardware A/B check therefore remains required before merge to prove that removal of the texture projection does not materially change old-project sound.

New scene serialization omits the historical texture fields.

## In scope

- remove the `TextureMode` enum and count;
- remove `TextureParams` and `kTexturePresets`;
- remove texture cycling/allow-list helpers and texture bias tracking from `GenreManager`;
- remove saved editable texture fields from `GenreSettings`;
- accept old texture keys as ignored legacy input;
- stop scene load/save from synchronizing or re-applying `TextureMode`;
- update regression coverage for old-scene loading and new-scene serialization.

## Out of scope

Do not change genre note/drum generation, existing genre or recipe choices, `applyGenreTimbre()`, `GrooveboxModeManager`, Song materialization, Phrase Core, MIDI, transport, pattern paging, synth TYPE ownership, or public synth parameter ranges.

## Acceptance checklist

- [x] `TextureMode`, `TextureParams`, `kTexturePresets`, `setTextureMode`, `textureMode`, and `applyTexture` have no runtime definitions or call sites.
- [x] Legacy compact and long-form texture keys are accepted as decode-only input; the source regression covers this contract.
- [x] Scene load no longer runs a texture projection over decoded synth/Tape/delay/distortion state.
- [x] New serialization omits historical texture fields (`tex`, `amt`, `cur`/`curated`, `sound`/`snd`).
- [x] GENRE -> FEEL -> GENERATION navigation remains unchanged from PR #130.
- [ ] Host regressions, Four-axis UI, Phrase Core, SDL, Cardputer ADV, fixed DRAM, and SEQTRAK MIDI-only builds are green on the exact final head.
- [ ] Physical smoke test confirms an old project sounds materially the same before and after migration.
