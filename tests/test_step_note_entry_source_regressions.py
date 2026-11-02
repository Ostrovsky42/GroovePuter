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
require("kEntryBaseNote = 48" in CPP and "kEntryOctaveSpan = 12" in CPP,
        "direct note rows must keep the C3 default with the upper row one octave up")
require("entryBaseNote() + kEntryOctaveSpan + upperIndex" in CPP,
        "upper row must sit exactly one octave above the lower row")
require("kEntryNoteMin = 24" in CPP and "kEntryNoteMax = 71" in CPP and
        "NOTE OUT OF RANGE" in CPP,
        "entry must refuse notes outside the engine 303 range instead of clipping")
require("if (key == '-' || key == '=')" in CPP and
        "shiftEntryOctave(key == '-' ? -1 : 1);" in CPP,
        "-/= must move the NOTE ENTRY octave")
require(CPP.index("shiftEntryOctave(key == '-'") < CPP.index("if (handleNoteEntryKey(key)) return true;"),
        "octave keys must be handled before the note handler")

handler_start = CPP.index("bool PatternEditPage::handleNoteEntryKey")
handler_end = CPP.index("bool PatternEditPage::handleEvent", handler_start)
handler = CPP[handler_start:handler_end]
require("advanceNoteEntryCursor();" not in handler,
        "note-key audition must not move the visible step cursor")

require("if (key == '\\n' || key == '\\r')" in CPP and
        "advanceNoteEntryCursor();" in CPP,
        "Enter must be the explicit commit-and-advance action")

public_start = CPP.index("bool PatternEditPage::handleEvent(UIEvent& ui_event)")
public = CPP[public_start:]
backspace_start = public.index("if (isBackspace)")
backspace_end = public.index("if (key == '\\n' || key == '\\r')", backspace_start)
backspace_block = public[backspace_start:backspace_end]
require("commitPatternMutation" in backspace_block and
        "GroovePuterUndo::PatternEdit::clearStep(pattern, step)" in backspace_block,
        "NOTE ENTRY Backspace must clear through the canonical R3 Pattern owner")
require("mini_acid_.clear303Step" not in backspace_block and
        "editCurrentSynthPattern" not in backspace_block,
        "NOTE ENTRY Backspace must not bypass R3 through a live Pattern mutator")

repeat_start = CPP.index("if (lowerKey == 'c')")
repeat_end = CPP.index("if (handleNoteEntryKey(key))", repeat_start)
repeat_block = CPP[repeat_start:repeat_end]
require("last_entered_note_" in repeat_block,
        "C must recall the last entered note on Cardputer")
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
require("case GROOVEPUTER_UP" in CPP and "case GROOVEPUTER_DOWN" in CPP,
        "arrow navigation must remain available for moving across grid rows")

print("step note entry source regressions passed")
