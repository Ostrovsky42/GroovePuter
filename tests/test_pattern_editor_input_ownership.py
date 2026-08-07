#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SYNTH = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text(encoding="utf-8")
SYNTH_HEADER = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text(encoding="utf-8")
DRUM = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
DRUM_HEADER = (ROOT / "src/ui/pages/drum_sequencer_page.h").read_text(encoding="utf-8")
PATTERN_BAR = (ROOT / "src/ui/components/pattern_selection_bar.h").read_text(encoding="utf-8")
BANK_BAR = (ROOT / "src/ui/components/bank_selection_bar.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


require('#include "pattern_edit_page_legacy.h"' in SYNTH,
        "synth wrapper must retain the established editor implementation")
require("bool PatternEditPage::handleEventLegacy" in SYNTH_HEADER,
        "synth legacy handler must be declared explicitly")
require("gridArrow && !ui_event.alt && !ui_event.meta" in SYNTH,
        "plain and selection arrows must be owned by the synth grid")
require("row = std::max(0, row - 1)" in SYNTH and
        "row = std::min(kPatternStepRows - 1, row + 1)" in SYNTH,
        "synth vertical arrows must clamp at the note-grid boundaries")
require("focus_ = Focus::Steps" in SYNTH,
        "synth pattern and bank shortcuts must restore step focus")
require("Bank: Ctrl+1 / Ctrl+2" in SYNTH,
        "plain B must no longer cycle synth banks")
require("patternIndexFromKey" in SYNTH and "scancodeToPatternIndex" in SYNTH,
        "Q-I pattern selection must remain available")
require("ARROWS:GRID Q-I:PAT" in SYNTH and "C1/2:BANK TAB:SUB" in SYNTH,
        "synth footer must describe the locked bindings")

require('#include "drum_sequencer_page_legacy.h"' in DRUM,
        "drum wrapper must retain the established sequencer implementation")
require("bool DrumSequencerPage::handleEventLegacy" in DRUM_HEADER,
        "outer drum legacy handler must be declared explicitly")
require("dynamic_cast<DrumSequencerMainPage*>" in DRUM,
        "only the main drum grid may receive the input lock")
require("page->focusGrid()" in DRUM,
        "drum pattern and bank shortcuts must restore grid focus")
require("std::clamp(" in DRUM and "NUM_DRUM_VOICES - 1" in DRUM,
        "drum vertical arrows must clamp at voice boundaries")
require("Bank: Ctrl+1 / Ctrl+2" in DRUM,
        "plain B must no longer cycle drum banks")
require("Q-I:PAT C1/2:BANK" in DRUM,
        "drum footer must describe the locked bindings")

require("handleEventLegacy" in PATTERN_BAR and "handleEventLegacy" in BANK_BAR,
        "selector mouse handlers must remain callable from retained sources")
require((ROOT / "src/ui/pages/pattern_edit_page_legacy.h").exists(),
        "synth retained source is missing")
require((ROOT / "src/ui/pages/drum_sequencer_page_legacy.h").exists(),
        "drum retained source is missing")
require(not (ROOT / "src/ui/pages/pattern_edit_page_legacy.inc").exists(),
        "Arduino-incompatible synth .inc must not ship")
require(not (ROOT / "src/ui/pages/drum_sequencer_page_legacy.inc").exists(),
        "Arduino-incompatible drum .inc must not ship")
require(not (ROOT / ".github/workflows/apply-pattern-input-lock.yml").exists(),
        "temporary self-modifying workflow must not ship")
require(not (ROOT / ".github/workflows/run-pattern-input-lock.yml").exists(),
        "temporary trigger workflow must not ship")
