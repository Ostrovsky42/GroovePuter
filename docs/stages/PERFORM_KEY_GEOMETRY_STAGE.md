# PERFORM Key Geometry — Hardware Stage

## Purpose

Validate the compact PERFORM visual update:

```text
Melodic targets: two physical keyboard rows
Drums target:    seven equal square pads
```

This stage changes drawing geometry only. It must not change MIDI notes, channels, scales, octave behavior, velocity, routing, transport ownership or audio rendering.

## Hardware list

- M5Stack Cardputer-Adv
- USB-C data/power cable
- Yamaha SEQTRAK optional for USB-MIDI validation

## Wiring

Standalone visual test:

```text
USB power/data -> Cardputer-Adv
```

Optional MIDI test:

```text
Cardputer-Adv USB-C data -> Yamaha SEQTRAK USB
```

PORT.A is not used. If PORT.A hardware remains connected, keep GPIO2 as SDA and GPIO1 as SCL and preserve the shared `Wire` bus.

## Build / flash

```bash
git switch feature/perform-key-geometry
git pull
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Use the exact PR head recorded in the PR description.

## Expected behavior

### Synth A / Synth B / DX

The PERFORM page shows two long rows matching the physical Cardputer keyboard:

```text
Q W E R T Y U I O P   upper octave
A S D F G H J K L     base octave
```

Each visual key contains:

- its physical key letter;
- the resolved MIDI note name and octave;
- a strong held-state fill and focus strip.

Pressing `Q` must highlight only `Q`, not the lower-row key with the same pitch class. Multiple held keys must remain independently visible.

### Drums

The PERFORM page shows seven equal square pads in one row:

```text
A KICK | S SNR | D CLAP | F H1 | G H2 | H P1 | J P2
```

All pads must have the same width and height. Pressing a drum key highlights only its matching pad.

## Troubleshooting

### Upper and lower rows highlight together

The visual must use `PerformanceKeyboard::isPhysicalKeyHeld()`, not pitch-class-only state.

### Note labels are clipped

Confirm the display is using the normal 240x135 Cardputer-Adv geometry and the standard 5x7 font. Labels should fit within the calculated key width.

### Drum pads are not square

Confirm `drawDrumPads()` derives one `padSize` and uses it for both width and height for all seven pads.

### Visual overlaps the status row

The instrument area must end before `LayoutManager::lineY(7)`. The final status line remains fixed at line 7.

### MIDI behavior changed

Treat this as a regression. This PR must not modify `PerformanceKeyboard`, event routing or target/channel mapping.

## Acceptance checklist

```text
BUILD
[ ] host-tests SUCCESS
[ ] SDL build SUCCESS
[ ] Cardputer-Adv build SUCCESS

PIANO GEOMETRY
[ ] two rows are visible
[ ] QWERTYUIOP is the upper row
[ ] ASDFGHJKL is the lower row
[ ] keys use most of the available width
[ ] keys are visibly longer than before
[ ] physical key letters are readable
[ ] resolved note labels are readable

PIANO STATE
[ ] A highlights only A
[ ] Q highlights only Q
[ ] A + Q can remain highlighted together
[ ] several held notes remain independently visible
[ ] scale and octave changes update note labels
[ ] Synth A/B/DX routing is unchanged

DRUM GEOMETRY
[ ] seven pads are visible in one row
[ ] every pad is square
[ ] every pad has identical dimensions
[ ] labels remain readable

DRUM STATE
[ ] A/S/D/F/G/H/J highlight their matching pads
[ ] simultaneous drum holds remain visible
[ ] CH1..7 routing is unchanged

REALTIME
[ ] no visible flicker
[ ] no full-screen redraw artifact beyond existing page redraw
[ ] no audio click or timing change from held-state updates
[ ] no watchdog/reset
```
