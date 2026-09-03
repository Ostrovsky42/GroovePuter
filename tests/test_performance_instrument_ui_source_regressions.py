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
        require(marker in tools,
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

    handler = block(page, "bool PerformPage::handleEvent", "void PerformPage::drawHeader")
    for scancode in (
        "GROOVEPUTER_LEFT", "GROOVEPUTER_RIGHT",
        "GROOVEPUTER_UP", "GROOVEPUTER_DOWN",
    ):
        require(scancode in handler,
                f"local Performance Instrument navigation must consume {scancode}")
    require("adjustSelectedValue" in handler and "toggleSelectedValue" in handler,
            "local edits must dispatch through one bounded command path")
    require("toolsLayerVisible_" in handler and "return true;" in handler,
            "local navigation must be consumed and never fall through to legacy commands")

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

    tool_handler = block(page, "bool PerformPage::handleToolKey", "void PerformPage::drawToolsLayer")
    require("keyboard_.panic()" not in tool_handler,
            "local tool edits must not directly introduce global panic")
    require("emitAllNotesOff" not in page and "AllNotesOff" not in page,
            "PERFORM UI must not own MIDI cleanup")

    require("drawToolsLayer" in page and "LayoutManager::clearContent(gfx);" in page,
            "context layer must stay inside the existing bounded PERFORM renderer")
    require("new " not in page and "std::vector" not in page,
            "Performance Instrument UI must remain fixed-allocation")
    require(page.count("LayoutManager::clearContent(gfx);") == 1,
            "context navigation must not introduce additional full-content clears")

    require("activeRate() const" in keyboard_header and "arpRate() const" in keyboard_header,
            "clocked owner must retain distinct active and pending rate observability")
    require('"%s NEXT"' in tools and
            "keyboard_.activeRate() != keyboard_.arpRate()" in tools,
            "RATE must visibly distinguish a pending NEXT_STEP value")

    print("Performance Instrument UI source regressions: PASS")


if __name__ == "__main__":
    main()
