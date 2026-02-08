# GENRE APPLY MODE - VALIDATION WALKTHROUGH

## Goal
Validate `Genre Apply Mode` end-to-end:
- `SOUND+PATTERN` regenerates patterns.
- `SOUND+PATTERN+TEMPO` regenerates patterns and sets BPM target.
- `SOUND ONLY` keeps existing patterns.
- Mode persists after save/load and reboot.

## Pre-check
- Build and flash current firmware.
- SD card inserted.
- Open `Genre Page`.

## Test 1: UI visibility
1. Press `Tab` until `APPLY_MODE` is focused.
2. Verify focused indicator is visible.
3. Verify footer hint explains `S+P` / `S+T` / `SND`.

Pass:
- Focus is clearly visible.
- Hint text is present and readable.

## Test 2: Toggle behavior
1. In `APPLY_MODE`, press `Space`.
2. Verify mode cycles `SND -> S+P -> S+T -> SND`.
3. Verify toast:
- `Apply Mode: SOUND+PATTERN (Regenerates)`
- `Apply Mode: SOUND+PATTERN+TEMPO`
- `Apply Mode: SOUND ONLY (Keeps patterns)`

Pass:
- Mode toggles both directions.
- Toast text matches current mode.

## Test 3: Apply with regeneration
1. Set mode to `S+P`.
2. Select different genre/texture.
3. Press `Enter`.
4. Verify toast contains `(Regenerated)`.
5. Open Pattern page and confirm pattern changed.

Pass:
- Toast says `Regenerated`.
- Pattern content changed.

## Test 4: Apply without regeneration
1. Set mode to `SND`.
2. Select different genre/texture.
3. Press `Enter`.
4. Verify toast contains `(Patterns kept)`.
5. Open Pattern page and confirm pattern unchanged.

Pass:
- Toast says `Patterns kept`.
- Pattern content unchanged.

## Test 4b: Apply with tempo
1. Set mode to `S+T`.
2. Note current BPM.
3. Select different genre/texture or press preset key `1..8`.
4. Press `Enter`.
5. Verify toast contains `BPM set`.
6. Verify BPM changed to target value for selected genre/preset.

Pass:
- Toast confirms BPM update.
- BPM changes together with apply.

## Test 5: Preset hotkeys
1. Set mode to `S+P`.
2. Press preset key `1..8`.
3. Verify toast contains `(Regenerated)`.
4. Repeat with mode `S+T` and verify `(Regenerated, BPM set)`.
5. Repeat with mode `SND` and verify `(Patterns kept)`.

Pass:
- Preset keys follow active apply mode.
- Behavior matches toast.

## Test 6: Persistence after reboot
1. Set `APPLY_MODE = S+T`.
2. Save scene in `Project Page`.
3. Reboot device.
4. Load saved scene.
5. Return to `Genre Page` and check mode.
6. Apply genre once and verify toast/action.

Pass:
- Mode restored correctly after reboot.
- Runtime behavior matches restored mode.

## Notes
- `Genre` persistence fields are stored in scene JSON as:
`genre.gen`, `genre.tex`, `genre.amt`, `genre.regen`, `genre.tempo`.
- Preset hotkeys use unified `applyCurrent()` flow and must respect `regen` and `tempo`.
