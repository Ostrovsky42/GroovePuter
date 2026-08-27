#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
H = ROOT / "src/dsp/miniacid_engine.h"
CPP = ROOT / "src/dsp/miniacid_engine.cpp"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected 1 match, found {count}")
    return text.replace(old, new, 1)


def replace_n(text: str, old: str, new: str, expected: int, label: str) -> str:
    count = text.count(old)
    if count != expected:
        raise SystemExit(f"{label}: expected {expected} matches, found {count}")
    return text.replace(old, new)


h = H.read_text(encoding="utf-8")
h = replace_once(
    h,
    "  void releasePatternSynthBHeldNote_(int16_t note);\n"
    "  void invalidatePhraseCrossBarLifetime_();\n",
    "  void releasePatternSynthBHeldNote_(int16_t note,\n"
    "                                       bool publishPatternNoteOff = true);\n"
    "  void clearPhraseCrossBarLifetime_();\n"
    "  void releasePhraseCrossBarLifetime_(\n"
    "      bool predecessorPatternCleanupFollows);\n",
    "miniacid helper declarations",
)
H.write_text(h, encoding="utf-8")

cpp = CPP.read_text(encoding="utf-8")
cpp = replace_once(
    cpp,
    "void MiniAcid::releasePatternSynthBHeldNote_(int16_t note) {\n"
    "  if (note < 0) return;\n"
    "  gateCountdownB_ = 0;\n"
    "  retrigB_ = {};\n"
    "  if (synthVoices_[1]) synthVoices_[1]->release();\n"
    "  publishPatternNoteOff_(1);\n"
    "}\n\n"
    "void MiniAcid::invalidatePhraseCrossBarLifetime_() {\n"
    "  const int16_t heldNote = phraseCrossBarLifetime_.hardBarrierRelease();\n"
    "  releasePatternSynthBHeldNote_(heldNote);\n"
    "}\n",
    "void MiniAcid::releasePatternSynthBHeldNote_(\n"
    "    int16_t note, bool publishPatternNoteOff) {\n"
    "  if (note < 0) return;\n"
    "  gateCountdownB_ = 0;\n"
    "  retrigB_ = {};\n"
    "  if (synthVoices_[1]) synthVoices_[1]->release();\n"
    "  if (publishPatternNoteOff) publishPatternNoteOff_(1);\n"
    "}\n\n"
    "void MiniAcid::clearPhraseCrossBarLifetime_() {\n"
    "  phraseCrossBarLifetime_.hardBarrierRelease();\n"
    "}\n\n"
    "void MiniAcid::releasePhraseCrossBarLifetime_(\n"
    "    bool predecessorPatternCleanupFollows) {\n"
    "  const int16_t heldNote = phraseCrossBarLifetime_.hardBarrierRelease();\n"
    "  if (heldNote < 0) return;\n"
    "  gateCountdownB_ = 0;\n"
    "  retrigB_ = {};\n"
    "  if (synthVoices_[1]) synthVoices_[1]->release();\n"
    "  if (!predecessorPatternCleanupFollows) publishPatternNoteOff_(1);\n"
    "}\n",
    "release/clear helper split",
)

old_calls = cpp.count("invalidatePhraseCrossBarLifetime_();")
if old_calls < 15:
    raise SystemExit(f"unexpected invalidate call count before migration: {old_calls}")
cpp = cpp.replace(
    "invalidatePhraseCrossBarLifetime_();",
    "releasePhraseCrossBarLifetime_(false);",
)
if "invalidatePhraseCrossBarLifetime_" in cpp:
    raise SystemExit("old invalidate helper survived")

