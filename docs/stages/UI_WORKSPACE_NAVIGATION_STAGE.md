# UI Workspace Navigation Stage

## Purpose

Replace the raw 14-page carousel with five musical workspaces while preserving existing pages and shortcuts.

```text
PERFORM -> PATTERN -> ARRANGE -> PLAYER -> GROOVE
```

Plain `[` / `]` cycles those workspaces. `Fn+M` opens a launcher. `Fn+Tab` keeps the existing PERFORM / PATTERN / ARRANGE cycle.

## Hardware list

- M5Stack Cardputer-Adv / ESP32-S3
- optional Yamaha SEQTRAK
- optional microSD card for MIDI Player and Project checks
- USB-C data cable when validating SEQTRAK

## Wiring

No wiring is required for standalone UI testing.

Optional MIDI validation:

```text
Cardputer-Adv USB-C data -> Yamaha SEQTRAK USB-C
```

PORT.A is unchanged: GPIO2 SDA / GPIO1 SCL.

## Build / Flash

```bash
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

## Expected behavior

Workspace ring:

```text
PERFORM
PATTERN  -> Sequencer Hub
ARRANGE  -> Song
PLAYER   -> realtime MIDI Player
GROOVE   -> Genre
```

`Fn+M` opens:

```text
PERFORM
PATTERN
ARRANGE
MIDI PLAYER
GROOVE
PROJECT
HELP
```

PATTERN has local choices `OVERVIEW / SYNTH A / SYNTH B / DRUMS`.

GROOVE has local choices `GENRE / MODE / FEEL / GENERATOR`.

Use Up/Down for groups, Left/Right for local choices, Enter or Space to open, Escape/back to close.

Existing direct shortcuts remain available, including `Fn+Tab`, `Alt+P`, `Alt+1..0`, `Alt+V`, and `Alt+H`.

## Troubleshooting

### Fn+M does not open

Fn must reach the existing `meta` modifier path used by `Fn+Tab`.

### `[` / `]` still visit every page

Verify the build contains `WorkflowPages::nextWorkspace()` in `MiniAcidDisplay::nextPage()` and `previousPage()`.

### A detail page seems missing

No page is deleted. Use the launcher child choices or the existing direct shortcut.

### PROJECT changes the ring position

PROJECT is intentionally outside the musical ring. Leaving it with `[` / `]` continues from the remembered musical workspace.

## Acceptance checklist

- [ ] boot still reaches the original Groove/Genre page
- [ ] `]` cycles GROOVE -> PERFORM -> PATTERN -> ARRANGE -> PLAYER -> GROOVE
- [ ] `[` cycles the same ring backwards
- [ ] no raw 14-page carousel remains on plain `[` / `]`
- [ ] Fn+Tab still cycles PERFORM / PATTERN / ARRANGE
- [ ] Fn+M opens and closes the launcher
- [ ] Up/Down selects launcher groups
- [ ] Left/Right selects PATTERN children
- [ ] Left/Right selects GROOVE children
- [ ] PATTERN Overview opens Sequencer Hub
- [ ] PATTERN Synth A opens Synth A editor
- [ ] PATTERN Synth B opens Synth B editor
- [ ] PATTERN Drums opens drum editor
- [ ] GROOVE Genre opens Genre
- [ ] GROOVE Mode opens Mode/Flavor
- [ ] GROOVE Feel opens Feel/Texture
- [ ] GROOVE Generator opens Generator settings
- [ ] PROJECT opens the existing Project page
- [ ] HELP opens existing page-aware help
- [ ] Alt+P still opens MIDI Player
- [ ] legacy direct shortcuts still work
- [ ] Back/backtick still returns to the previous actual page
- [ ] internal GroovePuter audio is unchanged
- [ ] SEQTRAK routing is unchanged
- [ ] SMF playback can continue while launcher opens/closes
- [ ] no audible timing regression while navigating
- [ ] no watchdog/reset
- [ ] host tests pass
- [ ] SDL build passes
- [ ] Cardputer-Adv build passes

## Follow-up

After hardware acceptance, handle visual cleanup separately:

1. split and polish PROJECT vs SETTINGS;
2. consolidate the GROOVE workspace visuals;
3. rewrite README with real screenshots/photos of the accepted UI.
