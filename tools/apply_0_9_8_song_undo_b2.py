#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SONG = ROOT / "src/ui/pages/song_page.cpp"
TEST = ROOT / "tests/test_song_undo_source_regressions.py"
WORKFLOW = ROOT / ".github/workflows/temp-0-9-8-song-undo-b2.yml"
SELF = Path(__file__).resolve()

text = SONG.read_text(encoding="utf-8")

old = '''enum class UndoActionType {
  None,
  Paste,
  Cut,
  Delete,
};

struct UndoCell {
  int row;
  int track;
  int pattern_index;
};

struct UndoHistory {
  UndoActionType action_type = UndoActionType::None;
  std::vector<UndoCell> cells;
  
  void clear() {
    action_type = UndoActionType::None;
    cells.clear();
  }
  
  void saveSingleCell(int row, int track, int pattern_index) {
    cells.clear();
    cells.push_back({row, track, pattern_index});
  }
  
  void saveArea(int min_row, int max_row, int min_track, int max_track, 
                const std::vector<int>& pattern_indices) {
    cells.clear();
    int idx = 0;
    for (int r = min_row; r <= max_row; ++r) {
      for (int t = min_track; t <= max_track; ++t) {
        if (idx < static_cast<int>(pattern_indices.size())) {
          cells.push_back({r, t, pattern_indices[idx]});
        }
        ++idx;
      }
    }
  }
};
'''
new = '''enum class UndoActionType : uint8_t {
  None,
  Paste,
  Cut,
  Delete,
};

struct UndoHistory {
  static constexpr int kMaxTracks = SongPosition::kTrackCount;
  static constexpr int kMaxCells = Song::kMaxPositions * kMaxTracks;

  UndoActionType action_type = UndoActionType::None;
  int8_t song_slot = -1;
  uint8_t old_song_length = 1;
  bool old_song_reverse = false;
  uint32_t mutation_revision = 0;
  uint8_t first_row = 0;
  uint8_t row_count = 0;
  uint8_t track_count = 0;
  SongTrack tracks[kMaxTracks] = {
      SongTrack::SynthA, SongTrack::SynthB, SongTrack::Drums, SongTrack::Voice};
  int16_t patterns[kMaxCells] = {};
  bool committed = false;

  void clear() {
    action_type = UndoActionType::None;
    song_slot = -1;
    row_count = 0;
    track_count = 0;
    mutation_revision = 0;
    committed = false;
  }

  void begin(UndoActionType action, MiniAcid& mini_acid, int row, int rows) {
    clear();
    action_type = action;
    song_slot = static_cast<int8_t>(mini_acid.activeSongSlot());
    old_song_length = static_cast<uint8_t>(mini_acid.songLength());
    old_song_reverse = mini_acid.isSongReverse();
    first_row = static_cast<uint8_t>(row);
    row_count = static_cast<uint8_t>(rows);
  }

  template <typename TrackResolver>
  bool captureArea(UndoActionType action,
                   MiniAcid& mini_acid,
                   int min_row,
                   int max_row,
                   int min_track,
                   int max_track,
                   TrackResolver&& resolve_track) {
    if (min_row < 0) min_row = 0;
    if (max_row >= Song::kMaxPositions) max_row = Song::kMaxPositions - 1;
    if (min_row > max_row || min_track > max_track) return false;
    const int rows = max_row - min_row + 1;
    const int columns = max_track - min_track + 1;
    if (columns <= 0 || columns > kMaxTracks || rows * columns > kMaxCells) return false;

    begin(action, mini_acid, min_row, rows);
    for (int t = min_track; t <= max_track; ++t) {
      bool valid = false;
      SongTrack track = resolve_track(t, valid);
      if (!valid || track_count >= kMaxTracks) {
        clear();
        return false;
      }
      tracks[track_count++] = track;
    }

    for (int r = 0; r < rows; ++r) {
      for (int t = 0; t < track_count; ++t) {
        patterns[r * track_count + t] = static_cast<int16_t>(
            mini_acid.songPatternAt(min_row + r, tracks[t]));
      }
    }
    return true;
  }

  bool captureSingle(UndoActionType action,
                     MiniAcid& mini_acid,
                     int row,
                     SongTrack track) {
    if (row < 0 || row >= Song::kMaxPositions) return false;
    begin(action, mini_acid, row, 1);
    track_count = 1;
    tracks[0] = track;
    patterns[0] = static_cast<int16_t>(mini_acid.songPatternAt(row, track));
    return true;
  }

  bool captureWholeSong(UndoActionType action, MiniAcid& mini_acid) {
    begin(action, mini_acid, 0, Song::kMaxPositions);
    track_count = kMaxTracks;
    tracks[0] = SongTrack::SynthA;
    tracks[1] = SongTrack::SynthB;
    tracks[2] = SongTrack::Drums;
    tracks[3] = SongTrack::Voice;
    for (int r = 0; r < Song::kMaxPositions; ++r) {
      for (int t = 0; t < track_count; ++t) {
        patterns[r * track_count + t] = static_cast<int16_t>(
            mini_acid.songPatternAt(r, tracks[t]));
      }
    }
    return true;
  }

  void commit() {
    mutation_revision = GroovePuterState::sceneRevisionSnapshot().currentRevision;
    committed = true;
  }

  bool empty() const {
    return action_type == UndoActionType::None || !committed ||
           row_count == 0 || track_count == 0;
  }

  bool currentFor(const MiniAcid& mini_acid) const {
    return !empty() && song_slot == mini_acid.activeSongSlot() &&
           mutation_revision == GroovePuterState::sceneRevisionSnapshot().currentRevision;
  }
};

static_assert(UndoHistory::kMaxCells == 512,
              "Song one-step Undo must remain explicitly bounded");
static_assert(sizeof(UndoHistory) <= 1056,
              "Song one-step Undo DRAM budget grew unexpectedly");
'''
assert text.count(old) == 1, "UndoHistory anchor changed"
text = text.replace(old, new)