# reset/start/stop/pause/continue already have authoritative physical+MIDI cleanup.
cpp = replace_once(
    cpp,
    "  phraseCrossBarLifetime_.reset();\n"
    "  LOG_PRINTLN(\"    - MiniAcid::reset: Start\");",
    "  clearPhraseCrossBarLifetime_();\n"
    "  LOG_PRINTLN(\"    - MiniAcid::reset: Start\");",
    "reset state-only clear",
)
cpp = replace_n(
    cpp,
    "  allLiveNotesOff();\n"
    "  releasePhraseCrossBarLifetime_(false);\n"
    "  publishPatternAllNotesOff_();",
    "  allLiveNotesOff();\n"
    "  clearPhraseCrossBarLifetime_();\n"
    "  publishPatternAllNotesOff_();",
    2,
    "start/continue authoritative cleanup",
)
cpp = replace_once(
    cpp,
    "  LOG_PRINTLN(\"[DSP] STOP command received\");\n"
    "  releasePhraseCrossBarLifetime_(false);\n"
    "  publishPatternAllNotesOff_();",
    "  LOG_PRINTLN(\"[DSP] STOP command received\");\n"
    "  clearPhraseCrossBarLifetime_();\n"
    "  publishPatternAllNotesOff_();",
    "stop authoritative cleanup",
)
cpp = replace_once(
    cpp,
    "  LOG_PRINTLN(\"[DSP] PAUSE command received\");\n"
    "  releasePhraseCrossBarLifetime_(false);\n"
    "  publishPatternAllNotesOff_();",
    "  LOG_PRINTLN(\"[DSP] PAUSE command received\");\n"
    "  clearPhraseCrossBarLifetime_();\n"
    "  publishPatternAllNotesOff_();",
    "pause authoritative cleanup",
)

# Existing PatternPlayer cleanup follows these R1-owned internal releases.
cpp = replace_once(
    cpp,
    "  if (enabled == songMode_) return;\n"
    "  releasePhraseCrossBarLifetime_(false);\n"
    "  songBarIndex_ = -1;\n"
    "  if (playing) publishPatternAllNotesOff_();",
    "  if (enabled == songMode_) return;\n"
    "  releasePhraseCrossBarLifetime_(true);\n"
    "  songBarIndex_ = -1;\n"
    "  if (playing) publishPatternAllNotesOff_();",
    "song-mode replacement cleanup",
)
cpp = replace_once(
    cpp,
    "  if (idx == 1 && muted) releasePhraseCrossBarLifetime_(false);",
    "  if (idx == 1 && muted) releasePhraseCrossBarLifetime_(true);",
    "mute B predecessor MIDI cleanup",
)
for name in ("set303PatternIndex", "shift303PatternIndex", "set303BankIndex"):
    old = (
        f"void MiniAcid::{name}(int voiceIndex, "
        + ("int16_t patternIndex) {\n" if name == "set303PatternIndex" else
           "int delta) {\n" if name == "shift303PatternIndex" else
           "int bankIndex) {\n")
        + "  releasePhraseCrossBarLifetime_(false);\n"
    )
    new = old.replace("releasePhraseCrossBarLifetime_(false)",
                      "releasePhraseCrossBarLifetime_(true)")
    cpp = replace_once(cpp, old, new, f"{name} predecessor MIDI cleanup")

cpp = replace_once(
    cpp,
    "void MiniAcid::applySceneStateFromManager() {\n"
    "  releasePhraseCrossBarLifetime_(false);\n"
    "  if (playing) publishPatternAllNotesOff_();",
    "void MiniAcid::applySceneStateFromManager() {\n"
    "  releasePhraseCrossBarLifetime_(true);\n"
    "  if (playing) publishPatternAllNotesOff_();",
    "scene replacement cleanup",
)

# Seek/jump may be in Song mode (broad cleanup follows) or Pattern mode
# (R1 must own the PatternPlayer NoteOff itself).
cpp = replace_once(
    cpp,
    "void MiniAcid::setSongPosition(int position) {\n"
    "  releasePhraseCrossBarLifetime_(false);",
    "void MiniAcid::setSongPosition(int position) {\n"
    "  releasePhraseCrossBarLifetime_(songMode_ && playing);",
    "seek cleanup ownership",
)

