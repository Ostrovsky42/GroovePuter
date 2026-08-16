#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def replace_once(text, old, new, label):
    if text.count(old) != 1:
        raise SystemExit(f"{label}: expected one match, found {text.count(old)}")
    return text.replace(old, new, 1)


def function_span(text, signature):
    start = text.find(signature)
    if start < 0:
        raise SystemExit(f"missing function: {signature}")
    brace = text.find('{', start)
    if brace < 0:
        raise SystemExit(f"missing opening brace: {signature}")
    i = brace
    depth = 0
    state = 'code'
    while i < len(text):
        c = text[i]
        n = text[i + 1] if i + 1 < len(text) else ''
        if state == 'code':
            if c == '/' and n == '/': state = 'line'; i += 2; continue
            if c == '/' and n == '*': state = 'block'; i += 2; continue
            if c == '"': state = 'string'; i += 1; continue
            if c == "'": state = 'char'; i += 1; continue
            if c == '{': depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    return start, i + 1
        elif state == 'line':
            if c == '\n': state = 'code'
        elif state == 'block':
            if c == '*' and n == '/': state = 'code'; i += 2; continue
        elif state == 'string':
            if c == '\\': i += 2; continue
            if c == '"': state = 'code'
        elif state == 'char':
            if c == '\\': i += 2; continue
            if c == "'": state = 'code'
        i += 1
    raise SystemExit(f"unterminated function: {signature}")


def replace_function(text, signature, replacement):
    start, end = function_span(text, signature)
    return text[:start] + replacement.rstrip() + text[end:]


song_h = ROOT / 'src/ui/pages/song_page.h'
song_cpp = ROOT / 'src/ui/pages/song_page.cpp'
phrase_h = ROOT / 'src/ui/pages/phrase_page.h'
phrase_cpp = ROOT / 'src/ui/pages/phrase_page.cpp'

h = song_h.read_text()
h = replace_once(h, '#include "src/state/scene_revision.h"\n',
                 '#include "src/state/scene_revision.h"\n#include "src/state/undo_owner.h"\n#include "src/state/song_phrase_undo_receipts.h"\n',
                 'song undo includes')
h = replace_once(h, '  bool handleEvent(UIEvent& ui_event) override;\n',
                 '  bool handleEvent(UIEvent& ui_event) override;\n', 'song handle declaration')
h = replace_once(h, '  bool deleteRowAtCursor();\n',
                 '  bool deleteRowAtCursor();\n  bool handleEventLegacyUnowned(UIEvent& ui_event);\n  bool commitPreparedSong(int songSlot, const Song& after, int preferredPosition);\n  bool undoSongMutation();\n',
                 'song r4 declarations')
song_h.write_text(h)

s = song_cpp.read_text()
s = replace_once(s, '#include "../../debug_log.h"\n',
                 '#include "../../debug_log.h"\n#include "../../state/undo_owner.h"\n#include "../../state/song_phrase_undo_receipts.h"\n',
                 'song cpp undo includes')

old_undo = re.compile(r'enum class UndoActionType \{.*?UndoHistory g_undo_history;\n', re.S)
m = old_undo.search(s)
if not m:
    raise SystemExit('legacy Song UndoHistory block not found')
shim = '''enum class UndoActionType : uint8_t { None, Paste, Cut, Delete };\n\nstruct UndoCell { int row; int track; int pattern_index; };\n\n// R4 compatibility shim only: application Cut/Paste/Undo are intercepted by\n// SongPage::handleEvent before the retained legacy handler. It owns no history\n// payload and therefore cannot become a second Undo owner.\nstruct LegacyUndoHistoryDisabled {\n  UndoActionType action_type = UndoActionType::None;\n  std::array<UndoCell, 0> cells{};\n  void clear() { action_type = UndoActionType::None; }\n  void saveSingleCell(int, int, int) {}\n  void saveArea(int, int, int, int, const std::vector<int>&) {}\n};\n\nSongPatternClipboard g_song_pattern_clipboard;\nSongAreaClipboard g_song_area_clipboard;\nSongSlotClipboard g_song_slot_clipboard;\nLegacyUndoHistoryDisabled g_undo_history;\n'''
s = s[:m.start()] + shim + s[m.end():]

