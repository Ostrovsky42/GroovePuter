# 0.9.7-R2 — MIDI Device Capability Model

## Purpose

Extend the existing `MidiDeviceProfile` owner without creating a second profile framework. R2 separates **Generic MIDI** from **General MIDI**, adds a small capability descriptor, and keeps runtime routing/UI/persistence selection out of scope.

Base stack:

```text
dev_0.9.4 @ d3db4e48ebc08862bdaf9f62532414f009839192
  -> 0.9.7-R1 @ 246ff995f962d285fe7d986e16c63607054931b5
  -> 0.9.7-R2
```

## Existing ownership retained

`MidiDeviceProfile` remains the profile identity owner. Existing persisted values stay stable:

```text
0 = SeqtrakNative
1 = GeneralMidi
2 = Custom
3 = GenericMidi   // new in R2
```

R2 does not add a second device/profile enum and does not move routing ownership out of `MidiOutputSettings`.

## Capability model

`MidiDeviceCapabilities` describes only device/profile facts:

- drum mapping kind;
- transport capabilities;
- whether synth channel defaults are device-fixed defaults;
- whether the profile has vendor-specific controls.

It does **not** own:

- output enable/disable intent;
- current live target;
- Pattern/Phrase/Song content;
- drum INTERNAL/SAMPLE/BOTH/MIDI ownership;
- active notes or queued MIDI events;
- SMF per-track route state;
- master/follower user choice.

### Built-in profiles

| Profile | Drum mapping | Fixed synth defaults | Vendor controls | Transport policy in R2 |
|---|---|---:|---:|---|
| `SeqtrakNative` | SEQTRAK native | yes | yes | existing hardware-validated conservative set |
| `GeneralMidi` | GM percussion CH10 | yes | no | historical behavior preserved in R2 |
| `GenericMidi` | none | no | no | Clock/Start/Stop TX only; no Continue/SPP/RX claims |
| `Custom` | user-defined | no | no | existing conservative custom policy |

`GenericMidi` deliberately disables all default drum routes. A generic MIDI endpoint must not silently receive GM percussion notes.

## Persistence compatibility boundary

`MidiSettingsCodec` remains schema v2 with the same payload/record size. R2 only extends the semantic range of the existing one-byte profile field.

No Cardputer runtime selection path can choose `GenericMidi` in R2, so existing saved settings are unchanged in normal firmware operation. Production 0.9.7 must still make an explicit persistence/downgrade decision before exposing Generic MIDI in UI; older firmware does not know profile value `3`.

## Hardware list

- No hardware is required for the R2 host contract.
- Cardputer ADV and SEQTRAK are not needed to validate this checkpoint.

## Wiring

None. R2 changes no GPIO, USB wiring, I2C, I2S, SD, audio, or MIDI physical connection.

## Build / test

From repository root:

```bash
bash tests/run_midi_device_capabilities_0_9_7_tests.sh
```

The runner also executes the R1 contract before compiling the R2 executable test.

## Expected behavior

- R1 contract remains green.
- `GenericMidi` is distinct from `GeneralMidi`.
- GM CH10 percussion defaults remain unchanged for `GeneralMidi`.
- `GenericMidi` has no default drum routes.
- SEQTRAK capabilities remain unchanged.
- Applying Generic MIDI preserves user-owned enable flags, live target, clock-source choice, and external-follow choice.
- Cardputer session still has no Generic profile-selection path.

## Troubleshooting

If the R2 source regression fails because `GenericMidi` appears in `cardputer_midi_settings_session.cpp`, runtime-selection work has leaked into the capability checkpoint and must be moved to R3.

If old `GeneralMidi` tests fail, R2 has accidentally changed legacy profile behavior; restore it and keep semantic migration separate.

If Generic MIDI enables drum routes, the implementation is again conflating Generic MIDI with General MIDI.

## Acceptance checklist

- [ ] Existing enum IDs `0/1/2` unchanged; `GenericMidi == 3`.
- [ ] Schema-v2 shape unchanged.
- [ ] SEQTRAK native mapping/capabilities unchanged.
- [ ] General MIDI percussion mapping unchanged.
- [ ] Generic MIDI exposes no fixed drum mapping.
- [ ] Generic MIDI does not claim Continue/SPP/RX support.
- [ ] User-owned routing/transport intent survives profile application.
- [ ] No Cardputer runtime/UI selection introduced.
- [ ] No USB-MIDI/audio/realtime path changed.
- [ ] R1 + R2 dedicated host tests pass.

## Next

R3 may add one bounded runtime profile owner/apply path. R4 owns persistence exposure/migration policy. UI and SEQTRAK hardware acceptance come only after those ownership boundaries are fixed.
