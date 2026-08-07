# Project pattern storage — Cardputer ADV acceptance

## Purpose

Run a short post-merge smoke for #102 on the 0.9 candidate: project pages remain isolated, Save As copies pages, Clear removes only the target namespace, and Synth A/B/Drums show canonical `page + bank + slot` addresses.

## Hardware

- M5Stack Cardputer ADV;
- FAT32 microSD;
- USB-C data cable.

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

Record `git rev-parse HEAD`. Change the serial path only if the device enumerates elsewhere.

## Storage smoke

1. Open project `alpha`.
2. Edit Synth A at page 2, bank B, slot 7 and Save.
3. Open project `beta`; confirm `2B7` is empty.
4. Return to `alpha`; confirm the pattern remains.
5. Save As to `alpha-copy`; confirm all pages were copied.
6. Clear `alpha-copy`; confirm it is empty.
7. Reboot immediately; confirm it stays empty.
8. Return to `alpha`; confirm its original page remains.
9. Verify Synth A, Synth B and Drums update page, bank and slot in the status address.

Expected layout:

```text
/patterns/alpha/page_01.gpp
/patterns/alpha-copy/page_01.gpp
```

Spaces become `_20`; literal underscores become `_5F`. Pattern `.gpp` format remains version 3.

## Expected behavior

- projects never see one another's pages;
- Save As copies valid main and backup pages;
- Clear removes `.gpp`, `.tmp` and `.bak` only for the active project;
- reboot does not resurrect cleared pages;
- corrupt primary may recover only from a valid backup in the same project;
- status shows `S-A`, `S-B` or `DRM` plus the real address.

## Input ownership

- arrows stay inside the editor grid;
- Up/Down stop at the first/last row;
- `Q`–`I` selects slots 1–8;
- `Ctrl+1` selects bank A and `Ctrl+2` bank B;
- plain `1`–`0` remain global mutes;
- plain `B` does not change bank.

## Troubleshooting

### Save As fails

Capture source/target project names, SD paths and serial output. The current `dev_0.9` base includes deterministic host file-copy behavior; do not weaken the storage tests.

### Clear pages return after reboot

Stop release acceptance. Capture the active project, encoded folder, directory listing before/after Clear and boot recovery log.

### Address is wrong

Record track, page, bank, slot, theme and navigation sequence.

## Acceptance checklist

- [ ] exact flashed head recorded;
- [ ] `alpha` and `beta` do not share `2B7`;
- [ ] Save As copies the edited page;
- [ ] Clear affects only `alpha-copy`;
- [ ] cleared pages stay cleared after reboot;
- [ ] `alpha` remains intact;
- [ ] Synth A address is correct;
- [ ] Synth B address is correct;
- [ ] Drums address is correct;
- [ ] arrows remain inside the grid;
- [ ] `Q`–`I`, `Ctrl+1/2`, `B` and global number keys retain their contracts;
- [ ] serial shows no SD, namespace, migration or recovery error.
