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

handler_start = CPP.index("bool PatternEditPage::handleNoteEntryKey")
handler_end = CPP.index("bool PatternEditPage::handleEvent", handler_start)
handler = CPP[handler_start:handler_end]
require("advanceNoteEntryCursor();" not in handler,
        "note-key audition must not move the visible step cursor")

require("if (key == '\\n' || key == '\\r')" in CPP and
        "advanceNoteEntryCursor();" in CPP,
        "Enter must be the explicit commit-and-advance action")
require("mini_acid_.clear303Step(step, voice_index_)" in CPP,
        "Backspace must clear the focused note step")

repeat_start = CPP.index("if (key == ';' || key == ':')")
repeat_end = CPP.index("if (handleNoteEntryKey(key))", repeat_start)
repeat_block = CPP[repeat_start:repeat_end]
require("last_entered_note_" in repeat_block,
        "semicolon must recall the last entered note")
require("advanceNoteEntryCursor();" not in repeat_block,
        "repeat-last must not move the cursor before Enter")

require("kFirstHoldRepeatMinMs = 250" in CPP and "kHoldRepeatMaxGapMs = 180" in CPP,
        "hold inference must distinguish Cardputer repeat cadence from fast taps")
require("writeStep = last_entered_step_ + 1;" in CPP,
        "held note must extend into following steps without moving the cursor")
require("pattern.steps[last_entered_step_].slide = true;" in CPP,
        "hold continuation must use the existing slide/legato representation")
require("pattern.steps[step].note = static_cast<int8_t>(note);" in CPP,
        "note entry must write through the existing SynthStep note field")
require("struct SynthStep" in SCENE and "uint8_t slide : 1" in SCENE,
        "source contract assumes persisted SynthStep note+slide semantics")
require("if (!note_entry_mode_" in CPP and "patternIndexFromKey(lowerKey)" in CPP,
        "Q-I pattern selection must remain available when NOTE ENTRY is disabled")

arrow_owner = "if (note_entry_mode_ && gridArrow && !ui_event.alt && !ui_event.ctrl)"
require(arrow_owner in CPP,
        "NOTE ENTRY must own Cardputer arrow scancodes even when Fn/meta is held")
arrow_pos = CPP.index(arrow_owner)
legacy_note_pos = CPP.index(
    "if (note_entry_mode_ && !ui_event.ctrl && !ui_event.meta && !ui_event.alt)")
require(arrow_pos < legacy_note_pos,
        "Fn arrow routing must run before the legacy meta-gated note-entry path")
require("case GROOVEPUTER_LEFT" in CPP and "case GROOVEPUTER_RIGHT" in CPP and
        "case GROOVEPUTER_UP" in CPP and "case GROOVEPUTER_DOWN" in CPP,
        "NOTE ENTRY must support all four grid-arrow directions")

print("step note entry source regressions passed")
