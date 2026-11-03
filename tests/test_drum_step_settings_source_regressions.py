#!/usr/bin/env python3
"""DRUMS STEP SETTINGS window (Alt+E on the drum grid): wiring, Undo, help."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
HDR = (ROOT / "src/ui/pages/drum_sequencer_page.h").read_text(encoding="utf-8")
LEGACY = (ROOT / "src/ui/pages/drum_sequencer_page_legacy.h").read_text(encoding="utf-8")
EDIT = (ROOT / "src/state/drum_step_edit.h").read_text(encoding="utf-8")
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
HELP = (ROOT / "src/ui/global_help_content.h").read_text(encoding="utf-8")
KEYS = (ROOT / "src/ui/docs/keys.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def body(text: str, signature: str) -> str:
    text += "\n"
    start = text.index(signature)
    return text[start:text.index("\n}\n", start)]


# Window state lives on the grid page (heap object): no new static buffer.
require("bool step_settings_open_ = false;" in LEGACY,
        "STEP SETTINGS must be closed by default")
for name in ("openStepSettings", "handleStepSettingsKey",
             "adjustStepSetting", "drawStepSettings"):
    require(name in LEGACY, "grid page must declare %s" % name)

# Registration: Alt+E without Ctrl/Meta, on the grid tab, before every other
# DRUMS binding of the public wrapper.
chord = body(CPP, "bool isDrumStepSettingsChord")
require("!event.alt || event.ctrl || event.meta" in chord and
        "GROOVEPUTER_E" in chord, "Alt+E (no Ctrl/Meta) must be the chord")
public = body(CPP, "bool DrumSequencerPage::handleEvent(UIEvent& ui_event)")
require("activePageIndex() == 0" in public[:public.index("isDrumStepSettingsChord")],
        "Alt+E opens only on the grid tab")
first_modal = public.index("step_settings_open_")
for later in ("isDrumOutputCycleKey(ui_event)", "GROOVEPUTER_APP_EVENT_UNDO",
              "singleCellBackspace", "gridArrow", "keyG &&",
              "patternIndexFromKey"):
    require(first_modal < public.index(later),
            "an open window must own keys before %s" % later)
require(public.index("step_settings_open_") < public.index("isDrumStepSettingsChord"),
        "a second Alt+E goes to the open window (it does not toggle it)")
require("GROOVEPUTER_KEY_DOWN" in public[:first_modal],
        "only keys are modal; Undo/Paste application events still pass")

# Closing: Enter (\n or \r) and Esc close; every other key is swallowed.
handler = body(LEGACY, "bool DrumSequencerMainPage::handleStepSettingsKey")
require("GROOVEPUTER_ESCAPE" in handler and "0x1B" in handler, "Esc must close")
require("'\\n'" in handler and "'\\r'" in handler, "Enter must close")
require("step_settings_open_ = false" in handler, "closing clears the flag")
require(handler.rstrip().endswith("return true;"),
        "the open window must swallow unrelated keys")
require("isDrumStepSettingsChord" not in handler,
        "Alt+E must not close the window")
require("'-'" in handler and "'='" in handler, "-/= must change the value")
require("void DrumSequencerPage::onExit()" in CPP and
        "step_settings_open_ = false" in body(CPP, "void DrumSequencerPage::onExit()"),
        "leaving DRUMS closes the window")

# Writes: one canonical drum mutation per press (one Undo step).
adjust = body(LEGACY, "void DrumSequencerMainPage::adjustStepSetting")
require("commitDrumPatternMutation" in adjust, "fields must be written via commitDrumPatternMutation")
require("applyField" in adjust and "anchor" in adjust,
        "the first cell of the area decides the value")
require("withAudioGuard" not in adjust and "currentScene()" not in adjust,
        "fields must not bypass the Undo owner")

# Limits and engine-backed ornaments only.
for needle in ("kProbMin = 0", "kProbMax = 100", "kProbStep = 5",
               "kTimingMin = -23", "kTimingMax = 23",
               "kVelocityMin = 1", "kVelocityMax = 127", "kVelocityStep = 5"):
    require(needle in EDIT, "missing limit %s" % needle)
for fx in ("DRUM_FX_FLAM", "DRUM_FX_ROLL"):
    require("fx == %s" % fx in ENGINE, "%s must be executed by the engine" % fx)
require("DRUM_FX_PITCH_UP;" not in EDIT and "DRUM_FX_REVERSE;" not in EDIT,
        "the window must not write FX the drum voices do not play")

# Draw: page footer while open and the window itself.
draw = body(LEGACY, "void DrumSequencerMainPage::draw(IGfx& gfx)")
require('"ENT/ESC:CLOSE"' in draw and "drawStepSettings(gfx)" in draw,
        "draw() must show the footer hint and paint the window")
require("ENT/ESC:CLOSE" in body(LEGACY, "void DrumSequencerMainPage::drawStepSettings"),
        "the window footer must say ENT/ESC:CLOSE")
for label in ('"PROB"', '"TIMING"', '"VEL"', '"FLAM/ROLL"'):
    require(label in LEGACY, "window must label %s" % label)

# Help: one DRUMS line, 38 characters at most; keys.md documents it.
drums = HELP[HELP.index("kDrumLines"):]
drums = drums[:drums.index("};")]
lines = re.findall(r'"(Alt\+E[^"]*)"', drums)
require(len(lines) == 1, "DRUMS help needs exactly one Alt+E line")
require(len(lines[0]) <= 38, "help line too long: %r" % lines[0])
require("`Alt+E` on the grid" in KEYS[KEYS.index("## DRUMS"):KEYS.index("## SONG")],
        "keys.md DRUMS must document Alt+E")

print("drum step settings source regressions: OK")
