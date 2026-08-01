# Workflow Page Navigation — Hardware Stage

## Purpose

Validate the revised GroovePuter navigation model on M5Stack Cardputer-Adv:

```text
Fn+Tab  enters the next top-level workflow
[ / ]   moves between pages and crosses workflow edges
Tab     changes local subpages owned by the current editor
Fn+M    opens the discoverable launcher
```

There is no dedicated Shift key on the Cardputer-Adv keyboard. Reverse navigation must therefore work with plain `[` and must not depend on `Shift+Fn+Tab`.

The five top-level workflows are:

```text
PERFORM -> GENERATE -> HUB -> SONG -> SETTINGS
```

This stage changes UI navigation only. Audio generation, MIDI routing, SMF scheduling, transport ownership, pattern contents and project persistence must remain unchanged.

## Hardware list

- M5Stack Cardputer-Adv
- USB-C data/power cable
- Yamaha SEQTRAK optional for USB-MIDI regression checks
- microSD card with at least one `.mid` file for Player/import checks

## Wiring

Standalone:

```text
USB power/data -> Cardputer-Adv
```

Optional MIDI regression:

```text
Cardputer-Adv USB-C data -> Yamaha SEQTRAK USB
```

PORT.A is not used. If PORT.A hardware remains attached, preserve GPIO2 SDA / GPIO1 SCL and keep the Cardputer keyboard and Scroll Unit on the shared `Wire` bus.

## Build / flash

```bash
git switch feature/workflow-page-navigation
git pull
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Use the exact PR head recorded in PR #26.

## Navigation map

### PERFORM

```text
MIDI KEYBOARD <-> MIDI PLAYER
```

- `]` from MIDI Keyboard opens MIDI Player.
- `[` from MIDI Player returns to MIDI Keyboard.
- `[` from MIDI Keyboard enters the previous workflow, SETTINGS.
- `]` from MIDI Player enters the next workflow, GENERATE.

### GENERATE

```text
GENRE <-> MODE / FLAVOR <-> FEEL / TEXTURE
```

At the edges:

```text
[ from GENRE          -> PERFORM / MIDI KEYBOARD
] from FEEL / TEXTURE -> HUB / OVERVIEW
```

### HUB

```text
OVERVIEW
<-> SYNTH A
<-> SYNTH B
<-> DRUMS
<-> SYNTH A SOUND
<-> SYNTH B SOUND
```

Existing local `Tab` behavior inside synth/drum editors remains unchanged.

At the edges:

```text
[ from OVERVIEW       -> GENERATE / GENRE
] from SYNTH B SOUND  -> SONG
```

### SONG

```text
SONG
```

Because SONG has one page:

```text
[ -> HUB / OVERVIEW
] -> SETTINGS / PROJECT SETUP
```

### SETTINGS

```text
PROJECT / SETUP <-> ADV GENERATOR
```

Project keeps load, save, scene management, MIDI import, routing, display and LED controls.

Inside `ADV GENERATOR`, plain `Tab` cycles:

```text
TIMING -> NOTES -> SCALE
```

TIMING includes Swing, Velocity Range, Ghost Probability and Microtiming.

At the edges:

```text
[ from PROJECT / SETUP -> SONG
] from ADV GENERATOR    -> PERFORM / MIDI KEYBOARD
```

## Expected behavior

- `Fn+Tab` always enters the first page of the next workflow.
- Plain `[` provides one-key backward navigation across workflow boundaries.
- Plain `]` provides one-key forward navigation across workflow boundaries.
- Inside a multi-page workflow, `[` and `]` move one page at a time.
- Current pages receive input before the global bracket handler, so local controls keep working.
- `Fn+M` lists the same five workflows and previews their pages.
- Direct hotkeys and Back remain available.
- No navigation action mutates transport, MIDI routing or synthesis state.

## Troubleshooting

### `[` wraps inside the same workflow

Confirm `WorkflowPages::nextWorkspace()` checks:

```text
direction < 0 && index == 0
direction > 0 && index == count - 1
```

and uses `nextMode()` at workflow boundaries.

### Reverse navigation requires Shift

This is a regression. The Cardputer-Adv has no dedicated Shift key. Plain `[` on the first page must enter the previous workflow.

### Generator Tab leaves the page

The current page must receive the event before the global bracket handler. Confirm `SettingsPage::handleEvent()` still consumes plain `Tab` for TIMING / NOTES / SCALE.

### Player cannot be reached from PERFORM

Open MIDI Keyboard and press `]`. `Alt+P` must also remain available as a direct shortcut.

### Audio or MIDI changes during navigation

Treat this as a regression. Workflow navigation may only change UI page ownership.

## Acceptance checklist

```text
BOOT
[ ] normal boot succeeds
[ ] current project/pattern is unchanged
[ ] no watchdog/reset

FN+TAB
[ ] PERFORM -> GENERATE
[ ] GENERATE -> HUB
[ ] HUB -> SONG
[ ] SONG -> SETTINGS
[ ] SETTINGS -> PERFORM
[ ] no Shift key is required

ONE-KEY BACKWARD NAVIGATION
[ ] GENERATE/GENRE [ -> PERFORM/MIDI KEYBOARD
[ ] HUB/OVERVIEW [ -> GENERATE/GENRE
[ ] SONG [ -> HUB/OVERVIEW
[ ] SETTINGS/PROJECT [ -> SONG
[ ] PERFORM/MIDI KEYBOARD [ -> SETTINGS/PROJECT

FORWARD EDGE NAVIGATION
[ ] PERFORM/MIDI PLAYER ] -> GENERATE/GENRE
[ ] GENERATE/FEEL ] -> HUB/OVERVIEW
[ ] HUB/SYNTH B SOUND ] -> SONG
[ ] SONG ] -> SETTINGS/PROJECT
[ ] SETTINGS/ADV GENERATOR ] -> PERFORM/MIDI KEYBOARD

LOCAL PAGES
[ ] PERFORM Keyboard <-> Player
[ ] GENERATE Genre <-> Mode <-> Feel
[ ] HUB reaches Overview, A, B, Drums, A Sound, B Sound
[ ] SETTINGS Project <-> Advanced Generator
[ ] Generator Tab cycles TIMING / NOTES / SCALE
[ ] Swing and Microtiming remain editable
[ ] synth/drum local Tab behavior remains unchanged

LAUNCHER / UI
[ ] Fn+M lists PERFORM / GENERATE / HUB / SONG / SETTINGS / HELP
[ ] Left/Right previews pages
[ ] launcher shows PAGE n/N
[ ] CYBER/CARBON switching still works
[ ] no new full-screen flashes

REALTIME
[ ] rapid [ / ] navigation causes no audible glitch
[ ] Fn+Tab during playback causes no timing change
[ ] MIDI Player survives launcher/navigation use
[ ] USB MIDI timing remains stable

BUILD GATES
[ ] host-tests SUCCESS
[ ] SDL build SUCCESS
[ ] Cardputer-Adv build SUCCESS
```
