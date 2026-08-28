#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        print(f"UI-P0 SOURCE REGRESSION: {message}", file=sys.stderr)
        raise SystemExit(1)


def between(text: str, start: str, end: str) -> str:
    begin = text.find(start)
    require(begin >= 0, f"missing start anchor: {start}")
    finish = text.find(end, begin + len(start))
    require(finish >= 0, f"missing end anchor: {end}")
    return text[begin:finish]


workflow = read("src/ui/workflow_mode.h")
feel = read("src/ui/pages/feel_page.cpp")
genre = read("src/ui/pages/genre_page.cpp")
phrase_h = read("src/ui/pages/phrase_page.h")
phrase_cpp = read("src/ui/pages/phrase_page.cpp")
synth = read("src/ui/pages/synth_sequencer_page.cpp")
pattern_h = read("src/ui/pages/pattern_edit_page.h")
engine = read("src/dsp/miniacid_engine.cpp")
generated = read("src/dsp/generated_phrase_song.h")
p1r = read("src/dsp/generated_phrase_p1r_materializer.h")
phrase_exec = read("src/generation/migration/phrase_execution.h")
semantic = read("src/generation/migration/phrase_semantic_result.h")
length = read("src/generation/composition/phrase_length_request.h")
request_state = read("src/state/generation_request_state.h")
scenes = read("scenes.h")

# P0-1: product workspace topology is already GENERATE=GENRE->FEEL and SONG=SONG->PHRASE.
require("static constexpr int kGeneratePages[] = {\n        kGenre, kFeel," in workflow,
        "GENERATE workflow must remain GENRE -> FEEL")
require("static constexpr int kSongPages[] = {\n        kArrange, kPhrase," in workflow,
        "SONG workflow must remain SONG -> PHRASE")
require("if (page == kTexture || page == kGeneration) return kFeel;" in workflow,
        "legacy Generation/Texture page ids must normalize to FEEL")

# P0-2: FEEL CYCLE is the existing scene.feel.patternBars owner, never phrase length.
require("const uint8_t next = shiftRepeatBars(scene.feel.patternBars, delta);" in feel,
        "FEEL cycle selector must read scene.feel.patternBars")
require("scene.feel.patternBars = next;" in feel,
        "FEEL cycle selector must write scene.feel.patternBars")
require("scene.feel.patternBars" not in phrase_h,
        "PhrasePage must not own FEEL cycle state")
require("if (prepared.request.forceSingleBarRows) scene.feel.patternBars = 1;" in generated,
        "I1 single-bar Song-row normalization must remain explicit and separate from requested phrase length")

# P0-3: frozen phrase-length policy is typed 1/2/4/8 admission, not UI coercion.
for value in ("phraseBars == 1", "phraseBars == 2", "phraseBars == 4", "phraseBars == 8"):
    require(value in length, f"phrase length domain missing {value}")
require("enum class PhraseLengthRequestStatus" in length and "Rejected" in length,
        "phrase length resolver must retain typed rejection")
require("resolveGenerationCompositionForPhraseBars" in length,
        "authoritative phrase-length resolver missing")

# P0-4: current product-request ownership gap remains explicit at I1.
require("uint8_t capture_length_ = 4;" in phrase_h,
        "P0 characterization expects PhrasePage-local capture_length_ ambiguity")
require("GeneratedPhraseSong::generate(\n      mini_acid_, capture_length_, songStart" in phrase_cpp,
        "current generated phrase request must still be sourced from capture_length_ at I1")
require("request.lengthBars = capture_length_;" in phrase_cpp,
        "the same local value must still also serve legacy PhraseCore capture at I1")

# P0-5: physical destination address and semantic bar ordinal remain distinct.
prepared_struct = between(phrase_exec, "struct PreparedPhraseExecution", "PhraseExecutionStatus preparePhraseExecution")
require("patternAddress" not in prepared_struct and "physicalPatternAddress" not in prepared_struct,
        "PreparedPhraseExecution must not retain physical pattern identity")
require("uint8_t phraseBarOrdinal" in phrase_exec and "int16_t physicalPatternAddress" in phrase_exec,
        "materializer API must keep semantic bar and physical destination as separate coordinates")
require("materializePreparedPhraseBar(\n        execution,\n        bar,\n        static_cast<int16_t>(globalPattern)" in p1r,
        "I1 must pass explicit semantic bar plus physical global destination")
require("kLogicalPhraseAttemptChannel = 0xFFFF" in p1r,
        "logical phrase reroll identity must not use a physical pattern address")

# P0-6: exact I1 follow-NOTES path: transport resolves physical selection; UI mirrors it.
require("pattern_page_->syncSongPatternContext();" in synth,
        "Synth NOTES tick must mirror Song pattern context")
require("if (!mini_acid_.songModeEnabled()) return;" in pattern_h,
        "NOTES follow must be scoped to Song mode")
require("pattern_row_cursor_ = mini_acid_.current303PatternIndex(voice_index_);" in pattern_h,
        "NOTES must mirror engine physical pattern index")
require("bank_index_ = mini_acid_.current303BankIndex(voice_index_);" in pattern_h,
        "NOTES must mirror engine physical bank index")
apply_song = between(engine, "void MiniAcid::applySongPositionSelection()", "// REWRITTEN LOGIC")
require("sceneManager_.songPatternAtSlot(songPlaybackSlot_, pos, SongTrack::SynthA)" in apply_song,
        "transport must resolve Synth A from authoritative Song row")
