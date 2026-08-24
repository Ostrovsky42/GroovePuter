#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

pattern_header = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text()
synth_page = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text()
engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text()


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


def function_body(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        raise AssertionError(f"missing function: {signature}")
    brace = text.find("{", start)
    if brace < 0:
        raise AssertionError(f"missing body for: {signature}")
    depth = 0
    for index in range(brace, len(text)):
        ch = text[index]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[brace + 1:index]
    raise AssertionError(f"unterminated body for: {signature}")


# The NOTES editor follows the already-owned Song runtime address. It must not
# invent a second page switch or mutate persistent Song/Pattern state.
require(
    pattern_header,
    "void syncRuntimePatternSelection()",
    "PatternEditPage must expose one bounded runtime-selection sync hook",
)
require(
    pattern_header,
    "mini_acid_.display303PatternIndex(voice_index_)",
    "Synth follow must consume the engine display/global pattern address",
)
require(
    pattern_header,
    "patternAddressFromGlobal",
    "Synth follow must reuse canonical PatternAddress decoding",
)
require(
    pattern_header,
    "address.page != mini_acid_.currentPageIndex()",
    "Synth follow must wait until async pattern paging has made the target page resident",
)
require(pattern_header, "bank_index_ = address.bank;", "Bank selection must follow Song")
require(pattern_header, "bank_cursor_ = address.bank;", "Bank cursor must follow Song")
require(pattern_header, "pattern_row_cursor_ = address.slot;", "Pattern 1..8 cursor must follow Song")

follow_body = function_body(pattern_header, "void syncRuntimePatternSelection()")
for forbidden in (
    "requestPageSwitch(",
    "setCurrentPage(",
    "setSongPosition(",
    "set303PatternIndex(",
    "set303BankIndex(",
    "markSceneMutated(",
):
    if forbidden in follow_body:
        raise AssertionError(
            f"UI follow hook must be read-only; found forbidden owner call: {forbidden}"
        )

# Parent Synth page runs the sync even if KNOBS/MORE is currently visible, so
# returning to NOTES during PLAY or after STOP cannot revive an old cursor.
tick_body = function_body(synth_page, "void SynthSequencerPage::tick()")
require(
    tick_body,
    "pattern_page_->syncRuntimePatternSelection();",
    "Synth parent must keep the hidden NOTES editor synchronized",
)
if tick_body.find("syncRuntimePatternSelection") > tick_body.find("synth_tab_ == SynthTab::Notes"):
    raise AssertionError("runtime selection sync must happen before the NOTES-only tick gate")

# Existing engine ownership is part of the foundation: Song row activation owns
# page switching and local bank/pattern selection.
selection_body = function_body(engine, "void MiniAcid::applySongPositionSelection()")
require(selection_body, "songPatternPage(firstGlobal)", "Song owner must resolve global page")
require(selection_body, "requestPageSwitch(tPage)", "Song owner must request page residency")
require(selection_body, "songPatternBank(patA)", "Song owner must resolve Synth A bank")
require(selection_body, "songPatternBank(patB)", "Song owner must resolve Synth B bank")
require(selection_body, "songPatternIndexInBank(patA)", "Song owner must resolve Synth A slot")
require(selection_body, "songPatternIndexInBank(patB)", "Song owner must resolve Synth B slot")

# STOP must freeze the last Song selection instead of forcing Song mode off or
# returning to page zero. This is what makes immediate post-stop editing useful.
stop_body = function_body(engine, "void MiniAcid::stop()")
require(stop_body, "playing = false;", "STOP must end transport")
for forbidden in ("setSongMode(false", "setCurrentPage(0", "requestPageSwitch(0"):
    if forbidden in stop_body:
        raise AssertionError(
            f"STOP must preserve the last Song edit location; found: {forbidden}"
        )

# Guard against accidentally reducing the established address space. Each bank
# has eight visible pattern slots; the project currently supports more than one
# global pattern page.
scenes = (ROOT / "scenes.h").read_text()
require(scenes, "static constexpr int kPatterns = 8;", "Pattern bank must keep eight slots")
match = re.search(r"static constexpr int kMaxPages\s*=\s*(\d+)\s*;", scenes)
if not match or int(match.group(1)) < 8:
    raise AssertionError("global pattern paging must support at least eight pages")

print("synth Song follow source regressions: PASS")
