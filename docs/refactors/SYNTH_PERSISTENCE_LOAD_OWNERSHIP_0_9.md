# Synth persistence / Scene Load ownership — GroovePuter 0.9

## Purpose

Close the remaining release blockers called out by PR #131 without mixing them into SID articulation (#139), TextureMode migration (#134), GenreManager ownership (#132), or TB303/DST stabilization.

This PR owns four contracts only:

1. versioned Synth A/B persistence for TYPE plus normalized parameters 0..5;
2. Scene Load ownership: restored TYPE/parameters win and Genre must not overwrite the loaded patch;
3. engine-aware neutral defaults for legacy scenes missing the new synth block;
4. explicit Save/Load dirty-state success semantics.

## P0 — synth persistence

Persist Synth A and Synth B independently.

Required stored state per synth:

- engine TYPE using the existing stable engine id/name (`TB303`, `SID`, `AY`, `SH101`, `SN76489`, `WAVEMORPH`);
- normalized parameter values for indices `0..5`;
- a synth-state schema version so malformed/newer data can be rejected transactionally.

Do not rename engine ids and do not change the runtime parameter index contract.

### Backward compatibility

Older Scene JSON without the versioned synth-state block must continue to load.

Legacy `synthEngines` and legacy TB303-shaped `synthParams` remain decode-only compatibility input. Missing new fields are not an error.

When a legacy Scene does not contain parameters for the selected engine, initialize that engine from its own runtime `Parameter` defaults. Do not reinterpret TB303 raw values as normalized values for another engine.

Malformed versioned synth state must fail the Scene transaction instead of partially applying TYPE or parameters.

## P0 — Scene Load ownership

The Scene file is authoritative after a successful normal Load.

Load ordering must be:

1. parse/validate into scratch state;
2. commit Scene;
3. instantiate/restore saved Synth A/B TYPE;
4. apply saved normalized parameters in index order for the active engine, bounded by that engine's `parameterCount()` and the persisted `0..5` ceiling;
5. restore remaining engine/FX state;
6. expose the loaded Scene to UI/runtime.

After step 4 no Genre load/reset path may call a timbre/sound macro that rewrites the restored synth patch.

Genre remains allowed to change a patch only on an explicit user action whose documented apply mode includes sound mutation.

## P1 — neutral engine defaults, release-required

For `AY`, `SH101`, `SN76489`, and `WAVEMORPH`, a Scene missing synth parameters must use each engine's own declared `Parameter` defaults.

Forbidden behavior:

- feeding TB303 raw defaults such as cutoff `800`, resonance `0.6`, env amount `400`, env decay `420` into another engine as normalized values;
- using one shared TB303-shaped fallback structure as the semantic default for every engine.

TB303 legacy scenes may continue to map legacy raw fields through the existing TB303 compatibility path.

## Save / Load revision contract

Dirty-state transitions are success-driven.

Required:

- successful explicit Save marks the current Scene revision saved/clean;
- successful explicit Load establishes the loaded Scene as the saved/clean baseline;
- failed Save does not clear dirty state or advance a saved revision marker;
- failed Load does not replace the current Scene, TYPE, parameters, or dirty baseline;
- recovery/autosave does not mark the user project clean and does not impersonate an explicit Save;
- no-op UI navigation is unrelated to this contract.

## Explicit boundary

Do not modify:

- SID articulation or labels from #139;
- TextureMode migration from #134;
- GenreManager ownership architecture from #132;
- TB303 envelope/DST work from #131;
- pattern `.gpp` format;
- Song/Generation/Phrase behavior;
- MIDI/transport;
- broad loudness, aliasing, or DSP architecture.

## Required host regressions

Tests must prove with real Scene codec and real swappable synth voices:

1. Synth A and B can save/load different TYPE values;
2. parameters indices `0..5` round-trip independently and remain normalized;
3. an old Scene without the new synth block still loads;
4. legacy TB303 parameter fields remain compatible;
5. missing fields use the selected engine's own defaults;
6. AY/SH101/SN76489/WAVEMORPH never receive `{800, 0.6, 400, 420, ...}` as normalized defaults;
7. loaded TYPE is active before loaded parameters are applied;
8. normal Load does not invoke Genre timbre/sound projection after patch restore;
9. malformed versioned synth data rolls back atomically;
10. successful Save clears dirty state;
11. failed Save preserves dirty state;
12. successful Load establishes a clean baseline;
13. failed Load preserves current state and dirty baseline;
14. recovery/autosave leaves user dirty state unchanged.

## Required validation

Before ready-for-review:

- focused synth persistence/load ownership host tests;
- `bash tests/run_host_tests.sh`;
- Four-axis UI / three-page Generate compatibility;
- Phrase Core;
- SDL build;
- Cardputer ADV normal firmware build;
- fixed DRAM budget;
- Cardputer ADV SEQTRAK MIDI-only build.

## Cardputer ADV hardware acceptance

No external wiring is required. Use the built-in Cardputer ADV controls/audio path.

For both Synth A and Synth B:

1. choose a different engine TYPE on each side;
2. edit every visible parameter, including indices 4 and 5 when the engine exposes them;
3. Save explicitly;
4. change TYPE and parameters to obviously different values;
5. Load the saved Scene;
6. confirm TYPE and parameters return exactly to the saved patch before any Genre interaction;
7. navigate GENRE/FEEL/GENERATION without applying Genre and confirm the patch does not change;
8. repeat with AY, SH101, SN76489, and WAVEMORPH from a legacy Scene lacking the new fields and confirm sane engine-native defaults;
9. force/observe a failed Save and failed Load path and confirm the current project remains dirty/intact;
10. trigger recovery/autosave and confirm the explicit Save indicator is not falsely cleared.

## Acceptance checklist

- [ ] Synth A/B TYPE round-trips independently.
- [ ] Normalized parameters 0..5 round-trip independently.
- [ ] Old Scenes remain loadable.
- [ ] Missing new fields use engine-native defaults.
- [ ] Loaded patch wins over Genre during normal Load.
- [ ] Non-TB303 engines never inherit TB303 raw defaults as normalized data.
- [ ] Explicit Save/Load success updates the revision baseline correctly.
- [ ] Failed Save/Load does not clear dirty state.
- [ ] Recovery/autosave does not clear user dirty state.
- [ ] Host/SDL/Cardputer/fixed-DRAM/SEQTRAK gates are green.
- [ ] Physical Cardputer ADV persistence smoke passes before merge.
