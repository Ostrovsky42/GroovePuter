# Project pattern storage — Cardputer ADV acceptance

## Purpose

Verify the merged #102 storage contract on the `dev_0.9` release candidate: pattern pages belong to one project, Save As copies them, Clear removes only the target namespace, and Synth A/B/Drums display one readable `page + bank + slot` address such as `2B7`.

This is a short post-merge smoke. It does not replace the development-time CRC, migration and failure-injection host tests.

## Hardware

- M5Stack Cardputer ADV;
- FAT32 microSD card;
- USB-C data cable.

No external modules are required.

## Wiring

None. Use the built-in display, keyboard and microSD slot.

## Build and flash

```bash
git fetch origin
git switch release/0.9-final-stabilization
git reset --hard origin/release/0.9-final-stabilization
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Change `/dev/ttyACM0` only if the device enumerates under another path. Record the exact PR head before flashing.

## Post-merge storage smoke

1. Create/open project `alpha`.
2. On Synth A, edit page 2, bank B, slot 7 and Save.
3. Create/open project `beta`; confirm the same address is empty.
4. Return to `alpha`; confirm its pattern is intact.
5. Use Save As from `alpha` to create `alpha-copy`; confirm the page was copied.
6. Clear `alpha-copy`; confirm its page is empty.
7. Reboot immediately; confirm `alpha-copy` stays empty.
8. Return to `alpha`; confirm its original page remains.
9. Check Synth A, Synth B and Drums status addresses after changing page, bank and slot.

Expected project layout:

```text
/patterns/alpha/page_01.gpp
/patterns/alpha-copy/page_01.gpp
```

Spaces and underscores are encoded in folder names; a space becomes `_20` and a literal underscore `_5F`.

## Expected behavior

- projects never see one another's pages;
- Save As copies all valid main and backup pages into the new namespace;
- Clear removes `.gpp`, `.tmp` and `.bak` only for the active project;
- reboot does not resurrect a cleared project;
- corrupt primary files may recover from a valid `.bak` in the same project only;
- status chrome shows `S-A`, `S-B` or `DRM` plus the real `page + bank + slot` address;
- pattern `.gpp` format remains version 3.

## Pattern editor input ownership

- arrows move only inside the visible grid;
- Up/Down stop at the first/last row and never enter BANK/PATTERNS selectors;
- `Q`–`I` selects slots 1–8 and leaves focus in the grid;
- `Ctrl+1` selects bank A and `Ctrl+2` bank B;
- plain `1`–`0` remain global track mutes;
- plain `B` does not change banks;
- existing Alt/Meta page and Shift/Ctrl selection behavior remains unchanged.

## Troubleshooting

### Save As reports copy failure

Record the exact SHA and serial output. Confirm `src/audio/pattern_paging.cpp` copies by the known source file size and does not depend on repeated `available()` after EOF.

### Cardputer compile reports `PatternPagingService` redefinition

Confirm `src/audio/pattern_paging.h` has the explicit `GROOVEPUTER_SRC_AUDIO_PATTERN_PAGING_H_` guard. Aliased Arduino paths can defeat `#pragma once` alone.

### Clear appears successful but pages return after reboot

Stop release acceptance. Capture the active project name, encoded SD folder, directory listing before/after Clear, and the first boot/recovery serial log.

### Address shows `PAT` or the wrong page

Record page, bank, slot, track, theme and navigation sequence. The status owner is not reporting the canonical address.

## Acceptance checklist

- [ ] exact flashed PR head recorded;
- [ ] project `alpha` and `beta` do not share one edited page;
- [ ] Save As copies the edited page;
- [ ] Clear affects only `alpha-copy`;
- [ ] cleared pages stay cleared after reboot;
- [ ] returning to `alpha` restores its original page;
- [ ] Synth A address is correct;
- [ ] Synth B address is correct;
- [ ] Drums address is correct;
- [ ] page, bank and slot each update the corresponding address component;
- [ ] Up/Down remain inside the editor grid;
- [ ] `Q`–`I` and `Ctrl+1/2` do not steal arrow focus;
- [ ] plain `B` leaves the bank unchanged;
- [ ] plain number keys still control global mutes;
- [ ] serial shows no SD write, namespace, migration or recovery error.
