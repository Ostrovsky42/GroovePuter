#include "song_page.h"
#include "../../../scenes.h"
#include "../ui_common.h"
#include "../retro_widgets.h"
#include "../amber_widgets.h"
#include "../retro_ui_theme.h"
#include "../amber_ui_theme.h"

namespace retro = RetroWidgets;
namespace amber = AmberWidgets;

#include <cctype>
#include <cstdio>
#include <vector>

#include "../help_dialog_frames.h"
#include "../components/mode_button.h"

namespace {
inline IGfxColor song303Color(int synthIndex) {
  // Keep 303A/303B colors consistent with other pages (bass/lead split).
  return (synthIndex == 0) ? IGfxColor(0x00D7FF) : IGfxColor(0xFF4FD8);
}

struct SongPatternClipboard {
  bool has_pattern = false;
  int pattern_index = -1;
};

struct SongAreaClipboard {
  bool has_area = false;
  int rows = 0;
  int tracks = 0;
  std::vector<int> pattern_indices;
};

enum class UndoActionType {
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

SongPatternClipboard g_song_pattern_clipboard;
SongAreaClipboard g_song_area_clipboard;
UndoHistory g_undo_history;
} // namespace

SongPage::SongPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
  : gfx_(gfx),
    mini_acid_(mini_acid),
    audio_guard_(audio_guard),
    cursor_row_(0),
    cursor_track_(0),
    scroll_row_(0),
    has_selection_(false),
    selection_start_row_(0),
    selection_start_track_(0),
    gen_mode_(SmartPatternGenerator::PG_RANDOM),
    show_genre_hint_(false),
    hint_timer_(0) {
  cursor_row_ = mini_acid_.currentSongPosition();
  if (cursor_row_ < 0) cursor_row_ = 0;
  int maxSongRow = mini_acid_.songLength() - 1;
  if (maxSongRow < 0) maxSongRow = 0;
  if (cursor_row_ > maxSongRow) cursor_row_ = maxSongRow;
  if (cursor_row_ >= Song::kMaxPositions) cursor_row_ = Song::kMaxPositions - 1;
}

int SongPage::clampCursorRow(int row) const {
  int maxRow = Song::kMaxPositions - 1;
  if (maxRow < 0) maxRow = 0;
  if (row < 0) row = 0;
  if (row > maxRow) row = maxRow;
  return row;
}

int SongPage::cursorRow() const {
  return clampCursorRow(cursor_row_);
}

int SongPage::cursorTrack() const {
  int track = cursor_track_;
  if (track < 0) track = 0;
  if (track > 5) track = 5;
  return track;
}

bool SongPage::cursorOnModeButton() const { return cursorTrack() == 5; }
bool SongPage::cursorOnPlayheadLabel() const { return cursorTrack() == 4; }

void SongPage::startSelection() {
  has_selection_ = true;
  selection_start_row_ = cursor_row_;
  selection_start_track_ = cursor_track_;
}

void SongPage::updateSelection() {
  if (!has_selection_) {
    startSelection();
  }
}

void SongPage::clearSelection() {
  has_selection_ = false;
  if (mini_acid_.loopModeEnabled()) {
    audio_guard_([&]() { mini_acid_.setLoopMode(false); });
  }
}

void SongPage::updateLoopRangeFromSelection() {
  if (!mini_acid_.loopModeEnabled()) return;
  if (!has_selection_) {
    audio_guard_([&]() { mini_acid_.setLoopMode(false); });
    return;
  }
  int min_row, max_row, min_track, max_track;
  getSelectionBounds(min_row, max_row, min_track, max_track);
  (void)min_track;
  (void)max_track;
  audio_guard_([&]() { mini_acid_.setLoopRange(min_row, max_row); });
}

void SongPage::getSelectionBounds(int& min_row, int& max_row, int& min_track, int& max_track) const {
  if (!has_selection_) {
    min_row = max_row = cursor_row_;
    min_track = max_track = cursor_track_;
    return;
  }
  min_row = std::min(selection_start_row_, cursor_row_);
  max_row = std::max(selection_start_row_, cursor_row_);
  min_track = std::min(selection_start_track_, cursor_track_);
  max_track = std::max(selection_start_track_, cursor_track_);
}

void SongPage::moveCursorHorizontal(int delta, bool extend_selection) {
  if (extend_selection) {
    updateSelection();
  } else {
    clearSelection();
  }
  int track = cursorTrack();
  track += delta;
  if (track < 0) track = 0;
  if (track > 5) track = 5;
  cursor_track_ = track;
  syncSongPositionToCursor();
  if (extend_selection) {
    updateLoopRangeFromSelection();
  }
}

void SongPage::moveCursorVertical(int delta, bool extend_selection) {
  if (delta == 0) return;
  if (cursorOnPlayheadLabel() || cursorOnModeButton()) {
    moveCursorHorizontal(delta, extend_selection);
    return;
  }
  if (extend_selection) {
    updateSelection();
  } else {
    clearSelection();
  }
  int row = cursorRow();
  row += delta;
  row = clampCursorRow(row);
  cursor_row_ = row;
  syncSongPositionToCursor();
  if (extend_selection) {
    updateLoopRangeFromSelection();
  }
}

void SongPage::syncSongPositionToCursor() {
  if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
    audio_guard_([&]() { mini_acid_.setSongPosition(cursorRow()); });
  }
}

SongTrack SongPage::trackForColumn(int col, bool& valid) const {
  valid = true;
  if (col == 0) return SongTrack::SynthA;
  if (col == 1) return SongTrack::SynthB;
  if (col == 2) return SongTrack::Drums;
  if (col == 3) return SongTrack::Voice;
  valid = false;
  return SongTrack::Voice;
}

int SongPage::bankIndexForTrack(SongTrack track) const {
  switch (track) {
    case SongTrack::SynthA:
      return mini_acid_.current303BankIndex(0);
    case SongTrack::SynthB:
      return mini_acid_.current303BankIndex(1);
    case SongTrack::Drums:
      return mini_acid_.currentDrumBankIndex();
    default:
      return 0;
  }
}

int SongPage::patternIndexFromKey(char key) const {
  switch (std::tolower(static_cast<unsigned char>(key))) {
    case 'q': return 0;
    case 'w': return 1;
    case 'e': return 2;
    case 'r': return 3;
    case 't': return 4;
    case 'y': return 5;
    case 'u': return 6;
    case 'i': return 7;
    default: return -1;
  }
}

