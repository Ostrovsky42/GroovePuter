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
    materializer = (ROOT / "src/dsp/song_pattern_materializer.h").read_text(encoding="utf-8")

    cell = function_body(
        song_page, "bool SongPage::generateCurrentCellPattern(")
    rollback = function_body(
        song_page, "bool SongPage::rollbackPendingCellGeneration(")
    row = function_body(song_page, "bool SongPage::generateEntireRow()")
    adapter = function_body(
        song_page,
        "SongPatternMaterializer::Result SongPage::materializeSongTracks")

    require("qwertyToPatternIndex(key)" in song_page,
            "Q..I assignment no longer uses the canonical pattern mapping")
    require("return assignPattern(patternIdx);" in song_page,
            "Q..I no longer assigns existing Song references")
    require("GROOVEPUTER_APP_EVENT_COPY" in song_page and
            "GROOVEPUTER_APP_EVENT_PASTE" in song_page and
            "UndoActionType::Paste" in song_page and
            "UndoActionType::Delete" in song_page,
            "Song copy/paste/undo paths disappeared")
    require("setActiveSongSlot(nextSlot)" in song_page and
            "setSongPlaybackSlot(nextPlaySlot)" in song_page,
            "Song A/B edit or playback switching disappeared")

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

    print("Song generation source regressions passed")


if __name__ == "__main__":
    main()
