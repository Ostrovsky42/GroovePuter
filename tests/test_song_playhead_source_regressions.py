#!/usr/bin/env python3
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_block(source: str, begin: str, end: str) -> str:
    start = source.index(begin)
    finish = source.index(end, start)
    return source[start:finish]


def main() -> None:
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
    song_page = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
    song_header = (ROOT / "src/ui/pages/song_page.h").read_text(encoding="utf-8")
    help_frames = (ROOT / "src/ui/help_dialog_frames.h").read_text(encoding="utf-8")
    key_docs = (ROOT / "src/ui/docs/keys.md").read_text(encoding="utf-8")

    require('#include "song_cycle_boundary.h"' in engine,
  "engine must use the tested bar-boundary helper")
    require("songStepCounter_" not in engine and "songStepCounter_" not in header,
  "legacy sixteenth-step Song counter must be removed")
    require("int songBarIndex_ = -1;" in header,
  "Song cycle must have an explicit pre-first-bar phase")

    cycle = function_block(engine, "int MiniAcid::cycleBarIndex() const",
                           "int16_t MiniAcid::currentDrumPatternIndex()")
    require("songBarIndex_" in cycle and "SEQ_STEPS" not in cycle,
  "UI cycle bar must expose bars directly")

    advance = engine[engine.index("void MiniAcid::advanceSongBar_()") :]
    require("nextSongCycleBoundary(" in advance,
  "bar callback must use the tested helper")
    require("if (boundary.advanceRow)" in advance,
  "Song row may advance only at the configured bar boundary")
    require("SEQ_STEPS *" not in advance,
  "bar callback must not wait for sixteenth-note counts")

    start = function_block(engine, "void MiniAcid::start()", "void MiniAcid::stop()")
    stop = function_block(engine, "void MiniAcid::stop()", "void MiniAcid::liveNoteOn")
    mode = function_block(engine, "void MiniAcid::setSongMode(bool enabled)",
                          "void MiniAcid::toggleSongMode()")
    position = function_block(engine, "void MiniAcid::setSongPosition(int position)",
                              "void MiniAcid::setSongPattern")
    scene = function_block(engine, "void MiniAcid::applySceneStateFromManager()",
                           "void MiniAcid::applyTextureFromScene_()")
    for name, block in (("start", start), ("stop", stop), ("mode", mode),
              ("position", position), ("scene", scene)):
        require("songBarIndex_ = -1;" in block,
      f"{name} lifecycle must restart the Song row cycle")

    require("if (songMode_)" in advance,
  "Pattern mode must never advance the Song playhead")
    require("isSongReverseAtSlot" in engine and "loopMode()" in engine,
  "reverse and loop traversal must remain intact")
    require("rowIsPause && !rehearsalAcknowledged_" in engine,
  "rehearsal pause rows must remain intact")

    cursor_block = function_block(song_page, "int SongPage::maxEditableTrackColumn() const",
                                  "int SongPage::maxPatternTrackColumn() const")
    require("return maxPatternTrackColumn();" in cursor_block,
  "Song keyboard cursor must stay on musical A/B/DR columns")
    require("return false;" in function_block(song_page,
  "bool SongPage::cursorOnModeButton() const", "bool SongPage::cursorOnPlayheadLabel() const"),
  "MODE must not remain a hidden keyboard column")
    horizontal = function_block(song_page, "void SongPage::moveCursorHorizontal",
                                "void SongPage::moveCursorVertical")
    require("setActiveSongSlot" in horizontal and "EDIT SLOT" in horizontal,
  "plain horizontal navigation must cross edit Song Slot boundaries explicitly")
    require("++assignment_bank_index_" not in horizontal and "--assignment_bank_index_" not in horizontal,
  "horizontal arrows must never mutate PAT assignment bank")
    require("nextSlot < kSongSlotCount" in horizontal and "nextSlot >= 0" in horizontal,
  "Song Slot boundary traversal must clamp instead of wrap")
    assign = function_block(song_page, "bool SongPage::assignPattern",
                            "bool SongPage::clearPattern")
    require(assign.count("assignment_bank_index_") >= 2,
  "Q..I assignment must use the visible Song pattern-bank context")
    require("int assignment_bank_index_ = 0;" in song_header,
  "Song pattern-bank context must be explicit UI state")
    require("activePlayRow" in song_page and "const bool activePlay" in song_page,
  "Song styles must expose a dedicated active-playhead row state")
    require('">%d"' in song_page,
  "active Song playhead must have an explicit row marker")
    require("PAT:%c" in song_page,
  "Song header/status must expose an unambiguous assignment bank")
    require(song_page.count("Final playhead overlay") >= 4,
  "all Song styles must redraw the playhead outline after pattern cells")
    require("textWidth(gfx, stateBuf) + 5" in song_page and "maxBarX" in song_page,
  "Minimal Song header must place bar status after dynamic E/P/PAT text without overlap")
    require(song_page.count("const int barW = 24;") >= 3 and "const int pos_w = 24;" in song_page,
  "Song row gutters must reserve 24px for four 6px glyphs in >128")
    require("track / edit slot" in help_frames and "track / PAT bank" not in help_frames and
            "col / mode focus" not in help_frames,
  "on-device Song help must expose track/edit-slot horizontal navigation")
    require('"B", "PAT bank A/B"' in help_frames and
            '"B", "PAT assignment bank A/B"' in help_frames and
            '"ALT+B", "flip stored ref bank"' in help_frames,
  "on-device Song help must separate PAT assignment from stored-reference flip")
    require('"B", "flip pattern bank A/B"' not in help_frames,
  "stale plain-B stored-reference flip must not remain in Song help")
    require('"CTRL+M", "Delete row"' in help_frames and
            '"CTRL+N", "Insert row"' in help_frames and
            '"ALT+M", "Toggle song mode"' in help_frames,
  "on-device Song help must match current row and Song-mode runtime keys")
    require("Merge A+B into one" not in help_frames and "Alternate A/B patterns" not in help_frames,
  "disabled legacy Song operations must not remain in on-device help")
    require("crossing the outer edge changes edit Song slot A/B" in key_docs and
            "Toggle visible `PAT:A/B` assignment bank" in key_docs,
  "canonical key map must document Song-slot boundaries and independent PAT bank")

    print("Song playhead source regressions: OK")

    # This file is already part of the canonical host runner. Keep the stacked
    # completion source gate in that runner without adding a second shell-test
    # entry or a parallel validation path.
    runpy.run_path(
        str(ROOT / "tests/test_smf_panel_completion_source_regressions.py"),
        run_name="__main__",
    )


if __name__ == "__main__":
    main()