require("sceneManager_.songPatternAtSlot(songPlaybackSlot_, pos, SongTrack::SynthB)" in apply_song,
        "transport must resolve Synth B from authoritative Song row")
require("sceneManager_.setCurrentSynthPatternIndex(0, pat);" in apply_song and
        "sceneManager_.setCurrentSynthPatternIndex(1, pat);" in apply_song,
        "transport must publish exact physical Synth selections")
require("requestPageSwitch(tPage);" in apply_song,
        "physical Song resolution must remain the auto-page source")

# P0-7: STOP freezes the current playback location instead of restoring pre-PLAY pattern selection.
stop_body = between(engine, "void MiniAcid::stop()", "void MiniAcid::pauseTransport()")
require("sceneManager_.setSongPosition(clampSongPosition(songPlayheadPosition_));" in stop_body,
        "STOP must preserve current Song playhead row")
require("patternModeSynthPatternIndex_" not in stop_body and "patternModeSynthBankIndex_" not in stop_body,
        "STOP must not restore pre-PLAY Synth physical selection")

# P0-8: typed rejection and execution failure are distinct product outcomes.
require('return "PHRASE LENGTH REJECTED";' in generated,
        "typed phrase-length rejection label missing")
require('return "PHRASE EXEC FAILED";' in generated,
        "phrase execution failure label missing")
require("LifecycleStatus::CommittedNow" in generated and "LifecycleStatus::PendingNextBar" in generated,
        "accepted phrase lifecycle outcomes missing")
require("LifecycleStatus::Busy" in generated and "LifecycleStatus::OutOfMemory" in generated,
        "execution-failure lifecycle outcomes missing")

# P0-9: DEPTH already has one non-UI session owner; UI must reuse it.
require("inline GroovePuterRhythm::RealizationLevel currentGenerationLevel()" in request_state,
        "generation depth read owner missing")
require("inline GroovePuterRhythm::RealizationLevel cycleGenerationLevel" in request_state,
        "generation depth write owner missing")
require("GroovePuterState::cycleGenerationLevel()" in feel,
        "plain P on FEEL must use canonical depth owner")
require("GroovePuterState::cycleGenerationLevel()" in genre,
        "plain P on GENRE must use canonical depth owner")

# P0-10: UI must not own harmonic WHAT/WHEN semantics.
ui_surface = "\n".join((genre, feel, phrase_cpp, pattern_h, synth))
for forbidden in (
    "chordProgressionEventAt(",
    "preparePhraseHarmonicClockProjection(",
    "makePhraseHarmonicTimeline(",
    "resolveGenerationCompositionForPhraseBars(",
):
    require(forbidden not in ui_surface,
            f"UI directly calls frozen musical policy: {forbidden}")

# P0-11: no invented Activity/VariationProfile owner is present on product generation surfaces.
for forbidden in ("ActivityLevel", "VariationProfile", '"ACTIVITY"'):
    require(forbidden not in "\n".join((genre, feel, phrase_cpp)),
            f"fake activity owner found: {forbidden}")

# P0-12: full semantic result remains caller-owned/transient; do not fake a persistent Phrase dashboard.
require("PhraseSemanticResult semantic{};" in phrase_exec,
        "P1R semantic carrier missing from prepared execution")
require("PhraseHarmonicTimeline harmonicTimeline{};" in semantic,
        "semantic harmonic timeline missing")
prepared_arrangement = between(generated, "struct PreparedPhraseArrangement", "struct GeneratedPhraseUndoPayload")
require("GeneratedPhraseP1R::PreparationEvidence p1r{};" in prepared_arrangement,
        "I1 compact P1R evidence missing")
require("PhraseSemanticResult" not in prepared_arrangement,
        "P0 exposure gap changed: full semantic result is now retained; audit must be revisited")
require("phraseGenerationIdentity" in p1r and "ProgressionId progression" in p1r and
        "harmonicEventPositions" in p1r,
        "expected compact I1 semantic evidence missing")

# P0-13: current G scopes are three real, distinct existing command paths; P5 must not add a fourth.
require("regenerateWithQuantizedCommit(" in genre,
        "GENRE G must retain canonical quantized generation boundary")
require("GeneratedPhraseSong::generate(" in phrase_cpp,
        "PHRASE G must retain generated phrase publication boundary")
require("mini_acid_.modeManager().generatePattern(" in synth,
        "Synth NOTES local G characterization changed")

# P0-14: physical storage capacity is 16 pages; minimum eight-page UX acceptance must not shrink it.
require(re.search(r"static constexpr int kMaxPages\s*=\s*16;", scenes) is not None,
        "physical page capacity changed; revisit UI-P0 assumptions")
require("songPatternFromPageBankIndex" in scenes and "songPatternPage" in scenes and
        "songPatternBank" in scenes and "songPatternIndexInBank" in scenes,
        "page+bank+slot physical identity helpers missing")

print("UI-P0 ownership/source characterization: OK")
print("- authoritative production base assumptions: I1")
print("- follow/STOP physical editor path: characterized")
print("- Phrase Length vs FEEL cycle: separated")
print("- product phrase-length request owner: intentionally NOT READY at I1")
print("- persistent semantic Phrase dashboard: intentionally NOT READY at I1")
print("- lifetime UI: blocked pending corrected C2/R1 hardware freeze")
