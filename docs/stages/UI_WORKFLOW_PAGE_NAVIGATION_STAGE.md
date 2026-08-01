# Workflow Page Navigation — Hardware Stage

## Purpose

Validate the revised GroovePuter navigation model on M5Stack Cardputer-Adv:

```text
Fn+Tab   next top-level workflow
[ / ]    previous/next page inside the current workflow
Fn+[ / ] previous/next workflow
Tab      local subpage/group owned by the current page
Fn+M     launcher
```

The five workflows are:

```text
PERFORM -> GENERATE -> HUB -> SONG -> SETTINGS
```

This stage changes UI navigation only. It must not change audio generation, MIDI routing, SMF scheduling, transport ownership, pattern contents or project persistence.

## Hardware list

- M5Stack Cardputer-Adv
- USB-C data/power cable
- Yamaha SEQTRAK optional for USB-MIDI regression checks
- microSD card containing at least one `.mid` file for Player/import checks

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

## Workflow map

### PERFORM

```text
[ / ]
MIDI KEYBOARD <-> MIDI PLAYER
```

### GENERATE

```text
[ / ]
GENRE <-> MODE / FLAVOR <-> FEEL / TEXTURE
```

### HUB

```text
[ / ]
OVERVIEW
<-> SYNTH A
<-> SYNTH B
<-> DRUMS
<-> SYNTH A SOUND
<-> SYNTH B SOUND
```

Existing local `Tab` behavior inside synth/drum editors remains unchanged.

### SONG

```text
SONG
```

This workflow contains one page, so plain `[` and `]` remain on SONG.

### SETTINGS

```text
[ / ]
PROJECT / SETUP <-> ADV GENERATOR
```

Project keeps load/save, scene management, MIDI import, routing, display and LED controls.

Inside `ADV GENERATOR`, plain `Tab` cycles:

```text
TIMING -> NOTES -> SCALE
```

TIMING contains Swing, Velocity Range, Ghost Probability and Microtiming.

## Cross-workflow navigation

Hold the physical Cardputer `Fn` key while pressing a bracket:

```text
Fn+[  previous workflow, first page
Fn+]  next workflow, first page
```

Expected ring:

```text
PERFORM <-> GENERATE <-> HUB <-> SONG <-> SETTINGS
```

Examples:

```text
GENERATE / FEEL + Fn+[  -> PERFORM / MIDI KEYBOARD
GENERATE / GENRE + Fn+] -> HUB / OVERVIEW
SONG + Fn+[             -> HUB / OVERVIEW
SONG + Fn+]             -> SETTINGS / PROJECT SETUP
```

No Shift key is required or assumed.

## Expected behavior

- `Fn+Tab` enters the first page of the next workflow.
- Plain `[` and `]` wrap only inside the current workflow.
- `Fn+[` and `Fn+]` switch workflows in both directions.
- Cross-workflow navigation always opens the target workflow's first page.
- Current pages receive input before global plain-bracket navigation, preserving local controls.
- `Fn+M` lists the same five workflows and previews their pages.
- Direct hotkeys and Back remain available.
- Navigation never mutates transport, MIDI routing or synthesis state.

## Troubleshooting

### Fn+brackets behave like plain brackets

Confirm `src/ui/workflow_mode.h` contains:

```text
M5Cardputer.Keyboard.keysState().fn
nextWorkspace(..., bool workflowModifier)
```

and the modifier branch uses:

```text
pageForMode(nextMode(mode, direction))
```

### Plain brackets leave the workflow

This is a regression. With no Fn held, `nextWorkspace()` must use `pageIndexInMode()` and `pageAt()` to wrap locally.

### Generator Tab leaves the page

Confirm `SettingsPage::handleEvent()` still consumes plain `Tab` for TIMING / NOTES / SCALE.

### Player cannot be reached from PERFORM

Open MIDI Keyboard and press plain `]`. `Alt+P` must also remain available as a direct shortcut.

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

PLAIN [ / ] — LOCAL PAGES
[ ] PERFORM Keyboard <-> Player and wraps locally
[ ] GENERATE Genre <-> Mode <-> Feel and wraps locally
[ ] HUB reaches all six pages and wraps locally
[ ] SONG stays on SONG
[ ] SETTINGS Project <-> Advanced Generator and wraps locally

FN+[ / ] — WORKFLOWS
[ ] PERFORM Fn+] -> GENERATE / GENRE
[ ] GENERATE Fn+] -> HUB / OVERVIEW
[ ] HUB Fn+] -> SONG
[ ] SONG Fn+] -> SETTINGS / PROJECT
[ ] SETTINGS Fn+] -> PERFORM / MIDI KEYBOARD
[ ] PERFORM Fn+[ -> SETTINGS / PROJECT
[ ] SETTINGS Fn+[ -> SONG
[ ] SONG Fn+[ -> HUB / OVERVIEW
[ ] HUB Fn+[ -> GENERATE / GENRE
[ ] GENERATE Fn+[ -> PERFORM / MIDI KEYBOARD
[ ] no Shift key is required

LOCAL SUBPAGES
[ ] Generator Tab cycles TIMING / NOTES / SCALE
[ ] Swing and Microtiming remain editable
[ ] synth/drum local Tab behavior remains unchanged

LAUNCHER / UI
[ ] Fn+M lists PERFORM / GENERATE / HUB / SONG / SETTINGS / HELP
[ ] Left/Right previews pages
[ ] launcher shows PAGE n/N
[ ] launcher explains `[ ] PAGE` and `FN+[ ] WORKFLOW`
[ ] CYBER/CARBON switching still works
[ ] no new full-screen flashes

REALTIME
[ ] rapid navigation causes no audible glitch
[ ] Fn+Tab during playback causes no timing change
[ ] MIDI Player survives launcher/navigation use
[ ] USB MIDI timing remains stable

BUILD GATES
[ ] host-tests SUCCESS
[ ] SDL build SUCCESS
[ ] Cardputer-Adv build SUCCESS
```