old = '''    std::vector<int> old_patterns;
    old_patterns.reserve((max_row - min_row + 1) * (max_track - min_track + 1));
    int cleared = 0;

    withAudioGuard([&]() {
      for (int r = min_row; r <= max_row; ++r) {
        for (int t = min_track; t <= max_track; ++t) {
          bool valid = false;
          SongTrack track = trackForColumn(t, valid);
          if (!valid) continue;
          int before = mini_acid_.songPatternAt(r, track);
          old_patterns.push_back(before);
          if (before >= 0) ++cleared;
          mini_acid_.clearSongPattern(r, track);
        }
      }
      if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
        mini_acid_.setSongPosition(min_row);
      }
    });

    g_undo_history.action_type = UndoActionType::Delete;
    g_undo_history.saveArea(min_row, max_row, min_track, max_track, old_patterns);
'''
new = '''    if (!g_undo_history.captureArea(
            UndoActionType::Delete, mini_acid_, min_row, max_row, min_track, max_track,
            [&](int column, bool& valid) { return trackForColumn(column, valid); })) {
      return false;
    }
    int cleared = 0;

    withAudioGuard([&]() {
      for (int r = min_row; r <= max_row; ++r) {
        for (int t = min_track; t <= max_track; ++t) {
          bool valid = false;
          SongTrack track = trackForColumn(t, valid);
          if (!valid) continue;
          int before = mini_acid_.songPatternAt(r, track);
          if (before >= 0) ++cleared;
          mini_acid_.clearSongPattern(r, track);
        }
      }
      if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
        mini_acid_.setSongPosition(min_row);
      }
    });
    g_undo_history.commit();
'''
assert text.count(old) == 1, "selection clear undo anchor changed"
text = text.replace(old, new)

old = '''  // Save undo state
  int current_pattern = mini_acid_.songPatternAt(row, track);
  g_undo_history.action_type = UndoActionType::Delete;
  g_undo_history.saveSingleCell(row, cursorTrack(), current_pattern);
  
  withAudioGuard([&]() {
    mini_acid_.clearSongPattern(row, track);
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      mini_acid_.setSongPosition(row);
    }
  });
'''
new = '''  if (!g_undo_history.captureSingle(UndoActionType::Delete, mini_acid_, row, track)) {
    return false;
  }
  withAudioGuard([&]() {
    mini_acid_.clearSongPattern(row, track);
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      mini_acid_.setSongPosition(row);
    }
  });
  g_undo_history.commit();
'''
assert text.count(old) == 1, "single clear undo anchor changed"
text = text.replace(old, new)