anchor = 'inline void drawLockIcon(IGfx& gfx, int x, int y, IGfxColor color) {'
helpers = '''inline int songTrackIndex(SongTrack track) {\n  const int index = static_cast<int>(track);\n  return index >= 0 && index < SongPosition::kTrackCount ? index : -1;\n}\n\ninline int songPatternValue(const Song& song, int row, SongTrack track) {\n  const int index = songTrackIndex(track);\n  if (row < 0 || row >= Song::kMaxPositions || index < 0) return -1;\n  return song.positions[row].patterns[index];\n}\n\ninline void setSongPatternValue(Song& song, int row, SongTrack track, int value) {\n  const int index = songTrackIndex(track);\n  if (row < 0 || row >= Song::kMaxPositions || index < 0) return;\n  song.positions[row].patterns[index] = static_cast<int16_t>(value);\n  if (value >= 0 && row >= song.length) song.length = row + 1;\n}\n\ninline void clearSongPatternValue(Song& song, int row, SongTrack track) {\n  const int index = songTrackIndex(track);\n  if (row < 0 || row >= Song::kMaxPositions || index < 0) return;\n  song.positions[row].patterns[index] = -1;\n}\n\ninline void clearSongValue(Song& song) {\n  for (int r = 0; r < Song::kMaxPositions; ++r) {\n    for (int t = 0; t < SongPosition::kTrackCount; ++t) song.positions[r].patterns[t] = -1;\n  }\n  song.length = 1;\n  song.reverse = false;\n}\n\ninline bool insertSongRowValue(Song& song, int position) {\n  const int used = std::clamp(song.length, 1, Song::kMaxPositions);\n  if (used >= Song::kMaxPositions) return false;\n  const int pos = std::clamp(position, 0, used);\n  for (int row = used; row > pos; --row) song.positions[row] = song.positions[row - 1];\n  for (int t = 0; t < SongPosition::kTrackCount; ++t) song.positions[pos].patterns[t] = -1;\n  song.length = used + 1;\n  return true;\n}\n\ninline bool deleteSongRowValue(Song& song, int position) {\n  int used = std::clamp(song.length, 1, Song::kMaxPositions);\n  const int pos = std::clamp(position, 0, used - 1);\n  if (used <= 1) {\n    for (int t = 0; t < SongPosition::kTrackCount; ++t) song.positions[0].patterns[t] = -1;\n    song.length = 1;\n    return true;\n  }\n  for (int row = pos; row < used - 1; ++row) song.positions[row] = song.positions[row + 1];\n  for (int t = 0; t < SongPosition::kTrackCount; ++t) song.positions[used - 1].patterns[t] = -1;\n  song.length = used - 1;\n  return true;\n}\n\n'''
s = replace_once(s, anchor, helpers + anchor, 'song pure helpers')

s = replace_function(s, 'bool SongPage::adjustSongPatternAtCursor(int delta)', r'''bool SongPage::adjustSongPatternAtCursor(int delta) {
  bool trackValid = false;
  const SongTrack track = trackForColumn(cursorTrack(), trackValid);
  if (!trackValid) return false;
  const int row = cursorRow();
  Scene& scene = mini_acid_.sceneManager().currentScene();
  const int slot = std::clamp(scene.activeSongSlot, 0, 1);
  Song after = scene.songs[slot];
  const int current = songPatternValue(after, row, track);
  int next = current;
  if (delta > 0) next = current < 0 ? 0 : current + 1;
  else if (delta < 0) next = current < 0 ? -1 : current - 1;
  next = std::clamp(next, -1, kMaxGlobalPatterns - 1);
  if (next == current) return false;
  if (next < 0) clearSongPatternValue(after, row, track);
  else setSongPatternValue(after, row, track, next);
  return commitPreparedSong(slot, after, row);
}''')

