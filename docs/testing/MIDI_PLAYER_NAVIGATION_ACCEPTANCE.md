# MIDI Player navigation acceptance — Cardputer ADV

## Purpose

Validate the 0.9 MIDI Player/File Browser and HUB MIDI navigation fixes without changing SMF scheduling, routing ownership, SD layout, or USB MIDI ownership.

## Hardware list

- M5Stack Cardputer ADV running this PR build.
- microSD card with `/midi` and at least one nested directory containing several `.mid` files.
- Optional for route verification: Yamaha SEQTRAK connected through the already-supported USB MIDI path.

## Wiring

No new wiring is introduced by this PR.

- microSD: use the normal Cardputer ADV slot.
- SEQTRAK test: use the existing validated GroovePuter USB-MIDI connection.
- Do not change I2C, audio, or power wiring for this test.

## Build / flash

From a clean checkout of this PR branch, use the repository's normal Cardputer ADV 0.9 build/flash procedure and pinned ESP32/M5Stack dependencies. Keep the same PSRAM/USB profile used by `dev_0.9_test` acceptance.

After flashing, open Serial Monitor with the normal GroovePuter baud rate and keep it visible during the cold-boot test.

## Expected behavior

1. Leave GroovePuter on `MIDI PLAYER`, reboot, and wait for startup. The PERFORM workflow must start on `MIDI KEYBOARD`; there must be no eager MIDI-folder read failure during page construction.
2. Open `MIDI PLAYER`, enter a nested `/midi/...` folder. The first row must be `< ..`; selecting it and pressing Enter returns to the parent folder without a MIDI load error. Esc remains a valid parent/back action.
3. In a directory longer than one screen, hold Up or Down. After the initial delay, selection must continue moving repeatedly while the key remains held and stop after release.
4. Load a multi-track MIDI file and open `HUB MIDI` with `H`. While playback is paused/stopped, Up/Down selects a layer. Left/Right immediately changes the draft route (`AUTO`, `CH1..CH10`) without first pressing `C`; Enter commits the route. Esc cancels an active route edit.
5. In `HUB MIDI`, Space must pause/resume/arm through the existing MIDI Player service. With PROJECT tempo and internal clock stopped, the existing `G START FIRST / THEN SPACE` safety rule must still apply.
6. With no active route edit, Esc must return from HUB MIDI to the same MIDI Player session. `H` remains a valid return shortcut.

## Troubleshooting

- `MIDI FOLDER OPEN FAILED` immediately after cold boot: confirm the persisted active page was MIDI Player before reboot and capture `[SESSION]` plus `[MIDI-FILES]` Serial lines.
- No held scrolling: verify the physical Cardputer Up/Down keys are used; capture input diagnostics if enabled. External keyboards whose arrows already use the global repeat path should remain unchanged.
- `PAUSE MIDI FIRST`: route editing is intentionally blocked during active playback.
- `SEQTRAK ROUTING REQUIRED`: explicit track routes are intentionally unavailable in RAW mode.
- `G START FIRST / THEN SPACE`: expected in PROJECT tempo with GroovePuter internal transport stopped.

## Acceptance checklist

- [ ] Cold boot from persisted MIDI Player lands on MIDI KEYBOARD with no eager browser error.
- [ ] Nested folder displays `< ..` as the first row.
- [ ] Enter on `< ..` returns to the parent directory with no load error.
- [ ] Esc still performs parent/back navigation in the browser.
- [ ] Holding Up/Down continuously scrolls a long MIDI directory.
- [ ] HUB MIDI Up/Down selects layers.
- [ ] HUB MIDI Left/Right enters route edit and changes `AUTO/CH1..CH10` directly.
- [ ] Enter confirms the selected route; Esc cancels a draft route.
- [ ] HUB MIDI Space pauses/resumes/arms MIDI using the existing Player transport rules.
- [ ] HUB MIDI Esc returns to the same Player session; H still works.
- [ ] No stuck notes, transport ownership regression, SD error, or new USB MIDI error is observed.
