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
- Fn+Tab, Fn+[ / ] and the Fn+M launcher return to that page instead of the workflow's first page.
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

## Observable hardware retest R3

The first two hardware tests still reproduced a return to workflow entry pages.
R3 changes the real Cardputer event route rather than only the persistence
model:

- Fn+Tab and Fn+[ / ] resolve through one `switchWorkflow_()` function;
- Fn+[ / ] is intercepted before synth/drum pages receive the event;
- every transition still records its page through `transitionToPage_()`;
- boot, workflow switching and NVS save print exact session values to Serial;
- the launcher title is `GROOVEPUTER / NAV R3`;
- the launcher displays one-based child memory as `MEM P G H S C`.

Example after selecting MIDI Player, FEEL/TEXTURE and Synth B Sound:

```text
MEM 2 3 6 1 1
```

The HUB value depends on the selected page:

```text
1 OVERVIEW
2 SYNTH A
3 SYNTH B
4 DRUMS
5 SYNTH A SOUND
6 SYNTH B SOUND
```

Relevant Serial records:

```text
[SESSION] load=1 active=... mem=...
[NAV] workflow dir=... current=... target=... mem=...
[SESSION] saved active=... mem=...
```

If Fn+M does not show `NAV R3`, the flashed image is not the tested PR head.

## Troubleshooting

### Workflow returns to its first page

First verify the launcher title contains `NAV R3`. Then compare the displayed
`MEM` values before Enter with the Serial `[NAV]` target. A reset in `MEM` means
the transition was not recorded; correct `MEM` with a wrong target identifies
an input-routing defect; correct target followed by a different boot value
identifies NVS persistence failure.

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

- [ ] Fn+M title contains `GROOVEPUTER / NAV R3`.
- [ ] Leave PERFORM on MIDI Player; Fn+M shows first MEM value `2`.
- [ ] Leave GENERATE on FEEL/TEXTURE; Fn+M shows second MEM value `3`.
- [ ] Leave HUB on Synth B Sound; Fn+M shows third MEM value `6`.
- [ ] Fn+Tab restores these pages.
- [ ] Fn+[ / ] restores these pages and is not consumed by local editors.
- [ ] Plain brackets still wrap only inside the current workflow.
- [ ] After Stop and one second, Serial prints `[SESSION] saved` with expected page IDs.
- [ ] Reboot prints `[SESSION] load=1` with the same memory.
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