#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        print(f"UI FINAL SOURCE REGRESSION: {message}", file=sys.stderr)
        raise SystemExit(1)


phrase_h = read("src/ui/pages/phrase_page.h")
phrase_cpp = read("src/ui/pages/phrase_page.cpp")
request_state = read("src/state/phrase_generation_request_state.h")
product_state = read("src/state/generated_phrase_product_state.h")
genre_h = read("src/ui/pages/genre_page.h")
genre_cpp = read("src/ui/pages/genre_page.cpp")
feel_cpp = read("src/ui/pages/feel_page.cpp")
pattern_h = read("src/ui/pages/pattern_edit_page.h")
synth_cpp = read("src/ui/pages/synth_sequencer_page.cpp")
engine = read("src/dsp/miniacid_engine.cpp")
scenes = read("scenes.h")

# P1/P2: Generated Phrase request length has one session owner and does not use
# legacy PhraseCore capture length or FEEL CYCLE.
require("requestedPhraseBars()" in request_state,
        "Generated Phrase request owner missing")
require("cycleRequestedPhraseBars" in request_state,
        "Generated Phrase request cycle missing")
for value in ("case 1:", "case 2:", "case 4:", "case 8:"):
    require(value in request_state, f"exact phrase request domain missing {value}")
require("scene.feel.patternBars" not in request_state,
        "request owner must not alias FEEL CYCLE")
require("request.lengthBars = capture_length_;" in phrase_cpp,
        "legacy PhraseCore capture length wiring changed unexpectedly")
require("const uint8_t requestedBars = GroovePuterState::requestedPhraseBars();" in phrase_cpp,
        "Generated Phrase command must read dedicated request owner")
require("mini_acid_, requestedBars, songStart" in phrase_cpp,
        "GeneratedPhraseSong must receive dedicated requestedPhraseBars")

# P2/P3: semantic bar and physical storage coordinates are explicit and never
# inferred by modulo arithmetic.
require("accepted.songStart + product_bar_cursor_" in phrase_cpp,
        "semantic bar focus must use accepted songStart + ordinal")
require("accepted.firstLocalSlot) + bar" in phrase_cpp,
        "physical target must use accepted firstLocalSlot + explicit bar")
require("patternAddress %" not in phrase_cpp and "Song row %" not in phrase_cpp,
        "Phrase UI must not infer semantic identity via modulo")
require("setSongPosition(row);" in phrase_cpp,
        "STOP-time semantic bar focus must reuse existing Song selection path")
require("if (mini_acid_.isPlaying())" in phrase_cpp and
        '"STOP TO FOCUS BAR"' in phrase_cpp,
        "Phrase UI must not redirect live transport while playing")

# P3: bounded read model exposes existing I1 evidence only. It must not own
# harmonic/progression policy or allocate dynamic buffers.
require("GeneratedPhraseAcceptedSnapshot" in product_state,
        "bounded generated Phrase snapshot missing")
require("phraseGenerationIdentity" in product_state and
        "harmonicEventPositions" in product_state and
        "progression" in product_state,
        "required I1 evidence missing from read model")
require("sizeof(GeneratedPhraseProductState) <= 24" in product_state,
        "bounded product-state size firewall missing")
for forbidden in ("std::vector", "std::string", "new ", "malloc", "ChordProgressionSource", "HarmonicRhythmPlan"):
    require(forbidden not in product_state,
            f"product read model owns forbidden/dynamic state: {forbidden}")

# Outcomes remain three separate product classes.
for token in ("Accepted", "TypedRejection", "ExecutionFailure"):
    require(token in product_state, f"generation outcome missing: {token}")
require("PhraseExecutionStatus::Rejected" in phrase_cpp,
        "typed rejection classification missing")
require("publishGeneratedPhraseTypedRejection" in phrase_cpp and
        "publishGeneratedPhraseExecutionFailure" in phrase_cpp,
        "typed rejection and execution failure must publish separately")

