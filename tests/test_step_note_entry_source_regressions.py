#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text(encoding="utf-8")
HDR = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text(encoding="utf-8")
SCENE = (ROOT / "scenes.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


require("bool note_entry_mode_ = false;" in HDR,
        "NOTE ENTRY must be opt-in so legacy pattern editing remains default")
require("lowerKey == 'n'" in CPP and "NOTE ENTRY: ON" in CPP,
        "N must toggle local NOTE ENTRY mode")
require('"asdfghjkl"' in CPP and '"qwertyuiop"' in CPP,
        "NOTE ENTRY must expose both Cardputer letter rows")
require("kEntryLowerBaseNote = 48" in CPP and "kEntryUpperBaseNote = 60" in CPP,
        "direct note rows must have stable C3/C4 bases")
require("pattern_edit_cursor_ = (activePatternStep() + 1) % SEQ_STEPS;" in CPP,
        "entered notes must auto-advance exactly one step")
require("mini_acid_.clear303Step(step, voice_index_)" in CPP,
        "Backspace must clear the focused note step")
require("key == ';' || key == ':'" in CPP and "last_entered_note_" in CPP,
        "semicolon must repeat the last entered note")
require("kFirstHoldRepeatMinMs = 250" in CPP and "kHoldRepeatMaxGapMs = 180" in CPP,
        "hold inference must distinguish Cardputer repeat cadence from fast taps")
require("pattern.steps[last_entered_step_].slide = true;" in CPP,
        "hold continuation must use the existing slide/legato representation")
require("pattern.steps[step].note = static_cast<int8_t>(note);" in CPP,
        "note entry must write through the existing SynthStep note field")
require("struct SynthStep" in SCENE and "uint8_t slide : 1" in SCENE,
        "source contract assumes persisted SynthStep note+slide semantics")
require("if (!note_entry_mode_" in CPP and "patternIndexFromKey(lowerKey)" in CPP,
        "Q-I pattern selection must remain available when NOTE ENTRY is disabled")

print("step note entry source regressions passed")