bool SongPage::adjustSongPatternAtCursor(int delta) {
  bool trackValid = false;
  SongTrack track = trackForColumn(cursorTrack(), trackValid);
  if (!trackValid) return false;
  int row = cursorRow();
  int current = mini_acid_.songPatternAt(row, track);
  int maxPattern = kSongPatternCount - 1;
  int next = current;
  if (delta > 0) next = current < 0 ? 0 : current + 1;
  else if (delta < 0) next = current < 0 ? -1 : current - 1;
  if (next > maxPattern) next = maxPattern;
  if (next < -1) next = -1;
  if (next == current) return false;
  withAudioGuard([&]() {
    if (next < 0) mini_acid_.clearSongPattern(row, track);
    else mini_acid_.setSongPattern(row, track, next);
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      mini_acid_.setSongPosition(row);
    }
  });
  return true;
}

bool SongPage::adjustSongPlayhead(int delta) {
  int len = mini_acid_.songLength();
  if (len < 1) len = 1;
  int maxPos = len - 1;
  if (maxPos < 0) maxPos = 0;
  if (maxPos >= Song::kMaxPositions) maxPos = Song::kMaxPositions - 1;
  int current = mini_acid_.songPlayheadPosition();
  int next = current + delta;
  if (next < 0) next = 0;
  if (next > maxPos) next = maxPos;
  if (next == current) return false;
  withAudioGuard([&]() { mini_acid_.setSongPosition(next); });
  setScrollToPlayhead(next);
  return true;
}

bool SongPage::assignPattern(int patternIdx) {
  bool trackValid = false;
  SongTrack track = trackForColumn(cursorTrack(), trackValid);
  if (!trackValid || cursorOnModeButton()) return false;
  int row = cursorRow();
  int bankIndex = bankIndexForTrack(track);
  int combined = songPatternFromBank(bankIndex, patternIdx);
  withAudioGuard([&]() {
    mini_acid_.setSongPattern(row, track, combined);
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      mini_acid_.setSongPosition(row);
    }
  });
  return true;
}

bool SongPage::clearPattern() {
  bool trackValid = false;
  SongTrack track = trackForColumn(cursorTrack(), trackValid);
  if (!trackValid) return false;
  int row = cursorRow();
  
  // Save undo state
  int current_pattern = mini_acid_.songPatternAt(row, track);
  g_undo_history.action_type = UndoActionType::Delete;
  g_undo_history.saveSingleCell(row, cursorTrack(), current_pattern);
  
  withAudioGuard([&]() {
    mini_acid_.clearSongPattern(row, track);
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      mini_acid_.setSongPosition(row);
    }
  });
  return true;
}

bool SongPage::toggleSongMode() {
  withAudioGuard([&]() { mini_acid_.toggleSongMode(); });
  return true;
}

bool SongPage::toggleLoopMode() {
  if (mini_acid_.loopModeEnabled()) {
    withAudioGuard([&]() { mini_acid_.setLoopMode(false); });
    return true;
  }
  if (!has_selection_) return false;
  int min_row, max_row, min_track, max_track;
  getSelectionBounds(min_row, max_row, min_track, max_track);
  (void)min_track;
  (void)max_track;
  withAudioGuard([&]() {
    mini_acid_.setLoopRange(min_row, max_row);
    mini_acid_.setLoopMode(true);
  });
  return true;
}

void SongPage::initModeButton(int x, int y, int w, int h) {
  auto button = std::make_shared<ModeButton>(
    [this]() { return mini_acid_.songModeEnabled(); },
    [this]() { toggleSongMode(); }
  );
  button->setBoundaries(Rect(x, y, w, h));
  mode_button_container_.addChild(button);
  mode_button_initialized_ = true;
}

void SongPage::setScrollToPlayhead(int playhead) {
  if (playhead < 0) playhead = 0;
  int rowHeight = gfx_.fontHeight() + 6;
  if (rowHeight < 8) rowHeight = 8;
  int visibleRows = (gfx_.height() - 20) / rowHeight;
  if (visibleRows < 1) visibleRows = 1;
  if (scroll_row_ > playhead) scroll_row_ = playhead;
  if (scroll_row_ + visibleRows - 1 < playhead) {
    scroll_row_ = playhead - visibleRows + 1;
    if (scroll_row_ < 0) scroll_row_ = 0;
  }
}

