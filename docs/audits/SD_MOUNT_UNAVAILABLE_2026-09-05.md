# SD mount reports CARD_NONE — 2026-09-05

Unresolved. Written down because the obvious software explanations were checked
and none of them holds, and because the remaining two hypotheses are
distinguished by one cheap physical test that has not been run yet.

## Symptom

Opening the MIDI PLAYER page triggers a mount attempt that fails:

```
[SD] mount begin  freeInt=26332 largest=16372
[SD] mount result=0 type=0 freeInt=26332 largest=16372
[MIDI-FILES] unavailable stage=mount path=/midi dirs=0 files=0 shown=0 errno=0
```

`type=0` is `CARD_NONE` from `SD.cardType()`, so the card is not identified at
initialisation. `errno=0`: this is not a filesystem error.

Mounting is lazy — `ensureCardputerSdMounted()` runs on first use, not at boot —
so logs from sessions that never opened an SD-backed page contain no `[SD]`
lines at all and say nothing either way.

## Timeline from the recorded logs

Across 149 logs in `logs/`:

| logs | result |
|---|---|
| 2026-08-16 … 2026-08-31, nine sessions | `mount result=1 type=2`, no failures |
| 2026-09-05, this session | `mount result=0 type=0`, 24 attempts, no successes |

The last success is `serial-phw-p1-acceptance-20260831-161040.log`, the PHW-P1
acceptance run, so the firmware on the device then was around `db4e0f49`.

Reflashing `db4e0f49` today, the operator reported the card present again. On
that same build sound, MIDI and the keyboard were reported dead, and the device
did not answer esptool's auto-reset on the first attempt, so that build was not
in a healthy state and the observation is softer than it looks.

## Ruled out

- **The SD path did not change.** `git log --since=2026-08-25` over
  `src/platform/cardputer_sd.cpp`, `cardputer_sd.h` and
  `cardputer_adv_hardware.h` returns nothing.
- **No pin collision with the new DIN/UART MIDI transport.** That work landed in
  the same window (`7f74833d`, `63e24aab`, `b6f1be41`, `1e52b02e`) and was the
  leading suspect. Its pins are `kCardputerUartMidiRxPin = 1` and
  `kCardputerUartMidiTxPin = 2`; SD uses 40, 39, 14 and 12. Disjoint.
- **No competing bus initialisation.** `SPI.begin()` and `SD.begin()` each
  appear exactly once in the tree, both inside `ensureCardputerSdMounted()`.
- **Build configuration is unchanged.** One commit touched `scripts/build.sh` in
  the window, `5ad44bb9`, and it only raises the Arduino loop task stack.
- **Not the diagnostic instrumentation.** The plain product image fails
  identically; the instrumented image is not involved.

## Two hypotheses

**A regression between `db4e0f49` and `48762854`.** Supported by the timeline and
by the card appearing on the older build. Against it: every code path that could
plausibly carry such a regression was checked above and none moved. Settling it
means bisecting roughly forty commits, each needing a flash and a manual check.

**The card does not survive a warm reset.** Every failing observation in this
session followed `esptool`'s `Hard resetting via RTS pin`, which resets the
controller without removing power from the card. An SD card left initialised can
refuse a fresh initialisation sequence until it is power-cycled, and the
controller then reports exactly `CARD_NONE`. The one success today came after
the device had been running normally rather than immediately after a flash.

## The test that separates them

Disconnect USB fully, wait, reconnect, and open MIDI PLAYER on the current
build without flashing anything first.

- Card appears: the failure is warm-reset behaviour, there is no regression, and
  a bisect would be looking for a commit that does not exist.
- Card still absent: the regression is real and the bisect is justified.

Until that is run, "SD broke between `db4e0f49` and `48762854`" is not a
supported conclusion.

## Adjacent, not the same

`tests/test_source_regressions.py::test_cardputer_sd_has_one_hardware_mount_path`
fails, but on an SMF browser streaming contract, not on the mount. It also fails
at `c63f64f1`, before any of this line's work.

`docs/audits/SMF_STABILITY_REGRESSION_AUDIT_2026-08-02.md` records something that
looked like an SD dropout in August, but there mount and `SD.open()` both kept
succeeding. Different failure.

## Resolved

Run: full USB power disconnect, wait, reconnect, no reflash, open MIDI PLAYER
on firmware already at `a87623c7` (SeqTrak MIDI-only build). Files appeared.

That is the discriminating result this document asked for. The card survives a
real cold boot; it only failed to reinitialise after `esptool`'s warm reset,
which resets the controller without cycling power to the card. Every failing
observation in this document followed such a reset.

So: no regression between `db4e0f49` and `48762854`. The bisect this document
kept open is cancelled — it would have been searching for a commit that does
not exist. `SPI.begin()`/`SD.begin()` and the pin assignment remain exactly as
checked above; nothing about them needed to change.

Practical consequence for this session's own workflow: after flashing via
`esptool`, mounting the card again requires a real power cycle, not just the
reset the upload already performs.


## Reopened

The "Resolved" verdict above was premature. A later capture on this same
firmware (`118aa489`, plain composite build) shows `Reset Reason: 1`
(`ESP_RST_POWERON`, confirmed against the installed SDK's
`esp_system.h` — not a warm esptool reset) followed immediately by
`[SD] mount result=0 type=0` on the early-init attempt and again during
`MiniAcid::init`. A genuine cold power-on still fails to see the card.

The same capture also shows two `Reset Reason: 4` (`ESP_RST_PANIC`) events and
a burst of four reboots within about a second, three of them dying before
`M5Cardputer.begin()` even completed. No panic backtrace reached the serial
log in either the live capture or the raw persisted file, which is consistent
with the crash disrupting the same USB-CDC/JTAG peripheral that carries the
console — the reboot symptom ("port disappeared: No such file or directory")
is what a panic-driven USB reset looks like from the host side, not
necessarily proof of a manual unplug.

So two things are open again, not one:
- SD: still not proven to survive a genuine power-on; the "files appeared"
  observation that closed this before may have been a one-off, or the card's
  physical seating may have changed since.
- A real, uninvestigated panic loop, with no captured cause.

Neither the warm-reset explanation nor "no regression" should be treated as
established until a session captures a clean, uninterrupted boot (no reboot
loop) with an explicit, deliberate SD check on that exact boot.
