#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text()
SOURCE = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


# P3-U1 RED #1: NOTES must project from the authoritative per-voice engine
# source. A UI-owned phrase/source boolean would be a second owner and is
# forbidden even if it happened to be initialized from MiniAcid once.
require("currentSequencedSource(voice_index_)" in SOURCE,
        "NOTES does not read MiniAcid::SequencedSource for the active synth voice")
require("MiniAcid::SequencedSource::Phrase" in SOURCE,
        "NOTES has no PHRASE projection branch")
require("drawPhraseStyle" in HEADER and "drawPhraseStyle" in SOURCE,
        "NOTES has no dedicated read-only PHRASE presentation boundary")
require("handlePhraseEvent" in HEADER and "handlePhraseEvent" in SOURCE,
        "NOTES has no PHRASE event-controller boundary")

for forbidden in ("phrase_source_", "phrase_mode_", "is_phrase_source_", "phraseSource_"):
    require(forbidden not in HEADER and forbidden not in SOURCE,
            f"NOTES introduced a second UI-owned source state: {forbidden}")

# Pattern remains the retained implementation. Source-awareness is a dispatch
# seam, not a rewrite of legacy Pattern behavior.
require("handleEventLegacy(ui_event)" in SOURCE,
        "Pattern event path no longer delegates to the retained implementation")
require("drawMinimalStyle(gfx)" in SOURCE and "drawRetroClassicStyle(gfx)" in SOURCE
        and "drawAmberStyle(gfx)" in SOURCE,
        "Pattern visual styles were removed instead of preserved behind source dispatch")

# Per-voice authority is load-bearing. This source expression must use the
# PatternEditPage's voice_index_, never a process-global or Synth-A-only flag.
require(SOURCE.count("currentSequencedSource(voice_index_)") >= 2,
        "draw and event routing are not both driven by the page's per-voice source")

print("P3-U1 source projection source-regression: OK")
