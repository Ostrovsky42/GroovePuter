# Deterministic Cardputer Input — Integrated Hardware Stage

## Purpose

Test the complete suspected failure chain on Cardputer ADV:

```text
physical keyboard input
-> workflow navigation
-> per-workflow page memory
-> NVS UI-session persistence
```

This branch contains the exact head of PR #46 plus the deterministic input
changes from PR #48. That makes the hardware run falsifiable: if page memory
still fails while the key-event invariant passes, the input theory is
insufficient and the remaining defect is in navigation ownership or
persistence.

The deterministic input delta itself changes only edge detection and repeat
policy. It does not change MIDI routing, DSP, page IDs or the NVS codec.

## Hardware list

- M5Stack Cardputer ADV
- USB-C data/power cable
- optional serial monitor at the repository's configured baud rate

## Wiring

No external wiring is required.

PORT.A remains unused by this test. If existing I2C hardware stays connected,
preserve the Cardputer ADV bus on GPIO2 SDA / GPIO1 SCL and keep shared devices
on `Wire`.

## Build / Flash

```bash
git switch agent/deterministic-cardputer-input
git pull
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

The pinned dependency install is required because the diagnosis assumes the
repository-pinned M5Cardputer version rather than an arbitrary local Arduino
library.

## Expected input behavior

- the application polls the complete `KeysState` every loop;
- changing from one same-size key combination to another is detected;
- newly pressed HID/word entries are emitted once;
- Fn, Alt and Ctrl combinations never auto-repeat;
- Tab, Enter and brackets never auto-repeat;
- only one unmodified arrow may auto-repeat;
- performance-note releases continue to follow the live HID set;
- logs identify `HID`, `WORD` and `REPEAT` sources with one press ID.

Example:

```text
[KEY] press=41 src=HID repeat=0 fn=1 alt=0 ctrl=0 shift=0 key=0x09 sc=...
```

Holding Fn+Tab for one second must not produce a `src=REPEAT` line.

## Expected workflow-memory behavior

Set these pages:

```text
PERFORM  -> MIDI Player
GENERATE -> FEEL / TEXTURE
HUB      -> any non-entry synth/drum page
```

Then leave and return through each route:

```text
Fn+M
Fn+Tab
Fn+[
Fn+]
```

All routes must restore the selected page within the current session. The
Fn+M launcher must show `GROOVEPUTER / NAV R3` and matching `MEM` values.

After Stop, keep the device running for at least one second. Serial must print:

```text
[SESSION] saved active=... mem=...
```

After reboot it must print the same memory in:

```text
[SESSION] load=1 active=... mem=...
```

## Interpretation

### Input invariant fails

If one physical press creates multiple transitions, or a same-size combination
replacement is ignored, deterministic input remains broken. Do not draw a
conclusion about workflow memory or NVS from that run.

### Input passes, in-session memory fails

The input theory is not sufficient. Continue with the single-owner navigation
refactor: remove page-memory mutation from `captureUiSession_()` and remove the
launcher-local long-lived workflow memory.

### In-session memory passes, reboot fails

The input theory explained the runtime failure. Investigate only the NVS save
boundary, stopped-state debounce, schema and loaded snapshot.

### Both pass

The previous workflow-memory failures were caused or materially amplified by
the physical input pipeline. PR #48 can proceed to review, while PR #46 still
requires a decision on whether to merge as one combined change or split its
recovery/persistence parts.

## Troubleshooting

### A held Fn shortcut still repeats

Confirm `src/input/cardputer_input_edges.h` rejects any event with `meta`,
`alt`, `ctrl` or `shift` and that the flashed build is from this branch.

### A same-size key replacement is ignored

Confirm the sketch no longer calls `Keyboard.isChange()` and that Serial logs a
new press ID when replacing one held key with another.

### Page memory changes unexpectedly

Capture consecutive `[KEY]`, `[NAV]` and `[SESSION]` lines. A single press ID
must correspond to at most one workflow transition.

### Reboot restores only volume

Stop transport, wait for `[SESSION] saved`, then reboot. If the saved memory is
correct but the loaded memory differs, record both complete lines.

### Notes remain stuck

Check that `reconcilePerformanceKeys(currentKeysState)` runs every loop, not only
when an edge is emitted.

## Acceptance checklist

```text
BUILD
[ ] full host suite passes
[ ] SDL build passes
[ ] Cardputer ADV build passes with --warnings all and pinned dependencies

ONE PRESS / ONE EVENT
[ ] tap Fn+Tab 20 times: exactly 20 workflow transitions
[ ] hold Fn+Tab for 1 second: exactly one transition
[ ] hold Fn+[ for 1 second: exactly one transition
[ ] hold Fn+] for 1 second: exactly one transition
[ ] hold Fn+M for 1 second: launcher changes state once
[ ] hold Enter for 1 second: confirmation occurs once

EDGE DETECTION
[ ] switch Fn+Tab directly to Fn+] without a long release: new key is detected
[ ] switch Fn+] directly to Fn+[: new key is detected
[ ] modifier release does not resend the held ordinary key

IN-SESSION MEMORY
[ ] PERFORM restores MIDI Player through all four routes
[ ] GENERATE restores FEEL / TEXTURE through all four routes
[ ] HUB restores the selected non-entry page through all four routes
[ ] launcher MEM agrees with the actual restored pages

REBOOT PERSISTENCE
[ ] Stop and wait at least one second
[ ] Serial prints the expected [SESSION] saved snapshot
[ ] reboot prints load=1 with the same snapshot
[ ] all three workflow pages restore after reboot
[ ] master volume still restores

ALLOWED REPEAT / REALTIME
[ ] unmodified arrows repeat after the delay
[ ] adding a modifier stops repeat immediately
[ ] no stuck performance notes
[ ] no new audio underruns
[ ] no watchdog reset
```