s = replace_function(s, 'bool SongPage::flipSongPatternBankAtCursorOrSelection()', r'''bool SongPage::flipSongPatternBankAtCursorOrSelection() {
  auto flipPattern = [](int pattern) -> int {
    if (pattern < 0) return pattern;
    const int bank = songPatternBank(pattern);
    const int index = songPatternIndexInBank(pattern);
    if (bank < 0 || index < 0) return pattern;
    return songPatternFromPageBankIndex(songPatternPage(pattern), bank == 0 ? 1 : 0, index);
  };
  Scene& scene = mini_acid_.sceneManager().currentScene();
  const int slot = std::clamp(scene.activeSongSlot, 0, 1);
  Song after = scene.songs[slot];
  int changed = 0;
  int preferred = cursorRow();
  if (has_selection_) {
    int min_row, max_row, min_track, max_track;
    getSelectionBounds(min_row, max_row, min_track, max_track);
    min_track = std::max(0, min_track);
    max_track = std::min(maxPatternTrackColumn(), max_track);
    if (min_track > max_track) return false;
    preferred = min_row;
    for (int r = min_row; r <= max_row; ++r) {
      for (int t = min_track; t <= max_track; ++t) {
        bool valid = false;
        const SongTrack track = trackForColumn(t, valid);
        if (!valid) continue;
        const int current = songPatternValue(after, r, track);
        const int next = flipPattern(current);
        if (next != current) { setSongPatternValue(after, r, track, next); ++changed; }
      }
    }
    clearSelection();
  } else {
    bool valid = false;
    const SongTrack track = trackForColumn(cursorTrack(), valid);
    if (!valid) return false;
    const int current = songPatternValue(after, preferred, track);
    const int next = flipPattern(current);
    if (next != current) { setSongPatternValue(after, preferred, track, next); ++changed; }
  }
  if (changed == 0) return false;
  const bool committed = commitPreparedSong(slot, after, preferred);
  if (committed) showToast(changed == 1 ? "Bank flip" : "Bank flip selection", 800);
  return committed;
}''')

s = replace_function(s, 'bool SongPage::assignPattern(int patternIdx)', r'''bool SongPage::assignPattern(int patternIdx) {
  if (cursorOnModeButton()) return false;
  Scene& scene = mini_acid_.sceneManager().currentScene();
  const int slot = std::clamp(scene.activeSongSlot, 0, 1);
  Song after = scene.songs[slot];
  const int combined = songPatternFromPageBankIndex(
      mini_acid_.currentPageIndex(), assignment_bank_index_, patternIdx);
  int preferred = cursorRow();
  if (has_selection_) {
    int min_row, max_row, min_track, max_track;
    getSelectionBounds(min_row, max_row, min_track, max_track);
    min_track = std::max(0, min_track);
    max_track = std::min(maxPatternTrackColumn(), max_track);
    if (min_track > max_track) return false;
    preferred = min_row;
    for (int r = min_row; r <= max_row; ++r) {
      for (int t = min_track; t <= max_track; ++t) {
        bool valid = false;
        const SongTrack track = trackForColumn(t, valid);
        if (valid) setSongPatternValue(after, r, track, combined);
      }
    }
    clearSelection();
  } else {
    bool valid = false;
    const SongTrack track = trackForColumn(cursorTrack(), valid);
    if (!valid) return false;
    setSongPatternValue(after, preferred, track, combined);
  }
  return commitPreparedSong(slot, after, preferred);
}''')

s = replace_function(s, 'bool SongPage::clearPattern()', r'''bool SongPage::clearPattern() {
  Scene& scene = mini_acid_.sceneManager().currentScene();
  const int slot = std::clamp(scene.activeSongSlot, 0, 1);
  Song after = scene.songs[slot];
  int preferred = cursorRow();
  int cleared = 0;
  if (has_selection_) {
    int min_row, max_row, min_track, max_track;
    getSelectionBounds(min_row, max_row, min_track, max_track);
    min_track = std::max(0, min_track);
    max_track = std::min(maxPatternTrackColumn(), max_track);
    if (min_track > max_track) return false;
    preferred = min_row;
    for (int r = min_row; r <= max_row; ++r) {
      for (int t = min_track; t <= max_track; ++t) {
        bool valid = false;
        const SongTrack track = trackForColumn(t, valid);
        if (!valid) continue;
        if (songPatternValue(after, r, track) >= 0) ++cleared;
        clearSongPatternValue(after, r, track);
      }
    }
    clearSelection();
  } else {
    bool valid = false;
    const SongTrack track = trackForColumn(cursorTrack(), valid);
    if (!valid) return false;
    if (songPatternValue(after, preferred, track) >= 0) ++cleared;
    clearSongPatternValue(after, preferred, track);
  }
  if (cleared == 0) return true;
  const bool committed = commitPreparedSong(slot, after, preferred);
  if (committed && cleared > 1) {
    char toast[48]; std::snprintf(toast, sizeof(toast), "Cleared %d cells", cleared); showToast(toast, 900);
  }
  return committed;
}''')

