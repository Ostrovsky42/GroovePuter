#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(relative: str, old: str, new: str) -> None:
    path = ROOT / relative
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise RuntimeError(f"expected one guarded block in {relative}, found {text.count(old)}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "src/ui/pages/genre_page.cpp",
    '            right = "FN+L/R:Recipe M:ApplyMode";',
    '            right = "ENTER:Apply M:ApplyMode";',
)

replace_once(
    "src/ui/pages/genre_page.cpp",
    '''    // ENTER: apply current selection / toggle apply mode
    if (key == '\\n' || key == '\\r') {
        if (focus_ == FocusArea::APPLY_MODE) {
            auto& gs = mini_acid_.sceneManager().currentScene().genre;
            cycleApplyMode(gs);
            UI::showToast(applyModeToast(mini_acid_), 1800);
            return true;
        }
        applyCurrent();
        return true;
    }
''',
    '''    // ENTER: apply the current genre/texture/recipe selection.
    // Apply mode is changed explicitly with M or Space.
    if (key == '\\n' || key == '\\r') {
        applyCurrent();
        return true;
    }
''',
)

marker = '''def test_ui_redraw_does_not_hold_audio_pause() -> None:
'''
insert = '''def test_enter_applies_selected_recipe() -> None:
    page = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
    start = page.index("// ENTER: apply the current genre/texture/recipe selection.")
    end = page.index("// SPACE: toggle apply mode", start)
    enter_block = page[start:end]
    require("applyCurrent();" in enter_block,
            "Enter must apply the selected recipe")
    require("cycleApplyMode" not in enter_block,
            "Enter must not cycle the apply mode")
    require('right = "ENTER:Apply M:ApplyMode";' in page,
            "Apply footer must document Enter and M controls")


'''
replace_once(
    "tests/test_source_regressions.py",
    marker,
    insert + marker,
)
replace_once(
    "tests/test_source_regressions.py",
    '''    test_recipe_selector_is_visible_and_navigable()
    test_ui_redraw_does_not_hold_audio_pause()
''',
    '''    test_recipe_selector_is_visible_and_navigable()
    test_enter_applies_selected_recipe()
    test_ui_redraw_does_not_hold_audio_pause()
''',
)

print("Genre Enter apply migration complete")
