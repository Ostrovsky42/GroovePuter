#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SYNTH_HEADER = (ROOT / "src/ui/pages/synth_sequencer_page.h").read_text()
SYNTH_SOURCE = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text()
PATTERN_SOURCE = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


# P3-U1 source contract: the existing Synth NOTES container becomes the
# source-aware presentation/controller. PatternEditPage remains the
# authoritative Pattern child instead of absorbing a second editor.
require("currentSequencedSource(voice_index_)" in SYNTH_SOURCE,
        "Synth NOTES controller does not read MiniAcid::SequencedSource for its voice")
require("MiniAcid::SequencedSource::Phrase" in SYNTH_SOURCE,
        "Synth NOTES controller has no PHRASE projection branch")
require("drawPhraseNotes" in SYNTH_HEADER and "drawPhraseNotes" in SYNTH_SOURCE,
        "Synth NOTES controller has no PHRASE presentation boundary")
require("handlePhraseNotesEvent" in SYNTH_HEADER and
        "handlePhraseNotesEvent" in SYNTH_SOURCE,
        "Synth NOTES controller has no PHRASE event boundary")

for forbidden in ("phrase_source_", "phrase_mode_", "is_phrase_source_", "phraseSource_"):
    require(forbidden not in SYNTH_HEADER and forbidden not in SYNTH_SOURCE,
            f"Synth NOTES introduced a second UI-owned source state: {forbidden}")

# Slice 2: actual PHRASE rendering must consume the one-event -> one-span
# projection. Continuation is rendering data only; no TIE-like event generation
# is permitted in the UI layer.
require('#include "../phrase_notes_projection.h"' in SYNTH_SOURCE,
        "PHRASE NOTES renderer does not include the read-only projection")
require("PhraseNotesProjection::project" in SYNTH_SOURCE,
        "PHRASE NOTES renderer does not consume projected event spans")
require("TIE" not in SYNTH_SOURCE and "Tie" not in SYNTH_SOURCE,
        "PHRASE NOTES renderer introduced synthetic TIE semantics")

# The existing Pattern page stays a child and remains the Pattern model owner.
require("std::make_shared<PatternEditPage>" in SYNTH_SOURCE,
        "PatternEditPage is no longer the retained Pattern NOTES child")
require("handleEventLegacy(ui_event)" in PATTERN_SOURCE,
        "Pattern event path no longer delegates to the retained implementation")

# Both presentation and input consult the per-instance voice index. This permits
# Synth A=PHRASE and Synth B=PATTERN without process-global source state.
require(SYNTH_SOURCE.count("currentSequencedSource(voice_index_)") >= 2,
        "draw and event routing are not both driven by the per-voice source")

print("P3-U1 source/phrase projection source-regression: OK")
