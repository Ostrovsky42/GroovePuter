# Song Arranger UX Acceptance

## Purpose
Make Song mode readable at a glance and keep the horizontal arranger contract explicit: arrows navigate musical tracks and cross edit Song Slot A/B only at the outer edge, while `PAT:A/B` is an independent assignment context selected with `B`.

## Hardware list
- M5Stack Cardputer-Adv (ESP32-S3)
- USB-C cable for flash and serial monitor
- No external MIDI device is required for this test

## Wiring
No external wiring is required. Use the built-in Cardputer-Adv display and keyboard.

## Build / Flash
```bash
export FQBN=esp32:esp32:m5stack_cardputer
export USB_MODE=hwcdc
./tests/run_host_tests.sh
./scripts/check_cardputer_dram.sh
arduino-cli compile --fqbn "$FQBN" -b "$FQBN" . \
  --build-property "build.extra_flags=-DGROOVEPUTER_USB_MODE=${USB_MODE}"
arduino-cli upload --fqbn "$FQBN" -p <PORT> .
arduino-cli monitor -p <PORT> -c baudrate=115200
```

## Expected behavior
- Song cursor moves only through the visible musical columns: Synth A, Synth B, Drums.
- There is no invisible fourth keyboard column.
- In edit Slot A, Right from Drums moves to edit Slot B / Synth A.
- In edit Slot B, Left from Synth A moves to edit Slot A / Drums.
- Left at Slot A / Synth A and Right at Slot B / Drums stop at the outer boundary; they do not wrap.
- Crossing a Song-slot boundary preserves the current `PAT:A/B` assignment context.
- `B` toggles `PAT:A/B` without changing an existing Song cell.
- `Alt+B` explicitly flips the bank of an already stored Song reference or selection.
- `Ctrl+B` remains the independent play Song slot A/B control.
- Shift/Ctrl selection remains inside one visible track rectangle and never changes edit Song slot.
- Q..I writes from the visible `PAT:A/B` assignment context.
- Status shows edit Song slot, play Song slot, and `PAT:A/B` assignment context.
- During Song playback the active playback row has a full-row highlight and a `>` marker. The cell cursor remains separately visible.

## Troubleshooting
- If an outer arrow does not switch edit slot, confirm you are at Slot A / Drums moving Right or Slot B / Synth A moving Left; the two absolute outer boundaries intentionally clamp.
- If Q..I assigns an unexpected pattern, confirm `PAT:A/B` in Song status; use `B` to change assignment bank.
- If `Alt+B` changes an existing cell, that is expected: it is now the explicit stored-reference bank flip.
- If the playhead is not highlighted, confirm Song mode is enabled and transport is playing.

## Acceptance checklist
- [ ] Cardputer boots and Song page opens without a black screen/reset.
- [ ] Left/Right never enters an invisible fourth column.
- [ ] Slot A / Drums -> Right -> Slot B / Synth A works without a modifier.
- [ ] Slot B / Synth A -> Left -> Slot A / Drums works without a modifier.
- [ ] Slot A / Synth A + Left and Slot B / Drums + Right do not wrap.
- [ ] Slot crossing preserves the current `PAT:A/B` context.
- [ ] `B` changes PAT assignment context without mutating Song data.
- [ ] `Alt+B` flips the stored reference bank.
- [ ] `Ctrl+B` changes play slot independently.
- [ ] Q..I assigns from the `PAT:A/B` bank shown in status.
- [ ] While PLAY is running, the playback row is immediately obvious and marked with `>`.
- [ ] Cursor and playback row remain visually distinguishable.
- [ ] Serial monitor shows no exception/reset while navigating and assigning patterns.