bool SongPage::handleEvent(UIEvent& ui_event) {
  // Handle mode button clicks
  if (mode_button_initialized_ && mode_button_container_.handleEvent(ui_event)) {
    return true;
  }

  if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT) {
    bool trackValid = false;
    SongTrack track = trackForColumn(cursorTrack(), trackValid);
    if (!trackValid || cursorOnModeButton() || cursorOnPlayheadLabel()) {
      return false;
    }
    switch (ui_event.app_event_type) {
      case GROOVEPUTER_APP_EVENT_COPY: {
        if (has_selection_) {
          // Copy selected area
          int min_row, max_row, min_track, max_track;
          getSelectionBounds(min_row, max_row, min_track, max_track);
          
          // Only copy valid track columns (0-3)
          if (max_track > 3) max_track = 3;
          if (min_track > 3) return false;
          
          int rows = max_row - min_row + 1;
          int tracks = max_track - min_track + 1;
          g_song_area_clipboard.rows = rows;
          g_song_area_clipboard.tracks = tracks;
          g_song_area_clipboard.pattern_indices.clear();
          g_song_area_clipboard.pattern_indices.reserve(rows * tracks);
          
          for (int r = min_row; r <= max_row; ++r) {
            for (int t = min_track; t <= max_track; ++t) {
              bool valid = false;
              SongTrack song_track = trackForColumn(t, valid);
              int pattern = valid ? mini_acid_.songPatternAt(r, song_track) : -1;
              g_song_area_clipboard.pattern_indices.push_back(pattern);
            }
          }
          g_song_area_clipboard.has_area = true;
          g_song_pattern_clipboard.has_pattern = false; // Clear single-cell clipboard
        } else {
          // Copy single cell
          int row = cursorRow();
          g_song_pattern_clipboard.pattern_index = mini_acid_.songPatternAt(row, track);
          g_song_pattern_clipboard.has_pattern = true;
          g_song_area_clipboard.has_area = false; // Clear area clipboard
        }
        return true;
      }
      case GROOVEPUTER_APP_EVENT_CUT: {
        if (has_selection_) {
          // Copy selected area first
          int min_row, max_row, min_track, max_track;
          getSelectionBounds(min_row, max_row, min_track, max_track);
          
          // Only copy/clear valid track columns (0-3)
          if (max_track > 3) max_track = 3;
          if (min_track > 3) return false;
          
          int rows = max_row - min_row + 1;
          int tracks = max_track - min_track + 1;
          g_song_area_clipboard.rows = rows;
          g_song_area_clipboard.tracks = tracks;
          g_song_area_clipboard.pattern_indices.clear();
          g_song_area_clipboard.pattern_indices.reserve(rows * tracks);
          
          // Save undo state and copy/clear
          std::vector<int> old_patterns;
          old_patterns.reserve(rows * tracks);
          
          withAudioGuard([&]() {
            for (int r = min_row; r <= max_row; ++r) {
              for (int t = min_track; t <= max_track; ++t) {
                bool valid = false;
                SongTrack song_track = trackForColumn(t, valid);
                if (valid) {
                  int pattern = mini_acid_.songPatternAt(r, song_track);
                  g_song_area_clipboard.pattern_indices.push_back(pattern);
                  old_patterns.push_back(pattern);
                  mini_acid_.clearSongPattern(r, song_track);
                }
              }
            }
          });
          
          g_song_area_clipboard.has_area = true;
          g_song_pattern_clipboard.has_pattern = false; // Clear single-cell clipboard
          
          // Save undo history
          g_undo_history.action_type = UndoActionType::Cut;
          g_undo_history.saveArea(min_row, max_row, min_track, max_track, old_patterns);
        } else {
          // Cut single cell
          int row = cursorRow();
          int current_pattern = mini_acid_.songPatternAt(row, track);
          
          g_song_pattern_clipboard.pattern_index = current_pattern;
          g_song_pattern_clipboard.has_pattern = true;
          g_song_area_clipboard.has_area = false; // Clear area clipboard
          
          // Save undo state
          g_undo_history.action_type = UndoActionType::Cut;
          g_undo_history.saveSingleCell(row, cursorTrack(), current_pattern);
          
          withAudioGuard([&]() {
            mini_acid_.clearSongPattern(row, track);
          });
        }
        return true;
      }
      case GROOVEPUTER_APP_EVENT_PASTE: {
        if (g_song_area_clipboard.has_area) {
          // Paste area
          int start_row = cursorRow();
          int start_track = cursorTrack();
          if (start_track > 3) return false;
          
          // Save old patterns for undo
          std::vector<int> old_patterns;
          int min_row = start_row;
          int max_row = start_row + g_song_area_clipboard.rows - 1;
          int min_track = start_track;
          int max_track = start_track + g_song_area_clipboard.tracks - 1;
          if (max_track > 3) max_track = 3;
          
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
            int idx = 0;
            for (int r = 0; r < g_song_area_clipboard.rows; ++r) {
              for (int t = 0; t < g_song_area_clipboard.tracks; ++t) {
                int target_row = start_row + r;
                int target_track = start_track + t;
                if (target_row >= Song::kMaxPositions || target_track > 3) {
                  ++idx;
                  continue;
                }
                bool valid = false;
                SongTrack song_track = trackForColumn(target_track, valid);
                if (valid && idx < static_cast<int>(g_song_area_clipboard.pattern_indices.size())) {
                  int pattern = g_song_area_clipboard.pattern_indices[idx];
                  if (pattern < 0) {
                    mini_acid_.clearSongPattern(target_row, song_track);
                  } else {
                    mini_acid_.setSongPattern(target_row, song_track, pattern);
                  }
                }
                ++idx;
              }
            }
            if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
              mini_acid_.setSongPosition(start_row);
            }
          });
          
          // Save undo history
          g_undo_history.action_type = UndoActionType::Paste;
          g_undo_history.saveArea(min_row, max_row, min_track, max_track, old_patterns);
        } else if (g_song_pattern_clipboard.has_pattern) {
          // Paste single cell
          int row = cursorRow();
          int track_idx = cursorTrack();
          int patternIndex = g_song_pattern_clipboard.pattern_index;
          
          // Save old pattern for undo
          int old_pattern = mini_acid_.songPatternAt(row, track);
          g_undo_history.action_type = UndoActionType::Paste;
          g_undo_history.saveSingleCell(row, track_idx, old_pattern);
          
          withAudioGuard([&]() {
            if (patternIndex < 0) {
              mini_acid_.clearSongPattern(row, track);
            } else {
              mini_acid_.setSongPattern(row, track, patternIndex);
            }
            if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
              mini_acid_.setSongPosition(row);
            }
          });
        } else {
          return false;
        }
        return true;
      }
      case GROOVEPUTER_APP_EVENT_UNDO: {
        if (g_undo_history.action_type == UndoActionType::None || g_undo_history.cells.empty()) {
          return false;
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
        
        // Clear undo history after use
        g_undo_history.clear();
        return true;
      }
      default:
        return false;
    }
  }
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  if (ui_event.alt && (ui_event.scancode == GROOVEPUTER_UP || ui_event.scancode == GROOVEPUTER_DOWN)) {
    int delta = ui_event.scancode == GROOVEPUTER_UP ? 1 : -1;
    if (cursorOnPlayheadLabel()) return adjustSongPlayhead(delta);
    return adjustSongPatternAtCursor(delta);
  }

  bool handled = false;
  // Cardputer keyboard may not have a practical Shift key, allow Ctrl as selection modifier too.
  bool extend_selection = ui_event.shift || ui_event.ctrl;
  switch (ui_event.scancode) {
    case GROOVEPUTER_LEFT:
      moveCursorHorizontal(-1, extend_selection);
      handled = true;
      break;
    case GROOVEPUTER_RIGHT:
      moveCursorHorizontal(1, extend_selection);
      handled = true;
      break;
    case GROOVEPUTER_UP:
      moveCursorVertical(-1, extend_selection);
      handled = true;
      break;
    case GROOVEPUTER_DOWN:
      moveCursorVertical(1, extend_selection);
      handled = true;
      break;
    default:
      break;
  }
  if (handled) return true;

  char key = ui_event.key;
  if (!key) return false;

  if (ui_event.ctrl && (key == 'l' || key == 'L')) {
    return toggleLoopMode();
  }

  if (ui_event.alt) {
      if (key == 'j' || key == 'J') { withAudioGuard([&]() { mini_acid_.setActiveSongSlot(0); }); return true; }
      if (key == 'k' || key == 'K') { withAudioGuard([&]() { mini_acid_.setActiveSongSlot(1); }); return true; }
  }

  if (ui_event.ctrl) {
      if (key == 'r' || key == 'R') { withAudioGuard([&]() { mini_acid_.setSongReverse(!mini_acid_.isSongReverse()); }); return true; }
      if (key == 'm' || key == 'M') { withAudioGuard([&]() { mini_acid_.mergeSongs(); }); return true; }
      if (key == 'n' || key == 'N') { withAudioGuard([&]() { mini_acid_.alternateSongs(); }); return true; }
  }

  if (cursorOnModeButton() && (key == '\n' || key == '\r')) {
    return toggleSongMode();
  }

  if (key == 'm' || key == 'M') {
    return toggleSongMode();
  }

  int patternIdx = patternIndexFromKey(key);
  if (cursorOnModeButton() && patternIdx >= 0) return false;
  if (patternIdx >= 0) return assignPattern(patternIdx);

  // Alt + dote = Reset Song
  if (ui_event.alt && (key == '.' || ui_event.key == ',')) {
      withAudioGuard([&]() {
          for (int r = 0; r < Song::kMaxPositions; ++r) {
              mini_acid_.clearSongPattern(r, SongTrack::SynthA);
              mini_acid_.clearSongPattern(r, SongTrack::SynthB);
              mini_acid_.clearSongPattern(r, SongTrack::Drums);
              mini_acid_.clearSongPattern(r, SongTrack::Voice);
          }
      });
      // showToast("Song Cleared"); // Not available in IPage
      return true;
  }

  if (key == '\b') {
    return clearPattern();
  }

  // Tab also clears pattern
  if (key == '\t') {
    return clearPattern();
  }

  if (key == 'g' || key == 'G') {
    if (ui_event.ctrl) {
        // Ctrl+G - Cycle Mode
        cycleGeneratorMode();
        show_genre_hint_ = true;
        hint_timer_ = millis() + 2000;
        return true;
    } else if (ui_event.alt && has_selection_) {
        // Alt+G with selection - Batch generate
        int min_row, max_row, min_track, max_track;
        getSelectionBounds(min_row, max_row, min_track, max_track);
        if (max_track > 3) max_track = 3;
        withAudioGuard([&]() {
            for (int r = min_row; r <= max_row; ++r) {
                for (int t = min_track; t <= max_track; ++t) {
                    bool valid = false;
                    SongTrack track = trackForColumn(t, valid);
                    if (!valid) continue;
                    // Temporarily set cursor for context-aware generation
                    int savedRow = cursor_row_;
                    int savedTrack = cursor_track_;
                    cursor_row_ = r;
                    cursor_track_ = t;
                    generateCurrentCellPattern();
                    cursor_row_ = savedRow;
                    cursor_track_ = savedTrack;
                }
            }
        });
        return true;
    } else {
        // G - Generate
        // Check for double tap
        uint32_t now = millis();
        if (now - last_g_press_ < 300) {
            // Double tap: Generate entire row
            generateEntireRow();
            last_g_press_ = 0; // Reset
        } else {
            // Single tap (so far): Generate current cell
            generateCurrentCellPattern();
            last_g_press_ = now;
        }
        return true;
    }
  }

  return false;
}

