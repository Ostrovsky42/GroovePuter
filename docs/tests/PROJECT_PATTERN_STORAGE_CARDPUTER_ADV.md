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
pio run -e m5stack-cardputer-adv
pio run -e m5stack-cardputer-adv -t upload
pio device monitor -b 115200
```

Use the repository's normal Cardputer ADV environment if the local environment name differs.

## Expected behavior

1. Create project `alpha`, edit a pattern on page 2, bank B, slot 7. The selector heading reads `PATTERNS 2B7` or `PATTERN 2B7`.
2. Create a new project. Page 2 is empty; project `alpha` remains unchanged.
3. Return to `alpha`. Page 2, bank B, slot 7 contains the original pattern.
4. Run **Save As**. The new project initially contains the same pattern pages.
5. Run **Clear Project** in the copied project. All its pages become empty; `alpha` still contains its original data.
6. On the SD card, page files are grouped by project:

```text
/patterns/alpha/page_01.gpp
/patterns/<copied-project>/page_01.gpp
```

The project folder may encode spaces as hexadecimal escape fragments such as `_20`.

## Troubleshooting

- `Pattern cleanup failed`: check that the SD card is writable and not full.
- Old global files under `/patterns/page_XX.gpp` are migrated once into the active project. Do not remove power during the first boot after updating.
- A corrupt main page may load its `.bak` sibling. Both copies are removed by **Clear Project**.
- If the address briefly shows the previous bank immediately after opening a workflow, press any navigation key and report the exact page and theme; it should settle on the next render frame.

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
- [ ] Returning to another project restores its pages.
- [ ] Serial log contains no SD write or pattern cleanup errors.
