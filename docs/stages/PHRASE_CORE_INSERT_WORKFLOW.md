# Phrase Core — Song insert workflow

## Purpose

Make the normal PHRASE CORE workflow predictable:

- `Enter` captures the selected 1/2/4/8-bar Song region into Phrase A/B/C/D.
- the screen shows an explicit `TO:<Song slot><row>` destination;
- `Ctrl+Left` / `Ctrl+Right` move that destination by one row;
- `Ctrl+Up` / `Ctrl+Down` move that destination by eight rows;
- `W` inserts the saved Phrase before the visible destination when that row is inside Song;
- if `TO:` is beyond the current Song end, `W` writes exactly there and keeps the gap empty;
- `Alt+W` replaces the Phrase lanes at the visible destination without shifting rows.

The previous normal `W` behavior rejected any occupied target with `OCCUPIED`. The target was also implicit in the engine Song position, so save-then-insert could fail or affect a row the screen did not identify.

Cardputer hardware note: the physical punctuation positions used for the arrow cluster are emitted as canonical arrow HID keys before page handlers receive input. Therefore raw `,` / `.` are not a reliable independent control on Cardputer-Adv. PHRASE uses `Ctrl+Arrow` instead.

## Hardware list

- M5Stack Cardputer-Adv.
- USB-C cable for flash and Serial Monitor.
- No external MIDI device is required.

## Wiring

No external wiring. PORT.A and external I2C are not used by this test.

## Build / Flash

```bash
export FQBN=esp32:esp32:m5stack_cardputer
export USB_MODE=hwcdc

python3 tests/test_phrase_ui_source_regressions.py

mkdir -p build/host-tests
g++ -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I. \
  tests/test_phrase_core.cpp \
  -o build/host-tests/test_phrase_core
./build/host-tests/test_phrase_core

g++ -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I. \
  tests/test_phrase_workspace.cpp \
  -o build/host-tests/test_phrase_workspace
./build/host-tests/test_phrase_workspace

./scripts/check_cardputer_dram.sh
arduino-cli compile --fqbn "$FQBN" -b "$FQBN" . \
  --build-property "build.extra_flags=-DGROOVEPUTER_USB_MODE=${USB_MODE}"
arduino-cli upload --fqbn "$FQBN" -p /dev/ttyACM0 .
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

### Normal insert

1. In SONG, place the source at the Song position you want to capture.
2. Open PHRASE CORE.
3. Select Phrase slot `1..4` and capture length with `Up/Down`.
4. Press `Enter` to capture.
5. After a successful capture, `TO:` automatically moves to the first row after the captured region.
6. Use `Ctrl+Left` / `Ctrl+Right` to move `TO:` by one row, or `Ctrl+Up` / `Ctrl+Down` by eight, if a different destination is wanted.
7. Press `W`.
8. If `TO:` points inside the current Song, the Phrase is inserted before that row and existing complete Song rows move down by the Phrase length.
9. If `TO:` points beyond the current Song end, the Phrase is materialized exactly at `TO:` and all intervening rows are explicitly empty.
10. After a successful `W`, `TO:` advances by the Phrase length, so repeated `W` builds consecutive Song regions.
11. `OCCUPIED` is not expected from normal `W` merely because the destination already contains patterns.

The destination is owned by PHRASE CORE and shown on-screen. It does not depend on an invisible Song playhead or on Song Mode being enabled.

### Explicit replace

At the visible `TO:` destination, `Alt+W` replaces Synth A / Synth B / Drums references covered by the Phrase. It does **not** insert rows and does **not** shift the rest of the Song. Other Song lanes, including Voice, remain in their existing rows. On PHRASE, `Alt+W` is reserved for this REPLACE command; on other pages the existing waveform-overlay shortcut remains unchanged.

### Capacity boundary

If the Phrase would extend past Song row 128, `W` must fail with `RANGE`. The Song must remain byte-for-byte unchanged by the failed insertion.

## Troubleshooting

- `EMPTY`: the selected Phrase slot has no valid captured/derived Phrase.
- `RANGE`: the Phrase would extend past the 128-row Song capacity.
- `OCCUPIED`: should no longer occur from normal PHRASE `W`. It remains a valid low-level result for the legacy non-inserting write API.
- Wrong destination row: read `TO:` on PHRASE and adjust it with `Ctrl+Arrow`; Left/Right are fine +/-1, Up/Down are coarse +/-8.
- Plain arrow keys change BAR/LEN instead of `TO:`: expected. Hold `Ctrl` to edit the destination.
- Blank rows before an inserted Phrase: expected when `TO:` was deliberately placed beyond the current Song end.
- `Alt+W` did not shift rows: expected; `Alt+W` is REPLACE, not INSERT.
- `Alt+W` toggles waveform while on PHRASE: regression; PHRASE must receive the key before the global waveform shortcut.

## Acceptance checklist

- [ ] Capture a 1-bar Phrase from a populated Song row with `Enter`.
- [ ] `TO:` becomes the row immediately after the captured region.
- [ ] `Ctrl+Left` / `Ctrl+Right` visibly move `TO:` by exactly one row and clamp to 1..128.
- [ ] `Ctrl+Up` / `Ctrl+Down` visibly move `TO:` by exactly eight rows and clamp to 1..128.
- [ ] Plain Left/Right still preview Phrase bars and plain Up/Down still change capture length.
- [ ] Point `TO:` at another populated Song row and press `W`; no `OCCUPIED` appears.
- [ ] The captured Phrase appears at the destination row.
- [ ] The old destination row moves down by one row.
- [ ] Repeat with a 2- or 4-bar Phrase; all following rows move by exactly the Phrase length.
- [ ] After `W`, `TO:` advances by exactly the inserted Phrase length.
- [ ] Existing Voice-lane values after the insertion point move with their complete Song rows and are not lost.
- [ ] Move `TO:` beyond the current Song end and press `W`; the Phrase appears exactly there and the gap is empty.
- [ ] Insert into an otherwise empty Song; no extra blank leading row is created when `TO:` is row 1.
- [ ] Press `Alt+W` on a populated destination; Phrase lanes are replaced, Song length does not grow, and waveform overlay does not toggle.
- [ ] On a non-PHRASE page, `Alt+W` still toggles the waveform overlay.
- [ ] Near the 128-row limit, an oversized insert returns `RANGE` and leaves the Song unchanged.
- [ ] Save/reboot/load and confirm the resulting Song arrangement persists.
