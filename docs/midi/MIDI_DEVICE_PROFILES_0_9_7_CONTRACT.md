# 0.9.7-R1 — MIDI Device Profiles contract

## Purpose

Freeze the device-profile ownership boundary before production integration.

This checkpoint starts from frozen `dev_0.9.4` and intentionally makes no runtime routing, Scene, UI, audio-thread, USB-MIDI packet, or persistence-schema changes. It inventories the profile support that already exists and defines what 0.9.7 may own later.

The existing code already has:

- `MidiDeviceProfile::{SeqtrakNative, GeneralMidi, Custom}`;
- SEQTRAK-specific synth/drum channel defaults;
- a General MIDI percussion mapping;
- per-profile transport capabilities;
- profile persistence inside `MidiOutputSettings` schema v2;
- Cardputer boot applying the persisted profile to transport capabilities.

0.9.7 therefore must extend the existing ownership model rather than introduce a second profile framework.

## Current implementation facts

### Existing persisted profile IDs

The current schema stores the profile enum numerically. These values are therefore compatibility-sensitive:

```text
SeqtrakNative = 0
GeneralMidi   = 1
Custom        = 2
```

R1 does not rename or renumber them.

### Existing SEQTRAK preset

Current defaults use zero-based MIDI channels:

```text
Synth A / live   CH8  -> 7
Synth B          CH9  -> 8

Kick             CH1 note 60
Snare            CH2 note 60
Clap             CH3 note 60
Closed Hat       CH4 note 60
Open Hat         CH5 note 60
Mid Tom          CH6 note 60
Rim              CH6 note 60
High Tom         CH7 note 60
```

Current validated transport claims are deliberately conservative:

```text
Clock TX/RX      yes
Start TX/RX      yes
Stop TX/RX       yes
Continue RX      yes
Continue TX      no
Song Position TX no
Song Position RX no
Continue policy  restart from beginning
```

### Existing `GeneralMidi` preset

The existing `GeneralMidi` value currently does two different jobs:

1. it selects a General MIDI percussion map on channel 10;
2. `midiTransportCapabilitiesForProfile()` treats it as a generic class-compliant transport.

Those meanings are not equivalent.

A generic MIDI device may be class-compliant without implementing the General MIDI percussion layout. Therefore 0.9.7 production work must not silently rename `GeneralMidi` to `GenericMidi` while retaining GM drum-map semantics.

The persisted enum value remains untouched in R1. Production integration must choose one explicit migration strategy:

- keep `GeneralMidi` as the GM-specific preset and add a separate Generic MIDI profile; or
- migrate the semantic model with an explicit schema/version compatibility plan.

No decision is hidden inside this research checkpoint.

## 0.9.7 ownership contract

A device profile MAY own device-specific defaults or capability claims such as:

- default MIDI channel layout;
- default drum note/channel mapping when the device contract actually defines one;
- validated transport capabilities;
- validated vendor-specific receiver controls, if any;
- device-specific quirks that can be represented as bounded data/capabilities.

A device profile MUST NOT own:

- whether MIDI output is globally enabled;
- whether live, Synth A, Synth B, or drums are enabled;
- the user's current live target;
- 0.9.5 drum output ownership (`INTERNAL/SAMPLE/BOTH/MIDI`);
- active notes or NoteOff ownership;
- queued MIDI events or route revisions;
- SMF per-track routing state;
- Pattern/Phrase/Song musical content;
- Scene generation identity;
- transport master/follower choice itself.

Applying a profile may replace device defaults, but must preserve user/session-owned enable flags and transport ownership controls unless the user explicitly requests a reset.

## Target profiles

### SEQTRAK

`SEQTRAK` is a concrete device profile. Only behavior verified from documentation plus direct hardware acceptance may be marked supported.

Existing validated channel/drum defaults and transport claims are retained as the current evidence baseline.

Vendor-specific controls such as receiver MONO/POLY must become capabilities of this profile only after their production stack is integrated and hardware-verified. R1 does not wire those controls.

### Generic MIDI

`Generic MIDI` is a conservative non-vendor profile:

- no Yamaha-specific CC assumptions;
- no fixed device-specific drum channels;
- no automatic claim that a device implements the GM percussion map;
- standard channel/note/transport behavior only where the project explicitly supports it.

This is intentionally distinct from the existing `GeneralMidi` enum value.

### Custom

`Custom` remains user-owned routing data with conservative capability claims. Applying `Custom` must not overwrite valid custom routes.

## Integration boundary

R1 is research/contracts/tests only.

Production 0.9.7 integration is deferred until the earlier release train is frozen. At integration time:

1. rebase/port this contract onto the exact accepted release baseline;
2. decide the `GeneralMidi` versus `Generic MIDI` compatibility strategy;
3. expose one bounded profile-selection owner;
4. make runtime capability consumers read that owner instead of adding device checks throughout the codebase;
5. add UI only after API/ownership tests are green;
6. run Generic MIDI host tests and SEQTRAK hardware acceptance on one exact SHA.

No profile-specific branch should be added to the realtime audio loop.

## Hardware list

R1 host contract tests require no hardware.

Future SEQTRAK production acceptance requires:

- M5Stack Cardputer ADV;
- Yamaha SEQTRAK;
- the project's existing USB-MIDI connection path;
- normal Cardputer ADV power/USB setup.

## Wiring

R1: none.

Future hardware acceptance uses the existing GroovePuter -> SEQTRAK MIDI connection. No GPIO/I2C wiring changes are introduced. Cardputer ADV PORT.A remains GPIO2 SDA / GPIO1 SCL for unrelated I2C peripherals.

## Build / test

From repository root:

```bash
bash tests/run_midi_device_profiles_0_9_7_tests.sh
```

The dedicated GitHub workflow is:

```text
MIDI Device Profiles 0.9.7 R1
```

## Expected behavior

The R1 gate proves:

- persisted profile numeric identities remain stable;
- existing SEQTRAK routing defaults remain unchanged;
- existing SEQTRAK transport capability claims remain conservative;
- current General MIDI drum-map behavior is recorded explicitly;
- profile application preserves user-owned enable/live/transport controls;
- Custom preserves custom routes;
- the current Cardputer profile-selection gap is recorded rather than silently treated as finished product behavior.

## Troubleshooting

If the C++ test fails after an intentional profile change, first determine whether the change modifies persisted identity, device defaults, or user-owned state. Do not simply update expected values if the change would alter schema compatibility or routing ownership.

If the source regression fails because a real profile-selection path has been added, that means R1 has been superseded by production integration. Update the contract and regression together on the production branch rather than weakening the R1 evidence silently.

## Acceptance checklist

- [ ] dedicated R1 runner passes;
- [ ] no production source files changed in the R1 PR;
- [ ] no persistence schema version changed;
- [ ] no UI/routing/audio callback behavior changed;
- [ ] SEQTRAK current defaults and validated transport claims are frozen by executable tests;
- [ ] `GeneralMidi` versus Generic MIDI semantic conflict is documented explicitly;
- [ ] production integration remains blocked on an explicit migration decision and later exact-SHA hardware acceptance.
