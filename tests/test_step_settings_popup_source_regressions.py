#!/usr/bin/env python3
"""STEP SETTINGS window (Alt+E on STEPS): registration, ownership, help."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text(encoding="utf-8")
HDR = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text(encoding="utf-8")
LEGACY = (ROOT / "src/ui/pages/pattern_edit_page_legacy.h").read_text(encoding="utf-8")
HELP = (ROOT / "src/ui/global_help_content.h").read_text(encoding="utf-8")
KEYS = (ROOT / "src/ui/docs/keys.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


# Window state lives on the page: no new static buffer.
require("bool step_settings_open_ = false;" in HDR,
        "STEP SETTINGS must be closed by default")
require("handleStepSettingsKey" in HDR and "drawStepSettings" in HDR,
        "page must declare the STEP SETTINGS handler and drawer")

# Registration: Alt+E opens, routed before NOTE ENTRY and before legacy.
public = CPP[CPP.index("bool PatternEditPage::handleEvent(UIEvent& ui_event)"):]
require("!ev.alt || ev.ctrl || ev.meta" in CPP and "GROOVEPUTER_E" in CPP and
        "isStepSettingsChord(ui_event)" in public and "openStepSettings()" in public,
        "Alt+E (no Ctrl/Meta) must open STEP SETTINGS")
require(public.index("openStepSettings()") < public.index("note_entry_mode_ = !note_entry_mode_"),
        "Alt+E must be resolved before the NOTE ENTRY layer")
require(public.index("step_settings_open_") < public.index("isAcceptEvent"),
        "an open window must own key input before ACCEPT/GO routing")

# The window is modal for keys; Undo (an application event) still passes.
handler = CPP[CPP.index("bool PatternEditPage::handleStepSettingsKey"):]
handler = handler[:handler.index("\n}\n")]
require("GROOVEPUTER_ESCAPE" in handler and "step_settings_open_ = false" in handler,
        "Esc must close the window")
require("return true;" in handler, "the open window must swallow unrelated keys")

# Every field is written through the canonical R3 mutation path (one Undo each).
adjust = CPP[CPP.index("void PatternEditPage::adjustStepSetting"):]
adjust = adjust[:adjust.index("\n}\n")]
require("commitPatternMutation" in adjust, "fields must be written via commitPatternMutation")
for field in ("probability", "timing", "velocity", "ghost"):
    require(re.search(r"\.%s\b" % field, adjust), "%s must be editable" % field)
require("mini_acid_.edit" not in adjust and "sceneManager().currentScene" not in adjust,
        "fields must not bypass the Undo owner")
require("kStepProbMax = 100" in CPP and "kStepTimingMax = 23" in CPP and
        "kStepVelocityMax = 127" in CPP and "kStepVelocityMin = 1" in CPP,
        "field limits must be explicit")

# Footer hint and overlay.
require("AltE:STEP" in LEGACY, "footer must name the combination")
require("drawStepSettings(gfx)" in LEGACY, "draw() must paint the window")

# Help: both synth sections, 38 characters at most; keys.md documents it.
# DRUMS has its own Alt+E line (tests/test_drum_step_settings_source_regressions.py).
lines = re.findall(r'"(Alt\+E[^"]*)"', HELP[:HELP.index("kDrumLines")])
require(len(lines) == 2, "Alt+E needs one help line in Synth A and one in Synth B")
for line in lines:
    require(len(line) <= 38, "help line too long: %r" % line)
require("`Alt+E` on STEPS" in KEYS, "keys.md must document Alt+E")

print("step settings popup source regressions: OK")
