# Workflow Session and Recovery Persistence

## Purpose

Keep navigation and device preferences stable across workflow switches and
reboots, while preserving the explicit distinction between a manually saved
project and a temporary recovery snapshot.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable
- microSD card containing at least two projects

## Wiring

No external wiring is required. PORT.A is unused. Existing Cardputer ADV bus
assumptions remain GPIO2 SDA / GPIO1 SCL.

## Build / Flash

```bash
bash tests/run_host_tests.sh
cd platform_sdl && make clean all CXX=g++
cd ..
bash scripts/build.sh --warnings all
```

Flash with the repository's normal Cardputer ADV procedure.

## Expected behavior

- Each of PERFORM, GENERATE, HUB, SONG and SETTINGS remembers its last page.
- Fn+Tab and Fn+[ / ] return to that page instead of the workflow's first page.
- Plain [ / ] still moves locally inside the current workflow.
- Reboot restores the active page, per-workflow pages, CYBER/CARBON/AMBER theme,
  waveform overlay state and master volume.
- Master volume is a device preference and loading another project does not
  change it.
- The selected project name remains stored transactionally on SD.
- Dirty project edits are recovery-saved after three idle seconds, but only
  while transport is stopped.
- Recovery autosave does not clear the `*` marker.
- Manual Save writes the main project and removes the recovery file.
- Rebooting after an unsaved edit loads recovery and keeps the project dirty.

NVS stores only the compact UI/device session. Project data and recovery remain
on the microSD card.

## Troubleshooting

### Workflow returns to its first page

Verify `tests/test_ui_session_state.cpp` passes and that all page transitions go
through `MiniAcidDisplay::transitionToPage_()`.

### Theme or volume resets after reboot

Stop playback, leave the device running for at least one second, then reboot.
NVS writes are deliberately deferred while transport is active.

### Unsaved edit is not recovered

Stop playback and allow at least three seconds after the last persistent edit.
Check Serial for `[AUTOSAVE] recovery revision=` and verify the microSD card is
mounted and writable.

### Project always appears dirty after manual Save

Check that the main scene write and recovery cleanup both succeeded. A failed
recovery cleanup intentionally keeps Save from establishing a clean baseline.

## Acceptance checklist

- [ ] Leave PERFORM on MIDI Player, switch away and back, and remain on Player.
- [ ] Leave GENERATE on FEEL/TEXTURE and return to FEEL/TEXTURE.
- [ ] Leave HUB on Synth B parameters and return to that page.
- [ ] Leave SETTINGS on Advanced Generator and return to it.
- [ ] Plain brackets still wrap only inside the current workflow.
- [ ] Reboot restores the active page and all remembered workflow pages.
- [ ] Reboot restores theme and waveform overlay state.
- [ ] Reboot restores master volume within one UI step.
- [ ] Loading another project does not change device master volume.
- [ ] Reboot restores the last selected project.
- [ ] An unsaved stopped edit is recovered after reboot and still shows `*`.
- [ ] Playback-time edits do not cause SD/NVS writes until transport stops.
- [ ] Manual Save clears `*` and removes the recovery snapshot.
- [ ] Host regressions pass.
- [ ] SDL build passes.
- [ ] Cardputer ADV build passes.
- [ ] No new audio underruns or watchdog resets occur.