const std::string & SongPage::getTitle() const {
  static std::string title = "SONG";
  return title;
}

void SongPage::draw(IGfx& gfx) {
  switch (visual_style_) {
    case VisualStyle::RETRO_CLASSIC:
      drawRetroClassicStyle(gfx);
      break;
    case VisualStyle::AMBER:
      drawAmberStyle(gfx);
      break;
    case VisualStyle::TE_GRID:
      drawTEGridStyle(gfx);
      break;
    case VisualStyle::MINIMAL:
    default:
      drawMinimalStyle(gfx);
      break;
  }
}

void SongPage::drawMinimalStyle(IGfx& gfx) {
  const Rect& bounds = getBoundaries();
  int x = bounds.x;
  int y = bounds.y;
  int w = bounds.w;
  int h = bounds.h;

  int body_y = y + 2;
  int body_h = h - 2;
  if (body_h <= 0) return;

  int label_h = gfx.fontHeight();
  int header_h = label_h + 4;
  int row_h = label_h + 4;
  if (row_h < 8) row_h = 8;
  int usable_h = body_h - header_h;
  if (usable_h < row_h) usable_h = row_h;
  int visible_rows = usable_h / row_h;
  if (visible_rows < 1) visible_rows = 1;

  int song_len = mini_acid_.songLength();
  int cursor_row = cursorRow();
  int playhead = mini_acid_.songPlayheadPosition();
  bool playingSong = mini_acid_.isPlaying() && mini_acid_.songModeEnabled();
  bool loopMode = mini_acid_.loopModeEnabled();

  if (playingSong) {
    int minTarget = std::min(cursor_row, playhead);
    int maxTarget = std::max(cursor_row, playhead);
    if (minTarget < scroll_row_) scroll_row_ = minTarget;
    if (maxTarget >= scroll_row_ + visible_rows) scroll_row_ = maxTarget - visible_rows + 1;
  } else {
    if (cursor_row < scroll_row_) scroll_row_ = cursor_row;
    if (cursor_row >= scroll_row_ + visible_rows) scroll_row_ = cursor_row - visible_rows + 1;
  }
  if (scroll_row_ < 0) scroll_row_ = 0;
  int maxStart = Song::kMaxPositions - visible_rows;
  if (maxStart < 0) maxStart = 0;
  if (scroll_row_ > maxStart) scroll_row_ = maxStart;

  int pos_col_w = 20;
  int spacing = 2;
  int modeBtnW = 55;
  int track_col_w = (w - pos_col_w - spacing * 6 - modeBtnW) / 4;
  if (track_col_w < 20) track_col_w = 20;

  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x, body_y, "POS");
  gfx.setTextColor(song303Color(0));
  gfx.drawText(x + pos_col_w + spacing, body_y, "303A");
  gfx.setTextColor(song303Color(1));
  gfx.drawText(x + pos_col_w + spacing + track_col_w, body_y, "303B");
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x + pos_col_w + spacing + track_col_w * 2, body_y, "Drums");
  gfx.drawText(x + pos_col_w + spacing + track_col_w * 3, body_y, "Voice");
  char lenBuf[32];
  snprintf(lenBuf, sizeof(lenBuf), "PLYHD %d:%d", playhead + 1, song_len);
  int lenX = x + pos_col_w + spacing + track_col_w * 4 + spacing + 5;
  int lenW = textWidth(gfx, lenBuf);
  bool playheadSelected = cursorOnPlayheadLabel();
  if (playheadSelected) {
    gfx.drawRect(lenX - 2, body_y - 1, lenW + 4, label_h + 2, COLOR_STEP_SELECTED);
  }
  gfx.drawText(lenX, body_y, lenBuf);

  if (loopMode) {
    int loopStart = mini_acid_.loopStartRow();
    int loopEnd = mini_acid_.loopEndRow();
    char loopBuf[32];
    snprintf(loopBuf, sizeof(loopBuf), "LOOP %d-%d", loopStart + 1, loopEnd + 1);
    int loopX = lenX + lenW + 8;
    gfx.setTextColor(IGfxColor::Yellow());
    gfx.drawText(loopX, body_y, loopBuf);
    gfx.setTextColor(COLOR_WHITE);
  }

  drawGeneratorHint(gfx);

  int grid_y = body_y + header_h;
  for (int i = 0; i < visible_rows; ++i) {
    int row_idx = scroll_row_ + i;
    if (row_idx >= Song::kMaxPositions) break;
    int ry = grid_y + i * row_h;
    if (row_idx == playhead && playingSong) {
      gfx.fillRect(x, ry, w, row_h, COLOR_PANEL);
    }
    char posBuf[16];
    snprintf(posBuf, sizeof(posBuf), "%02d", row_idx + 1);
    gfx.setTextColor(COLOR_MUTED);
    gfx.drawText(x, ry + 2, posBuf);

    for (int t = 0; t < 4; ++t) {
      bool valid = false;
      SongTrack track = trackForColumn(t, valid);
      int pattern = mini_acid_.songPatternAt(row_idx, track);
      int tx = x + pos_col_w + spacing + t * (track_col_w + spacing);
      bool isSelected = false;
      if (has_selection_) {
          int min_r, max_r, min_t, max_t;
          getSelectionBounds(min_r, max_r, min_t, max_t);
          isSelected = row_idx >= min_r && row_idx <= max_r && t >= min_t && t <= max_t;
      } else {
          isSelected = row_idx == cursor_row && t == cursor_track_;
      }
      
      if (isSelected) {
        gfx.drawRect(tx - 1, ry, track_col_w + 1, row_h, isSelected && !has_selection_ ? COLOR_STEP_SELECTED : COLOR_STEP_HILIGHT);
      }
      if (pattern >= 0) {
        char patBuf[16];
        snprintf(patBuf, sizeof(patBuf), "%d-%d", (pattern / 8) + 1, (pattern % 8) + 1);
        gfx.setTextColor(t < 2 ? song303Color(t) : COLOR_WHITE);
        gfx.drawText(tx + 2, ry + 2, patBuf);
      } else {
        gfx.setTextColor(COLOR_GRAY);
        gfx.drawText(tx + 2, ry + 2, "---");
      }
    }
    if (row_idx == playhead) {
      gfx.setTextColor(COLOR_ACCENT);
      gfx.drawText(x + w - 15, ry + 2, ">>");
    }
  }

  int modeX = x + pos_col_w + spacing + track_col_w * 4 + spacing + 5;
  int modeW = w - modeX - 5;
  int modeY = body_y + header_h;
  int modeH = visible_rows * row_h;
  if (!mode_button_initialized_) {
    initModeButton(modeX, modeY, modeW, modeH);
  } else {
    mode_button_container_.setBoundaries(Rect(modeX, modeY, modeW, modeH));
    bool modeSelected = cursorOnModeButton();
    if (modeSelected) {
      gfx.drawRect(modeX - 2, modeY - 2, modeW + 4, modeH + 4, COLOR_STEP_SELECTED);
    }
    mode_button_container_.draw(gfx);
  }
}

