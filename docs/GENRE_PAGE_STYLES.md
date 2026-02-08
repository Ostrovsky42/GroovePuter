# Genre Page Styles (Current)

`GenrePage` supports three runtime visual styles via global `Alt+\\`:
- `MINIMAL` (carbon)
- `RETRO_CLASSIC` (cyber)
- `AMBER`

No page-specific compile flag is required for normal use.

## Layout Behavior
- Header: `GENRE/TEXTURE` + transport info
- Two lists: Genre and Texture (both scroll-aware)
- Preset strip: `1..8`
- Apply mode badge: `SND`, `S+P`, `S+T`
- Footer hints change by focused area

## Focus and Controls
- `Tab`: focus cycle `Genre -> Texture -> Presets -> Apply`
- `Arrows`: move/select/adjust
- `Enter`: apply current choice (or toggle apply mode when Apply row focused)
- `Space`: toggle apply mode in Apply row
- `M`: cycle apply mode globally on Genre page
- `C`: curated/advanced compatibility
- `G`: cycle Groove mode (`ACID/MINIMAL/BREAKS/DUB/ELECTRO`)

## Apply Modes
- `SND`: sound only, keep existing patterns
- `S+P`: apply sound and regenerate patterns
- `S+T`: apply sound, regenerate patterns, and apply target tempo

## Practical Note
Genre page is for musical selection + apply policy. Deep mode/flavor corridor inspection and macro routing now lives on `ModePage` (`GROOVE LAB`).