old = '''        if (wholeSongScope) {
          g_song_slot_clipboard.song = mini_acid_.song();
          g_song_slot_clipboard.source_slot = mini_acid_.activeSongSlot();
          g_song_slot_clipboard.has_song = true;
          withAudioGuard([&]() {
'''
new = '''        if (wholeSongScope) {
          g_song_slot_clipboard.song = mini_acid_.song();
          g_song_slot_clipboard.source_slot = mini_acid_.activeSongSlot();
          g_song_slot_clipboard.has_song = true;
          if (!g_undo_history.captureWholeSong(UndoActionType::Cut, mini_acid_)) return false;
          withAudioGuard([&]() {
'''
assert text.count(old) == 1, "whole Song cut anchor changed"
text = text.replace(old, new)

old = '''          });
          showToast("Song cut", 900);
          return true;
'''
new = '''          });
          g_undo_history.commit();
          showToast("Song cut", 900);
          return true;
'''
assert text.count(old) == 1, "whole Song cut commit anchor changed"
text = text.replace(old, new)

old = '''          // Save undo state and copy/clear
          std::vector<int> old_patterns;
          old_patterns.reserve(rows * tracks);
          
          withAudioGuard([&]() {
'''
new = '''          if (!g_undo_history.captureArea(
                  UndoActionType::Cut, mini_acid_, min_row, max_row, min_track, max_track,
                  [&](int column, bool& valid) { return trackForColumn(column, valid); })) {
            return false;
          }
          withAudioGuard([&]() {
'''
assert text.count(old) == 1, "area cut capture anchor changed"
text = text.replace(old, new)

text = text.replace('''                  g_song_area_clipboard.pattern_indices.push_back(pattern);
                  old_patterns.push_back(pattern);
                  mini_acid_.clearSongPattern(r, song_track);
''','''                  g_song_area_clipboard.pattern_indices.push_back(pattern);
                  mini_acid_.clearSongPattern(r, song_track);
''',1)

old = '''          // Save undo history
          g_undo_history.action_type = UndoActionType::Cut;
          g_undo_history.saveArea(min_row, max_row, min_track, max_track, old_patterns);
'''
new = '''          g_undo_history.commit();
'''
assert text.count(old) == 1, "area cut commit anchor changed"
text = text.replace(old, new)

old = '''          // Save undo state
          g_undo_history.action_type = UndoActionType::Cut;
          g_undo_history.saveSingleCell(row, cursorTrack(), current_pattern);
          
          withAudioGuard([&]() {
            mini_acid_.clearSongPattern(row, track);
          });
'''
new = '''          if (!g_undo_history.captureSingle(UndoActionType::Cut, mini_acid_, row, track)) {
            return false;
          }
          withAudioGuard([&]() {
            mini_acid_.clearSongPattern(row, track);
          });
          g_undo_history.commit();
'''
assert text.count(old) == 1, "single cut anchor changed"
text = text.replace(old, new)

old = '''        if (wholeSongScope) {
          if (!g_song_slot_clipboard.has_song) return false;
          Song pasted = g_song_slot_clipboard.song;
          withAudioGuard([&]() {
'''
new = '''        if (wholeSongScope) {
          if (!g_song_slot_clipboard.has_song) return false;
          Song pasted = g_song_slot_clipboard.song;
          if (!g_undo_history.captureWholeSong(UndoActionType::Paste, mini_acid_)) return false;
          withAudioGuard([&]() {
'''
assert text.count(old) == 1, "whole Song paste anchor changed"
text = text.replace(old, new)

old = '''          });
          showToast("Song pasted", 900);
          return true;
'''
new = '''          });
          g_undo_history.commit();
          showToast("Song pasted", 900);
          return true;
'''
assert text.count(old) == 1, "whole Song paste commit anchor changed"
text = text.replace(old, new)

