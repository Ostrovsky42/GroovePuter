# Held value acceleration acceptance — Cardputer ADV

## Purpose

Verify that physical Cardputer arrow keys repeat globally again for continuous numeric controls and that held-value acceleration ramps smoothly without changing discrete navigation.

## Hardware list

- M5Stack Cardputer ADV.
- Normal GroovePuter audio output is optional; no external MIDI device is required.

## Wiring

No additional wiring is required.

- Use the built-in Cardputer keyboard and display.
- PORT.A and external I2C devices are not involved.

## Build / Flash steps

1. Build `agent/20260807-02-held-slider-acceleration-fix` with the same Cardputer ADV profile used for `dev_0.9_test`.
2. Flash the firmware normally.
3. Open Serial Monitor at the normal GroovePuter baud rate if input diagnostics are needed.

## Expected behavior

- A short arrow press changes a continuous numeric value by exactly one normal step.
- Holding the same arrow starts repeating after the existing 350 ms delay.
- Continued hold ramps gradually through normal multipliers `x1 -> x2 -> x3 -> x4`; there is no previous `x2 -> x4 -> x5` jump.
- Releasing the key, reversing direction, changing focus, or entering a discrete selector resets the acceleration.
- Ctrl/Fine editing remains precise and does not accelerate.
- Discrete menus, TYPE selectors, banks, recipes and page/list navigation do not acquire value-step acceleration.
- MIDI file browser Up/Down still scrolls continuously while held, but advances only once per global repeat event.

## Troubleshooting

- If a slider moves once and stops, capture `[KEY]` lines. A held physical arrow should produce `src=REPEAT` events after the initial HID event.
- If MIDI browser selection moves twice per repeat, verify the browser-local `serviceHeldNavigation` fallback is absent; repeat ownership must be global only.
- If a short tap skips values, confirm the control itself is not applying a pre-existing coarse base step or modifier.
- If Ctrl/Fine accelerates, capture the modifier state from `[KEY]`; modified events must not arm global repeat acceleration.

## Acceptance checklist

- [ ] Short tap on a continuous value changes exactly one normal step.
- [ ] Hold begins repeating after the normal delay.
- [ ] Acceleration feels gradual and reaches `x2`, then `x3`, then at most normal `x4`.
- [ ] Release resets the next press to `x1`.
- [ ] Direction reversal resets the next step to `x1`.
- [ ] Ctrl/Fine remains unaccelerated.
- [ ] GENRE Morph held editing accelerates smoothly.
- [ ] Synth A and Synth B continuous parameter editing accelerates smoothly.
- [ ] Drum Automation continuous VALUE/SWING/HUMAN controls accelerate smoothly.
- [ ] FEEL continuous controls retain held acceleration where applicable.
- [ ] Discrete selectors and list navigation remain one item per repeat event.
- [ ] MIDI browser held Up/Down scrolls continuously with no double-repeat.
- [ ] No stuck key, navigation conflict, audio underrun or watchdog reset is introduced.