s = replace_function(s, 'bool SongPage::insertRowAtCursor()', r'''bool SongPage::insertRowAtCursor() {
  const int row = cursorRow();
  Scene& scene = mini_acid_.sceneManager().currentScene();
  const int slot = std::clamp(scene.activeSongSlot, 0, 1);
  Song after = scene.songs[slot];
  if (!insertSongRowValue(after, row)) return false;
  if (!commitPreparedSong(slot, after, row)) return false;
  char toast[32]; std::snprintf(toast, sizeof(toast), "INS row %d", row + 1); showToast(toast, 900);
  return true;
}''')

s = replace_function(s, 'bool SongPage::deleteRowAtCursor()', r'''bool SongPage::deleteRowAtCursor() {
  const int row = cursorRow();
  Scene& scene = mini_acid_.sceneManager().currentScene();
  const int slot = std::clamp(scene.activeSongSlot, 0, 1);
  Song after = scene.songs[slot];
  if (!deleteSongRowValue(after, row)) return false;
  if (!commitPreparedSong(slot, after, row)) return false;
  cursor_row_ = clampCursorRow(cursor_row_);
  char toast[32]; std::snprintf(toast, sizeof(toast), "DEL row %d", row + 1); showToast(toast, 900);
  return true;
}''')

s = replace_once(s, 'bool SongPage::handleEvent(UIEvent& ui_event) {',
                 'bool SongPage::handleEventLegacyUnowned(UIEvent& ui_event) {',
                 'rename legacy song handler')

