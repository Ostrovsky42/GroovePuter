# 0.9.7-R5a — MIDI Settings Boot Order

## Purpose

Make persisted MIDI settings/profile state available before the Cardputer USB MIDI dispatcher starts. This is a lifecycle prerequisite for later profile-to-wire route binding.

R5a does **not** change physical MIDI routes, active-note ownership, Pattern/Performance event semantics, persistence schema, UI profile selection, or the audio callback.

Stack:

```text
R4 route projection @ 8f002405e1cf9b674fced54dff3492ef219ca53b
  -> R5a boot order
```

## Root cause

Before R5a the boot order was effectively:

```text
registerCardputerUsbMidiSink()
  -> create MidiDispatchTask
  -> g_output.begin()

...later...
MiniAcidDisplay construction
  -> CardputerMidiSettingsBinding
  -> initializeCardputerMidiSettingsSession()
  -> NVS load / MidiDeviceProfileRuntime initialize
```

That is safe for the historical hardcoded SEQTRAK routing, but wrong for future profile-based startup routing: the dispatcher would observe the runtime's safe SEQTRAK fallback before the persisted profile was restored.

## Change

`GroovePuter.ino` now explicitly includes the Cardputer MIDI settings session and calls:

```cpp
GroovePuterPlatform::initializeCardputerMidiSettingsSession();
```

immediately after the bounded SMF runtime setup and before `registerCardputerUsbMidiSink(...)`.

The existing late `CardputerMidiSettingsBinding` remains temporarily as an idempotent compatibility call. `CardputerMidiSettingsSession::initialize()` already returns immediately after the first initialization, so no second NVS load is performed.

## Ownership boundary

- `GroovePuter.ino` owns visible boot sequencing.
- `cardputer_midi_settings_session.cpp` remains the only Preferences/NVS implementation owner.
- `MidiDeviceProfileRuntime` remains the single control-side settings/profile snapshot owner.
- USB transport/output remain free of Preferences/NVS dependencies.
- R5a does not consume `MidiOutputRouteProjection` yet.

## Hardware list

- M5Stack Cardputer ADV for the final embedded compile/boot smoke.
- SEQTRAK is **not required** for the R5a lifecycle contract because no wire routing behavior changes.

## Wiring

No wiring changes.

For an optional Cardputer smoke:

```text
Cardputer ADV USB-C -> development computer
```

No PORT.A, I2C, I2S, SD, or external MIDI wiring changes are introduced by R5a.

## Build / test

From repository root:

```bash
bash tests/run_midi_settings_boot_order_0_9_7_tests.sh
bash tests/run_host_tests.sh
bash scripts/build.sh
```

The dedicated R5a runner executes the complete R1 -> R4 device-profile contract stack before checking the new boot-order source contract.

## Expected behavior

Software/source contract:

```text
initializeCardputerMidiSettingsSession()
        <
registerCardputerUsbMidiSink()
        <
MiniAcidDisplay allocation
```

The exact ordering is asserted directly from `GroovePuter.ino`.

On Cardputer ADV the existing `[MIDI-SETTINGS] load=...` message should occur during setup before the USB MIDI runtime is registered. Boot must otherwise behave exactly as before.

## Troubleshooting

### R5a source regression says settings initialize after USB

Do not move profile projection into USB as a workaround. Restore the explicit settings bootstrap before `registerCardputerUsbMidiSink()`.

### Cardputer compile cannot find the settings initializer

Verify `GroovePuter.ino` includes:

```cpp
#include "src/platform/cardputer_midi_settings_session.h"
```

The public header is intentionally lightweight and does not expose Preferences/NVS.

### Settings appear to load twice

The late UI binding may still invoke the initializer, but the session is idempotent. If logs show two real NVS loads, the `if (initialized_) return;` contract has regressed.

### USB routing changed

That is outside R5a. `UsbMidiOutput` must remain free of `midi_output_route_projection.h`, and the historical Pattern drum mapping must remain untouched until the later binding checkpoint.

## Acceptance checklist

- [ ] explicit settings-session include in `GroovePuter.ino`;
- [ ] settings initialization occurs before USB dispatcher registration;
- [ ] USB dispatcher registration remains before heavy `MiniAcidDisplay` allocation;
- [ ] Preferences/NVS remains outside sketch, USB dispatcher and audio code;
- [ ] settings initialization remains idempotent;
- [ ] no route projection dependency in `UsbMidiOutput`;
- [ ] no Pattern/Performance wire mapping changes;
- [ ] R1 -> R4 contracts remain green;
- [ ] dedicated R5a source gate passes;
- [ ] host aggregate passes;
- [ ] Cardputer ADV normal + fixed DRAM passes;
- [ ] SEQTRAK MIDI-only compile passes;
- [ ] SDL passes.

## Next

After R5a is green, the first live routing checkpoint may consume the already-built `MidiOutputRouteProjection` **at startup only**. Pattern Synth A/B and Pattern drums should be migrated first. Live Performance A/B/DX/Drums and runtime profile switching remain separate ownership work.
