#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def replace_between(text: str, start: str, end: str, replacement: str, label: str) -> str:
    begin = text.find(start)
    if begin < 0:
        raise RuntimeError(f"{label}: missing start anchor {start!r}")
    finish = text.find(end, begin + len(start))
    if finish < 0:
        raise RuntimeError(f"{label}: missing end anchor {end!r}")
    return text[:begin] + replacement + text[finish:]


# Song: retain the large implementation in-place, rename only its old public
# handler and append the narrow R4 owner implementation.
path = "src/ui/pages/song_page.cpp"
text = read(path)
text = replace_once(
    text,
    "bool SongPage::handleEvent(UIEvent& ui_event) {",
    "bool SongPage::handleEventLegacyUnowned(UIEvent& ui_event) {",
    "song legacy handler rename",
)
if '#include "song_page_r4_owner.inc"' in text:
    raise RuntimeError("song owner include already present")
text = text.rstrip() + '\n\n#include "song_page_r4_owner.inc"\n'
write(path, text)

# Pattern editor: add a canonical Song helper and route chaining through it while
# keeping plain pattern selection runtime-only.
path = "src/ui/pages/pattern_edit_page.h"
text = read(path)
text = replace_once(
    text,
    '#include "src/state/synth_pattern_edit.h"\n',
    '#include "src/state/synth_pattern_edit.h"\n#include "../../state/song_edit.h"\n',
    "pattern song_edit include",
)
marker = "  // Audio exclusion and persistent-revision ownership are deliberately\n"
song_helper = '''  template <typename PrepareFn>\n  bool commitSongMutation(PrepareFn&& prepare) {\n    using GroovePuterUndo::SongUndoPayload;\n    using GroovePuterUndo::UndoKind;\n    SceneManager& manager = mini_acid_.sceneManager();\n    SongUndoPayload before{};\n    if (!GroovePuterUndo::captureCurrentSongUndo(manager, before)) return false;\n    Song after = before.before;\n    std::forward<PrepareFn>(prepare)(after);\n    if (GroovePuterUndo::sameSong(before.before, after)) return false;\n    if (!GroovePuterUndo::songUndoTargetAvailable(manager, before)) return false;\n    return GroovePuterUndo::undoOwner().commitPrepared(\n        UndoKind::Song, before, [&]() {\n          const auto apply = [&]() {\n            manager.currentScene().songs[before.songSlot] = after;\n          };\n          if (audio_guard_) audio_guard_(apply);\n          else apply();\n        });\n  }\n\n'''
text = replace_once(text, marker, song_helper + marker, "pattern Song owner helper")
write(path, text)

path = "src/ui/pages/pattern_edit_page.cpp"
text = read(path)
start = "  // Q-I is the only keyboard path for slots 1-8 outside NOTE ENTRY."
end = "  // Bank selection has one unambiguous binding."
replacement = '''  // Q-I is the only keyboard path for slots 1-8 outside NOTE ENTRY. Selection\n  // is runtime-only; optional chaining is a separate persistent Song edit.\n  if (!note_entry_mode_ && !ui_event.ctrl && !ui_event.meta && !ui_event.alt) {\n    int patternIdx = patternIndexFromKey(lowerKey);\n    if (patternIdx < 0) {\n      patternIdx = scancodeToPatternIndex(ui_event.scancode);\n    }\n    if (patternIdx >= 0) {\n      if (mini_acid_.songModeEnabled()) return true;\n      setPatternCursor(patternIdx);\n      withAudioGuard([&]() {\n        mini_acid_.set303PatternIndex(voice_index_, patternIdx);\n      });\n      if (chaining_mode_) {\n        const SongTrack track = voice_index_ == 0\n            ? SongTrack::SynthA\n            : SongTrack::SynthB;\n        commitSongMutation([&](Song& song) {\n          for (int row = 0; row < Song::kMaxPositions; ++row) {\n            if (GroovePuterUndo::SongEdit::patternAt(song, row, track) == -1) {\n              GroovePuterUndo::SongEdit::setPattern(song, row, track, patternIdx);\n              break;\n            }\n          }\n        });\n      }\n      focus_ = Focus::Steps;\n      return true;\n    }\n  }\n\n'''
text = replace_between(text, start, end, replacement, "pattern chaining")
write(path, text)

