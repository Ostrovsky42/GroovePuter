#!/usr/bin/env python3
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

    advance = engine[engine.index("void MiniAcid::advanceSongBar_()"):]
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

    print("Song playhead source regressions: OK")


if __name__ == "__main__":
    main()
