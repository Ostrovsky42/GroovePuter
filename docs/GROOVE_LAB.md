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

## Important: Groove vs Genre
- `GROOVE LAB` operates on `GrooveboxMode` (5 modes: ACID/MINIMAL/BREAKS/DUB/ELECTRO).
- `GenrePage` operates on `GenerativeMode` (9 genres: Acid..Chip) + texture layer.
- They are related but not identical systems.
- When applying a genre in `GenrePage`, the engine now auto-maps genre to a compatible `GrooveboxMode` so voicing/feel are coherent.

## Conflict Guard (5-mode vs 9-genre)
To reduce "mode fights genre" behavior:
- Genre apply keeps genre as source of truth for generation.
- Groove mode is auto-realigned to mapped genre mode during apply/state sync.
- This keeps macro voicing coherent while preserving per-genre rhythm identity.

## Recipe Layer (New DSP Core)
Genre now has a second layer (subgenre recipe) that does not change `GenerativeMode` count:
- base = `GenerativeMode` preset
- recipe = parameter/drum override
- morph = optional blend to target recipe

This is compiled inside `GenreManager` and consumed by DSP as already-compiled params.  
Bar-safe switching is done with queued recipe commit on step `0` (bar boundary) to avoid mid-bar mask jumps.

## Related Docs
- `docs/drum_genre_templates.md`
- `docs/DRUM_AUTOMATION.md`
