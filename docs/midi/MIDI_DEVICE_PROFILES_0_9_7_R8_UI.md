# 0.9.7-R8 Device Profile UI

## Purpose

Expose the already-validated Device Profile selection contract in the existing `PROJECT` page without changing active MIDI routing while the device is running.

This is a UI-only checkpoint over R7 persistence. It does not add live profile switching, custom route editing, output ownership, or a new page/workflow ID.

## Hardware

- M5Stack Cardputer ADV
- USB MIDI cable/host as required by the selected profile test
- Optional Yamaha SEQTRAK for final hardware acceptance

## Controls

Open `PROJECT`, then press `Tab` until the `MIDI` section is active.

```text
DEVICE   < SEQTRAK >
DEVICE   < GM >
DEVICE   < GENERIC >
```

- `Left/Right`: preview `SEQTRAK -> GM -> GENERIC` presets.
- `Enter`: persist the preview as the next-boot Device Profile.
- Leaving the MIDI section without `Enter` discards the unsaved preview.
- `CUSTOM` may be displayed when loaded from existing persisted state, but it is not offered in the preset cycle because there is no Custom route editor yet.

The information panel reports:

```text
Saved:<profile>
Apply:ENTER SAVE   # preview differs from persisted selection
Apply:REBOOT       # selection is saved but differs from active runtime
Apply:ACTIVE       # saved selection is already active
```

## Realtime boundary

`Left/Right` never changes `MidiDeviceProfileRuntime`, USB lanes, active-note ownership, transport capabilities, or published startup routes.

`Enter` only writes the existing schema-v2 next-boot intent through `selectCardputerMidiDeviceProfileForNextBoot()`.

The selected profile becomes active only after reboot, when R5a restores MIDI settings before the USB dispatcher starts.

## Build / test

```bash
bash tests/run_midi_device_profile_ui_0_9_7_tests.sh
bash tests/run_host_tests.sh
bash scripts/build.sh
```

The dedicated R8 runner executes the complete R1-R7 Device Profile chain first, then the UI helper behavior test and source ownership regressions.

## Expected behavior

1. Boot with SEQTRAK active.
2. Open `PROJECT`, `Tab` to `MIDI`.
3. `Right` previews `GM`; the row is marked as edited and the information panel shows `Apply:ENTER SAVE`.
4. `Tab` away and back before saving: the preview returns to the persisted profile.
5. Preview `GM` again and press `Enter`: the UI shows/surfaces `REBOOT` while active routing remains SEQTRAK.
6. Reboot: `GM` is now active and the UI reports `Apply:ACTIVE`.
7. Repeat for `GENERIC` and back to `SEQTRAK`.

## Troubleshooting

- If `Enter` reports a save failure, inspect the Cardputer serial log for `[MIDI-SETTINGS] profile-save=0` and verify NVS/Preferences health.
- If the wire mapping changes before reboot, treat it as a release blocker: R8 must not live-apply a profile.
- If `CUSTOM` appears in the Left/Right preset cycle, treat it as a regression; it is display-only in R8.
- If ADV fixed-DRAM or `sizeof(ProjectPage) <= 256` fails, do not increase the page budget; reduce UI state instead.

## Acceptance checklist

- `PROJECT -> MIDI` is reachable with `Tab`.
- `Left/Right` cycles only SEQTRAK / GM / GENERIC.
- `CUSTOM` is display-only.
- Unsaved preview is discarded when leaving the MIDI section.
- `Enter` persists next-boot intent.
- Active MIDI routing does not change before reboot.
- Saved profile becomes active after reboot.
- Core, SDL, ADV + fixed DRAM, and SEQTRAK MIDI-only builds remain green.
- No changes to `INTERNAL / SAMPLE / BOTH / MIDI` output ownership.
