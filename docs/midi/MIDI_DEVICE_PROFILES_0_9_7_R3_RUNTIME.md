# 0.9.7-R3 — MIDI Device Profile Runtime Owner

## Purpose

Introduce one bounded control-side owner for the currently loaded/applied MIDI device profile settings. R3 removes the duplicate long-lived `MidiOutputSettings` snapshot from the Cardputer persistence session and centralizes profile application + transport-capability identity.

R3 deliberately does **not** bind profiles to USB note lanes yet.

Stack:

```text
0.9.7-R1 @ 246ff995f962d285fe7d986e16c63607054931b5
  -> 0.9.7-R2 @ f4ebc439406c68cd780e58ddb2b9c3565bcec4fb
  -> 0.9.7-R3
```

## Ownership

`MidiDeviceProfileRuntime` owns exactly one `MidiOutputSettings` snapshot plus a revision counter.

It may:

- initialize from the persisted settings record;
- sanitize that record once on adoption;
- apply a profile transactionally to a copy;
- commit the complete candidate settings snapshot;
- synchronize `MidiTransportCapabilityRuntime` after commit;
- update user-owned transport source/follow fields in the same snapshot.

It may not:

- allocate heap memory;
- perform NVS/SD I/O;
- send USB MIDI;
- mutate active-note ownership;
- rebuild USB lanes;
- touch Pattern/Phrase/Song/generation state;
- run in the audio callback.

## Transaction contract

```text
current settings
      |
      v
copy candidate
      |
apply device defaults to candidate
      |
compare
  |        |
 same    changed
  |        |
 no-op    commit complete candidate
           |
           +-> revision++
           +-> publish transport capability profile
```

A profile change never partially mutates the live snapshot.

## Cardputer session integration

Before R3, `CardputerMidiSettingsSession` owned its own resident `MidiOutputSettings settings_` and separately pushed `settings_.profile` into the transport capability runtime.

R3 changes boot to:

```text
NVS load -> local loadedSettings
             |
             v
MidiDeviceProfileRuntime::initialize()
             |
             +-> one owned settings snapshot
             +-> transport capability identity
```

Transport source/follow persistence updates the same runtime snapshot before encoding it back to NVS.

## Important non-goal: USB note routing

`UsbMidiOutput` still owns its existing `UsbMidiRouteConfig`, fixed lane table, SEQTRAK pattern-drum channel mapping, and CC26 behavior. R3 intentionally does not include `midi_device_profile_runtime.h` in USB output code.

This keeps the next boundary explicit: a later adapter must translate profile/settings into USB route configuration rather than letting both layers become independent routing owners.

## Hardware list

- No hardware is required for the dedicated R3 host contract.
- Cardputer ADV compile remains part of the repository PR matrix because the Arduino-only settings session changes.
- SEQTRAK hardware is not required until profile-to-routing binding exists.

## Wiring

None. No GPIO, USB physical wiring, I2C, I2S, SD, or audio wiring changes.

## Build / test

From repository root:

```bash
bash tests/run_midi_device_profile_runtime_0_9_7_tests.sh
```

The runner first executes R1 + R2 contracts, then the R3 source and executable regressions.

## Expected behavior

- persisted settings initialize one runtime snapshot;
- transport capability profile matches the adopted device profile;
- Generic/GM/SEQTRAK semantics from R2 remain unchanged;
- profile apply preserves user-owned enable/live/clock/follow intent;
- reapplying an identical profile is a no-op and does not increment revision;
- Cardputer session no longer owns a duplicate `MidiOutputSettings settings_`;
- USB lanes/routing are unchanged.

## Troubleshooting

If R1/R2 fail on the R3 branch, determine whether the failure is an intentionally superseded source assertion or a real semantic regression. Do not weaken SEQTRAK/GM/Generic behavioral checks to make R3 pass.

If Cardputer linking fails, verify R3 remains header-only; no new runtime `.cpp` should need to be added to a target source list.

If USB output begins including `midi_device_profile_runtime.h`, routing binding leaked into R3 and must be moved to the later adapter stage.

## Acceptance checklist

- [ ] one bounded runtime settings owner;
- [ ] no heap allocation/locks/I/O in owner;
- [ ] transactional candidate -> commit profile apply;
- [ ] transport capability identity updates only after adoption/commit;
- [ ] user-owned transport intent remains separate from device capabilities;
- [ ] Cardputer session duplicate settings member removed;
- [ ] persistence session still saves transport source/follow changes;
- [ ] R1 + R2 contracts remain green;
- [ ] no USB lane/routing changes;
- [ ] dedicated R3 host gate passes;
- [ ] normal Cardputer ADV/SDL/Core matrix remains green.

## Next

The next profile step is not UI. First add a bounded `MidiOutputSettings -> UsbMidiRouteConfig` adapter and prove ownership/active-note safety. Persistence exposure/migration and UI come only after routing actually consumes the profile settings.