void SongPage::drawTEGridStyle(IGfx& gfx) {
  const Rect& bounds = getBoundaries();
  int x = bounds.x;
  int y = bounds.y;
  int w = bounds.w;
  int h = bounds.h;

  // TE color palette: strict monochrome
  const IGfxColor TE_BLACK = IGfxColor::Black();
  const IGfxColor TE_WHITE = IGfxColor::White();
  const IGfxColor TE_GRID = IGfxColor(0x404040);
  const IGfxColor TE_ACCENT = IGfxColor(0xC0C0C0);
  const IGfxColor TE_DIM = IGfxColor(0x808080);

  // Clear background
  gfx.fillRect(x, y, w, h, TE_BLACK);

  // === HEADER: Brutalist bar ===
  int header_h = 11;
  gfx.fillRect(x, y, w, header_h, TE_WHITE);
  gfx.setTextColor(TE_BLACK);

  // Title
  char titleBuf[32];
  int slot = mini_acid_.activeSongSlot();
  bool reverse = mini_acid_.isSongReverse();
  snprintf(titleBuf, sizeof(titleBuf), "SONG %c%s", 'A' + slot, reverse ? " REV" : "");
  gfx.drawText(x + 2, y + 2, titleBuf);

  // Status indicators (right-aligned)
  char statusBuf[32];
  int pos = mini_acid_.currentSongPosition() + 1;
  int len = mini_acid_.songLength();
  bool playing = mini_acid_.isPlaying();
  bool songMode = mini_acid_.songModeEnabled();
  snprintf(statusBuf, sizeof(statusBuf), "%s %03d/%03d",
           playing ? (songMode ? ">" : "||") : "[]", pos, len);
  int statusW = textWidth(gfx, statusBuf);
  gfx.drawText(x + w - statusW - 2, y + 2, statusBuf);

  // === GRID LAYOUT ===
  int grid_y = y + header_h + 2;
  int grid_h = h - header_h - 14; // Leave space for footer

  // Grid parameters (TE style: tight, square cells)
  int cell_h = 9;
  int cell_w = 28;
  int pos_w = 18;
  int track_count = 4;
  int spacing = 1;

  int visible_rows = grid_h / (cell_h + spacing);
  if (visible_rows < 1) visible_rows = 1;

  // Scroll logic
  int cursor_row = cursorRow();
  int playhead = mini_acid_.songPlayheadPosition();

  if (playing && songMode) {
    int minTarget = std::min(cursor_row, playhead);
    int maxTarget = std::max(cursor_row, playhead);
    if (minTarget < scroll_row_) scroll_row_ = minTarget;
    if (maxTarget >= scroll_row_ + visible_rows) scroll_row_ = maxTarget - visible_rows + 1;
  } else {
    if (cursor_row < scroll_row_) scroll_row_ = cursor_row;
    if (cursor_row >= scroll_row_ + visible_rows) scroll_row_ = cursor_row - visible_rows + 1;
  }
  if (scroll_row_ < 0) scroll_row_ = 0;

  // === COLUMN HEADERS ===
  int header_y = grid_y;
  gfx.setTextColor(TE_DIM);
  gfx.drawText(x + 2, header_y, "#");

  const char* track_labels[] = {"A", "B", "D", "V"};
  for (int t = 0; t < track_count; ++t) {
    int tx = x + pos_w + t * (cell_w + spacing);
    gfx.drawText(tx + 2, header_y, track_labels[t]);
  }

  // Separator line
  gfx.drawLine(x, header_y + 10, x + w - 1, header_y + 10, TE_GRID);

  // === GRID CELLS ===
  int cells_y = header_y + 12;

  int min_sel_row, max_sel_row, min_sel_track, max_sel_track;
  if (has_selection_) {
    getSelectionBounds(min_sel_row, max_sel_row, min_sel_track, max_sel_track);
  }

  for (int i = 0; i < visible_rows; ++i) {
    int row_idx = scroll_row_ + i;
    if (row_idx >= Song::kMaxPositions) break;

    int ry = cells_y + i * (cell_h + spacing);

    // Row number
    char rowBuf[8];
    snprintf(rowBuf, sizeof(rowBuf), "%02d", row_idx + 1);
    gfx.setTextColor(row_idx < len ? TE_ACCENT : TE_DIM);
    gfx.drawText(x + 2, ry + 1, rowBuf);

    // Playhead indicator
    if (row_idx == playhead && playing && songMode) {
      gfx.fillRect(x, ry, pos_w - 2, cell_h, TE_GRID);
    }

    // Track cells
    for (int t = 0; t < track_count; ++t) {
      int tx = x + pos_w + t * (cell_w + spacing);

      bool valid = false;
      SongTrack track = trackForColumn(t, valid);
      int pattern = mini_acid_.songPatternAt(row_idx, track);

      // Selection check
      bool isSelected = false;
      if (has_selection_) {
        isSelected = row_idx >= min_sel_row && row_idx <= max_sel_row &&
                     t >= min_sel_track && t <= max_sel_track;
      } else {
        isSelected = row_idx == cursor_row && t == cursor_track_;
      }

      // Cell background
      IGfxColor cellBg = TE_BLACK;
      if (isSelected) cellBg = TE_ACCENT;
      else if (row_idx == playhead && playing && songMode) cellBg = TE_GRID;

      gfx.fillRect(tx, ry, cell_w, cell_h, cellBg);

      // Cell border (TE style: always visible grid)
      gfx.drawRect(tx, ry, cell_w, cell_h, TE_GRID);

      // Pattern content
      if (pattern >= 0) {
        int bank = (pattern / 8) + 1;
        int slot = (pattern % 8) + 1;
        char patBuf[8];
        snprintf(patBuf, sizeof(patBuf), "%d-%d", bank, slot);

        IGfxColor textColor = isSelected ? TE_BLACK : TE_WHITE;
        gfx.setTextColor(textColor);
        gfx.drawText(tx + 2, ry + 1, patBuf);
      } else {
        gfx.setTextColor(isSelected ? TE_BLACK : TE_DIM);
        gfx.drawText(tx + 2, ry + 1, "--");
      }
    }
  }

  // === FOOTER: Command bar (TE style) ===
  int footer_y = y + h - 11;
  gfx.drawLine(x, footer_y - 1, x + w - 1, footer_y - 1, TE_GRID);

  gfx.setTextColor(TE_DIM);
  const char* cmd_text = "Q-I:PAT G:GEN A+J/K:SLOT C+R:REV C+M:MRG";
  gfx.drawText(x + 2, footer_y + 2, cmd_text);

  // Loop indicator
  if (mini_acid_.loopModeEnabled()) {
    int loopStart = mini_acid_.loopStartRow() + 1;
    int loopEnd = mini_acid_.loopEndRow() + 1;
    char loopBuf[16];
    snprintf(loopBuf, sizeof(loopBuf), "LP:%d-%d", loopStart, loopEnd);
    int loopW = textWidth(gfx, loopBuf);
    gfx.fillRect(x + w - loopW - 4, footer_y, loopW + 4, 11, TE_WHITE);
    gfx.setTextColor(TE_BLACK);
    gfx.drawText(x + w - loopW - 2, footer_y + 2, loopBuf);
  }

  // Generator hint overlay
  drawGeneratorHint(gfx);
}

