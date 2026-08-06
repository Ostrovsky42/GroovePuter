# Project pattern storage — Cardputer ADV acceptance

## Purpose

Verify that pattern pages belong to one project, that **New** and **Clear Project** remove only the selected project's page files, and that pattern selectors show one readable `page + bank + slot` address such as `2B7`.

## Hardware

- M5Stack Cardputer ADV
- microSD card formatted as FAT32
- USB-C cable

No external modules are required.

## Wiring

None. Use the built-in display, keyboard and microSD slot.

## Build and flash

```bash
git switch agent/project-pattern-storage-address
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Change `/dev/ttyACM0` if the Cardputer ADV appears under another device path.

## Expected behavior

1. Create project `alpha`, edit a pattern on page 2, bank B, slot 7. The selector heading reads `PATTERNS 2B7` or `PATTERN 2B7`.
2. Create a new project. Page 2 is empty; project `alpha` remains unchanged.
3. Return to `alpha`. Page 2, bank B, slot 7 contains the original pattern.
4. Run **Save As**. The new project initially contains the same pattern pages.
5. Run **Clear Project** in the copied project. All its pages become empty and remain empty after reboot; `alpha` still contains its original data.
6. On the SD card, page files are grouped by project:

```text
/patterns/alpha/page_01.gpp
/patterns/<copied-project>/page_01.gpp
```

Spaces and underscores in project names are encoded in folder names. For example, a space becomes `_20` and a literal underscore becomes `_5F`.

## Troubleshooting

- `Project clear not saved`: check that the SD card is writable and not full. Do not trust the clear operation until it succeeds.
- `Pattern cleanup failed`: creation succeeded in RAM, but project page files could not be removed.
- Old global files under `/patterns/page_XX.gpp` are migrated once into the active project. Do not remove power during the first boot after updating.
- A corrupt main page may load its `.bak` sibling. Both copies are removed by **Clear Project**.
- If the address briefly shows the previous bank immediately after opening a workflow, report the exact page and theme. Continuous redraw should correct it on the next frame.

## Acceptance checklist

- [ ] Synth A selector shows the full address, for example `PATTERNS 1A1`.
- [ ] Synth B selector shows its selected bank and slot.
- [ ] Drum selector shows the full address.
- [ ] `[` and `]` change the page portion of the address.
- [ ] Bank A/B changes the letter portion of the address.
- [ ] Pattern 1–8 changes the final digit.
- [ ] New project cannot see pages from the previous project.
- [ ] Save As copies all page files.
- [ ] Clear removes `.gpp`, `.tmp` and `.bak` only for the current project.
- [ ] Clear remains effective after immediate reboot.
- [ ] Returning to another project restores its pages.
- [ ] Serial log contains no SD write or pattern cleanup errors.