# Song-row edits only get broad cleanup when they replace the active row.
cpp = replace_once(
    cpp,
    "void MiniAcid::setSongPattern(int position, SongTrack track, int16_t patternIndex) {\n"
    "  releasePhraseCrossBarLifetime_(false);\n"
    "  sceneManager_.setSongPattern(position, track, patternIndex);",
    "void MiniAcid::setSongPattern(int position, SongTrack track, int16_t patternIndex) {\n"
    "  const bool activeSelectionCleanup =\n"
    "      songMode_ && playing && position == currentSongPosition() &&\n"
    "      activeSongSlot() == songPlaybackSlot_;\n"
    "  releasePhraseCrossBarLifetime_(activeSelectionCleanup);\n"
    "  sceneManager_.setSongPattern(position, track, patternIndex);",
    "setSongPattern cleanup ownership",
)
cpp = replace_once(
    cpp,
    "void MiniAcid::clearSongPattern(int position, SongTrack track) {\n"
    "  releasePhraseCrossBarLifetime_(false);\n"
    "  sceneManager_.clearSongPattern(position, track);",
    "void MiniAcid::clearSongPattern(int position, SongTrack track) {\n"
    "  const bool activeSelectionCleanup =\n"
    "      songMode_ && playing && position == currentSongPosition() &&\n"
    "      activeSongSlot() == songPlaybackSlot_;\n"
    "  releasePhraseCrossBarLifetime_(activeSelectionCleanup);\n"
    "  sceneManager_.clearSongPattern(position, track);",
    "clearSongPattern cleanup ownership",
)

# A no-op slot setter is not a replacement barrier.
cpp = replace_once(
    cpp,
    "void MiniAcid::setSongPlaybackSlot(int slot) {\n"
    "  releasePhraseCrossBarLifetime_(false);\n"
    "  if (slot < 0) slot = 0;\n"
    "  if (slot > 1) slot = 1;\n"
    "  if (songPlaybackSlot_ == slot) return;\n"
    "  songPlaybackSlot_ = slot;",
    "void MiniAcid::setSongPlaybackSlot(int slot) {\n"
    "  if (slot < 0) slot = 0;\n"
    "  if (slot > 1) slot = 1;\n"
    "  if (songPlaybackSlot_ == slot) return;\n"
    "  releasePhraseCrossBarLifetime_(songMode_ && playing);\n"
    "  songPlaybackSlot_ = slot;",
    "song slot replacement cleanup",
)

# Non-ordinary apply has broad PatternPlayer cleanup only in Song mode.
cpp = replace_once(
    cpp,
    "  if (!songMode_) {\n"
    "    if (!ordinaryPhraseTransition) releasePhraseCrossBarLifetime_(false);\n"
    "    return;\n"
    "  }\n"
    "  if (!ordinaryPhraseTransition) releasePhraseCrossBarLifetime_(false);\n"
    "  if (playing) {",
    "  if (!songMode_) {\n"
    "    if (!ordinaryPhraseTransition) releasePhraseCrossBarLifetime_(false);\n"
    "    return;\n"
    "  }\n"
    "  if (!ordinaryPhraseTransition) releasePhraseCrossBarLifetime_(true);\n"
    "  if (playing) {",
    "applySongPositionSelection cleanup ownership",
)

# Boundary Release is internal-only here; applySongPositionSelection emits the
# one authoritative broad PatternPlayer cleanup after the playhead moves.
cpp = replace_once(
    cpp,
    "      releasePatternSynthBHeldNote_(result.noteToRelease);",
    "      releasePatternSynthBHeldNote_(result.noteToRelease, false);",
    "boundary release before broad cleanup",
)
cpp = replace_once(
    cpp,
    "    } else {\n"
    "      releasePhraseCrossBarLifetime_(false);\n"
    "    }\n"
    "  }\n\n"
    "  // Update SceneManager FIRST so applySongPositionSelection sees new value",
    "    } else {\n"
    "      releasePhraseCrossBarLifetime_(true);\n"
    "    }\n"
    "  }\n\n"
    "  // Update SceneManager FIRST so applySongPositionSelection sees new value",
    "non-sequential transition cleanup ownership",
)

# Guard against accidental duplicate full cleanup on authoritative paths.
for forbidden in (
    "[DSP] STOP command received\");\n  releasePhraseCrossBarLifetime_(",
    "[DSP] PAUSE command received\");\n  releasePhraseCrossBarLifetime_(",
):
    if forbidden in cpp:
        raise SystemExit(f"authoritative transport cleanup still calls explicit R1 release: {forbidden}")

CPP.write_text(cpp, encoding="utf-8")
print("R1 release-parity patch applied")