void SongPage::drawRetroClassicStyle(IGfx& gfx) {
    const Rect& bounds = getBoundaries();
    int x = bounds.x;
    int y = bounds.y;
    int w = bounds.w;
    int h = bounds.h;

    char modeBuf[32];
    std::snprintf(modeBuf, sizeof(modeBuf), "%s", mini_acid_.songModeEnabled() ? "PLAY" : "EDIT");
    const char* title = mini_acid_.isSongReverse() ? "SONG REV" : "SONG";
    retro::drawHeaderBar(gfx, x, y, w, 12, title, modeBuf, mini_acid_.isPlaying(), (int)mini_acid_.bpm(), mini_acid_.currentSongPosition());

    int slotX = x + 4;
    int slotY = y + 14;
    retro::SelectorConfig slotCfg;
    slotCfg.x = slotX; slotCfg.y = slotY; slotCfg.w = 60; slotCfg.h = 10;
    slotCfg.label = "SLOT";
    slotCfg.count = 2;
    slotCfg.selected = mini_acid_.activeSongSlot();
    slotCfg.cursor = slotCfg.selected;
    slotCfg.showCursor = false;
    slotCfg.enabled = true;
    retro::drawSelector(gfx, slotCfg);

    // Draw grid headers
    int row_h = 10;
    int header_y = y + 26;
    int grid_y = header_y + 12;
    int pos_w = 25;
    int track_w = (w - pos_w - 4) / 4;

    gfx.setTextColor(IGfxColor(RetroTheme::TEXT_SECONDARY));
    gfx.drawText(x + 4, header_y, "POS");
    gfx.setTextColor(IGfxColor(RetroTheme::NEON_ORANGE));
    gfx.drawText(x + pos_w + 4, header_y, "A");
    gfx.setTextColor(IGfxColor(RetroTheme::NEON_MAGENTA));
    gfx.drawText(x + pos_w + track_w + 4, header_y, "B");
    gfx.setTextColor(IGfxColor(RetroTheme::NEON_CYAN));
    gfx.drawText(x + pos_w + track_w * 2 + 4, header_y, "DR");
    gfx.setTextColor(IGfxColor(RetroTheme::TEXT_SECONDARY));
    gfx.drawText(x + pos_w + track_w * 3 + 4, header_y, "VO");

    int visible_rows = (h - grid_y - 12) / row_h;
    if (visible_rows < 1) visible_rows = 1;
    
    int cursor_row = cursorRow();
    int playhead = mini_acid_.songPlayheadPosition();
    bool songMode = mini_acid_.songModeEnabled();

    // Scroll logic
    if (cursor_row < scroll_row_) scroll_row_ = cursor_row;
    if (cursor_row >= scroll_row_ + visible_rows) scroll_row_ = cursor_row - visible_rows + 1;
    if (scroll_row_ < 0) scroll_row_ = 0;

    for (int i = 0; i < visible_rows; ++i) {
        int ridx = scroll_row_ + i;
        if (ridx >= Song::kMaxPositions) break;
        int ry = grid_y + i * row_h;

        if (ridx == playhead && mini_acid_.isPlaying() && songMode) {
            gfx.fillRect(x, ry, w, row_h, IGfxColor(RetroTheme::BG_INSET));
        }

        char buf[16];
        int rowLabel = ridx + 1;
        if (rowLabel < 0) rowLabel = 0;
        if (rowLabel > 9999) rowLabel = 9999;
        std::snprintf(buf, sizeof(buf), "%02d", rowLabel);
        gfx.setTextColor(IGfxColor(RetroTheme::TEXT_DIM));
        gfx.drawText(x + 4, ry, buf);

        for (int t = 0; t < 4; ++t) {
            int tx = x + pos_w + t * track_w + 4;
            bool valid = false;
            int pattern = mini_acid_.songPatternAt(ridx, trackForColumn(t, valid));
            bool isSelected = false;
            if (has_selection_) {
                int min_r, max_r, min_t, max_t;
                getSelectionBounds(min_r, max_r, min_t, max_t);
                isSelected = ridx >= min_r && ridx <= max_r && t >= min_t && t <= max_t;
            } else {
                isSelected = ridx == cursor_row && t == cursor_track_;
            }

            if (isSelected) {
                gfx.drawRect(tx - 2, ry - 1, track_w, row_h, IGfxColor(RetroTheme::SELECT_BRIGHT));
            }

            if (pattern >= 0) {
                int bank = (pattern / 8) + 1;
                int slot = (pattern % 8) + 1;
                if (bank < 0) bank = 0;
                if (bank > 999) bank = 999;
                if (slot < 0) slot = 0;
                if (slot > 99) slot = 99;
                std::snprintf(buf, sizeof(buf), "%d-%d", bank, slot);
                IGfxColor pColor = IGfxColor(RetroTheme::TEXT_PRIMARY);
                if (t == 0) pColor = IGfxColor(RetroTheme::NEON_ORANGE);
                else if (t == 1) pColor = IGfxColor(RetroTheme::NEON_MAGENTA);
                else if (t == 2) pColor = IGfxColor(RetroTheme::NEON_CYAN);
                gfx.setTextColor(pColor);
                gfx.drawText(tx, ry, buf);
            } else {
                gfx.setTextColor(IGfxColor(RetroTheme::GRID_DIM));
                gfx.drawText(tx, ry, "--");
            }
        }
    }

    retro::drawFooterBar(gfx, x, y + h - 12, w, 12, "A+J/K:Slot C+R:Rev C+M:Mrg C+N:Alt", "SONG");
}

