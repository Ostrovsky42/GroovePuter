Automating Options-Based Parameters
==================================

Problem
-------
Option-based parameters (e.g., oscillator type, filter type) are automated by storing lane `y` values in the 0-255 range and then mapping those values to option indices at runtime. If the option list grows or reorders later, existing automation values map to different options, making old patterns unstable. The editor also allows entering any 0-255 value for option lanes and relies on later clamping, which is confusing and imprecise.

Goals
-----
- Make option-based automation stable when option lists change.
- Represent option-based automation in discrete steps that align with options.
- Keep legacy lanes readable and migratable.
- Update lane editing UI to reflect option-based ranges.

Non-Goals
---------
- Redesign the automation node model or interpolation behavior.
- Introduce a full schema versioning system for scene data.

Current Behavior
----------------
- `AutomationLane` stores nodes with `y` in `[0, 255]`.
- `MiniAcid::applySynthAutomation` treats those values as normalized, then scales to parameter ranges or to option index ranges.
- `AutomationLaneEditor` uses a fixed 32-step `y` grid and maps `y` to `[0, 255]` even for option-based parameters.

Proposed Behavior (Overview)
----------------------------
- Option-based lanes store a per-lane options list (labels) and use discrete `y` indices in `[0, optionCount - 1]`.
- Serialization includes an `options` array for option-based lanes; `y` values are stored as indices in that range.
- Runtime automation maps the evaluated index back to the current parameter options using label matching, so inserts/reorders in the parameter options do not break existing automation.
- The UI adapts its `y` grid to the option count and uses discrete steps (no 0-255 mapping).

Data Model
----------
Add option metadata to `AutomationLane` (in `scenes.h`):
- `uint8_t optionCount` (0 means no options metadata).
- `std::array<std::string, kAutomationMaxOptions> optionLabels` or equivalent fixed-size storage.
- Helper accessors:
  - `bool hasOptions() const` (true if `optionCount > 0`).
  - `const char* optionLabelAt(int index) const`.
  - `int clampOptionIndex(int index) const`.

Recommended limits (to keep memory bounded):
- `kAutomationMaxOptions = 16` (or smaller if memory is tight).
- `kAutomationOptionLabelMaxLen = 12` (truncate on load if needed).

Behavioral rules:
- When a lane targets an option-based parameter and has no option metadata, it should be initialized with the current parameter option labels.
- Lane `y` values for option-based lanes are always treated as indices into `optionLabels`.
- Non-option lanes continue to use `y` in `[0, 255]`.

Serialization
-------------
Extend automation lane JSON objects to include an `options` array when applicable.

New schema (lane object):
- `param` (int)
- `enabled` (bool)
- `options` (array of strings, optional)
- `nodes` (array of `{x, y}` objects)

Rules:
- If `options` is present, `y` values are indices in `[0, options.size - 1]`.
- If `options` is absent, `y` values are legacy `[0, 255]`.
- On load:
  - If `options` is present, store it in `AutomationLane`.
  - If `options` is absent and the parameter has options, convert `y` values from `[0, 255]` to `[0, optionCount - 1]` using the current parameter option count, then initialize `optionLabels` from the current parameter.
  - If `options` is absent and the parameter is continuous, keep legacy values.
- On save:
  - For option-based lanes (has option metadata), write `options` and indexed `y` values.
  - For non-option lanes, omit `options` and write legacy `y` values.

All serialization paths must be updated:
- `SceneManager::writeSceneJson` (manual writer in `scenes.h`).
- `serializeSynthPattern` / `deserializeSynthPattern` in `scenes.cpp`.
- `SceneJsonObserver` evented parser/writer in `scenes.cpp`.

Runtime Application
-------------------
Update `MiniAcid::applySynthAutomation` to use lane option metadata.

Algorithm per lane:
1. Evaluate lane `y` value.
2. If the lane has options metadata and the parameter has options:
   - Clamp `y` to `[0, lane.optionCount - 1]`.
   - Look up the label from the lane's `optionLabels[y]`.
   - Find the matching option index in the current parameter options (string match).
   - If found, use that index as the parameter value.
   - If not found, fall back to a safe index (e.g., 0 or clamped `y` against current options).
3. If the lane has no options metadata and the parameter has options:
   - Use the legacy mapping (scale `y/255` to `[0, optionCount - 1]`).
4. If the parameter is continuous:
   - Use the legacy normalized mapping (`y/255` to min/max range).

Note: For option-based lanes, evaluation should be step-wise (no interpolation between option indices), matching the discrete UI behavior.

This ensures that adding new options or reordering options preserves old automation meaning when labels match.

UI: AutomationLaneEditor
------------------------
Update `AutomationLaneEditor` to use a dynamic `y` range:
- For option-based lanes, set `ySteps = optionCount`.
- For non-option lanes, keep `ySteps = 32` (legacy grid).

Behavioral changes:
- `cursorValue()` returns an option index for option-based lanes.
- `valueToYIndex()` and `yIndexToPixel()` should use the dynamic `ySteps`.
- The option labels drawn on the right should align exactly to the discrete rows for option-based lanes (no 0-255 mapping).
- For option-based lanes, draw only horizontal segments between nodes; skip diagonal interpolation lines to avoid implying in-between option values.
- When a lane is created or edited for an option-based parameter without existing metadata, initialize its `optionLabels` from the current parameter options so editing is discrete from the start.

Assumptions/Constraints:
- Current option counts are small (<= 8). If counts exceed the available grid rows, we accept cramped spacing for now (no scrolling).

Migration Notes
---------------
- Existing scene files without `options` will still load.
- Option-based lanes from legacy data will be converted to indexed form on load (using current options). Once saved, they become stable across future option list changes.
- Older builds will ignore `options` and treat indexed `y` values as 0-255. This may compress values toward the low end for option-based lanes; document this as a compatibility caveat.

Edge Cases
----------
- Option list size <= 1: clamp all `y` to 0; UI should show a single row.
- Lane has options metadata but the parameter no longer has options: treat as continuous with normalized mapping from `[0, optionCount - 1]` to `[min, max]`, or clamp to `min` (choose one behavior in implementation).
- Labels missing from current options: fall back to nearest or default index; keep behavior deterministic.

Open Questions
--------------
- Should option metadata be updated when the user edits a lane after options have changed, or should it remain frozen to preserve historical meaning?
- Is it acceptable that older builds interpret indexed `y` values as low-range 0-255 values? If not, do we need a scene schema version or a dual-field representation?
