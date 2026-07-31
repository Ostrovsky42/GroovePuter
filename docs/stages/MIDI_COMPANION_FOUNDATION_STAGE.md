# MIDI Companion configuration foundation

## Purpose

Prepare platform-neutral configuration and persistence components for the future
`feature/midi-companion-completion` stage while PR #8 owns the real-time MIDI
dispatch architecture.

This foundation intentionally does not connect settings to `UsbMidiOutput`,
`MiniAcid`, TinyUSB, UI pages, scenes, or the PR #8 scheduler. It can be rebased
onto the accepted PR #8 implementation without requiring a second timing path.

## Hardware assumptions

- Primary target: M5Stack Cardputer-Adv, ESP32-S3.
- External instrument profile: Yamaha SEQTRAK.
- Existing native USB-C MIDI transport remains owned by PR #8.
- No GPIO, PORT.A, I2C, SPI, or audio wiring is changed by this stage.
- MIDI channels are stored internally as `0..15` and displayed as `1..16`.

## Device profiles

### SEQTRAK NATIVE

The official SEQTRAK MIDI channel layout is represented directly:

```text
CH 1  KICK
CH 2  SNARE
CH 3  CLAP
CH 4  HAT 1
CH 5  HAT 2
CH 6  PERC 1
CH 7  PERC 2
CH 8  SYNTH 1
CH 9  SYNTH 2
CH 10 DX
CH 11 SAMPLER
```

GroovePuter has eight internal drum voices but SEQTRAK has seven native drum
tracks. The safe initial mapping is explicit:

```text
Kick       -> KICK
Snare      -> SNARE
Clap       -> CLAP
Closed Hat -> HAT 1
Open Hat   -> HAT 2
Mid Tom    -> PERC 1
High Tom   -> PERC 2
Rim        -> PERC 1 (shared)
```

Every route remains editable. The default SEQTRAK trigger note is MIDI note 60;
its exact audible interpretation must be confirmed during the later drum-MIDI
hardware stage rather than hidden in runtime code.

Official channel source:

```text
https://manual.yamaha.com/mi/de/seqtrak/ru/SEQTRAK_user_guide_Ru_D0_018.html
```

### GENERAL MIDI

All eight drum voices use zero-based channel `9` (UI channel 10) and conventional
GM percussion notes:

```text
Kick 36, Snare 38, Closed Hat 42, Open Hat 46,
Mid Tom 43, High Tom 47, Rim 37, Clap 39
```

### CUSTOM

Custom mode preserves the current channels, drum routes, enable flags, live
target, and gate value. Selecting CUSTOM must not silently rewrite a user map.

## Configuration model

`MidiOutputSettings` contains only device/global output policy:

```text
profile
master enable
live enable and Synth A/B target
Pattern Synth A/B enables and channels
Drum output enable
8 per-voice drum routes
bounded drum gate duration
```

These values do not belong to `Scene` because changing the connected external
device must not modify musical project data.

## Persistence contract

The foundation defines a platform-neutral fixed blob:

```text
magic: GPMD
schema: 1
payload: 32 bytes
CRC32
encoded size: 44 bytes
```

The codec performs no heap allocation. The storage adapter is abstract so the
future Cardputer integration can use NVS or another global device store without
adding Arduino dependencies to the model.

Load policy:

```text
valid blob     -> Loaded
missing blob   -> SEQTRAK defaults
corrupt blob   -> selected fallback-profile defaults
storage error  -> leave active output unchanged
```

## Wiring

No additional wiring is required for this host-only foundation.

Future hardware integration remains:

```text
Cardputer-Adv native USB-C data port
-> data-capable USB-C cable
-> Yamaha SEQTRAK USB-C port
```

A charge-only cable is not suitable.

## Build and test

Run the platform-neutral tests:

```bash
bash tests/run_host_tests.sh
```

Focused command:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_midi_companion_settings.cpp \
  src/midi/midi_companion_settings.cpp \
  src/midi/midi_companion_settings_codec.cpp \
  -o build/host-tests/test_midi_companion_settings

build/host-tests/test_midi_companion_settings
```

The regular SDL and Cardputer builds must also continue to pass even though the
new files are not connected to runtime yet.

## Expected behavior

Host tests confirm:

- SEQTRAK channels are stored zero-based and match tracks 1-9 externally;
- all eight GroovePuter drum voices have explicit routes;
- the eighth voice is not silently discarded;
- General MIDI uses channel 10 with conventional notes;
- UI channel conversion clamps to `1..16`;
- profile changes preserve runtime enable flags;
- CUSTOM preserves user routes;
- invalid channels, notes, target, profile, and gate values are sanitized;
- settings encode/decode exactly;
- CRC, length, magic, and version errors are rejected transactionally;
- storage errors do not overwrite active settings.

## Troubleshooting

### Host compile fails on a platform header

The foundation accidentally gained an Arduino, M5, TinyUSB, DSP, or scene
dependency. Remove the dependency instead of adding a host stub.

### CUSTOM resets the routes

`applyMidiDeviceProfile(Custom, ...)` must preserve the existing valid map.
Profile defaults should only replace routes for SEQTRAK NATIVE or GENERAL MIDI.

### Corrupt settings are accepted

Check magic, schema version, payload size, exact encoded length, route ranges,
and CRC32. Decode must assign the output only after all validation passes.

### Storage error loads defaults

That is incorrect. Only missing or corrupt data may load defaults. A backend
error must leave the current settings unchanged and return `StorageError`.

## Acceptance checklist

- [ ] `test_midi_companion_settings` passes with `-Werror`.
- [ ] source-boundary regression passes.
- [ ] SEQTRAK profile maps channels 1-7 and synth channels 8/9 correctly.
- [ ] every internal drum voice has a visible route.
- [ ] CUSTOM profile preserves current routes.
- [ ] encode/decode round-trip is exact.
- [ ] corrupt blob leaves decode output unchanged.
- [ ] missing storage resolves to explicit defaults.
- [ ] storage error preserves active settings.
- [ ] no scene schema field is added.
- [ ] no TinyUSB, Arduino, M5, DSP, UI, or scheduler dependency is introduced.
- [ ] existing host tests pass.
- [ ] SDL build passes.
- [ ] Cardputer-Adv build passes with pinned core 3.2.2.

## Out of scope

- PR #8 scheduler and `MidiDispatchTask`;
- `MusicalEventTarget::Drums`;
- runtime route application and generation invalidation;
- wire-level synth-note ownership changes;
- drum NoteOn/NoteOff scheduling;
- MIDI settings UI;
- actual NVS/Preferences adapter;
- MIDI Clock/Start/Stop;
- SMF realtime playback;
- BLE-MIDI, MIDI input, CC, Program Change, Pitch Bend, or SysEx;
- scene schema changes.