void SongPage::drawAmberStyle(IGfx& gfx) {
    const Rect& bounds = getBoundaries();
    int x = bounds.x;
    int y = bounds.y;
    int w = bounds.w;
    int h = bounds.h;

    char modeBuf[32];
    std::snprintf(modeBuf, sizeof(modeBuf), "%s", mini_acid_.songModeEnabled() ? "PLAY" : "EDIT");
    const char* title = mini_acid_.isSongReverse() ? "SONG REV" : "SONG";
    amber::drawHeaderBar(gfx, x, y, w, 12, title, modeBuf, mini_acid_.isPlaying(), (int)mini_acid_.bpm(), mini_acid_.currentSongPosition());

    amber::SelectionBarConfig slotCfg;
    slotCfg.x = x + 4; slotCfg.y = y + 14; slotCfg.w = 60; slotCfg.h = 10;
    slotCfg.label = "SLOT";
    slotCfg.count = 2;
    slotCfg.selected = mini_acid_.activeSongSlot();
    slotCfg.cursor = slotCfg.selected;
    slotCfg.showCursor = false;
    // slotCfg.enabled = true; // remove if not in struct
    amber::drawSelectionBar(gfx, slotCfg);

    int row_h = 10;
    int grid_y = y + 38;
    int pos_w = 25;
    int track_w = (w - pos_w - 4) / 4;

    int visible_rows = (h - grid_y - 12) / row_h;
    if (visible_rows < 1) visible_rows = 1;

    int cursor_row = cursorRow();
    int playhead = mini_acid_.songPlayheadPosition();
    bool songMode = mini_acid_.songModeEnabled();

    if (cursor_row < scroll_row_) scroll_row_ = cursor_row;
    if (cursor_row >= scroll_row_ + visible_rows) scroll_row_ = cursor_row - visible_rows + 1;
    if (scroll_row_ < 0) scroll_row_ = 0;

    for (int i = 0; i < visible_rows; ++i) {
        int ridx = scroll_row_ + i;
        if (ridx >= Song::kMaxPositions) break;
        int ry = grid_y + i * row_h;

        if (ridx == playhead && mini_acid_.isPlaying() && songMode) {
            gfx.fillRect(x, ry, w, row_h, IGfxColor(AmberTheme::BG_INSET));
        }

        char buf[16];
        int rowLabel = ridx + 1;
        if (rowLabel < 0) rowLabel = 0;
        if (rowLabel > 9999) rowLabel = 9999;
        std::snprintf(buf, sizeof(buf), "%02d", rowLabel);
        gfx.setTextColor(IGfxColor(AmberTheme::TEXT_DIM));
        gfx.drawText(x + 4, ry, buf);

        for (int t = 0; t < 4; ++t) {
            int tx = x + pos_w + t * track_w + 4;
            bool valid = false;
            int pattern = mini_acid_.songPatternAt(ridx, trackForColumn(t, valid));
            bool isSelected = false;
            if (has_selection_) {
                int min_r, max_r, min_t, max_t;
                getSelectionBounds(min_r, max_r, min_t, max_t);
                isSelected = ridx >= min_r && ridx <= max_r && t >= min_t && t <= max_t;
            } else {
                isSelected = ridx == cursor_row && t == cursor_track_;
            }

            if (isSelected) {
                gfx.drawRect(tx - 2, ry - 1, track_w, row_h, IGfxColor(AmberTheme::SELECT_BRIGHT));
            }

            if (pattern >= 0) {
                int bank = (pattern / 8) + 1;
                int slot = (pattern % 8) + 1;
                if (bank < 0) bank = 0;
                if (bank > 999) bank = 999;
                if (slot < 0) slot = 0;
                if (slot > 99) slot = 99;
                std::snprintf(buf, sizeof(buf), "%d-%d", bank, slot);
                gfx.setTextColor(IGfxColor(AmberTheme::TEXT_PRIMARY));
                gfx.drawText(tx, ry, buf);
            } else {
                gfx.setTextColor(IGfxColor(AmberTheme::GRID_DIM));
                gfx.drawText(tx, ry, "--");
            }
        }
    }

    amber::drawFooterBar(gfx, x, y + h - 12, w, 12, "A+J/K:Slot C+R:Rev C+M:Mrg C+N:Alt", "SONG");
}

std::unique_ptr<MultiPageHelpDialog> SongPage::getHelpDialog() {
  return std::make_unique<MultiPageHelpDialog>(*this);
}

int SongPage::getHelpFrameCount() const {
  return 2;
}

