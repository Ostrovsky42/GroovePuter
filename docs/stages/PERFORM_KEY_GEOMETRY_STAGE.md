# PERFORM Key Geometry — Hardware Stage

## Purpose

Validate the compact PERFORM visual update:

```text
Melodic targets: two piano-shaped note rows
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
git switch feature/perform-piano-key-shapes
git pull
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Use the exact PR head recorded in the PR description.

## Expected behavior

### Synth A / Synth B / DX

The PERFORM page shows two compact piano rows corresponding to the two physical Cardputer note rows.

Each row uses piano geometry:

- natural notes are long light keys;
- sharp notes are shorter dark keys drawn above the white-key bed;
- the upper and lower input rows remain visually separate;
- the physical Cardputer letters are not printed on melodic keys.

Each playable key shows only the resolved note and octave using a compact 3x5 label:

```text
C4
D#4
A2
```

Scale and octave changes must update those labels. Pressing `Q` must affect only the note owned by `Q`, not the lower-row key with the same pitch class. Multiple held keys must remain independently visible.

### Drums

The PERFORM page keeps seven equal square pads in one row:

```text
A KICK | S SNR | D CLAP | F H1 | G H2 | H P1 | J P2
```

Drum letters remain visible because they are the direct mapping for the seven fixed native lanes. All pads must have the same width and height. Pressing a drum key highlights only its matching pad.

## Troubleshooting

### Upper and lower rows highlight together

The visual must use `PerformanceKeyboard::isPhysicalKeyHeld()`, not pitch-class-only state.

### Note labels are clipped

The piano labels use a local 3x5 immediate-mode glyph renderer rather than the standard 5x7 UI font. Confirm `C4` and `D#4` fit inside both white and black keys without overlapping the next key.

### Black keys do not look shorter

Confirm `drawPianoKeyRow()` performs two drawing passes: the long white-key bed first, then black keys using the calculated `blackW` and `blackH`.

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
[ ] two piano rows are visible
[ ] natural notes use long light keys
[ ] sharp notes use shorter dark keys
[ ] both rows use most of the available width
[ ] no Q/W/A/S physical letters appear on melodic keys
[ ] resolved note labels such as C4 and D#4 are readable
[ ] note labels do not overlap adjacent keys

PIANO STATE
[ ] upper-row and lower-row keys highlight independently
[ ] several held notes remain independently visible
[ ] white-key held state is clear in CYBER and CARBON
[ ] black-key held state is clear in CYBER and CARBON
[ ] scale changes update key shapes and note labels
[ ] octave changes update note labels
[ ] Synth A/B/DX routing is unchanged

DRUM GEOMETRY
[ ] seven pads are visible in one row
[ ] every pad is square
[ ] every pad has identical dimensions
[ ] drum key letters and lane labels remain readable

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