# Drum Q-I: plain selector is runtime-only; chaining publishes one fixed Song
# receipt through the canonical owner. Do not broaden Drum ownership in R4.
path = "src/ui/pages/drum_sequencer_page.cpp"
text = read(path)
text = replace_once(
    text,
    '#include "src/state/scene_revision.h"\n',
    '#include "src/state/scene_revision.h"\n#include "src/state/song_edit.h"\n#include "src/state/undo_owner.h"\n#include "src/state/undo_receipts.h"\n',
    "drum Song owner includes",
)
start = "  // Q-I changes the active slot but never hands keyboard focus to the selector."
end = "  if (ui_event.ctrl && !ui_event.alt && !ui_event.meta &&\n      (key == '1' || key == '2')) {"
replacement = '''  // Q-I changes the active Drum slot as runtime state. Optional chaining is a\n  // separate persistent Song mutation owned by the canonical UndoOwner.\n  if (!ui_event.ctrl && !ui_event.meta && !ui_event.alt) {\n    int patternIdx = page->patternIndexFromKey(lowerKey);\n    if (patternIdx < 0) {\n      patternIdx = scancodeToPatternIndex(ui_event.scancode);\n    }\n    if (patternIdx >= 0) {\n      if (page->mini_acid_.songModeEnabled()) return true;\n      page->setDrumPatternCursor(patternIdx);\n      page->focusGrid();\n      const auto selectPattern = [&]() {\n        page->mini_acid_.setDrumPatternIndex(patternIdx);\n      };\n      if (page->audio_guard_) page->audio_guard_(selectPattern);\n      else selectPattern();\n\n      if (page->chaining_mode_) {\n        SceneManager& manager = page->mini_acid_.sceneManager();\n        GroovePuterUndo::SongUndoPayload before{};\n        if (GroovePuterUndo::captureCurrentSongUndo(manager, before)) {\n          Song after = before.before;\n          for (int row = 0; row < Song::kMaxPositions; ++row) {\n            if (GroovePuterUndo::SongEdit::patternAt(\n                    after, row, SongTrack::Drums) == -1) {\n              GroovePuterUndo::SongEdit::setPattern(\n                  after, row, SongTrack::Drums, patternIdx);\n              break;\n            }\n          }\n          if (!GroovePuterUndo::sameSong(before.before, after) &&\n              GroovePuterUndo::songUndoTargetAvailable(manager, before)) {\n            GroovePuterUndo::undoOwner().commitPrepared(\n                GroovePuterUndo::UndoKind::Song, before, [&]() {\n                  const auto apply = [&]() {\n                    manager.currentScene().songs[before.songSlot] = after;\n                  };\n                  if (page->audio_guard_) page->audio_guard_(apply);\n                  else apply();\n                });\n          }\n        }\n      }\n      return true;\n    }\n  }\n\n'''
text = replace_between(text, start, end, replacement, "drum chaining")
write(path, text)

# Phrase: current capture/derive/clear prepare a detached PhraseBank; W prepares
# a detached Song. Generated Phrase -> Song is deliberately untouched.
path = "src/ui/pages/phrase_page.cpp"
text = read(path)

capture = '''bool PhrasePage::captureCurrentRegion() {\n  const Scene& scene = mini_acid_.sceneManager().currentScene();\n  PhraseWorkspace::CaptureRequest request{};\n  request.targetSlot = selected_slot_;\n  request.sourceSongSlot =\n      static_cast<uint8_t>(std::clamp(scene.activeSongSlot, 0, 1));\n  request.startRow = static_cast<uint8_t>(\n      std::clamp(mini_acid_.currentSongPosition(), 0,\n                 Song::kMaxPositions - 1));\n  request.lengthBars = capture_length_;\n  request.role = capture_role_;\n  request.source = PhraseCore::Source::InternalPattern;\n  request.trackMask = PhraseCore::kAllTracks;\n\n  const PhraseCore::Result result = commitPhraseMutation(\n      [&](PhraseCore::PhraseBank& preparedBank) {\n        return PhraseWorkspace::capturePrepared(scene, request, preparedBank);\n      });\n  if (result) {\n    preview_bar_ = 0;\n    const int nextRow = std::min(\n        Song::kMaxPositions - 1,\n        static_cast<int>(request.startRow) + static_cast<int>(request.lengthBars));\n    destination_row_ = static_cast<uint8_t>(nextRow);\n    invalidatePreview();\n  }\n  showResult("CAPTURED", result);\n  return true;\n}\n\n'''
text = replace_between(text, "bool PhrasePage::captureCurrentRegion() {", "bool PhrasePage::generatePhraseToSong() {", capture, "phrase capture")

derive = '''bool PhrasePage::deriveFromParent() {\n  PhraseWorkspace::DeriveRequest request{};\n  request.targetSlot = selected_slot_;\n  request.parentSlot = parent_slot_;\n  request.role = capture_role_;\n  const PhraseCore::Result result = commitPhraseMutation(\n      [&](PhraseCore::PhraseBank& preparedBank) {\n        return PhraseWorkspace::derivePrepared(request, preparedBank);\n      });\n  if (result) {\n    preview_bar_ = 0;\n    invalidatePreview();\n  }\n  showResult("DERIVED", result);\n  return true;\n}\n\n'''
text = replace_between(text, "bool PhrasePage::deriveFromParent() {", "bool PhrasePage::writeToCurrentRow(bool overwrite) {", derive, "phrase derive")

