#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SYNTH = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text(encoding="utf-8")
SYNTH_HEADER = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text(encoding="utf-8")
SYNTH_LEGACY = (ROOT / "src/ui/pages/pattern_edit_page_legacy.h").read_text(encoding="utf-8")
DRUM = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")
DRUM_HEADER = (ROOT / "src/ui/pages/drum_sequencer_page.h").read_text(encoding="utf-8")
DRUM_LEGACY = (ROOT / "src/ui/pages/drum_sequencer_page_legacy.h").read_text(encoding="utf-8")
PATTERN_BAR = (ROOT / "src/ui/components/pattern_selection_bar.h").read_text(encoding="utf-8")
BANK_BAR = (ROOT / "src/ui/components/bank_selection_bar.h").read_text(encoding="utf-8")
DISPLAY = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
DRUM_AUTOMATION = (ROOT / "src/ui/pages/drum_automation_page.cpp").read_text(encoding="utf-8")
HELP = (ROOT / "src/ui/global_help_content.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


require('#include "pattern_edit_page_legacy.h"' in SYNTH,
        "synth wrapper must retain the established editor implementation")
require("bool handleEventLegacy(UIEvent& ui_event);" in SYNTH_HEADER,
        "synth legacy handler must be declared explicitly")
require("gridArrow && !ui_event.alt && !ui_event.meta" in SYNTH,
        "plain and selection arrows must be owned by the synth grid")
require("row = std::max(0, row - 1)" in SYNTH and
        "row = std::min(kPatternStepRows - 1, row + 1)" in SYNTH,
        "synth vertical arrows must clamp at the note-grid boundaries")
require("focus_ = Focus::Steps" in SYNTH,
        "synth pattern and bank shortcuts must restore step focus")
require("Bank: Ctrl+1 / Ctrl+2" not in SYNTH,
        "plain B interceptor must be removed from synth wrapper")
require("nextBank = (activeBankCursor() + 1) % kBankCount" in SYNTH_LEGACY,
        "plain B must fall through to the retained bank toggle")
require("patternIndexFromKey" in SYNTH and "scancodeToPatternIndex" in SYNTH,
        "Q-I pattern selection must remain available")
require("ARROWS:GRID Q-I:PAT" in SYNTH and "C1/2:BANK Alt[]:PAGE" in SYNTH,
        "synth footer must describe the locked bindings with pattern page hint")

require('#include "drum_sequencer_page_legacy.h"' in DRUM,
        "drum wrapper must retain the established sequencer implementation")
require("bool handleEventLegacy(UIEvent& ui_event);" in DRUM_HEADER,
        "outer drum legacy handler must be declared explicitly")
require("activePageIndex() != 0" in DRUM,
        "drum input lock must apply only to the main grid tab")
require("static_cast<DrumSequencerMainPage*>" in DRUM,
        "known main-tab type must use a no-RTTI cast")
require("dynamic_cast" not in DRUM,
        "Cardputer builds disable RTTI; drum routing must not use dynamic_cast")
require("friend class DrumSequencerPage" in DRUM,
        "outer drum page must have explicit access to the retained main grid")
require("page->focusGrid()" in DRUM,
        "drum pattern and bank shortcuts must restore grid focus")
require("std::clamp(" in DRUM and "NUM_DRUM_VOICES - 1" in DRUM,
        "drum vertical arrows must clamp at voice boundaries")
require("Bank: Ctrl+1 / Ctrl+2" not in DRUM,
        "plain B interceptor must be removed from drum wrapper")
require("nextBank = (activeBankCursor() + 1) % kBankCount" in DRUM_LEGACY,
        "plain B must fall through to the retained bank toggle in drums")
require("ARROWS:GRID Q-I:PAT" in DRUM and "C1/2:BANK Alt[]:PAGE" in DRUM,
        "drum footer must describe the locked bindings with pattern page hint")

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

# Alt+[/] pattern-page cycling uses kMaxPages, not kPageCount
require("kMaxPages" in DISPLAY and "alt && (event.key == '['" in DISPLAY and "alt && (event.key == ']'" in DISPLAY,
        "pattern page switching must be present globally")
alt_bracket_block = DISPLAY[DISPLAY.find("alt && (event.key == '['"):DISPLAY.find("alt && (event.key == '['") + 500]
require("kMaxPages" in alt_bracket_block and "kPageCount" not in alt_bracket_block,
        "Alt+[ / ] must use kMaxPages for pattern-page wraparound, not kPageCount")

# Plain [/] (no modifier) remains UI-page navigation
require("if (event.key == ']') { nextPage(); return true; }" in DISPLAY and
        "if (event.key == '[') { previousPage(); return true; }" in DISPLAY,
        "plain [ / ] must remain UI-page navigation")

# Fn+[/] (meta modifier) remains workflow switching
require("event.meta && (event.key == '[' || event.key == '{')" in DISPLAY and
        "switchWorkflow_(-1)" in DISPLAY,
        "Fn+[ must remain workflow switching via meta modifier")
require("event.meta && (event.key == ']' || event.key == '}')" in DISPLAY and
        "switchWorkflow_(1)" in DISPLAY,
        "Fn+] must remain workflow switching via meta modifier")

# Drum Automation page must not be affected
require('#include "pattern_edit_page.h"' not in DRUM_AUTOMATION,
        "Drum Automation must remain independent of pattern editor")
require("kMaxPages" not in DRUM_AUTOMATION and "requestPageSwitch" not in DRUM_AUTOMATION,
        "Drum Automation must not gain pattern-page bindings")
require("Ctrl+1" not in DRUM_AUTOMATION and "Bank: Ctrl+1" not in DRUM_AUTOMATION,
        "Drum Automation must not reference pattern bank shortcuts")

# Help text must be updated and stale bindings removed
require("Alt+[ / ]   Prev/next pattern page" in HELP,
        "Global help must describe Alt+[/] as pattern page switching")
require("Alt+[ / ]   Pattern page" in HELP,
        "Synth A help must describe Alt+[/] as pattern page switching")
require("Ctrl+1/2    Bank A/B (direct)" in HELP,
        "Drum help must document Ctrl+1/2 as direct bank selection")
require("B:Bank" not in HELP,
        "stale 'B:Bank' compact hint must be removed from help text")