void SongPage::drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const {
  if (bounds.w <= 0 || bounds.h <= 0) return;
  switch (frameIndex) {
    case 0:
      drawHelpPageSong(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    case 1:
      drawHelpPageSongCont(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    default:
      break;
  }
}

void SongPage::cycleGeneratorMode() {
    int mode = static_cast<int>(gen_mode_);
    mode = (mode + 1) % SmartPatternGenerator::Mode::COUNT;
    gen_mode_ = static_cast<SmartPatternGenerator::Mode>(mode);
}

void SongPage::drawGeneratorHint(IGfx& gfx) {
    if (!show_genre_hint_ || millis() > hint_timer_) {
        show_genre_hint_ = false;
        return;
    }
    
    const char* mode_names[] = { "RND", "SMART", "EVOL", "FILL" };
    int modeIdx = static_cast<int>(gen_mode_);
    const char* current_mode = (modeIdx < 4) ? mode_names[modeIdx] : "?";
    
    int hintW = 60;
    int hintH = 12;
    int hintX = gfx.width() - hintW - 60; // Left of mode button
    int hintY = 2;
    
    // Background
    gfx.fillRect(hintX, hintY, hintW, hintH, COLOR_BLACK);
    gfx.drawRect(hintX, hintY, hintW, hintH, COLOR_ACCENT);
    
    gfx.setTextColor(COLOR_WHITE);
    char buf[16];
    snprintf(buf, sizeof(buf), "GEN:%s", current_mode);
    gfx.drawText(hintX + 4, hintY + 2, buf);
}

bool SongPage::generateCurrentCellPattern() {
    int row = cursorRow();
    int col = cursorTrack();
    
    bool valid = false;
    SongTrack track = trackForColumn(col, valid);
    if (!valid) return false;
    
    int current_bank = bankIndexForTrack(track);
    int patternIdx = -1;
    
    // Get current pattern info
    int current = mini_acid_.songPatternAt(row, track);
    int current_idx = (current < 0) ? -1 : (current % 8);
    
    // FillAuto mode: interpolate between neighbors
    if (gen_mode_ == SmartPatternGenerator::PG_FILL) {
        int prevPattern = (row > 0) ? mini_acid_.songPatternAt(row - 1, track) : -1;
        int nextPattern = (row < Song::kMaxPositions - 1) ? mini_acid_.songPatternAt(row + 1, track) : -1;
        
        if (prevPattern >= 0 && nextPattern >= 0) {
            // Both exist: average
            int prevIdx = prevPattern % 8;
            int nextIdx = nextPattern % 8;
            patternIdx = (prevIdx + nextIdx) / 2;
        } else if (prevPattern >= 0) {
            // Only prev: slight evolution
            int prevIdx = prevPattern % 8;
            patternIdx = prevIdx + ((rand() % 3) - 1);  // -1, 0, +1
            if (patternIdx < 0) patternIdx = 0;
            if (patternIdx > 7) patternIdx = 7;
        } else if (nextPattern >= 0) {
            // Only next: lead-in
            int nextIdx = nextPattern % 8;
            patternIdx = (nextIdx > 0) ? nextIdx - 1 : 0;
        } else {
            // No context: fall back to genre-smart
            gen_mode_ = SmartPatternGenerator::PG_GENRE;
        }
    }
    
    // GenreSmart mode: use GenerativeParams
    if (gen_mode_ == SmartPatternGenerator::PG_GENRE && patternIdx < 0) {
        const auto& genParams = mini_acid_.genreManager().getGenerativeParams();
        
        if (track == SongTrack::Drums) {
            // Drums: sparse = simple, dense = complex
            if (genParams.sparseKick && genParams.sparseHats) {
                patternIdx = rand() % 3;  // 0-2: simple
            } else if (!genParams.sparseKick && !genParams.sparseHats) {
                patternIdx = 4 + (rand() % 3);  // 4-6: complex
            } else {
                patternIdx = 2 + (rand() % 3);  // 2-4: medium
            }
        } else if (track == SongTrack::SynthA || track == SongTrack::SynthB) {
            // Synths: complexity from note count + slides
            float avgNotes = (genParams.minNotes + genParams.maxNotes) / 2.0f;
            float complexity = avgNotes / 16.0f;  // Normalize 0-1
            complexity += genParams.slideProbability * 0.3f;
            if (complexity > 1.0f) complexity = 1.0f;
            
            patternIdx = (int)(complexity * 6.0f);  // Map to 0-6
            patternIdx += (rand() % 3) - 1;  // Add variance
            if (patternIdx < 0) patternIdx = 0;
            if (patternIdx > 6) patternIdx = 6;
        } else {
            // Voice: random phrase
            patternIdx = rand() % 8;
        }
    }
    
    // Fallback to other modes via SmartPatternGenerator
    if (patternIdx < 0) {
        uint8_t track_id = 0;
        if (track == SongTrack::SynthB) track_id = 1;
        else if (track == SongTrack::Drums) track_id = 2;
        else if (track == SongTrack::Voice) track_id = 3;
        
        uint32_t new_pattern = generator_.generatePattern(
            gen_mode_,
            mini_acid_.genreManager().generativeMode(),
            track_id,
            (current_idx >= 0) ? current_idx : 99
        );
        patternIdx = new_pattern % 8;
    }
    
    int final_pattern = current_bank * 8 + patternIdx;
    
    withAudioGuard([&]() {
        mini_acid_.setSongPattern(row, track, final_pattern);
        if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
            mini_acid_.setSongPosition(row);
        }
    });
    
    return true;
}

void SongPage::generateEntireRow() {
    int row = cursorRow();
    int current_bank_A = mini_acid_.current303BankIndex(0);
    int current_bank_B = mini_acid_.current303BankIndex(1);
    int current_bank_Drums = mini_acid_.currentDrumBankIndex();
    
    withAudioGuard([&]() {
        for (int col = 0; col < 4; col++) {
            bool valid = false;
            SongTrack track = trackForColumn(col, valid);
            if (!valid) continue;
            
            uint32_t current = mini_acid_.songPatternAt(row, track);
            uint32_t current_idx = (current < 0) ? 99 : (uint32_t)current;
            
            uint8_t track_id = 0;
            int bank = 0;
            
            if (track == SongTrack::SynthA) { track_id = 0; bank = current_bank_A; }
            else if (track == SongTrack::SynthB) { track_id = 1; bank = current_bank_B; }
            else if (track == SongTrack::Drums) { track_id = 2; bank = current_bank_Drums; }
            else if (track == SongTrack::Voice) { track_id = 3; bank = 0; }
            
             uint32_t new_pattern = generator_.generatePattern(
                gen_mode_,
                mini_acid_.genreManager().generativeMode(),
                track_id,
                current_idx
            );
            
            int final_pattern = bank * 8 + (new_pattern % 8);
            mini_acid_.setSongPattern(row, track, final_pattern);
        }
        
        if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
            mini_acid_.setSongPosition(row);
        }
    });
}
