# Compact synth controls

"
    "## Purpose

"
    "Fit all Synth A/B parameter controls on the Cardputer ADV 240x135 display without changing DSP behavior or keyboard mappings.

"
    "## Hardware list

"
    "- M5Stack Cardputer ADV
"
    "- USB-C cable

"
    "## Wiring

"
    "No external wiring is required. The built-in display and keyboard are used.

"
    "## Build and flash

"
    "```bash
"
    "./scripts/build_cardputer_adv.sh
"
    "```

"
    "Flash the generated Cardputer ADV firmware with the existing project workflow.

"
    "## Expected behavior

"
    "- The four continuous synth parameters remain rotary controls but are visibly smaller.
"
    "- `TYPE`, `OSC`, and `FLT` appear as compact selector knobs in one lower row.
"
    "- `DST` and `DLY` appear as explicit ON/OFF toggle switches in the same row.
"
    "- Values, labels, focus outline, direct A/Z-S/X-D/C-F/V controls, arrows, and fine adjustment remain functional.
"
    "- No parameter, engine, effect, DSP, persistence, or audio behavior changes.

"
    "## Troubleshooting

"
    "- If a label is clipped, record the active synth engine and parameter label; compact values are intentionally bounded to their cell.
"
    "- If focus skips a control, verify that the active engine exposes parameter 5/6; unavailable selectors are intentionally hidden.
"
    "- If the screen shows the old large knobs, verify the firmware was built from this branch head.

"
    "## Acceptance checklist

"
    "- [ ] Four main knobs fit above the lower control row without overlap.
"
    "- [ ] TYPE/OSC/FLT selectors are readable and track their current values.
"
    "- [ ] DST/DLY toggles clearly show ON and OFF.
"
    "- [ ] Left/Right focus reaches every visible control.
"
    "- [ ] Up/Down and direct key pairs modify the same parameters as before.
"
    "- [ ] Ctrl/Shift fine adjustment remains unchanged.
"
    "- [ ] SDL and Cardputer ADV builds pass.
"
    