write_song = '''bool PhrasePage::writeToCurrentRow(bool overwrite) {\n  const Scene& scene = mini_acid_.sceneManager().currentScene();\n  const PhraseCore::SlotSummary source =\n      PhraseWorkspace::summary(scene, selected_slot_);\n  PhraseWorkspace::WriteRequest request{};\n  request.sourceSlot = selected_slot_;\n  request.destinationSongSlot =\n      static_cast<uint8_t>(std::clamp(scene.activeSongSlot, 0, 1));\n  request.startRow = destination_row_;\n  request.overwrite = overwrite;\n\n  const PhraseCore::Result result = commitSongMutation(\n      [&](Song& preparedSong) {\n        return PhraseWorkspace::writeToSongPrepared(\n            scene.phraseBank, request, preparedSong);\n      });\n  if (result) {\n    if (!overwrite && source.valid) {\n      const int nextRow = std::min(\n          Song::kMaxPositions - 1,\n          static_cast<int>(request.startRow) + static_cast<int>(source.lengthBars));\n      destination_row_ = static_cast<uint8_t>(nextRow);\n    }\n    invalidatePreview();\n  }\n  showResult(overwrite ? "REPLACED" : "INSERTED", result);\n  return true;\n}\n\n'''
text = replace_between(text, "bool PhrasePage::writeToCurrentRow(bool overwrite) {", "bool PhrasePage::clearCurrentSlot() {", write_song, "phrase write")

clear = '''bool PhrasePage::clearCurrentSlot() {\n  const PhraseCore::Result result = commitPhraseMutation(\n      [&](PhraseCore::PhraseBank& preparedBank) {\n        return PhraseWorkspace::clearPrepared(selected_slot_, preparedBank);\n      });\n  if (result) {\n    preview_bar_ = 0;\n    invalidatePreview();\n  }\n  showResult("CLEAR", result);\n  return true;\n}\n\nbool PhrasePage::undoPreparedOwnedState() {\n  using GroovePuterUndo::PhraseUndoPayload;\n  using GroovePuterUndo::SongUndoPayload;\n  using GroovePuterUndo::UndoKind;\n  using GroovePuterUndo::UndoResult;\n\n  auto& owner = GroovePuterUndo::undoOwner();\n  if (!owner.hasUndo()) return false;\n\n  if (owner.kind() == UndoKind::Phrase) {\n    const UndoResult result = owner.undoPrepared<PhraseUndoPayload>(\n        UndoKind::Phrase,\n        [&](const PhraseUndoPayload& receipt) {\n          return GroovePuterUndo::phraseUndoTargetAvailable(\n              mini_acid_.sceneManager(), receipt);\n        },\n        [&](const PhraseUndoPayload& receipt) {\n          const auto restore = [&]() {\n            GroovePuterUndo::restorePhraseUndo(\n                mini_acid_.sceneManager(), receipt);\n          };\n          if (audio_guard_) audio_guard_(restore);\n          else restore();\n        });\n    if (result == UndoResult::Restored) {\n      invalidatePreview();\n      UI::showToast("Undo Phrase", 900);\n      return true;\n    }\n    if (result == UndoResult::TargetUnavailable) {\n      UI::showToast("Undo target unavailable", 1100);\n      return true;\n    }\n    return result == UndoResult::Expired;\n  }\n\n  if (owner.kind() == UndoKind::Song) {\n    const UndoResult result = owner.undoPrepared<SongUndoPayload>(\n        UndoKind::Song,\n        [&](const SongUndoPayload& receipt) {\n          return GroovePuterUndo::songUndoTargetAvailable(\n              mini_acid_.sceneManager(), receipt);\n        },\n        [&](const SongUndoPayload& receipt) {\n          const auto restore = [&]() {\n            GroovePuterUndo::restoreSongUndo(\n                mini_acid_.sceneManager(), receipt);\n          };\n          if (audio_guard_) audio_guard_(restore);\n          else restore();\n        });\n    if (result == UndoResult::Restored) {\n      invalidatePreview();\n      UI::showToast("Undo Song", 900);\n      return true;\n    }\n    if (result == UndoResult::TargetUnavailable) {\n      UI::showToast("Undo target unavailable", 1100);\n      return true;\n    }\n    return result == UndoResult::Expired;\n  }\n\n  return false;\n}\n\n'''
text = replace_between(text, "bool PhrasePage::clearCurrentSlot() {", "void PhrasePage::draw(IGfx& gfx) {", clear, "phrase clear/undo")

