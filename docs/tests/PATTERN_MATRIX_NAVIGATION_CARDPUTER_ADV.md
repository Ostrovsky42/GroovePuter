# Pattern matrix navigation — Cardputer ADV acceptance

## Purpose

Verify the fixed `PAGE + BANK + SLOT` pattern-address model, direct Song selection across all 16 pattern pages, and the composite address printed directly on Synth A/B note-editor screens.

## Hardware list

- M5Stack Cardputer ADV;
- USB-C data cable;
- FAT32 microSD if project persistence is also being exercised.

## Wiring

None. Use the built-in display, keyboard, audio codec, and optional microSD slot. No external I2C, SPI, or PORT.A device is required.

## Build / Flash steps

```bash
git fetch origin
git switch agent/20260807-02-pattern-matrix-navigation
git reset --hard origin/agent/20260807-02-pattern-matrix-navigation
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Record `git rev-parse HEAD`. Change the serial device only if Cardputer ADV enumerates elsewhere.

## Expected behavior

Pattern identity is one matrix:

```text
PAGE 1..16 x BANK A/B x SLOT 1..8 = 256 addresses
```

The coordinates are independent:

```text
1A1 -> page 2 -> 2A1
2A1 -> slot 2 -> 2A2
2A2 -> bank B -> 2B2
2B2 -> page 3 -> 3B2
```

In Song:

- `Ctrl+1..8` selects pattern pages 1..8;
- `Ctrl+Fn+1..8` selects pattern pages 9..16;
- `Q..I` assigns slot 1..8 on the current pattern page and current target-track bank;
- `Alt+[` / `Alt+]` remains the sequential previous/next pattern-page path;
- selecting a slot never resets PAGE to 1.

On Synth A/B note-editor screens, the note/pattern header must print the full composite address next to the engine name, for example `2A2 TB303` or `16B8 SID`. The global status chrome must agree (`S-A 2A2`, `S-B 16B8`). Both use the current pattern page, bank, and local slot.

## Troubleshooting

### `Ctrl+Fn+1` does not select page 9

Confirm the Song page is active and capture the key event modifiers in Serial. `Ctrl+Fn+digits` must reach Song; the global direct UI-page shortcut intentionally ignores modified digits while Ctrl is held.

### `2A1` becomes `1A2`

Stop acceptance. Song assignment lost the current pattern-page coordinate. Record the active track, page, bank, slot, and exact key sequence.

### Composite address is missing on the note screen

Record Synth A/B, visual style, page, bank, and slot. The note/pattern header and status chrome must derive the same address from `currentPageIndex()`, current bank, and local pattern slot; neither may display a flattened global pattern number.

### Page save/load fails

Capture Serial output and the active project namespace. Do not treat a failed page switch as a successful navigation result.

## Acceptance checklist

- [ ] `bash tests/run_host_tests.sh` passes;
- [ ] `bash scripts/build.sh --warnings all` passes;
- [ ] `Ctrl+1` selects page 1 and `Ctrl+8` selects page 8 in Song;
- [ ] `Ctrl+Fn+1` selects page 9 and `Ctrl+Fn+8` selects page 16 in Song;
- [ ] `Alt+[` / `Alt+]` traverses the same 1..16 pattern-page space;
- [ ] from `1A1`, selecting page 2 keeps bank/slot and produces `2A1`;
- [ ] from `2A1`, selecting slot 2 produces `2A2`, not `1A2`;
- [ ] switching bank from `2A2` produces `2B2`;
- [ ] moving to page 3 from `2B2` produces `3B2`;
- [ ] Song cell assignment uses the current pattern page and target-track bank;
- [ ] Synth A note screen shows the full composite address;
- [ ] Synth B note screen shows the full composite address;
- [ ] the composite address stays readable in the supported visual styles;
- [ ] Serial shows no page-save/load, project-namespace, or rollback errors during the smoke test.