wrapper = r'''

bool SongPage::commitPreparedSong(int songSlot, const Song& after, int preferredPosition) {
  SceneManager& manager = mini_acid_.sceneManager();
  GroovePuterUndo::SongUndoPayload before{};
  if (!GroovePuterUndo::captureSongUndo(manager, songSlot, before)) return false;
  if (GroovePuterUndo::songsEqual(before.before, after)) return false;
  return GroovePuterUndo::undoOwner().commitPrepared(
      UndoKind::Song, before, [&]() {
        withRuntimeAudioGuard([&]() {
          manager.currentScene().songs[songSlot] = after;
          if (manager.currentScene().activeSongSlot == songSlot &&
              mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
            const int length = std::clamp(after.length, 1, Song::kMaxPositions);
            mini_acid_.setSongPosition(std::clamp(preferredPosition, 0, length - 1));
          }
        });
      });
}

bool SongPage::undoSongMutation() {
  SceneManager& manager = mini_acid_.sceneManager();
  const GroovePuterUndo::UndoResult result =
      GroovePuterUndo::undoOwner().undoPrepared<GroovePuterUndo::SongUndoPayload>(
          UndoKind::Song,
          [&](const GroovePuterUndo::SongUndoPayload& receipt) {
            return GroovePuterUndo::songUndoTargetAvailable(manager, receipt);
          },
          [&](const GroovePuterUndo::SongUndoPayload& receipt) {
            withRuntimeAudioGuard([&]() {
              GroovePuterUndo::restoreSongUndo(manager, receipt);
              if (manager.currentScene().activeSongSlot == receipt.songSlot &&
                  mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
                const int length = std::clamp(receipt.before.length, 1, Song::kMaxPositions);
                mini_acid_.setSongPosition(std::clamp(cursorRow(), 0, length - 1));
              }
            });
          });
  if (result == GroovePuterUndo::UndoResult::Restored) {
    cursor_row_ = clampCursorRow(cursor_row_);
    showToast("Undo: Song restored", 850);
    return true;
  }
  if (result == GroovePuterUndo::UndoResult::Expired) {
    showToast("Undo expired", 800);
    return true;
  }
  return false;
}

bool SongPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT) {
    const bool wholeSongScope = cursorOnModeButton() || cursorOnPlayheadLabel();
    bool trackValid = false;
    const SongTrack track = trackForColumn(cursorTrack(), trackValid);
    Scene& scene = mini_acid_.sceneManager().currentScene();
    const int slot = std::clamp(scene.activeSongSlot, 0, 1);

    if (ui_event.app_event_type == GROOVEPUTER_APP_EVENT_UNDO) return undoSongMutation();

    if (ui_event.app_event_type == GROOVEPUTER_APP_EVENT_CUT) {
      Song after = scene.songs[slot];
      int preferred = cursorRow();
      if (wholeSongScope) {
        g_song_slot_clipboard.song = after;
        g_song_slot_clipboard.source_slot = slot;
        g_song_slot_clipboard.has_song = true;
        g_song_area_clipboard.has_area = false;
        g_song_pattern_clipboard.has_pattern = false;
        clearSongValue(after);
        preferred = 0;
      } else if (has_selection_) {
        int min_row, max_row, min_track, max_track;
        getSelectionBounds(min_row, max_row, min_track, max_track);
        min_track = std::max(0, min_track);
        max_track = std::min(maxPatternTrackColumn(), max_track);
        if (min_track > max_track) return false;
        g_song_area_clipboard.pattern_indices.clear();
        g_song_area_clipboard.rows = max_row - min_row + 1;
        g_song_area_clipboard.tracks = max_track - min_track + 1;
        for (int r = min_row; r <= max_row; ++r) {
          for (int t = min_track; t <= max_track; ++t) {
            bool valid = false; const SongTrack tr = trackForColumn(t, valid);
            g_song_area_clipboard.pattern_indices.push_back(valid ? songPatternValue(after, r, tr) : -1);
            if (valid) clearSongPatternValue(after, r, tr);
          }
        }
        g_song_area_clipboard.has_area = true;
        g_song_pattern_clipboard.has_pattern = false;
        preferred = min_row;
      } else {
        if (!trackValid) return false;
        const int row = cursorRow();
        g_song_pattern_clipboard.pattern_index = songPatternValue(after, row, track);
        g_song_pattern_clipboard.has_pattern = true;
        g_song_area_clipboard.has_area = false;
        clearSongPatternValue(after, row, track);
        preferred = row;
      }
      const bool committed = commitPreparedSong(slot, after, preferred);
      if (committed && wholeSongScope) showToast("Song cut", 900);
      return committed;
    }

    if (ui_event.app_event_type == GROOVEPUTER_APP_EVENT_PASTE) {
      Song after = scene.songs[slot];
      int preferred = cursorRow();
      if (wholeSongScope) {
        if (!g_song_slot_clipboard.has_song) return false;
        after = g_song_slot_clipboard.song;
        preferred = 0;
      } else if (g_song_area_clipboard.has_area) {
        int startRow = cursorRow();
        int startTrack = cursorTrack();
        if (has_selection_) {
          int maxRow, maxTrack; getSelectionBounds(startRow, maxRow, startTrack, maxTrack);
        }
        const int tracks = std::min(g_song_area_clipboard.tracks,
                                    maxPatternTrackColumn() - startTrack + 1);
        if (tracks <= 0) return false;
        int index = 0;
        for (int r = 0; r < g_song_area_clipboard.rows; ++r) {
          for (int t = 0; t < g_song_area_clipboard.tracks; ++t, ++index) {
            if (t >= tracks || startRow + r >= Song::kMaxPositions ||
                index >= static_cast<int>(g_song_area_clipboard.pattern_indices.size())) continue;
            bool valid = false; const SongTrack tr = trackForColumn(startTrack + t, valid);
            if (!valid) continue;
            const int value = g_song_area_clipboard.pattern_indices[index];
            if (value < 0) clearSongPatternValue(after, startRow + r, tr);
            else setSongPatternValue(after, startRow + r, tr, value);
          }
        }
        preferred = startRow;
      } else if (g_song_pattern_clipboard.has_pattern) {
        if (!trackValid) return false;
        const int row = cursorRow();
        const int value = g_song_pattern_clipboard.pattern_index;
        if (value < 0) clearSongPatternValue(after, row, track);
        else setSongPatternValue(after, row, track, value);
        preferred = row;
      } else {
        return false;
      }
      const bool committed = commitPreparedSong(slot, after, preferred);
      if (has_selection_) clearSelection();
      if (committed && wholeSongScope) showToast("Song pasted", 900);
      return committed;
    }

    return handleEventLegacyUnowned(ui_event);
  }

  if (ui_event.event_type == GROOVEPUTER_KEY_DOWN && ui_event.alt &&
      (ui_event.key == '\b' || ui_event.key == 0x7F)) {
    Scene& scene = mini_acid_.sceneManager().currentScene();
    const int slot = std::clamp(scene.activeSongSlot, 0, 1);
    Song after = scene.songs[slot];
    clearSongValue(after);
    const bool committed = commitPreparedSong(slot, after, 0);
    if (committed) { clearSelection(); showToast("Song reset", 900); }
    return committed;
  }

  return handleEventLegacyUnowned(ui_event);
}
'''
s += wrapper
song_cpp.write_text(s)