text = replace_once(
    text,
    "bool PhrasePage::handleEvent(UIEvent& ui_event) {\n  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;",
    "bool PhrasePage::handleEvent(UIEvent& ui_event) {\n  if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT &&\n      ui_event.app_event_type == GROOVEPUTER_APP_EVENT_UNDO) {\n    return undoPreparedOwnedState();\n  }\n  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;",
    "phrase app Undo",
)
write(path, text)

# R3 source test: Song chaining now validates the stronger canonical R4 owner.
path = "tests/test_pattern_mutations_0_9_8_r3_source_regressions.py"
text = read(path)
old = '''    require("songMutated = true" in selector and\n            "if (songMutated) GroovePuterState::markSceneMutated();" in selector,\n            "Song chaining must keep persistent revision ownership separate from selector navigation")'''
new = '''    require("commitSongMutation" in selector and\n            "SongEdit::setPattern" in selector and\n            "markSceneMutated" not in selector,\n            "Song chaining must route through canonical R4 Song ownership while selector navigation stays runtime-only")'''
text = replace_once(text, old, new, "R3 chaining regression")
write(path, text)

# General revision regression: Song arrangement now reaches the revision tracker
# through UndoOwner; mode/position are TIME and their audio guard must be clean.
path = "tests/test_scene_revision_source_regressions.py"
text = read(path)
text = replace_once(
    text,
    '    song_source = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")\n',
    '    song_source = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")\n    song_owner = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text(encoding="utf-8")\n',
    "scene regression song owner read",
)
old = '''    # GENERATION no longer owns a standalone page. Song is the materialization\n    # owner, and its persistent guard marks every successful mutation path.\n    require("SongPatternMaterializer::Result materializeSongTracks" in song_header and\n            "bool generateCurrentCellPattern" in song_header and\n            "bool generateEntireRow" in song_header,\n            "Song must own current generation/materialization entry points")\n    persistent_guard = song_header.index("void withAudioGuard")\n    runtime_guard = song_header.index("void withRuntimeAudioGuard", persistent_guard)\n    require("GroovePuterState::markSceneMutated();" in\n            song_header[persistent_guard:runtime_guard],\n            "Song persistent mutations, including generation, must reach the revision tracker")\n    require("generateCurrentCellPattern();" in song_source and\n            "generateEntireRow();" in song_source,\n            "Song generation gestures must route through the current materialization owner")'''
new = '''    # R4 separates committed Song arrangement from transport/TIME. Arrangement\n    # mutations publish through UndoOwner; audio exclusion alone is runtime-only.\n    require("SongPatternMaterializer::Result materializeSongTracks" in song_header and\n            "bool generateCurrentCellPattern" in song_header and\n            "bool generateEntireRow" in song_header,\n            "Song must retain current generation/materialization entry points")\n    song_commit_start = song_header.index("template <typename PrepareFn>")\n    song_guard_start = song_header.index("template <typename F>", song_commit_start)\n    song_commit = song_header[song_commit_start:song_guard_start]\n    runtime_guard = song_header.index("void withRuntimeAudioGuard", song_guard_start)\n    song_guard = song_header[song_guard_start:runtime_guard]\n    require("undoOwner().commitPrepared" in song_commit and\n            "UndoKind::Song" in song_commit and\n            "markSceneMutated" not in song_commit and\n            "GroovePuterState::markSceneMutated();" in owner_commit,\n            "Song arrangement mutations must reach revision through canonical Undo ownership")\n    require("markSceneMutated" not in song_guard,\n            "Song audio guard must remain runtime-only after R4")\n    require("commitSongMutation" in song_owner and\n            "handleEventLegacyUnowned" in song_owner,\n            "Song R4 owner wrapper must separate persistent edits from retained runtime routing")\n    require("generateCurrentCellPattern();" in song_source and\n            "generateEntireRow();" in song_source,\n            "Song generation gestures must retain their materialization owner")'''
text = replace_once(text, old, new, "scene regression Song ownership")
old = '''    require("withAudioGuard([&]() { mini_acid_.toggleSongMode();" in song_source,\n            "persisted Song mode must remain a tracked mutation")\n    require("withAudioGuard([&]() { mini_acid_.setSongPosition(next);" in song_source,\n            "persisted Song position must remain a tracked mutation")'''
new = '''    require("withAudioGuard([&]() { mini_acid_.toggleSongMode();" in song_source,\n            "Song mode must retain its audio-safe runtime route")\n    require("withAudioGuard([&]() { mini_acid_.setSongPosition(next);" in song_source,\n            "Song position must retain its audio-safe TIME route")'''
text = replace_once(text, old, new, "scene regression TIME semantics")
write(path, text)

print("R4 surgical source patch applied")
