# 0.9.7-R5 — Pattern route binding

## Purpose

Bind the already-audited Device Profile model to **Pattern MIDI output only** without introducing live route mutation.

## Ownership

```text
NVS midi_cfg
  -> MidiDeviceProfileRuntime (authoritative settings owner)
  -> MidiOutputRouteProjection (pure derived model)
  -> MidiPatternStartupRoutes (minimal startup snapshot)
  -> UsbMidiOutput::begin()
  -> fixed Pattern lanes + physical drum notes
```

`UsbMidiOutput` snapshots routes once while its lanes are configured. It never re-reads the profile while running.

## Wire behavior

### SEQTRAK NATIVE

- Pattern Synth A -> CH8
- Pattern Synth B -> CH9
- Pattern drums keep the historical native lane map and note 60.

### GENERAL MIDI

- Pattern Synth A -> CH1
- Pattern Synth B -> CH2
- drums -> CH10
- Kick 36, Snare 38, Closed Hat 42, Open Hat 46, Mid Tom 43, High Tom 47, Rim 37, Clap 39.

### GENERIC MIDI

- Pattern Synth A -> CH1
- Pattern Synth B -> CH2
- Pattern drums are disabled because Generic MIDI must not imply a GM percussion map.

## Realtime contract

Profile state is **not** read from the MIDI dispatcher/audio path. The physical Pattern drum note is resolved before `UsbMidiOutput::begin()` accepts events. The same mapped `(channel,note)` is retained by existing wire ownership until scoped NoteOff/AllNotesOff cleanup.

There is no runtime profile switch in R5. Publishing a different control-side profile after `begin()` cannot move existing USB lanes.

## Validation

```bash
bash tests/run_midi_pattern_route_binding_0_9_7_tests.sh
```

The dedicated test executes the complete R1 -> R5a chain first, then checks SEQTRAK, GM and Generic Pattern routes plus GM Pattern/SMF shared-note ownership.

## Out of scope

- Performance Synth A/B/DX routing
- live Performance drum mapping
- runtime profile switching
- UI profile selection
- persistence write API for profile changes
- changing CC26 behavior

Those remain later explicit 0.9.7 checkpoints.
