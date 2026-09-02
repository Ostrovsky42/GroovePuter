# Synth Track Tabs — Cardputer-Adv

## Purpose

Validate the collapsed synth-track UI hierarchy:

```text
SYNTH A/B
Tab: NOTES -> KNOBS -> MORE -> NOTES
```

Standalone `SYNTH A SOUND` / `SYNTH B SOUND` pages must no longer appear in HUB navigation. Synth DSP, engine state, pattern data and persistence behavior must remain unchanged.

## Hardware list

- M5Stack Cardputer-Adv
- USB-C data/power cable
- Headphones/speaker or Yamaha SEQTRAK optional for audio/MIDI smoke testing

## Wiring

Standalone UI/audio test:

```text
USB-C -> Cardputer-Adv
```

No PORT.A device is required. If one remains attached, preserve the project invariant: PORT.A I2C uses GPIO2 SDA / GPIO1 SCL on the shared `Wire` bus.

## Build / flash

```bash
git switch agent/20260808-05-synth-track-tabs
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Also run the source regression:

```bash
python3 tests/test_synth_track_tab_source_regressions.py
```

## Expected behavior

HUB top-level page ring is:

```text
OVERVIEW <-> SYNTH A <-> SYNTH B <-> DRUMS
```

Inside either synth track, plain `Tab` cycles:

```text
[NOTES] KNOBS MORE
NOTES [KNOBS] MORE
NOTES KNOBS [MORE]
```

`NOTES` is the existing note/pattern editor. `KNOBS` reuses the existing four-knob synth parameter UI. `MORE` reuses engine/type/OSC/filter/distortion/delay controls.

Old persisted page ids 3 and 4 must normalize to SYNTH A and SYNTH B respectively instead of reopening standalone SOUND pages.

## Troubleshooting

### Tab skips KNOBS or MORE

Confirm `SynthSequencerPage::SynthTab` contains exactly `Notes`, `Knobs`, `More` and the Tab handler advances modulo 3.

### MAIN / MORE appears in addition to the new indicator

This is a visual regression. The synth-track indicator must cover the old internal params switcher so only one local hierarchy is visible.

### MORE footer says `TAB MAIN`

This is a regression. The parent synth track owns Tab navigation; MORE must show `TAB NOTES`.

### HUB still shows SYNTH A SOUND / SYNTH B SOUND

Confirm `WorkflowMode::Hub` has four pages and the page list is `kPattern, kSynthA, kSynthB, kDrums`.

### Old saved UI session opens an invalid page

Confirm `normalizeLegacyPage()` maps `kSynthAParameters -> kSynthA` and `kSynthBParameters -> kSynthB`.

## Acceptance checklist

```text
BUILD
[ ] source regression PASS
[ ] host tests PASS
[ ] SDL build PASS
[ ] Cardputer-Adv build PASS

HUB
[ ] OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS
[ ] no standalone SYNTH A SOUND page
[ ] no standalone SYNTH B SOUND page
[ ] [ / ] still navigates HUB pages

SYNTH A
[ ] opens on NOTES
[ ] Tab -> KNOBS
[ ] Tab -> MORE
[ ] Tab -> NOTES
[ ] indicator matches active state
[ ] only one local tab indicator is visible

SYNTH B
[ ] opens on NOTES
[ ] Tab -> KNOBS
[ ] Tab -> MORE
[ ] Tab -> NOTES
[ ] indicator matches active state
[ ] only one local tab indicator is visible

NOTES
[ ] pattern selection unchanged
[ ] bank selection unchanged
[ ] note editing unchanged
[ ] accent/slide editing unchanged
[ ] playback cursor unchanged

KNOBS
[ ] four knobs render
[ ] Left/Right changes focus
[ ] Up/Down changes value
[ ] hold acceleration still works
[ ] Ctrl fine adjustment still works

MORE
[ ] TYPE changes synth engine
[ ] OSC/P5 changes correctly
[ ] FLT/P6 changes correctly
[ ] DST toggles correctly
[ ] DLY toggles correctly
[ ] footer says TAB NOTES

PERSISTENCE / AUDIO
[ ] engine selection persists as before
[ ] synth parameters persist as before
[ ] no pattern data changes when switching tabs
[ ] no transport stop/restart when switching tabs
[ ] no audible glitch from repeated Tab switching
[ ] legacy saved page 3 opens SYNTH A
[ ] legacy saved page 4 opens SYNTH B
```
