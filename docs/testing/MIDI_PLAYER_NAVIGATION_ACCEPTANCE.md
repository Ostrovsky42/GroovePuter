# MIDI Player navigation acceptance — Cardputer ADV

## Purpose

Validate the 0.9 MIDI Player/File Browser and HUB MIDI navigation fixes, including live per-track route changes, without moving SMF scheduling, SD layout, transport ownership, or USB MIDI ownership into the UI.

## Hardware list

- M5Stack Cardputer ADV running this PR build.
- microSD card with `/midi` and at least one nested directory containing several `.mid` files.
- Yamaha SEQTRAK connected through the already-supported USB MIDI path for live route verification.

## Wiring

No new wiring is introduced by this PR.

- microSD: use the normal Cardputer ADV slot.
- SEQTRAK test: use the existing validated GroovePuter USB-MIDI connection.
- Do not change I2C, audio, or power wiring for this test.

## Build / flash

From a clean checkout of this PR branch, use the repository's normal Cardputer ADV 0.9 build/flash procedure and pinned ESP32/M5Stack dependencies. Keep the same PSRAM/USB profile used by `dev_0.9_test` acceptance.

After flashing, open Serial Monitor with the normal GroovePuter baud rate and keep it visible during the cold-boot and live-reroute tests.

## Expected behavior

1. Leave GroovePuter on `MIDI PLAYER`, reboot, and wait for startup. The PERFORM workflow must start on `MIDI KEYBOARD`; there must be no eager MIDI-folder read failure during page construction.
2. Open `MIDI PLAYER`, enter a nested `/midi/...` folder. The first row must be `< ..`; selecting it and pressing Enter returns to the parent folder without a MIDI load error. Esc remains a valid parent/back action.
3. In a directory longer than one screen, hold Up or Down. After the initial delay, selection must continue moving repeatedly while the key remains held and stop after release.
4. Load a multi-track MIDI file and open `HUB MIDI` with `H`. Up/Down selects a layer. Plain Left/Right changes that physical track's route **immediately**; there is no route-edit mode and no Enter confirmation.
5. Route order is directional from `AUTO`: Left goes `DX -> SYN2 -> SYN1 -> CH7 ... CH1 -> AUTO`; Right goes immediately to the drum side `CH1 -> CH2 ... CH7 -> SYN1 -> SYN2 -> DX -> AUTO`.
6. Keep the MIDI file playing and change a selected track route several times. Playback must continue. The selected track may have its currently sounding note cut at the route boundary, but there must be no stuck note and unrelated MIDI tracks must continue uninterrupted.
7. Enter in `HUB MIDI` remains the selected-layer mute toggle. It is not required after Left/Right and must not commit a route.
8. Route changes remain blocked only in RAW routing mode with `SEQTRAK ROUTING REQUIRED`; playback state (`PLAYING`, `ARMED`, `PAUSED`, `STOPPED`) must not add a `PAUSE MIDI FIRST` requirement.
9. In `HUB MIDI`, Space must pause/resume/arm through the existing MIDI Player service. With PROJECT tempo and internal clock stopped, the existing `G START FIRST / THEN SPACE` safety rule must still apply.
10. Esc returns from HUB MIDI to the same MIDI Player session. `H` remains a valid return shortcut.

## Troubleshooting

- `MIDI FOLDER OPEN FAILED` immediately after cold boot: confirm the persisted active page was MIDI Player before reboot and capture `[SESSION]` plus `[MIDI-FILES]` Serial lines.
- No held scrolling: verify the physical Cardputer Up/Down keys are used; capture input diagnostics if enabled. External keyboards whose arrows already use the global repeat path should remain unchanged.
- `SEQTRAK ROUTING REQUIRED`: expected only when the MIDI Player is in RAW routing mode. Switch back to SEQTRAK routing before assigning physical destinations.
- A route changes but playback pauses: regression; capture the HUB MIDI state plus `[MIDI-DISPATCH]` lines. Live reroute must not call Player pause/stop/toggle transport.
- A note hangs after changing route: regression; capture the selected track, old/new destination, and `[MIDI-DISPATCH]` diagnostics. The route-revision filter plus scoped old-route NoteOff should prevent this.
- `G START FIRST / THEN SPACE`: expected in PROJECT tempo with GroovePuter internal transport stopped.

## Acceptance checklist

- [ ] Cold boot from persisted MIDI Player lands on MIDI KEYBOARD with no eager browser error.
- [ ] Nested folder displays `< ..` as the first row.
- [ ] Enter on `< ..` returns to the parent directory with no load error.
- [ ] Esc still performs parent/back navigation in the browser.
- [ ] Holding Up/Down continuously scrolls a long MIDI directory.
- [ ] HUB MIDI Up/Down selects layers.
- [ ] HUB MIDI Right from AUTO immediately selects CH1/drum routing.
- [ ] HUB MIDI Left from AUTO immediately selects DX, then SYN2, then SYN1 before the drum channels.
- [ ] Left/Right commits each route immediately; no Enter confirmation is required.
- [ ] Enter still toggles the selected MIDI layer mute and does not commit routing.
- [ ] Route changes work while MIDI is PLAYING/ARMED without pausing transport.
- [ ] A live route change releases active notes from the old destination without silencing unrelated tracks.
- [ ] Already queued lookahead events from the old route do not reach USB after the route change.
- [ ] Repeated live route changes produce no stuck notes.
- [ ] RAW mode still shows `SEQTRAK ROUTING REQUIRED` for explicit routes.
- [ ] HUB MIDI Space pauses/resumes/arms MIDI using the existing Player transport rules.
- [ ] HUB MIDI Esc returns to the same Player session; H still works.
- [ ] No SD error, transport ownership regression, global SMF panic, or new USB MIDI error is observed.
