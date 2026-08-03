# Deterministic Cardputer Input — Hardware Stage

## Purpose

Make one physical Cardputer key press produce at most one UI navigation event.
This stage fixes input edge detection and repeat policy only. It does not add
NVS persistence, project autosave, new workflow pages, MIDI routing or DSP
changes.

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
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

The pinned dependency install remains:

```bash
bash scripts/install_arduino_deps.sh
```

This is important because the diagnosis assumes the repository-pinned
M5Cardputer version rather than an arbitrary local Arduino library.

## Expected behavior

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

## Troubleshooting

### A held Fn shortcut still repeats

Confirm `src/input/cardputer_input_edges.h` rejects any event with `meta`,
`alt`, `ctrl` or `shift` and that the flashed build is from this branch.

### A same-size key replacement is ignored

Confirm the sketch no longer calls `Keyboard.isChange()` and that Serial logs a
new press ID when replacing one held key with another.

### Notes remain stuck

Check that `reconcilePerformanceKeys(currentKeysState)` runs every loop, not only
when an edge is emitted.

### Local build uses another M5Cardputer version

Run `scripts/install_arduino_deps.sh`, then rebuild with verbose Arduino output
and confirm the selected library path.

## Automated validation

The one-shot production patch runner completed the focused checks before
committing the branch:

```text
source regression: PASS
edge/repeat unit test: PASS
```

The normal PR workflow must still pass host, SDL and Cardputer ADV compilation.
Hardware behavior remains unverified until the checklist below is completed.

## Acceptance checklist

```text
BUILD
[ ] host source regression passes
[ ] host edge/repeat unit test passes
[ ] Cardputer ADV build passes with --warnings all

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

ALLOWED REPEAT
[ ] unmodified UP repeats after the delay
[ ] unmodified DOWN repeats after the delay
[ ] adding Fn/Alt/Ctrl/Shift stops arrow repeat immediately

REALTIME
[ ] no stuck performance notes
[ ] no new audio underruns
[ ] no watchdog reset
```
