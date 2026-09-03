#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def block(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


def main() -> None:
    header = (ROOT / "src/ui/pages/perform_page.h").read_text(encoding="utf-8")
    page = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")
    keyboard_header = (ROOT / "src/input/performance_keyboard.h").read_text(encoding="utf-8")
    keyboard_source = (ROOT / "src/input/performance_keyboard.cpp").read_text(encoding="utf-8")
    pulse = (ROOT / "src/input/performance_pulse.h").read_text(encoding="utf-8")

    require("enum class PerformanceToolContext" in header,
            "PERFORM tools must define a local KEY/CHORD/ARP/RHYTHM navigation context")
    for context in ("Key", "Chord", "Arp", "Rhythm"):
        require(context in header,
                f"PERFORM tools must expose the {context} navigation context")
    require("selectedContext_" in header and "selectedRow_" in header,
            "PERFORM may store only local context/row navigation state")
    require("toolsFullRedraw_" not in header and "toolsSelectionDirty_" not in header,
            "MiniAcidDisplay repaints the whole frame every update, so PERFORM must "
            "not keep partial-repaint dirtiness that leaves labels blank after frame one")
    for forbidden in (
        "PerformanceUiState", "root_", "scale_", "arpRate_", "chordMode_",
        "euclideanPulses_", "ratchetCount_",
    ):
        require(forbidden not in header,
                f"PERFORM page must not duplicate authoritative musical state: {forbidden}")

    require("HeldPerformanceSnapshot" not in page and
            "captureHeldPerformanceKeys" not in page and
            "restoreHeldPerformanceKeys" not in page,
            "UI must not snapshot, panic, and synthetically replay held keys")
    require("keyboard_.keyDown(" not in page,
            "PERFORM UI must never synthesize physical keyDown ownership")

    tools = block(page, "void PerformPage::drawToolsLayer", "bool PerformPage::handleEvent")
    require('kToolContextNames[] = {\n    "KEY", "CHORD", "ARP", "RHYTHM"' in page,
            "Tab layer must declare the four musician-facing context labels")
    for marker in ("[KEY]", "[CHORD]", "[ARP]", "[RHYTHM]"):
        require(marker in page,
                f"active context must have a structural non-color marker: {marker}")

    for getter in (
        "keyboard_.rootName()", "keyboard_.scaleName()", "keyboard_.octaveShift()",
        "keyboard_.velocity()", "keyboard_.chordModeName()", "keyboard_.chordInversion()",
        "keyboard_.chordSpreadName()", "keyboard_.voiceLeading()",
        "keyboard_.chordMemorySize()", "keyboard_.arpDirection()",
        "keyboard_.arpRateName()", "keyboard_.arpOctaves()", "keyboard_.gatePercent()",
        "keyboard_.latchEnabled()", "keyboard_.ratchetCount()",
        "keyboard_.euclideanLength()", "keyboard_.euclideanPulses()",
        "keyboard_.euclideanRotation()", "keyboard_.strumDirection()",
        "keyboard_.strumMs()",
    ):
        require(getter in tools, f"UI projection must read authoritative value via {getter}")

    chord_labels = {
        "Off": "OFF",
        "Major": "MAJ",
        "Minor": "MIN",
        "Fifth": "5TH",
        "Sus2": "SUS2",
        "Sus4": "SUS4",
        "Dominant7": "7",
        "Major7": "MAJ7",
        "Minor7": "MIN7",
        "ScaleTriad": "SCALE3",
        "ScaleSeventh": "SCALE7",
        "Memory": "MEM",
    }
    for mode, label in chord_labels.items():
        require(f"PerformanceChordMode::{mode}: return \"{label}\"" in keyboard_source,
                f"chord mode {mode} must have stable musician-facing label {label}")

    for rate in ('"1/4"', '"1/8"', '"1/8T"', '"1/16"', '"1/16T"', '"1/32"'):
        require(rate in pulse, f"Performance rate identity must remain musician-facing: {rate}")
    require('PerformanceArpDirection::AsPlayed: return "AS PLAYED"' in page,
            "AS PLAYED must remain visibly distinct and must not be sorted by the UI")

    require('"N/A"' in page,
            "dependency-disabled controls must render deterministic N/A states")
    require("rotationIsAudible" in page and "strumIsAudible" in page,
            "N/A dependency predicates must remain explicit and deterministic")

    tool_handler = block(page, "bool PerformPage::handleToolKey", "void PerformPage::drawToolsLayer")
    for scancode in (
        "GROOVEPUTER_LEFT", "GROOVEPUTER_RIGHT",
        "GROOVEPUTER_UP", "GROOVEPUTER_DOWN",
    ):
        require(scancode in tool_handler,
                f"local Performance Instrument navigation must consume {scancode}")
    require("adjustSelectedValue" in tool_handler and "toggleSelectedValue" in tool_handler,
            "local edits must dispatch through one bounded command path")
    require("case GROOVEPUTER_LEFT:\n            adjustSelectedValue(-1);" in tool_handler and
            "case GROOVEPUTER_RIGHT:\n            adjustSelectedValue(1);" in tool_handler,
            "Left/Right must edit the selected value; context switching is Tab/Shift+Tab")
    require("UIInput::navCode(event)" in tool_handler,
            "arrow handling must use the shared nav decoder (scancode or key form)")
    for keycap in ("case ';'", "case ','", "case '.'", "case '/'"):
        require(keycap in tool_handler,
                f"Cardputer arrow keycap character must be swallowed in tools: {keycap}")
    require("case '-':" not in tool_handler and "case '=':" not in tool_handler,
            "-/= must keep their live PERFORM octave meaning inside the tools layer")
    require("toolsLayerVisible_ && event.key == '\\b'" in page,
            "Backspace must step to the previous context: the Cardputer has no Shift key")
    require("keyboard_.panic()" not in tool_handler,
            "local tool edits must not directly introduce global panic")

    handler = block(page, "bool PerformPage::handleEvent", "void PerformPage::drawHeader")
    require("toolsLayerVisible_ && handleToolKey(event)" in handler and
            "GROOVEPUTER_ESCAPE" in handler and "return true;" in handler,
            "local navigation must be consumed before unrelated PERFORM commands")

    for command in (
        "cycleRoot", "cycleScale", "shiftOctave", "adjustVelocity",
        "cycleChordMode", "cycleChordInversion", "toggleChordSpread",
        "toggleVoiceLeading", "captureChordMemory", "clearChordMemory",
        "toggleArpeggiator", "cycleArpDirection", "cycleArpRate",
        "cycleArpOctaves", "cycleGate", "toggleLatch",
        "cycleRatchet", "cycleEuclideanLength", "cycleEuclideanPulses",
        "rotateEuclidean", "cycleStrumDirection", "cycleStrum",
    ):
        require(command in page,
                f"UI edits must call existing PerformanceKeyboard command path: {command}")

    require("emitAllNotesOff" not in page and "AllNotesOff" not in page,
            "PERFORM UI must not own MIDI cleanup")
    require("new " not in page and "std::vector" not in page,
            "Performance Instrument UI must remain fixed-allocation")

    require(tools.index("LayoutManager::clearContent(gfx);") < tools.index("drawToolTabs("),
            "tools surface must clear and fully repaint every frame: the display "
            "clears the screen before each page draw, so a partial repaint blanks labels")
    require("fullRedraw" not in tools and "toolsSelectionDirty_" not in tools,
            "tools surface must not gate label/tab drawing behind dirtiness flags")
    tabs = block(page, "void PerformPage::drawToolTabs", "void PerformPage::drawToolsLayer")
    require("kToolContextActiveNames[i]" in tabs and "x += (len + 3) * charW;" in tabs,
            "context tabs must occupy fixed slots so labels do not shift when switching")
    require("selectedRowHint()" in tools,
            "tools surface must show what -/+ and Enter do for the selected row")
    require("uint8_t selectedRow_[static_cast<int>(PerformanceToolContext::Count)]" in header,
            "each context must remember its own selected row")
    require('UI::showToast("PERFORM: KEY' not in page,
            "opening the tools layer must not cover the fresh surface with a toast")
    draw_content = block(page, "void PerformPage::drawContent", "void PerformPage::drawFooter")
    tools_pos = draw_content.index("if (toolsLayerVisible_)")
    normal_clear_pos = draw_content.index("LayoutManager::clearContent(gfx);")
    require(tools_pos < normal_clear_pos,
            "tools surface must return before the legacy live-PERFORM full content clear")

    require("activeRate() const" in keyboard_header and "arpRate() const" in keyboard_header,
            "clocked owner must retain distinct active and pending rate observability")
    require('"%s NEXT"' in tools and
            "keyboard_.activeRate() != keyboard_.arpRate()" in tools,
            "RATE must visibly distinguish a pending NEXT_STEP value")

    print("Performance Instrument UI source regressions: PASS")


if __name__ == "__main__":
    main()