old = '''          // Save old patterns for undo
          std::vector<int> old_patterns;
          int min_row = start_row;
          int max_row = start_row + source_rows - 1;
          int min_track = start_track;
          int max_track = start_track + paste_tracks - 1;
          
          for (int r = min_row; r <= max_row; ++r) {
            for (int t = min_track; t <= max_track; ++t) {
              if (r >= Song::kMaxPositions) {
                old_patterns.push_back(-1);
                continue;
              }
              bool valid = false;
              SongTrack song_track = trackForColumn(t, valid);
              int pattern = valid ? mini_acid_.songPatternAt(r, song_track) : -1;
              old_patterns.push_back(pattern);
            }
          }
          
          withAudioGuard([&]() {
'''
new = '''          int min_row = start_row;
          int max_row = start_row + source_rows - 1;
          int min_track = start_track;
          int max_track = start_track + paste_tracks - 1;
          if (!g_undo_history.captureArea(
                  UndoActionType::Paste, mini_acid_, min_row, max_row, min_track, max_track,
                  [&](int column, bool& valid) { return trackForColumn(column, valid); })) {
            return false;
          }
          withAudioGuard([&]() {
'''
assert text.count(old) == 1, "area paste capture anchor changed"
text = text.replace(old, new)

old = '''          // Save undo history
          g_undo_history.action_type = UndoActionType::Paste;
          g_undo_history.saveArea(min_row, max_row, min_track, max_track, old_patterns);
'''
new = '''          g_undo_history.commit();
'''
assert text.count(old) == 1, "area paste commit anchor changed"
text = text.replace(old, new)

old = '''          // Save old pattern for undo
          int old_pattern = mini_acid_.songPatternAt(row, track);
          g_undo_history.action_type = UndoActionType::Paste;
          g_undo_history.saveSingleCell(row, track_idx, old_pattern);
          
          withAudioGuard([&]() {
'''
new = '''          if (!g_undo_history.captureSingle(UndoActionType::Paste, mini_acid_, row, track)) {
            return false;
          }
          withAudioGuard([&]() {
'''
assert text.count(old) == 1, "single paste capture anchor changed"
text = text.replace(old, new)

old = '''          });
        } else {
          return false;
        }
        if (has_selection_) clearSelection();
        return true;
      }
      case GROOVEPUTER_APP_EVENT_UNDO: {
'''
new = '''          });
          g_undo_history.commit();
        } else {
          return false;
        }
        if (has_selection_) clearSelection();
        return true;
      }
      case GROOVEPUTER_APP_EVENT_UNDO: {
'''
assert text.count(old) == 1, "single paste commit anchor changed"
text = text.replace(old, new)

old = '''      case GROOVEPUTER_APP_EVENT_UNDO: {
        if (!trackValid) return false;
        if (g_undo_history.action_type == UndoActionType::None || g_undo_history.cells.empty()) {
          showToast("Nothing to undo", 700);
          return true;
        }
        
        // Restore all cells from undo history
        withAudioGuard([&]() {
          for (const auto& cell : g_undo_history.cells) {
            bool valid = false;
            SongTrack song_track = trackForColumn(cell.track, valid);
            if (valid && cell.row >= 0 && cell.row < Song::kMaxPositions) {
              if (cell.pattern_index < 0) {
                mini_acid_.clearSongPattern(cell.row, song_track);
              } else {
                mini_acid_.setSongPattern(cell.row, song_track, cell.pattern_index);
              }
            }
          }
          if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
            if (!g_undo_history.cells.empty()) {
              mini_acid_.setSongPosition(g_undo_history.cells[0].row);
            }
          }
        });
        
        // One-step semantics: a successful restore consumes the receipt.
        g_undo_history.clear();
        showToast("Undo: restored", 800);
        return true;
      }
'''
new = '''      case GROOVEPUTER_APP_EVENT_UNDO: {
        if (g_undo_history.empty()) {
          showToast("Nothing to undo", 700);
          return true;
        }
        if (!g_undo_history.currentFor(mini_acid_)) {
          g_undo_history.clear();
          showToast("Undo expired", 800);
          return true;
        }

        const int restore_row = g_undo_history.first_row;
        withAudioGuard([&]() {
          for (int r = 0; r < g_undo_history.row_count; ++r) {
            const int row = g_undo_history.first_row + r;
            for (int t = 0; t < g_undo_history.track_count; ++t) {
              const SongTrack song_track = g_undo_history.tracks[t];
              const int16_t pattern =
                  g_undo_history.patterns[r * g_undo_history.track_count + t];
              if (pattern < 0) {
                mini_acid_.clearSongPattern(row, song_track);
              } else {
                mini_acid_.setSongPattern(row, song_track, pattern);
              }
            }
          }
          mini_acid_.setSongLength(g_undo_history.old_song_length);
          mini_acid_.setSongReverse(g_undo_history.old_song_reverse);
          if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
            int position = restore_row;
            if (position >= mini_acid_.songLength()) position = mini_acid_.songLength() - 1;
            if (position < 0) position = 0;
            mini_acid_.setSongPosition(position);
          }
        });

        // One-step semantics: a successful restore consumes the receipt.
        g_undo_history.clear();
        showToast("Undo: restored", 800);
        return true;
      }
'''
assert text.count(old) == 1, "Undo restore anchor changed"
text = text.replace(old, new)