ph = phrase_h.read_text()
ph = replace_once(ph, '  bool clearCurrentSlot();\n',
                  '  bool clearCurrentSlot();\n  bool undoCurrentEdit();\n', 'phrase undo declaration')
phrase_h.write_text(ph)

p = phrase_cpp.read_text()
p = replace_once(p, '#include "phrase_page.h"\n',
                 '#include "phrase_page.h"\n#include "src/state/undo_owner.h"\n#include "src/state/song_phrase_undo_receipts.h"\n',
                 'phrase undo includes')
insert_before = 'void PhrasePage::draw(IGfx& gfx) {'
undo_impl = r'''bool PhrasePage::undoCurrentEdit() {
  SceneManager& manager = mini_acid_.sceneManager();
  auto& owner = GroovePuterUndo::undoOwner();
  GroovePuterUndo::UndoResult result = GroovePuterUndo::UndoResult::KindMismatch;
  if (owner.kind() == UndoKind::Phrase) {
    result = owner.undoPrepared<GroovePuterUndo::PhraseBankUndoPayload>(
        UndoKind::Phrase,
        [](const GroovePuterUndo::PhraseBankUndoPayload&) { return true; },
        [&](const GroovePuterUndo::PhraseBankUndoPayload& receipt) {
          auto restore = [&]() { GroovePuterUndo::restorePhraseBankUndo(manager, receipt); };
          if (audio_guard_) audio_guard_(restore); else restore();
        });
  } else if (owner.kind() == UndoKind::Song) {
    result = owner.undoPrepared<GroovePuterUndo::SongUndoPayload>(
        UndoKind::Song,
        [&](const GroovePuterUndo::SongUndoPayload& receipt) {
          return GroovePuterUndo::songUndoTargetAvailable(manager, receipt);
        },
        [&](const GroovePuterUndo::SongUndoPayload& receipt) {
          auto restore = [&]() { GroovePuterUndo::restoreSongUndo(manager, receipt); };
          if (audio_guard_) audio_guard_(restore); else restore();
        });
  }
  if (result == GroovePuterUndo::UndoResult::Restored) {
    preview_bar_ = 0; invalidatePreview(); UI::showToast("Undo: restored", 850); return true;
  }
  if (result == GroovePuterUndo::UndoResult::Expired) {
    UI::showToast("Undo expired", 800); return true;
  }
  return false;
}

'''
p = replace_once(p, insert_before, undo_impl + insert_before, 'phrase undo implementation')
p = replace_once(p,
                 'bool PhrasePage::handleEvent(UIEvent& ui_event) {\n  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;\n',
                 'bool PhrasePage::handleEvent(UIEvent& ui_event) {\n  if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT &&\n      ui_event.app_event_type == GROOVEPUTER_APP_EVENT_UNDO) {\n    return undoCurrentEdit();\n  }\n  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;\n',
                 'phrase app undo interception')
phrase_cpp.write_text(p)

print('R4 Song/Phrase transform applied')
