# Remove TextureMode runtime model

## Purpose

Remove `TextureMode` as a runtime genre axis after the user-facing TEXTURE page was removed in PR #130. New projects must no longer expose, edit, apply, or persist a separate texture selection.

## Compatibility contract

Older scene JSON may still contain the historical genre fields `tex`, `amt`, `curated`, and `snd`. The loader must accept those keys without failing, but they are decode-only compatibility input and must not recreate editable runtime texture state.

Previously materialized sound must remain owned by the parameters that are already persisted independently in the scene: synth parameters, delay state, Tape/FEEL state, distortion, and related engine settings. Loading an old scene must not re-apply a historical texture preset on top of those saved values.

New scene serialization must omit the historical texture fields.

## In scope

- remove the `TextureMode` enum and count;
- remove `TextureParams` and `kTexturePresets`;
- remove texture cycling/allow-list helpers and texture bias tracking from `GenreManager`;
- remove saved editable texture fields from `GenreSettings`;
- accept old texture keys as ignored legacy input;
- stop scene load/save from synchronizing or re-applying `TextureMode`;
- update regression coverage for old-scene loading and new-scene serialization.

## Out of scope

Do not change genre note/drum generation, existing genre or recipe choices, `GrooveboxModeManager`, Song materialization, Phrase Core, MIDI, transport, or pattern paging.

## Acceptance checklist

- [ ] `TextureMode`, `TextureParams`, `kTexturePresets`, `setTextureMode`, `textureMode`, and `applyTexture` have no runtime definitions or call sites.
- [ ] A legacy scene containing historical texture keys still loads successfully.
- [ ] Synth/tape/delay/distortion values already stored in that legacy scene are not overwritten by a texture preset during load.
- [ ] Re-saving that scene does not emit `tex`, `amt`, `curated`, or `snd` in the genre object.
- [ ] GENRE -> FEEL -> GENERATION navigation remains unchanged from PR #130.
- [ ] Host regressions, Four-axis UI, Phrase Core, SDL, Cardputer ADV, fixed DRAM, and SEQTRAK MIDI-only builds are green.
- [ ] Physical smoke test confirms an old project sounds materially the same before and after migration.
