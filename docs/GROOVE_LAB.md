# Groove Lab (Mode Page)

Source: `src/ui/pages/mode_page.cpp`.

## Purpose
`GROOVE LAB` is the control surface for:
- Groove `Mode` (ACID/MINIMAL/BREAKS/DUB/ELECTRO)
- Groove `Flavor` (5 per mode)
- Sound macro routing (`applySoundMacros`)
- Corridor preview after budget rules (`delayMix`, `tapeSpace`)

This page is the operational UI for the new `groove_profile` pipeline.

## Controls
- `Tab`: focus rows (`MODE`, `FLAVOR`, `MACROS`, `PREVIEW`)
- `Up/Down`: move focus
- `Left/Right`: adjust focused row
- `Enter`: action on focused row
- `A`: apply flavor voicing preset to `303A`
- `B`: apply flavor voicing preset to `303B`
- `D`: randomize drum pattern using flavor rules
- `M`: toggle `applySoundMacros`
- `Space`: preview regenerate and start playback

## Corridor Line
Displayed in-page:
- `N`: notes min..max
- `A`: accent probability
- `S`: slide probability
- `SW`: swing amount

These come from `GrooveProfile::getCorridors(mode, flavor)` and then pass through `GrooveProfile::applyBudgetRules(...)`.

## Budget Line
Displayed as:
- `DUCK OFF` or `DUCK ON`
- current `delayMix` and `tapeSpace`

`DUCK ON` means budget rules reduced density/accents to avoid mix overload (notably in DUB profiles).

## Separation of Responsibility
- `GrooveProfile` (`src/dsp/groove_profile.*`): source of truth for corridors + budget rules
- `ModeManager`: applies selected mode/flavor to generator + presets
- `GenrePage`: selection/apply workflow, not corridor authority
