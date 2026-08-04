# Synth parameter MAIN / MORE tabs

## Purpose

Keep the four continuously performed Synth A/B parameters compact and immediately available while moving infrequent engine, oscillator, filter, distortion, and delay settings to a discoverable `MORE` tab.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable

## Wiring

No external wiring is required. The test uses the built-in 240×135 display and keyboard.

## Build and flash

```bash
./scripts/build_cardputer_adv.sh
```

Flash the generated Cardputer ADV firmware using the existing project workflow.

This UI stage expects the consolidated Cardputer word/HID Tab input fix from PR #63 before final hardware acceptance. `Fn+Tab` must remain global workflow navigation.

## Expected behavior

### MAIN

- `MAIN / MORE` is always visible at the top; `MAIN` is selected on the page's first entry.
- Four radius-13 continuous knobs show parameters 1–4.
- `A/Z`, `S/X`, `D/C`, and `F/V` remain direct real-time controls only on `MAIN`.
- A bottom summary shows the current engine and the next two parameter values.
- `DST` and `DLY` badges show current effect state.
- `TAB >` advertises the second tab.

### MORE

- Plain `Tab` switches to `MORE`; another plain `Tab` returns to `MAIN`.
- `TYPE`, parameter 5, parameter 6, `DST`, and `DLY` use five stable full-width rows.
- `Left/Right` changes the focused row; `Up/Down` changes its value.
- The focused row uses a filled background, not a thin focus frame.
- Discrete values use `< value >`; effects use track-and-thumb switches.
- Missing parameter 5/6 rows remain visible as disabled `--` rows, so the list does not jump.
- Current distortion and delay are post-engine per-voice stages and remain available for all selectable synth engines.
- MAIN and MORE remember their own last focused control.
- The selected tab and both focus positions are session-local UI state; they are not written into scenes or project persistence.

## Troubleshooting

- Plain `Tab` does nothing on hardware: verify the build includes the word-only/HID Tab input consolidation from PR #63.
- `Fn+Tab` opens `MORE`: verify the page rejects modified Tab events and top-level workflow navigation receives `event.meta`.
- Focus lands on a disabled row: verify `updateTabFocusability()` runs after the active engine changes.
- A row label is clipped: record the synth engine and parameter label; the right-side value is intentionally bounded.
- The old one-screen lower row is still visible: confirm the firmware was built from the latest PR #64 head.

## Acceptance checklist

- [ ] First page entry opens on `MAIN` with `MAIN / MORE` visible.
- [ ] Four continuous knobs are radius 13 and do not overlap values, labels, key hints, or summary.
- [ ] Plain `Tab` switches `MAIN → MORE → MAIN` exactly once per press.
- [ ] `Fn+Tab` changes workflow and never changes the local synth tab.
- [ ] `Left/Right` cycles only controls belonging to the visible tab.
- [ ] Returning to each tab restores that tab's previous focus.
- [ ] `Up/Down` changes the focused MORE row.
- [ ] Focus is shown by a filled row.
- [ ] Stepper arrows and switch positions match current values.
- [ ] Missing P5/P6 rows stay visible as disabled `--` rows.
- [ ] `DST/DLY` work on TB303 and at least one non-TB303 engine.
- [ ] `A/Z`, `S/X`, `D/C`, `F/V` change parameters only on MAIN.
- [ ] No DSP, audio routing, scene persistence, or MIDI behavior changes.
- [ ] Host tests and Cardputer ADV build pass.
