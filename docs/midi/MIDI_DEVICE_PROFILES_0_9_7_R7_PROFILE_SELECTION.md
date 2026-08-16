# 0.9.7-R7 — persisted Device Profile selection

## Purpose

Expose a safe control-side Device Profile selection contract for the later UI without mutating an already-running USB MIDI route table.

## Contract

Device Profile selection is **next-boot intent**:

```text
active runtime profile + frozen USB routes
              remain unchanged

user selects profile
        -> build profile candidate
        -> persist schema-v2 midi_cfg
        -> pending profile changes
        -> RESTART REQUIRED

next boot
        -> load selected profile
        -> initialize MidiDeviceProfileRuntime
        -> publish frozen startup routes
        -> start USB dispatcher
```

The selection path never calls `MidiDeviceProfileRuntime::applyProfile()` and never republishes the startup route snapshot while USB is running.

## Persistence interaction

A later transport-control change before reboot must not erase the pending profile. R7 composes the saved record from:

- current active runtime settings;
- pending next-boot profile defaults;
- latest transport source/follow intent.

Schema-v2 already stores the profile byte, so no storage migration is introduced.

## UI-facing API

`src/platform/cardputer_midi_settings_session.h` exposes:

```cpp
MidiDeviceProfile pendingCardputerMidiDeviceProfile();
bool selectCardputerMidiDeviceProfileForNextBoot(MidiDeviceProfile profile);
bool cardputerMidiDeviceProfileRestartRequired();
```

The desktop implementation is an in-memory control model so SDL/UI tests do not pull Preferences/NVS into the link.

## Validation

```bash
bash tests/run_midi_profile_selection_0_9_7_tests.sh
```

The runner executes R1 -> R6 first, checks source ownership, then verifies schema-v2 profile round trips and preservation of pending profile intent across a subsequent transport settings save.

## Out of scope

- runtime/live profile switching;
- Custom Performance route editor;
- Device Profile UI rendering/input;
- drum INTERNAL/SAMPLE/BOTH/MIDI output ownership.
