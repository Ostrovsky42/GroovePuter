# Scene Revision / Dirty Tracking Stage

## Purpose

Wave 1 / A2 records whether persistent project state in RAM differs from the
last successful manual save or scene load. The A1 status chrome appends `*` when
`currentRevision != persistedRevision`.

The tracker is runtime metadata only. It is not added to Scene JSON and does not
change the sound or compatibility of existing scenes.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable for build/flash and serial monitoring
- microSD card for save/load acceptance

## Wiring

No external wiring is required. PORT.A is unused. Existing Cardputer ADV I2C
wiring remains:

```text
SDA GPIO2
SCL GPIO1
```

## Build / Flash

```bash
bash tests/run_host_tests.sh
python3 tests/test_scene_revision_source_regressions.py
mkdir -p build/host-tests
g++ -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_scene_revision.cpp \
  -o build/host-tests/test_scene_revision
build/host-tests/test_scene_revision
bash scripts/build.sh --warnings all
```

Flash with the existing repository process after the Cardputer ADV build passes.

## Expected behavior

- Boot or a successful scene load starts clean: no trailing `*`.
- A persistent edit adds `*` immediately.
- Navigation, focus changes, Play/Stop, live notes, LiveMix and audition controls do not add `*`.
- A successful manual save removes `*`.
- Failed save/load operations never clear an existing `*`.
- Project clear/reset and successful MIDI import are persistent mutations.

Tracked mutation gateways include pattern/drum editing, Drum Automation, synth
and voice parameters, Genre/Texture/Feel, generator settings, Song editing,
phrase generation, sampler pad/kit settings, Tape state, persistent Project
settings and global track mutes.

The existing scene codec also persists Song mode, Song position, loop mode and
loop range, so changes to those fields intentionally mark the project dirty.
LiveMix, the separate Song playback audition slot, voice preview/stop, sampler
prelisten and Tape stutter remain runtime-only and use non-persistent paths.

Autosave is not treated as the user's manual persisted baseline. A failed manual
save restores the exact revision counters that were present before the save
attempt; a successful MIDI import that also saves establishes a clean baseline.

## Hardware smoke test

Tested on M5Stack Cardputer ADV on 2026-08-03.

Confirmed:

- the A1 status chrome is visible in firmware;
- a persistent edit adds the trailing `*`;
- successful Save/Load clears the marker as expected;
- basic navigation and transport use do not break the displayed state.

The forced SD-failure cases and the maximum-density underrun soak were not
repeated as part of this smoke test.

## Memory and realtime

```text
SceneRevisionState: 8 bytes, one process-wide instance
Scene schema delta: 0 bytes
Dynamic allocations: none
AudioTask work: none
Per-sample work: none
```

The UI reads two counters while constructing its existing status snapshot. No
Scene hashing, copying or serialization is performed during draw or playback.

## Troubleshooting

### `*` does not appear after an edit

Confirm the mutation uses an existing persistent mutation gateway or has an
explicit `GroovePuterState::markSceneMutated()` call. Do not add dirty updates
to draw code or generic input dispatch.

### `*` remains after Save

Check that the save returned success. The baseline is updated only in the
successful branch of `ProjectPage::saveCurrentScene()`.

### Navigation or audition marks the project dirty

The source regression rejects dirty calls in the global transport/live-note
blocks and verifies explicit runtime guards for mixed Song and Voice pages.
Keep selection, preview and audition changes outside persistent mutation helpers.

## Acceptance checklist

- [x] First persistent step edit adds `*`.
- [ ] Drum edit, Drum Automation, synth parameter edit and randomize add `*`.
- [ ] Genre, Texture, Feel and generator setting edits add `*`.
- [ ] Song edit and phrase generation add `*`.
- [ ] Sampler pad/kit, Voice parameter and Tape persistent edits add `*`.
- [x] Basic navigation and Play/Stop do not break dirty-state display.
- [ ] Live notes, LiveMix and all audition controls do not add `*`.
- [ ] Persisted Song mode/position/loop changes add `*`.
- [x] Successful Save and Load remove `*`.
- [ ] Failed save does not remove an existing `*`.
- [ ] Failed load does not remove an existing `*`.
- [ ] Clear/reset leaves the project dirty until saved.
- [ ] Old scene JSON loads without schema migration.
- [ ] No Scene hashing/serialization occurs in the frame or playback loop.
- [x] Host regressions pass.
- [x] SDL build passes.
- [x] Cardputer ADV build passes.