# B2 must eliminate transient undo payload vectors; the clipboard's own vector remains out of scope.
assert "std::vector<UndoCell>" not in text
assert "std::vector<int> old_patterns" not in text
SONG.write_text(text, encoding="utf-8")

TEST.write_text(r'''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SONG = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
CORE = (ROOT / "src/ui/ui_core.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require("GROOVEPUTER_APP_EVENT_UNDO" in CORE,
            "shared UI event contract must retain Undo")
    require("bool key_z" in SONG and "GROOVEPUTER_Z" in SONG,
            "Song must normalize the Z shortcut across key/scancode paths")
    require("(ui_event.ctrl || ui_event.meta) && !ui_event.alt && key_z" in SONG,
            "Song Undo must be reachable via Ctrl/Meta+Z without stealing Alt+Z")
    require("app_evt.app_event_type = GROOVEPUTER_APP_EVENT_UNDO" in SONG,
            "Song shortcut must route through the existing application Undo event")
    require("case GROOVEPUTER_APP_EVENT_UNDO" in SONG,
            "existing Song Undo handler must remain reachable")

    require("static constexpr int kMaxCells = Song::kMaxPositions * kMaxTracks" in SONG,
            "Undo payload must have an explicit compile-time cell bound")
    require("static_assert(UndoHistory::kMaxCells == 512" in SONG,
            "Undo bound must stay explicit for the current Song schema")
    require("static_assert(sizeof(UndoHistory) <= 1056" in SONG,
            "Undo DRAM budget must stay compile-time guarded")
    require("std::vector<UndoCell>" not in SONG,
            "Undo history must not allocate a dynamic cell vector")
    require("std::vector<int> old_patterns" not in SONG,
            "destructive Song edits must not allocate transient undo vectors")
    require("SongTrack tracks[kMaxTracks]" in SONG,
            "Undo must capture stable SongTrack identity, not lane-focus columns")
    require("mutation_revision == GroovePuterState::sceneRevisionSnapshot().currentRevision" in SONG,
            "Undo receipt must expire after a newer Scene mutation")
    require('showToast("Undo expired"' in SONG,
            "stale Undo must fail closed with feedback")
    require("old_song_length" in SONG and "old_song_reverse" in SONG,
            "Undo must restore Song structural metadata affected by whole-Song operations")
    require("captureWholeSong(UndoActionType::Cut" in SONG and
            "captureWholeSong(UndoActionType::Paste" in SONG,
            "whole-Song cut/paste must participate in bounded one-step Undo")
    require("g_undo_history.clear();" in SONG,
            "successful Undo must consume the one-step receipt")
    require("GROOVEPUTER_APP_EVENT_REDO" not in CORE and
            "GROOVEPUTER_APP_EVENT_REDO" not in SONG,
            "0.9.8-B2 must not introduce Redo")
    print("Song Undo source regressions: OK")


if __name__ == "__main__":
    main()
''', encoding="utf-8")

if WORKFLOW.exists():
    WORKFLOW.unlink()
if SELF.exists():
    SELF.unlink()
