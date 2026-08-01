# UI Theme System — Hardware Stage

## Purpose

Validate the two accepted public visual themes — CYBER and CARBON — as coherent full-device styles rather than page-local color swaps.

CYBER is the expressive GroovePuter identity. CARBON is the darker, restrained readability-first alternative. AMBER was rejected during visual review and is no longer part of the public theme cycle. Its enum/constants remain temporarily for source compatibility until PROJECT/SETTINGS is redesigned.

This stage is visual-only. It must not change audio, MIDI routing, SMF timing, transport ownership or page-navigation semantics.

## Hardware list

- M5Stack Cardputer-Adv
- USB-C data/power cable
- Yamaha SEQTRAK optional for concurrent MIDI playback checks
- phone camera for photo/video readability checks

## Wiring

Standalone visual test:

```text
USB power/data -> Cardputer-Adv
```

Optional realtime regression:

```text
Cardputer-Adv USB-C data -> Yamaha SEQTRAK USB
```

PORT.A is not used by this test. If PORT.A hardware is attached, preserve GPIO2 SDA / GPIO1 SCL and keep the shared keyboard/Scroll bus on `Wire`, not `Wire1`.

## Build / flash

```bash
git switch agent/ui-theme-system
git pull
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Use the exact PR head recorded in the PR description for acceptance.

## Expected behavior

`Alt+\\` switches the accepted public styles:

```text
CYBER <-> CARBON
```

The same switch works while the `Fn+M` launcher is open. AMBER and the reserved `MINIMAL_DARK` / STAGE slot must not appear in this normal cycle.

Common surfaces follow the active theme:

- standard header/footer;
- launcher background/focus/details;
- list rows;
- value/toggle rows;
- segmented bars;
- button grids;
- info boxes;
- generic settings/generator/feel pages.

Specialized CYBER sequencer pages keep their dedicated widgets. CARBON uses common restrained hardware styling and legacy minimal paths with matching dark constants.

## Visual intent

### CYBER

The characteristic GroovePuter instrument identity:

- softened cyan/magenta accents;
- dark blue-black panels;
- cool primary text;
- obvious focus and musical activity;
- no pure saturated primaries that bloom heavily on phone cameras.

CYBER remains the default candidate unless hardware testing finds a concrete readability regression.

### CARBON

The readability-first alternative:

- deeper near-black/graphite surfaces;
- light-gray body text instead of broad bright-white fields;
- bright pixels reserved for focus, selection and musical activity;
- cyan focus and restrained green activity;
- fewer large luminous areas in phone photo/video.

CARBON should feel quieter than CYBER, not flatter or lower quality.

## Troubleshooting

### AMBER still appears

The normal launcher/global `Alt+\\` path must be binary. A legacy AMBER label may remain inside the old PROJECT settings page until that page is replaced; do not treat it as an accepted public theme. The PROJECT/SETTINGS redesign must remove that legacy selector.

### Theme changes only on some pages

Confirm the branch contains `src/ui/ui_theme.h` and that `src/ui/ui_widgets.cpp` and `src/ui/layout_manager.cpp` use `UI::themePalette()`.

### Launcher does not follow the theme

Confirm `src/ui/workspace_launcher_overlay.h` includes `ui_theme.h`, shows `UI::themeName(UI::currentStyle)`, and calls `UI::nextThemeStyle()`.

### CARBON is too bright

Identify which semantic role is excessive: body text, secondary text, focus, selected fill or activity. Do not dim focus/activity merely to compensate for overly bright body text.

### CARBON is too dark

Check in this order:

1. selected/focused state;
2. primary labels;
3. secondary HUD in hand;
4. panel separation.

Increase only the failing semantic token. Do not lift the entire background or turn all labels white.

### CYBER blooms on camera

Test at normal phone auto-exposure first. CYBER intentionally avoids pure `0x00FFFF` / `0xFF00FF`; if clipping remains, reduce the offending semantic token rather than adding animation or dimming all text.

### Audio/MIDI changes while switching theme

Treat this as a regression. Theme selection is UI state only and must not mutate audio/MIDI ownership or transport state.

## Acceptance checklist

```text
BOOT / NAVIGATION
[ ] normal boot unchanged
[ ] Fn+M launcher unchanged functionally
[ ] [ / ] workspace navigation unchanged
[ ] old hotkeys still work

PUBLIC THEME CYCLE
[ ] Alt+\\ CYBER -> CARBON
[ ] Alt+\\ CARBON -> CYBER
[ ] repeated switching never enters AMBER
[ ] repeated switching never enters STAGE
[ ] switching works while launcher is open
[ ] launcher header immediately shows active theme

GLOBAL CONSISTENCY
[ ] launcher clearly changes theme
[ ] standard header/footer clearly change theme
[ ] GENERATOR lists/bars clearly change theme
[ ] FEEL controls clearly change theme
[ ] generic PROJECT/settings rows remain readable
[ ] specialized sequencer pages still look intentional

CYBER
[ ] remains the strongest expressive instrument identity
[ ] cyan/magenta identity is immediately obvious
[ ] no major neon bloom in phone photo/video
[ ] no new scanline/strobe/flicker behavior
[ ] selected/focused state is distinct from active musical state

CARBON
[ ] surfaces are visibly darker than the previous pass
[ ] broad bright-white areas are reduced
[ ] primary text remains readable at ~1 m
[ ] secondary labels remain readable at ~40 cm
[ ] in-hand HUD remains useful
[ ] focus/activity remains obvious without flooding the screen
[ ] camera image retains dark negative space around information

CAMERA
[ ] CYBER and CARBON are distinguishable in a still photo
[ ] text edges survive phone auto-exposure
[ ] CARBON does not become a field of white labels
[ ] no new timer-driven visual effects
[ ] no full-screen flash when changing musical state

REALTIME
[ ] theme switch causes no audible glitch
[ ] MIDI timing remains stable during/after theme switch
[ ] SMF playback remains stable while launcher/theme is used
[ ] no watchdog/reset

LEGACY PROJECT PAGE
[ ] old Visual Style row may still show AMBER before PROJECT/SETTINGS redesign
[ ] AMBER from that legacy row is not considered accepted behavior
[ ] returning to global Alt+\\ normalizes selection back to CARBON/CYBER

BUILD GATES
[ ] host-tests SUCCESS
[ ] SDL build SUCCESS
[ ] Cardputer-Adv build SUCCESS
```

Production scope is frozen after this pass. Further visual changes require hardware findings, except removal of the legacy PROJECT selector in the planned PROJECT/SETTINGS redesign.
