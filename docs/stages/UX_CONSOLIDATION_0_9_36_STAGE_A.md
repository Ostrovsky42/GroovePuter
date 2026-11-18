# 0.9.36 UX Consolidation — Stage A

## Purpose

Reduce visible control complexity without removing musical capability.

Stage A establishes one product rule:

> A feature that is not available in the current build must have zero UI footprint.

The first application is experimental TILT in PERFORM > INPUT. In the normal
Cardputer ADV product build, TILT is not compiled and therefore must not add
cursor rows, headings, hints, or dead `N/A` controls.

This stage does not change musical generation, MIDI routing, DSP, looper,
partner, macro, persistence, or transport semantics.

## Product baseline

The 0.9.35 integration keeps the same five workflows and twelve reachable
workflow pages as 0.9.24, but PERFORM became denser.

Before Stage A:

```text
PERFORM > INPUT
11 MIDI/HELPER rows
+ 7 TILT rows shown as N/A in the normal build
= 18 selectable rows
```

After Stage A:

```text
normal product build: 11 selectable rows
GROOVEPUTER_TILT build: 18 selectable rows
```

## Hardware list

- M5Stack Cardputer ADV
- USB-C data/power cable
- optional external MIDI device for regression checks

No SEQTRAK is required for the UI acceptance itself.

## Wiring

No external wiring is required for this stage.

If existing PORT.A I2C hardware remains connected, preserve the Cardputer ADV
shared bus:

```text
SDA GPIO2
SCL GPIO1
bus Wire
```

Do not move Cardputer keyboard/shared devices to `Wire1`.

## Build / Flash

Use the 0.9.36 Stage A branch:

```bash
git fetch origin
git switch agent/0.9.36-ux-consolidation-a
git reset --hard origin/agent/0.9.36-ux-consolidation-a

bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

The normal product build must not define `GROOVEPUTER_TILT`.

For the experimental TILT build, use the repository's existing TILT build flag
path; do not change production defaults just to expose the experiment.

## Expected behavior

### Normal product build

Open PERFORM, then the tools layer, then INPUT.

The cursor sequence must end after:

```text
MIDI IN
LISTEN CH
SEND TO
PLAY AS
LEAD STYLE
HELPER
REPLY BARS
REPLY CH
FREEZE RIFF
VARIATION
NEW RIFF IF
```

There must be no:

```text
--- TILT ---
TILT TARGET
TILT SENS
TILT X
TILT Y
TILT Z
TILT ZERO
```

and no TILT rows rendered as `N/A`.

### Experimental TILT build

When `GROOVEPUTER_TILT` is compiled in, the TILT group remains after HELPER
with the six controls supported by current `main` and the existing apostrophe
live toggle.

## Product control rules introduced by this stage

These rules guide the rest of 0.9.36:

1. Disabled capability = zero visible UI footprint.
2. One screen should have one primary musical job.
3. Global navigation and local editing must stay visually and behaviorally distinct.
4. Advanced configuration must not displace primary performance controls.
5. Existing expert shortcuts may remain, but primary footers should teach only the shortest route.
6. New capability is not accepted merely because it fits in an existing screen.

Planned follow-up stages:

```text
Stage B  PERFORM information architecture
         reduce seven equal tool contexts to fewer musician-facing groups

Stage C  PROJECT / SETUP progressive disclosure
         separate everyday project actions from device/MIDI administration

Stage D  SONG / FORM simplification
         keep eight form capabilities without presenting eight equal concepts at once

Stage E  first-session workflow + footer/hint audit
         one obvious path: generate -> play -> arrange -> save
```

## Troubleshooting

### TILT still appears as N/A

Confirm the flashed build is from this branch and that `GROOVEPUTER_TILT` is
not defined by local build flags.

### INPUT starts on an invalid remembered row

Stage A intentionally reduces the valid production INPUT row range. Existing
view continuity outside the new range must clamp to row 0 rather than expose a
hidden row.

### TILT build lost its controls

Confirm `GROOVEPUTER_TILT` is defined at compile time. The enum rows, row
boundaries, draw block, hints, and adjustment cases are all compiled together.

### Musical behavior changed

That is a regression. Stage A changes only UI reachability of an unavailable
experimental feature. MIDI, generation, performance, looper, partner, macro,
transport, and DSP behavior must remain unchanged.

## Acceptance checklist

```text
AUTOMATED
[ ] full host tests pass
[ ] performance UI source regression passes
[ ] Cardputer ADV product build passes with --warnings all
[ ] optional GROOVEPUTER_TILT build compiles

NORMAL PRODUCT BUILD
[ ] PERFORM > INPUT has 11 selectable rows
[ ] Down from NEW RIFF IF wraps to MIDI IN
[ ] no TILT heading is visible
[ ] no TILT row is visible
[ ] no dead TILT N/A state is visible
[ ] KEY / CHORD / ARP / RHYTHM / LOOP / MACRO behavior is unchanged

EXPERIMENTAL TILT BUILD
[ ] PERFORM > INPUT has 18 selectable rows
[ ] TILT group follows HELPER
[ ] all seven TILT controls remain editable
[ ] apostrophe live toggle still works

REALTIME / REGRESSION
[ ] live keyboard produces no stuck notes
[ ] transport behavior is unchanged
[ ] looper behavior is unchanged
[ ] partner/helper behavior is unchanged
[ ] no new audio underruns
[ ] no watchdog reset
```
