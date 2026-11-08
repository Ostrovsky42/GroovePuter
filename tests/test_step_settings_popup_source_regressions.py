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

# Custom-value mark: a square + accent value, driven by the pure rules header.
require("synthFieldCustom" in LEGACY and "fillRect(x + 3" in LEGACY,
        "synth window must mark custom fields")
require("stepOnGrid" in adjust, "synth PROB/VEL must step on the 5 grid")

# CHANCE (was GHOST): the note plays with 80% probability.
require('"CHANCE"' in LEGACY and '"GHOST"' not in LEGACY,
        "the ghost flag is shown as CHANCE")
require('"80%"' in LEGACY, "CHANCE ON shows 80%")

# MIXED for a selection whose steps differ.
require('"MIXED"' in LEGACY and "synthFieldDiffers" in LEGACY,
        "synth window must show MIXED for differing values")

# One hint for both windows, within 38 characters; PROB x CHANCE shown; reset keys.
MARKS = (ROOT / "src/state/step_setting_marks.h").read_text(encoding="utf-8")
DRUMLEG = (ROOT / "src/ui/pages/drum_sequencer_page_legacy.h").read_text(encoding="utf-8")
hint = re.search(r'kStepSettingsHint = "([^"]*)"', MARKS).group(1)
require(hint == "UP/DN FIELD  L/R VALUE  ENT/ESC CLOSE" and len(hint) <= 38, "shared hint text")
require("kStepSettingsHint" in LEGACY and "kStepSettingsHint" in DRUMLEG,
        "both windows use the shared hint")
require("effectiveChancePercent" in LEGACY, "CHANCE row shows the PROB x CHANCE product")
require("resetStepSetting(ui_event.ctrl ? -1 : step_settings_field_)" in handler,
        "Backspace resets a field, Ctrl+Backspace all")
require("synthStepHasNonTimingSettings" in LEGACY, "grid dot for PROB/VEL/CHANCE")
require("PATTERN CLEARED  CTRL+Z UNDO" in LEGACY and "Pattern Cleared" not in LEGACY,
        "pattern clear toast is upper case")

# Opt (TOGGLE_SOURCE) is ignored while the window is open.
SYNTH = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")
opt = SYNTH.index("GROOVEPUTER_APP_EVENT_TOGGLE_SOURCE) {\n    // STEP SETTINGS")
require("stepSettingsOpen()) return true" in SYNTH[opt:opt + 300] and
        SYNTH[opt:opt + 400].index("stepSettingsOpen()") < SYNTH[opt:opt + 400].index("toggleSource()"),
        "Opt must be ignored while STEP SETTINGS is open")

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
