#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated function: {signature}")


def main() -> None:
    song_page = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
    song_r4_owner = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text(encoding="utf-8")
    phrase_page = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")
    phrase_header = (ROOT / "src/ui/pages/phrase_page.h").read_text(encoding="utf-8")
    phrase_generator = (ROOT / "src/dsp/phrase_generator.h").read_text(encoding="utf-8")
    generated_phrase = (ROOT / "src/dsp/generated_phrase_song.h").read_text(encoding="utf-8")
    materializer = (ROOT / "src/dsp/song_pattern_materializer.h").read_text(encoding="utf-8")

    cell = function_body(
        song_page, "bool SongPage::generateCurrentCellPattern(")
    rollback = function_body(
        song_page, "bool SongPage::rollbackPendingCellGeneration(")
    row = function_body(song_page, "bool SongPage::generateEntireRow()")
    adapter = function_body(
        song_page,
        "SongPatternMaterializer::Result SongPage::materializeSongTracks")
    generated_action = function_body(
        phrase_page, "bool PhrasePage::generatePhraseToSong()")

    require("qwertyToPatternIndex(key)" in song_page,
            "Q..I assignment no longer uses the canonical pattern mapping")
    require("return assignPattern(patternIdx);" in song_page,
            "Q..I no longer assigns existing Song references")
    require("GROOVEPUTER_APP_EVENT_COPY" in song_page,
            "Song copy path disappeared")
    require("GROOVEPUTER_APP_EVENT_PASTE" in song_r4_owner and
            "GROOVEPUTER_APP_EVENT_UNDO" in song_r4_owner and
            "undoPreparedSongState" in song_r4_owner,
            "Song paste/undo no longer route through the canonical R4 owner")
    require("UndoActionType::Paste" not in song_page and
            "UndoActionType::Delete" not in song_page,
            "legacy Song page-local Undo ownership returned")

    # 0.9.9-D3 separates edit selection from audible playback ownership. EDIT
    # A/B still uses setActiveSongSlot(), while Ctrl+B must request a bounded
    # row-boundary PLAY switch instead of mutating songPlaybackSlot immediately.
    require("setActiveSongSlot(nextSlot)" in song_page,
            "Song EDIT A/B switching disappeared")
    ctrl_b_start = song_page.index("if (ui_event.ctrl && key_b)")
    ctrl_b_end = song_page.index(
        "if (ui_event.alt && !ui_event.ctrl && key_b)", ctrl_b_start)
    ctrl_b = song_page[ctrl_b_start:ctrl_b_end]
    require("requestSongPlaybackSwitch" in ctrl_b and
            "setSongPlaybackSlot(nextPlaySlot)" not in ctrl_b,
            "D3 PLAY A/B switching must use bounded row-boundary activation")

    require("materializeSongTracks(row, trackMask)" in cell,
            "single G no longer materializes the selected track")
    require("kEditableTrackMask" in row,
            "double G no longer materializes all editable row tracks")
    require("GEN %s -> %s" in cell,
            "single-cell generation toast lost destination reference")
    require("GENERATED ROW %d" in row,
            "row generation toast is missing")
    require("NO EMPTY PATTERN SLOTS" in cell and
            "NO EMPTY PATTERN SLOTS" in row,
            "no-free-slot error is not surfaced")

    require("rollbackPendingCellGeneration(cursorRow())" in song_page,
            "double G does not roll back its provisional single-cell commit")
    require("restoreSceneRevision" in rollback and
            "oldReference" in rollback and "oldSongLength" in rollback,
            "double-G rollback does not restore data and dirty revision")
    require("referenceCount != 1" in rollback,
            "double-G rollback may clear a destination shared after generation")

    combined = adapter + cell + rollback + row
    require(re.search(r"\b(?:s?rand)\s*\(", combined) is None,
            "Song generation path still uses rand()/srand()")
    require("GrooveboxModeManager generator(mini_acid_)" in adapter,
            "Song generation bypasses the production generator")
    require("setModeLocal" in adapter and "setFlavorLocal" in adapter,
            "Song generation does not inherit current mode/flavor")
    require("getCompiledGenerativeParams" in adapter and
            "getBehavior" in adapter,
            "Song generation does not use current genre constraints")
    require("withRuntimeAudioGuard" in adapter,
            "Song materialization commit is not protected by the audio guard")

    require("globalPatternIsReferenced" in materializer,
            "copy-on-write does not inspect Song references")
    require("slotContentIsEmpty" in materializer,
            "free-slot allocation does not inspect destination content")
    require("PreparedMaterial prepared{}" in materializer,
            "row generation lost its fixed-size preparation buffer")
    require(materializer.index("commitPrepared([&]()") >
            materializer.index("generateTrack("),
            "materializer writes before all generation is prepared")
    require(materializer.count("markSceneMutated()") == 1,
            "materializer must own exactly one successful dirty mutation")
    require("Midi" not in materializer and "Transport" not in materializer and
            "TinyUSB" not in materializer,
            "Song materializer acquired transport ownership")

    require("bool generatePhraseToSong();" in phrase_header,
            "Phrase page lost the generated Phrase -> Song action")
    require("!ui_event.ctrl && !ui_event.alt && !ui_event.meta && lower == 'g'" in phrase_page,
            "Phrase page plain G no longer owns generated 1/2/4/8B materialization")
    # UI-P2 gives generated Phrase G its own dedicated request-bars owner,
    # separate from the legacy PhraseCore capture_length_ (never conflate the
    # two "phrase length" concepts).
    require("GroovePuterState::requestedPhraseBars()" in generated_action and
            "GeneratedPhraseSong::generate" in generated_action,
            "Phrase G does not use the dedicated Phrase request length and current adapter")
    # PHW-P1: PHRASE (product) and PHRASE CORE are separate page instances
    # sharing generatePhraseToSong(). CORE keeps the exact pre-PHW-P1
    # destination_row_ target; product resolves TO fresh via
    # resolvedToRow() (APPEND/EXPLICIT), never a cached destination.
    require("? static_cast<int>(destination_row_)\n      : resolvedToRow();"
            in generated_action,
            "Phrase G no longer starts at the visible TO destination")
    require("currentSongPosition()" not in generated_action,
            "Phrase G must not use a hidden Song playhead as its destination")
    require("static_cast<int>(songStart) + phraseResult.bars" in generated_action,
            "successful Phrase G no longer advances the visible TO destination")

    # 0.9.9-D2 intentionally removes the old STOP-only Phrase guard. The UI now
    # consumes an explicit lifecycle result and reports pending next-bar activation.
    require("GeneratedPhraseSong::Result" in generated_action and
            "LifecycleStatus::PendingNextBar" in generated_action,
            "Phrase G no longer exposes the D2 live lifecycle")
    require("STOP PLAYBACK FOR PHRASE" not in generated_action,
            "D2 Phrase G regressed to the old STOP-only rejection")
    require("mini_acid_.stop()" not in generated_action and
            "mini_acid_.start()" not in generated_action,
            "Phrase generation must never hide a stop/generate/restart cycle")
    require("G:GEN ENT/D/W" in phrase_page,
            "Phrase Core footer does not expose G together with its command legend")
    require("ui_event.ctrl && !ui_event.alt && !ui_event.meta" in phrase_page,
            "Phrase page must still expose the Ctrl+arrow TO/8-bar destination controls")

    # D2 moved the multi-object Phrase mutation out of PhraseGenerator's direct
    # Scene-writing helper. All bars are staged first, then one canonical UndoOwner
    # COMMIT publishes Pattern bytes + Song refs, and PLAY arms the existing C owner.
    require("PreparedPhraseArrangement" in generated_phrase and
            "applyPreparedPersistent" in generated_phrase,
            "D2 generated Phrase lost bounded PREPARE/persistent apply separation")
    require("GroovePuterUndo::undoOwner().commitPrepared" in generated_phrase,
            "D2 generated Phrase bypasses the canonical one-level Undo owner")
    require("acquireWriteLease()" in generated_phrase and
            "completePhraseActivation" in generated_phrase,
            "D2 generated Phrase bypasses the existing bounded C pending owner")
    require("markSceneMutated()" not in generated_phrase,
            "D2 generated Phrase reintroduced a second dirty/revision path")
    require("AtlasRuntime::applyRecipe" in generated_phrase and
            "atlasVariationForRole" in generated_phrase,
            "Atlas Phrase generation lost P1/P2/P1/P3 role mapping")
    require("migrateStrongRhythmMaterial" in generated_phrase and
            "tonalMaterializationEnabled = true" in generated_phrase,
            "generated phrases bypass current strong-rhythm/Stage15 tonal migration")
    require("PhraseGenerator::deriveBar" in generated_phrase,
            "procedural phrase bars are independently regenerated instead of related")
    require(re.search(r"\b(?:s?rand)\s*\(", generated_phrase) is None,
            "generated Phrase path must use deterministic seed state, not rand()/srand()")

    require("songRowsAreAvailable" in phrase_generator,
            "fresh Phrase G must preflight an empty destination range")
    require("globalPatternIsReferenced" in phrase_generator and
            "localSlotIsSafeForPhrase" in phrase_generator and
            "findSafeContiguousEmptySlots" in phrase_generator,
            "Phrase allocator can overwrite empty-looking slots still referenced by Song")
    require("findSafeContiguousEmptySlots(\n      scene, request.pageIndex, request.bars)" in phrase_generator,
            "Phrase commit path is not using the reference-safe contiguous allocator")
    require("destination.positions[row + phraseBars]" not in phrase_generator,
            "fresh Phrase G must not silently inherit saved-Phrase INSERT row shifting")

    print("Song generation source regressions passed")


if __name__ == "__main__":
    main()
