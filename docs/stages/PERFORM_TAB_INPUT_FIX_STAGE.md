# PERFORM Tab input fix

## Purpose

Make the dedicated Cardputer Tab key reliably open and close the local
`PERFORMANCE TOOLS` layer on the PERFORM page.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable for flashing and Serial Monitor

## Wiring

No external wiring is required. The built-in keyboard is used.

## Build and flash

```bash
./scripts/build_cardputer_adv.sh
```

Flash the generated Cardputer ADV firmware with the existing project workflow.

## Expected behavior

1. Open the PERFORM page.
2. Press the dedicated Tab key once.
3. `PERFORMANCE TOOLS` appears and the toast says `PERFORMANCE TOOLS: 1-8`.
4. Press Tab again; the layer closes.
5. `Fn+Tab` still changes workflow and does not toggle the local layer.
6. One physical Tab press toggles the layer exactly once, whether the keyboard
   library reports Tab only in `word` or in both HID and `word`.

Serial should show one dispatched event with `key=0x09`; the source may be `HID`
or `WORD` depending on the M5Cardputer library representation.

## Troubleshooting

- If Tab changes workflow, confirm Fn is not physically held or stuck.
- If no `[KEY]` line appears, verify the firmware was built from this PR head.
- If two toasts appear from one press, capture the `[KEY]` lines; HID/word
  deduplication has regressed.

## Acceptance checklist

- [ ] Plain Tab opens `PERFORMANCE TOOLS`.
- [ ] Plain Tab closes it on the next press.
- [ ] Exactly one toggle occurs per physical press.
- [ ] `Fn+Tab` continues to switch workflows.
- [ ] Number keys `1..8` operate tools while the layer is visible.
- [ ] Host, SDL, and Cardputer ADV CI jobs pass.
