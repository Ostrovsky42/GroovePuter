# Workflow Page Navigation — Hardware Stage

## Purpose

Validate the revised GroovePuter navigation model:

```text
Fn+Tab  changes the top-level workflow
[ / ]   changes the page inside that workflow
Tab     changes local subpages owned by the current editor
```

The five top-level workflows are:

```text
PERFORM -> GENERATE -> HUB -> SONG -> SETTINGS
```

This stage changes navigation only. It must not change audio generation, MIDI routing, SMF scheduling, transport ownership, pattern contents or project persistence.

## Hardware list

- M5Stack Cardputer-Adv
- USB-C data/power cable
- Yamaha SEQTRAK optional for concurrent USB-MIDI regression checks
- microSD card containing at least one `.mid` file for Player/import checks

## Wiring

Standalone navigation test:

```text
USB power/data -> Cardputer-Adv
```

Optional MIDI regression:

```text
Cardputer-Adv USB-C data -> Yamaha SEQTRAK USB
```

PORT.A is not used. If PORT.A hardware remains attached, preserve the project invariant GPIO2 SDA / GPIO1 SCL and keep the Cardputer keyboard and Scroll Unit on the shared `Wire` bus.

## Build / flash

```bash
git switch feature/workflow-page-navigation
git pull
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Use the exact PR head recorded in the PR description.

## Navigation map

### PERFORM

```text
[ / ]
MIDI KEYBOARD <-> MIDI PLAYER
```

### GENERATE

```text
[ / ]
GENRE -> MODE / FLAVOR -> FEEL / TEXTURE -> ADV GENERATOR
```

Inside `ADV GENERATOR`, plain `Tab` must still cycle:

```text
TIMING -> NOTES -> SCALE
```

Swing, velocity range, ghost probability and microtiming remain in the TIMING local group.

### HUB

```text
[ / ]
OVERVIEW
-> SYNTH A
-> SYNTH B
-> DRUMS
-> SYNTH A SOUND
-> SYNTH B SOUND
```

Existing local `Tab` behavior inside synth/drum editors remains unchanged.

### SONG

```text
SONG
```

This workflow has one page, so `[` and `]` remain on SONG.

### SETTINGS

```text
PROJECT / SETUP
```

Load, save, scene management, MIDI import, routing, display and LED controls remain available through the existing Project page and dialogs. A future PROJECT/SETTINGS redesign can split these into local subpages without changing the workflow contract introduced here.

## Expected behavior

- `Fn+Tab` moves forward through all five workflows.
- `Shift+Fn+Tab` moves backward through all five workflows when Shift is available.
- Entering a workflow opens its first page.
- `[` and `]` wrap only inside the active workflow.
- `PERFORM ]` opens MIDI Player; another `]` returns to MIDI Keyboard.
- `GENERATE ]` reaches Mode, Feel and advanced Generator without leaving GENERATE.
- `HUB ]` reaches each concrete instrument/editor page.
- plain `Tab` is still handled by the current page before global navigation.
- `Fn+M` launcher lists the same five workflows and previews their pages with Left/Right.
- old direct hotkeys remain available as accelerators.
- Backspace/Back still toggles the previous actual page.

## Troubleshooting

### Brackets still jump between workflows

Confirm `src/ui/workflow_mode.h` contains page-aware `Workspace` states and `nextWorkspace()` uses `pageIndexInMode()` plus `pageAt()`.

### Fn+Tab only shows three modes

Confirm `WorkflowMode` contains `Perform`, `Generate`, `Hub`, `Song` and `Settings`, and `nextMode()` uses a count of five.

### Generator Tab leaves the page

The current page must get first refusal before the global bracket handler. Confirm `SettingsPage::handleEvent()` still consumes plain `Tab` for TIMING / NOTES / SCALE.

### Player cannot be reached from PERFORM

Open MIDI Keyboard, press `]`, and confirm the second PERFORM page is MIDI Player. Also verify `Alt+P` still works as a direct shortcut.

### SETTINGS appears incomplete

This PR intentionally keeps the existing Project page intact. It reorganizes access but does not yet split Project, MIDI, display, LED and system settings into separate visual pages.

### Audio or MIDI changes during navigation

Treat this as a regression. Workflow navigation must only change UI page ownership and must not mutate transport, MIDI queues, synthesis state or routing.

## Acceptance checklist

```text
BOOT
[ ] normal boot succeeds
[ ] current project/pattern is unchanged
[ ] no watchdog/reset

FN+TAB WORKFLOWS
[ ] PERFORM -> GENERATE
[ ] GENERATE -> HUB
[ ] HUB -> SONG
[ ] SONG -> SETTINGS
[ ] SETTINGS -> PERFORM
[ ] reverse workflow navigation works when Shift is available

PERFORM [ / ]
[ ] MIDI KEYBOARD -> MIDI PLAYER
[ ] MIDI PLAYER -> MIDI KEYBOARD
[ ] Player transport and route controls still work
[ ] live keyboard routing remains unchanged

GENERATE [ / ]
[ ] GENRE -> MODE / FLAVOR
[ ] MODE / FLAVOR -> FEEL / TEXTURE
[ ] FEEL / TEXTURE -> ADV GENERATOR
[ ] ADV GENERATOR -> GENRE
[ ] Generator Tab cycles TIMING / NOTES / SCALE
[ ] Swing and microtiming remain editable

HUB [ / ]
[ ] OVERVIEW -> SYNTH A
[ ] SYNTH A -> SYNTH B
[ ] SYNTH B -> DRUMS
[ ] DRUMS -> SYNTH A SOUND
[ ] SYNTH A SOUND -> SYNTH B SOUND
[ ] SYNTH B SOUND -> OVERVIEW
[ ] synth/drum local Tab behavior remains unchanged

SONG
[ ] [ and ] remain inside SONG
[ ] arrangement editing remains unchanged

SETTINGS
[ ] Project page opens
[ ] load dialog opens/closes
[ ] save/save-as works
[ ] MIDI import browser opens
[ ] MIDI advanced import dialog still uses Tab locally
[ ] routing/display/LED controls remain accessible

LAUNCHER / UI
[ ] Fn+M lists PERFORM / GENERATE / HUB / SONG / SETTINGS / HELP
[ ] Left/Right previews pages inside selected workflow
[ ] launcher shows PAGE n/N
[ ] CYBER/CARBON switching still works
[ ] no new permanent HUD or full-screen flashes

REALTIME
[ ] rapid [ / ] navigation causes no audible glitch
[ ] Fn+Tab during playback causes no timing change
[ ] MIDI Player playback survives launcher/navigation use
[ ] USB MIDI timing remains stable

BUILD GATES
[ ] host-tests SUCCESS
[ ] SDL build SUCCESS
[ ] Cardputer-Adv build SUCCESS
```