# DEPTH row and plain P share the same existing generation_request_state owner.
require("Depth," in genre_h, "GENRE DEPTH focus row missing")
require('"DEPTH"' in genre_cpp, "GENRE DEPTH label missing")
require("GroovePuterState::cycleGenerationLevel(delta)" in genre_cpp,
        "focused DEPTH Left/Right must use canonical owner")
require("GroovePuterState::cycleGenerationLevel()" in genre_cpp,
        "plain P must use canonical DEPTH owner")

# FEEL CYCLE remains scene.feel.patternBars, owned by the FEEL page.
require("scene.feel.patternBars = next;" in feel_cpp,
        "FEEL cycle owner changed")
# PHW-P1 redesigned the generated-Phrase product screen around
# LENGTH/DEPTH/TO/BAR placement and admissibility (see spec sections 1-2,
# 25); FEEL CYCLE is no longer displayed there at all, so there is no risk
# of the two being conflated on-screen. scene.feel.patternBars must still
# not appear in PhrasePage's own state (checked in test_0_9_9_ui_p0_source_
# regressions.py), only its FEEL-page display convention changed.
require("scene.feel.patternBars" not in phrase_h,
        "PhrasePage must not own FEEL cycle state")

# I1 follow/STOP remains authoritative. UI mirrors engine physical selection;
# STOP must not restore pattern-mode selection.
require("syncSongPatternContext();" in synth_cpp,
        "Synth NOTES must keep I1 follow projection")
require("mini_acid_.current303PatternIndex(voice_index_)" in pattern_h and
        "mini_acid_.current303BankIndex(voice_index_)" in pattern_h,
        "NOTES follow must mirror engine physical pattern/bank")
stop_begin = engine.find("void MiniAcid::stop()")
stop_end = engine.find("void MiniAcid::pauseTransport()", stop_begin)
require(stop_begin >= 0 and stop_end > stop_begin, "STOP implementation anchors missing")
stop_body = engine[stop_begin:stop_end]
require("setSongPosition(clampSongPosition(songPlayheadPosition_))" in stop_body,
        "STOP must retain authoritative Song playhead")
require("patternModeSynthPatternIndex_" not in stop_body and
        "patternModeSynthBankIndex_" not in stop_body,
        "STOP must not restore pre-PLAY synth selection")

# Existing 16-page storage model is authoritative; UI must not shrink it.
require(re.search(r"static constexpr int kMaxPages\s*=\s*16;", scenes) is not None,
        "16-page storage capacity changed")
require("accepted.pageIndex < 0 || accepted.pageIndex >= kMaxPages" in phrase_cpp,
        "Phrase physical mapping must respect full page capacity")

# No new UI-local musical policy/lifetime owner. Cross-bar visualization stays
# gated until corrected C2/R1/hardware is replayed onto this line.
ui_text = "\n".join(
    path.read_text(encoding="utf-8")
    for path in (ROOT / "src/ui").rglob("*")
    if path.suffix in (".h", ".cpp")
)
for forbidden in (
    "chordProgressionEventAt(",
    "preparePhraseHarmonicClockProjection(",
    "makePhraseHarmonicTimeline(",
    "ActivityLevel",
    "VariationProfile",
    "MelodicCrossBarLifetime",
    "A_CONTINUATION",
    "A_OVERLAP",
):
    require(forbidden not in ui_text,
            f"UI owns forbidden musical/lifetime policy: {forbidden}")

print("UI final ownership/source regressions: OK")
print("- Generated Phrase request owner: dedicated 1/2/4/8 session state")
print("- PhraseCore capture length: separate")
print("- FEEL CYCLE: separate scene.feel.patternBars owner")
print("- semantic bar -> physical target: explicit, no modulo identity")
print("- ACCEPTED / TYPED REJECTION / EXECUTION FAILURE: distinct")
print("- DEPTH row + P shortcut: one canonical owner")
print("- I1 follow/STOP: reused, no second transport mapper")
print("- 16-page storage: preserved")
print("- cross-bar UI: intentionally gated pending corrected R1/hardware")
