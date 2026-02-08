#include "song_page.h"
#include "../../../scenes.h"
#include "../../debug_log.h"
#include "../key_normalize.h"
#include "../ui_input.h"
#include "../ui_common.h"
#include "../retro_widgets.h"
#include "../amber_widgets.h"
#include "../retro_ui_theme.h"
#include "../amber_ui_theme.h"

namespace retro = RetroWidgets;
namespace amber = AmberWidgets;
using UI::showToast;

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

inline void formatSongPatternLabel(int pattern, char* out, int outSize) {
  if (!out || outSize <= 0) return;
  if (pattern < 0) {
    std::snprintf(out, outSize, "---");
    return;
  }
  const int bank = songPatternBank(pattern);
  const int slot = songPatternIndexInBank(pattern) + 1;
  if (bank < 0 || slot <= 0) {
    std::snprintf(out, outSize, "---");
    return;
  }
  std::snprintf(out, outSize, "%c-%d", static_cast<char>('A' + bank), slot);
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

struct SongSlotClipboard {
  bool has_song = false;
  Song song;
  int source_slot = 0;
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
SongSlotClipboard g_song_slot_clipboard;
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
    selection_locked_(false),
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
  int maxTrack = maxEditableTrackColumn();
  if (track > maxTrack) track = maxTrack;
  return track;
}

bool SongPage::cursorOnModeButton() const { return false; }
bool SongPage::cursorOnPlayheadLabel() const { return false; }

int SongPage::visibleTrackCount() const { return 3; }

int SongPage::maxEditableTrackColumn() const { return visibleTrackCount() - 1; }

SongTrack SongPage::thirdLaneTrack() const {
  return voice_lane_visible_ ? SongTrack::Voice : SongTrack::Drums;
}

const char* SongPage::laneShortLabel() const { return voice_lane_visible_ ? "VO" : "DR"; }

bool SongPage::hasVoiceDataInSlot(int slot) const {
  if (slot < 0 || slot > 1) return false;
  int len = mini_acid_.songLength();
  if (slot != mini_acid_.activeSongSlot()) {
    // Fall back to full max range for non-active slot, guarded by API bounds.
    len = Song::kMaxPositions;
  }
  for (int r = 0; r < len; ++r) {
    int pat = mini_acid_.songPatternAtSlot(slot, r, SongTrack::Voice);
    if (pat >= 0) return true;
  }
  return false;
}

bool SongPage::hasVoiceDataInActiveSlot() const {
  return hasVoiceDataInSlot(mini_acid_.activeSongSlot());
}

void SongPage::startSelection() {
  has_selection_ = true;
  selection_locked_ = false;
  selection_start_row_ = cursor_row_;
  selection_start_track_ = cursorTrack();
  LOG_DEBUG_UI("Selection START anchor=(r%d,c%d)", selection_start_row_, selection_start_track_);
}

void SongPage::updateSelection() {
  if (!has_selection_) {
    startSelection();
  } else {
    LOG_DEBUG_UI("Selection UPDATE anchor=(r%d,c%d) cursor=(r%d,c%d)",
                 selection_start_row_, selection_start_track_, cursor_row_, cursor_track_);
  }
}

void SongPage::clearSelection() {
  if (has_selection_) {
    LOG_DEBUG_UI("Selection CLEAR anchor=(r%d,c%d) cursor=(r%d,c%d)",
                 selection_start_row_, selection_start_track_, cursor_row_, cursor_track_);
  }
  has_selection_ = false;
  selection_locked_ = false;
  if (mini_acid_.loopModeEnabled()) {
    audio_guard_([&]() { mini_acid_.setLoopMode(false); });
  }
}

bool SongPage::moveSelectionFrameBy(int deltaRow, int deltaTrack) {
  if (!has_selection_) return false;
  int min_row, max_row, min_track, max_track;
  getSelectionBounds(min_row, max_row, min_track, max_track);

  int dst_min_row = min_row + deltaRow;
  int dst_max_row = max_row + deltaRow;
  int dst_min_track = min_track + deltaTrack;
  int dst_max_track = max_track + deltaTrack;

  int maxTrack = maxEditableTrackColumn();
  if (dst_min_row < 0 || dst_max_row >= Song::kMaxPositions) return false;
  if (dst_min_track < 0 || dst_max_track > maxTrack) return false;

  selection_start_row_ += deltaRow;
  selection_start_track_ += deltaTrack;
  cursor_row_ += deltaRow;
  cursor_track_ += deltaTrack;
  syncSongPositionToCursor();
  updateLoopRangeFromSelection();
  return true;
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
  int maxTrack = maxEditableTrackColumn();
  int currentTrack = cursor_track_;
  if (currentTrack < 0) currentTrack = 0;
  if (currentTrack > maxTrack) currentTrack = maxTrack;

  if (!has_selection_) {
    min_row = max_row = cursor_row_;
    min_track = max_track = currentTrack;
    return;
  }
  int startTrack = selection_start_track_;
  if (startTrack < 0) startTrack = 0;
  if (startTrack > maxTrack) startTrack = maxTrack;
  min_row = std::min(selection_start_row_, cursor_row_);
  max_row = std::max(selection_start_row_, cursor_row_);
  min_track = std::min(startTrack, currentTrack);
  max_track = std::max(startTrack, currentTrack);
}

void SongPage::moveCursorHorizontal(int delta, bool extend_selection) {
  int before_row = cursor_row_;
  int before_track = cursor_track_;
  if (extend_selection) {
    updateSelection();
  }
  // Don't clear selection on plain arrow - Cardputer fires arrow-without-Ctrl
  // when Ctrl is released before arrow key.  Selection is cleared on action
  // (pattern assign, ESC, etc.) instead.
  int track = cursorTrack();
  track += delta;
  if (track < 0) track = 0;
  int maxTrack = maxEditableTrackColumn();
  if (track > maxTrack) track = maxTrack;
  cursor_track_ = track;
  syncSongPositionToCursor();
  if (extend_selection) {
    updateLoopRangeFromSelection();
  }
  LOG_DEBUG_INPUT("Cursor H delta=%d extend=%d (%d,%d)->(%d,%d) sel=%d",
                  delta, (int)extend_selection,
                  before_row, before_track, cursor_row_, cursor_track_, (int)has_selection_);
}

void SongPage::moveCursorVertical(int delta, bool extend_selection) {
  if (delta == 0) return;
  int before_row = cursor_row_;
  int before_track = cursor_track_;
  if (cursorOnPlayheadLabel() || cursorOnModeButton()) {
    moveCursorHorizontal(delta, extend_selection);
    return;
  }
  if (extend_selection) {
    updateSelection();
  }
  // Same as horizontal: don't clear selection on plain arrow movement
  int row = cursorRow();
  row += delta;
  row = clampCursorRow(row);
  cursor_row_ = row;
  syncSongPositionToCursor();
  if (extend_selection) {
    updateLoopRangeFromSelection();
  }
  LOG_DEBUG_INPUT("Cursor V delta=%d extend=%d (%d,%d)->(%d,%d) sel=%d",
                  delta, (int)extend_selection,
                  before_row, before_track, cursor_row_, cursor_track_, (int)has_selection_);
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
  if (col == 2) return thirdLaneTrack();
  valid = false;
  return SongTrack::Drums;
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
  // Key is already normalized (lowercase) at input source
  // Use centralized QWERTY→pattern mapping
  return qwertyToPatternIndex(key);
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

bool SongPage::flipSongPatternBankAtCursorOrSelection() {
  auto flipPattern = [](int pattern) -> int {
    if (pattern < 0) return pattern;
    int bank = songPatternBank(pattern);
    int slot = songPatternIndexInBank(pattern);
    if (bank < 0 || slot < 0) return pattern;
    int flippedBank = (bank == 0) ? 1 : 0;
    return songPatternFromBank(flippedBank, slot);
  };

  if (has_selection_) {
    int min_row, max_row, min_track, max_track;
    getSelectionBounds(min_row, max_row, min_track, max_track);
    if (min_track < 0) min_track = 0;
    int maxCol = maxEditableTrackColumn();
    if (max_track > maxCol) max_track = maxCol;
    if (min_track > max_track) return false;

    int changed = 0;
    withAudioGuard([&]() {
      for (int r = min_row; r <= max_row; ++r) {
        for (int t = min_track; t <= max_track; ++t) {
          bool valid = false;
          SongTrack track = trackForColumn(t, valid);
          if (!valid) continue;
          int current = mini_acid_.songPatternAt(r, track);
          int next = flipPattern(current);
          if (next != current) {
            mini_acid_.setSongPattern(r, track, next);
            ++changed;
          }
        }
      }
      if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
        mini_acid_.setSongPosition(min_row);
      }
    });

    clearSelection();
    if (changed > 0) {
      char toast[40];
      std::snprintf(toast, sizeof(toast), "Bank flip: %d", changed);
      showToast(toast, 900);
    }
    return changed > 0;
  }

  bool trackValid = false;
  SongTrack track = trackForColumn(cursorTrack(), trackValid);
  if (!trackValid) return false;
  int row = cursorRow();
  int current = mini_acid_.songPatternAt(row, track);
  int next = flipPattern(current);
  if (next == current) return false;
  withAudioGuard([&]() {
    mini_acid_.setSongPattern(row, track, next);
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      mini_acid_.setSongPosition(row);
    }
  });
  showToast("Bank flip", 700);
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
  if (cursorOnModeButton()) return false;

  // If we have a selection, fill the entire selected area
  if (has_selection_) {
    int min_row, max_row, min_track, max_track;
    getSelectionBounds(min_row, max_row, min_track, max_track);
    // Selection can include non-track columns (playhead/mode). Clamp to real track cells.
    if (min_track < 0) min_track = 0;
    int maxCol = maxEditableTrackColumn();
    if (max_track > maxCol) max_track = maxCol;
    if (min_track > max_track) {
      LOG_WARN_UI("Selection does not include track columns: [%d..%d]", min_track, max_track);
      return false;
    }
    LOG_INFO_UI("Fill selection [%d-%d] x [%d-%d] with pattern %d",
                min_row, max_row, min_track, max_track, patternIdx);
    int write_count = 0;
    int changed_count = 0;
    withAudioGuard([&]() {
      for (int r = min_row; r <= max_row; ++r) {
        for (int t = min_track; t <= max_track; ++t) {
          bool valid = false;
          SongTrack track = trackForColumn(t, valid);
          if (!valid) continue;
          int bankIndex = bankIndexForTrack(track);
          int combined = songPatternFromBank(bankIndex, patternIdx);
          int before = mini_acid_.songPatternAt(r, track);
          mini_acid_.setSongPattern(r, track, combined);
          int after = mini_acid_.songPatternAt(r, track);
          ++write_count;
          if (before != after) ++changed_count;
          LOG_DEBUG_UI("Fill cell r=%d c=%d track=%d before=%d target=%d after=%d",
                       r, t, (int)track, before, combined, after);
        }
      }
    });
    clearSelection();
    char toast[48];
    std::snprintf(toast, sizeof(toast), "Fill %d/%d -> P%d",
                  changed_count, write_count, patternIdx + 1);
    showToast(toast, 1200);
    LOG_INFO_UI("Fill result: changed=%d/%d", changed_count, write_count);
    return true;
  }

  // Single cell: assign to cursor position
  bool trackValid = false;
  SongTrack track = trackForColumn(cursorTrack(), trackValid);
  if (!trackValid) return false;
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
  if (has_selection_) {
    int min_row, max_row, min_track, max_track;
    getSelectionBounds(min_row, max_row, min_track, max_track);
    if (min_track < 0) min_track = 0;
    int maxCol = maxEditableTrackColumn();
    if (max_track > maxCol) max_track = maxCol;
    if (min_track > max_track) return false;

    std::vector<int> old_patterns;
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
    clearSelection();
    char toast[48];
    std::snprintf(toast, sizeof(toast), "Cleared %d cells", cleared);
    showToast(toast, 900);
    return true;
  }

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
    auto trackFromIndex = [](int idx) {
      switch (idx) {
        case 0: return SongTrack::SynthA;
        case 1: return SongTrack::SynthB;
        case 2: return SongTrack::Drums;
        default: return SongTrack::Voice;
      }
    };

    bool wholeSongScope = cursorOnModeButton() || cursorOnPlayheadLabel();
    bool trackValid = false;
    SongTrack track = trackForColumn(cursorTrack(), trackValid);

    switch (ui_event.app_event_type) {
      case GROOVEPUTER_APP_EVENT_COPY: {
        if (wholeSongScope) {
          g_song_slot_clipboard.song = mini_acid_.song();
          g_song_slot_clipboard.source_slot = mini_acid_.activeSongSlot();
          g_song_slot_clipboard.has_song = true;
          g_song_area_clipboard.has_area = false;
          g_song_pattern_clipboard.has_pattern = false;
          showToast("Song copied", 900);
          return true;
        }
        if (!trackValid) return false;
        if (has_selection_) {
          // Copy selected area
          int min_row, max_row, min_track, max_track;
          getSelectionBounds(min_row, max_row, min_track, max_track);
          
          int maxCol = maxEditableTrackColumn();
          if (max_track > maxCol) max_track = maxCol;
          if (min_track > maxCol) return false;
          
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
          selection_locked_ = true;
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
        if (wholeSongScope) {
          g_song_slot_clipboard.song = mini_acid_.song();
          g_song_slot_clipboard.source_slot = mini_acid_.activeSongSlot();
          g_song_slot_clipboard.has_song = true;
          withAudioGuard([&]() {
            for (int r = 0; r < Song::kMaxPositions; ++r) {
              mini_acid_.clearSongPattern(r, SongTrack::SynthA);
              mini_acid_.clearSongPattern(r, SongTrack::SynthB);
              mini_acid_.clearSongPattern(r, SongTrack::Drums);
              mini_acid_.clearSongPattern(r, SongTrack::Voice);
            }
            mini_acid_.setSongLength(1);
            mini_acid_.setSongReverse(false);
            if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
              mini_acid_.setSongPosition(0);
            }
          });
          showToast("Song cut", 900);
          return true;
        }
        if (!trackValid) return false;
        if (has_selection_) {
          // Copy selected area first
          int min_row, max_row, min_track, max_track;
          getSelectionBounds(min_row, max_row, min_track, max_track);
          
          int maxCol = maxEditableTrackColumn();
          if (max_track > maxCol) max_track = maxCol;
          if (min_track > maxCol) return false;
          
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
        if (wholeSongScope) {
          if (!g_song_slot_clipboard.has_song) return false;
          Song pasted = g_song_slot_clipboard.song;
          withAudioGuard([&]() {
            for (int r = 0; r < Song::kMaxPositions; ++r) {
              mini_acid_.clearSongPattern(r, SongTrack::SynthA);
              mini_acid_.clearSongPattern(r, SongTrack::SynthB);
              mini_acid_.clearSongPattern(r, SongTrack::Drums);
              mini_acid_.clearSongPattern(r, SongTrack::Voice);
            }
            mini_acid_.setSongLength(pasted.length);
            mini_acid_.setSongReverse(pasted.reverse);
            for (int r = 0; r < pasted.length && r < Song::kMaxPositions; ++r) {
              for (int t = 0; t < SongPosition::kTrackCount; ++t) {
                int pattern = pasted.positions[r].patterns[t];
                SongTrack tr = trackFromIndex(t);
                if (pattern < 0) mini_acid_.clearSongPattern(r, tr);
                else mini_acid_.setSongPattern(r, tr, pattern);
              }
            }
            if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
              mini_acid_.setSongPosition(0);
            }
          });
          showToast("Song pasted", 900);
          return true;
        }
        if (!trackValid) return false;
        if (g_song_area_clipboard.has_area) {
          // Paste area
          int start_row = cursorRow();
          int start_track = cursorTrack();
          bool useSelectionAnchor = false;
          if (has_selection_) {
            int min_row, max_row, min_col, max_col;
            getSelectionBounds(min_row, max_row, min_col, max_col);
            start_row = min_row;
            start_track = min_col;
            useSelectionAnchor = true;
          }
          int maxCol = maxEditableTrackColumn();
          if (start_track > maxCol) return false;
          int source_tracks = g_song_area_clipboard.tracks;
          int source_rows = g_song_area_clipboard.rows;
          if (source_tracks <= 0 || source_rows <= 0) return false;

          // Keep paste rectangular. With explicit selection anchor, never auto-shift
          // (paste must match visible selection frame).
          int paste_tracks = source_tracks;
          int maxVisibleTracks = maxCol + 1;
          if (paste_tracks > maxVisibleTracks) paste_tracks = maxVisibleTracks;
          if (!useSelectionAnchor && start_track + paste_tracks - 1 > maxCol) {
            start_track = maxCol - paste_tracks + 1;
          }
          if (start_track < 0) start_track = 0;
          int availableToRight = maxCol - start_track + 1;
          if (paste_tracks > availableToRight) paste_tracks = availableToRight;
          if (paste_tracks <= 0) return false;
          
          // Save old patterns for undo
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
            for (int r = 0; r < source_rows; ++r) {
              for (int t = 0; t < paste_tracks; ++t) {
                int target_row = start_row + r;
                int target_track = start_track + t;
                int idx = r * source_tracks + t;  // left part of source block if source wider than grid
                if (target_row >= Song::kMaxPositions || target_track > maxCol) continue;
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
        if (has_selection_) clearSelection();
        return true;
      }
      case GROOVEPUTER_APP_EVENT_UNDO: {
        if (!trackValid) return false;
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

  // ESC or backtick: clear selection if active
  if (ui_event.scancode == GROOVEPUTER_ESCAPE || ui_event.key == '`' || ui_event.key == '~') {
    if (has_selection_) {
      clearSelection();
      return true;
    }
  }

  int nav = UIInput::navCode(ui_event);
  if (ui_event.alt && (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN)) {
    int delta = nav == GROOVEPUTER_UP ? 1 : -1;
    if (cursorOnPlayheadLabel()) return adjustSongPlayhead(delta);
    return adjustSongPatternAtCursor(delta);
  }

  bool handled = false;
  // Cardputer keyboard may not have a practical Shift key, allow Ctrl as selection modifier too.
  bool extend_selection = ui_event.shift || ui_event.ctrl;
  if (extend_selection && selection_locked_) selection_locked_ = false;
  if (selection_locked_ && has_selection_ && !extend_selection) {
    switch (nav) {
      case GROOVEPUTER_LEFT: return moveSelectionFrameBy(0, -1);
      case GROOVEPUTER_RIGHT: return moveSelectionFrameBy(0, 1);
      case GROOVEPUTER_UP: return moveSelectionFrameBy(-1, 0);
      case GROOVEPUTER_DOWN: return moveSelectionFrameBy(1, 0);
      default: break;
    }
  }
  switch (nav) {
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

  // Log modifier+key combos for debugging song controls
  if (ui_event.ctrl || ui_event.alt) {
    LOG_DEBUG_INPUT("SongPage key=%d(0x%02X)'%c' ctrl=%d alt=%d shift=%d",
                    (int)(unsigned char)key, (unsigned char)key,
                    (key >= 32 && key < 127) ? key : '?',
                    ui_event.ctrl, ui_event.alt, ui_event.shift);
  }

  // Robust Ctrl-letter handling: some keyboard paths deliver control chars (0x01..0x1A)
  // instead of printable letters when Ctrl is held.
  if (ui_event.ctrl) {
    unsigned char u = static_cast<unsigned char>(key);
    if (u >= 1 && u <= 26) {
      key = static_cast<char>('a' + (u - 1));
    }
  }
  char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));

  bool key_b = (lowerKey == 'b') || (ui_event.scancode == GROOVEPUTER_B);
  bool key_c = (lowerKey == 'c') || (ui_event.scancode == GROOVEPUTER_C);
  bool key_l = (lowerKey == 'l') || (ui_event.scancode == GROOVEPUTER_L);
  bool key_m = (lowerKey == 'm') || (ui_event.scancode == GROOVEPUTER_M);
  bool key_n = (lowerKey == 'n') || (ui_event.scancode == GROOVEPUTER_N);
  bool key_r = (lowerKey == 'r') || (ui_event.scancode == GROOVEPUTER_R);
  bool key_v = (lowerKey == 'v') || (ui_event.scancode == GROOVEPUTER_V);
  bool key_x = (lowerKey == 'x') || (ui_event.scancode == GROOVEPUTER_X);

  // Local fallback for Ctrl+C / Ctrl+V, same pattern as Pattern/Drum pages.
  // Needed because some keyboard paths do not route app events globally.
  if (ui_event.ctrl && key_c) {
    UIEvent app_evt = ui_event;
    app_evt.event_type = GROOVEPUTER_APPLICATION_EVENT;
    app_evt.app_event_type = GROOVEPUTER_APP_EVENT_COPY;
    return handleEvent(app_evt);
  }
  if (ui_event.ctrl && key_v) {
    UIEvent app_evt = ui_event;
    app_evt.event_type = GROOVEPUTER_APPLICATION_EVENT;
    app_evt.app_event_type = GROOVEPUTER_APP_EVENT_PASTE;
    return handleEvent(app_evt);
  }

  if (ui_event.ctrl && key_l) {
    LOG_INFO_UI("Toggle loop mode");
    return toggleLoopMode();
  }

  if (ui_event.alt && key_x) {
    bool enable = !mini_acid_.liveMixModeEnabled();
    withAudioGuard([&]() { mini_acid_.setLiveMixMode(enable); });
    showToast(enable ? "LiveMix: ON" : "LiveMix: OFF", 900);
    return true;
  }

  if (ui_event.ctrl && key_b) {
    if (!mini_acid_.liveMixModeEnabled()) {
      withAudioGuard([&]() { mini_acid_.setLiveMixMode(true); });
    }
    int nextPlaySlot = mini_acid_.songPlaybackSlot() == 0 ? 1 : 0;
    withAudioGuard([&]() { mini_acid_.setSongPlaybackSlot(nextPlaySlot); });
    showToast(nextPlaySlot == 0 ? "Play: A" : "Play: B", 900);
    return true;
  }

  if (ui_event.alt && !ui_event.ctrl && key_b) {
    int nextSlot = mini_acid_.activeSongSlot() == 0 ? 1 : 0;
    LOG_INFO_UI("Switch to Song Slot %c", 'A' + nextSlot);
    withAudioGuard([&]() { mini_acid_.setActiveSongSlot(nextSlot); });
    showToast(nextSlot == 0 ? "Slot: A" : "Slot: B", 800);
    return true;
  }

  if (!ui_event.alt && !ui_event.ctrl && key_b) {
    return flipSongPatternBankAtCursorOrSelection();
  }

  // Song operations (Ctrl held)
  if (ui_event.ctrl && key_r) {
    bool wasSongMode = mini_acid_.songModeEnabled();
    bool newReverse = !mini_acid_.isSongReverse();
    int len = mini_acid_.songLength();
    LOG_INFO_UI("Song reverse: %s (songMode=%d len=%d pos=%d)",
                newReverse ? "ON" : "OFF",
                (int)wasSongMode, len, mini_acid_.currentSongPosition());
    withAudioGuard([&]() {
      if (!mini_acid_.songModeEnabled()) {
        mini_acid_.setSongMode(true);
      }
      mini_acid_.queueSongReverseToggle();
    });
    bool songModeNow = mini_acid_.songModeEnabled();
    if (len <= 1 && !mini_acid_.isPlaying()) {
      showToast(newReverse ? "Reverse ON (len=1)" : "Reverse OFF (len=1)", 1200);
    } else if (mini_acid_.isPlaying() && mini_acid_.songModeEnabled()) {
      showToast(newReverse ? "Reverse queued (bar)" : "Forward queued (bar)", 1300);
    } else if (!wasSongMode && songModeNow) {
      showToast(newReverse ? "SongMode ON + Reverse ON" : "SongMode ON + Reverse OFF", 1400);
    } else {
      showToast(newReverse ? "Reverse: ON" : "Reverse: OFF", 1000);
    }
    return true;
  }
  if (ui_event.ctrl && key_m) {
    LOG_INFO_UI("Merge songs (other -> active slot %d)", mini_acid_.activeSongSlot());
    withAudioGuard([&]() { mini_acid_.mergeSongs(); });
    showToast("Merge: done", 1000);
    return true;
  }
  if (ui_event.ctrl && key_n) {
    LOG_INFO_UI("Alternate songs (interleave slots)");
    withAudioGuard([&]() { mini_acid_.alternateSongs(); });
    showToast("Alternate: done", 1000);
    return true;
  }

  if (cursorOnModeButton() && (key == '\n' || key == '\r')) {
    return toggleSongMode();
  }

  // Song mode toggle - but NOT when Ctrl is held (avoid conflict with Ctrl+M = merge)
  if (!ui_event.ctrl && !ui_event.alt && key_m) {
    LOG_INFO_UI("Toggle song mode");
    return toggleSongMode();
  }

  if (!ui_event.ctrl && !ui_event.alt && (lowerKey == 'v' || ui_event.scancode == GROOVEPUTER_V)) {
    if (!voice_lane_visible_ && !hasVoiceDataInActiveSlot()) {
      showToast("No VO data in slot", 1000);
      return true;
    }
    voice_lane_visible_ = !voice_lane_visible_;
    showToast(voice_lane_visible_ ? "Lane: VOICE" : "Lane: DRUMS", 1000);
    return true;
  }

  if (!ui_event.ctrl && !ui_event.alt && (lowerKey == 'x' || ui_event.scancode == GROOVEPUTER_X)) {
    split_compare_ = !split_compare_;
    showToast(split_compare_ ? "Split: ON" : "Split: OFF", 900);
    return true;
  }

  int patternIdx = patternIndexFromKey(lowerKey);
  if (patternIdx < 0) patternIdx = scancodeToPatternIndex(ui_event.scancode);
  if (cursorOnModeButton() && patternIdx >= 0) return false;
  if (patternIdx >= 0) return assignPattern(patternIdx);

  // Alt + Backspace = Reset Song
  if (ui_event.alt && (key == '\b' || key == 0x7F)) {
      withAudioGuard([&]() {
          for (int r = 0; r < Song::kMaxPositions; ++r) {
              mini_acid_.clearSongPattern(r, SongTrack::SynthA);
              mini_acid_.clearSongPattern(r, SongTrack::SynthB);
              mini_acid_.clearSongPattern(r, SongTrack::Drums);
              mini_acid_.clearSongPattern(r, SongTrack::Voice);
          }
      });
      UI::showToast("Song Cleared");
      return true;
  }

  if (key == '\b') {
    return clearPattern();
  }

  // Tab clears pattern (legacy behavior)
  if (key == '\t') {
    return clearPattern();
  }

  if (lowerKey == 'g') {
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
        int maxCol = maxEditableTrackColumn();
        if (max_track > maxCol) max_track = maxCol;
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
  // Split compare is currently implemented in TE grid renderer.
  // Force that renderer while split is active so view switch is always visible.
  if (split_compare_) {
    drawTEGridStyle(gfx);
    return;
  }

  switch (visual_style_) {
    case ::VisualStyle::RETRO_CLASSIC:
      drawRetroClassicStyle(gfx);
      break;
    case ::VisualStyle::AMBER:
      drawAmberStyle(gfx);
      break;
    case ::VisualStyle::MINIMAL_DARK:
      drawTEGridStyle(gfx);
      break;
    case ::VisualStyle::MINIMAL:
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

  int track_count = visibleTrackCount();
  int pos_col_w = 0;
  int spacing = 2;
  int modeBtnW = 55;
  int track_col_w = (w - pos_col_w - spacing * (track_count + 2) - modeBtnW) / track_count;
  if (track_col_w < 20) track_col_w = 20;

  gfx.setTextColor(song303Color(0));
  gfx.drawText(x + pos_col_w + spacing, body_y, "303A");
  gfx.setTextColor(song303Color(1));
  gfx.drawText(x + pos_col_w + spacing + track_col_w, body_y, "303B");
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x + pos_col_w + spacing + track_col_w * 2, body_y, voice_lane_visible_ ? "Voice" : "Drums");
  char lenBuf[32];
  snprintf(lenBuf, sizeof(lenBuf), "PLYHD %d:%d", playhead + 1, song_len);
  int lenX = x + pos_col_w + spacing + track_col_w * track_count + spacing + 5;
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
    for (int t = 0; t < track_count; ++t) {
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
        formatSongPatternLabel(pattern, patBuf, sizeof(patBuf));
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

  int modeX = x + pos_col_w + spacing + track_col_w * track_count + spacing + 5;
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

  // Base Colors
  IGfxColor TE_BLACK = IGfxColor::Black();
  IGfxColor TE_WHITE = IGfxColor::White();
  IGfxColor TE_GRID  = IGfxColor(0x353535);
  IGfxColor TE_ACCENT = IGfxColor(0xD8D8D8);
  IGfxColor TE_DIM    = IGfxColor(0x8A8A8A);
  IGfxColor TE_ROW_A  = IGfxColor(0x101010);
  IGfxColor TE_ROW_B  = IGfxColor(0x151515);

  // Dimmed Colors
  uint16_t TE_ROW_A_DIM = 0x1010; 
  uint16_t TE_ROW_B_DIM = 0x0000; 
  uint16_t TE_DIM_GRID  = 0x2020;
  uint16_t TE_DIM_TEXT  = 0x6060;
  uint16_t TE_DIM_DIM   = 0x3030;

  // Apply Theme
  if (visual_style_ == ::VisualStyle::RETRO_CLASSIC) {
    TE_BLACK = IGfxColor(RetroTheme::BG_DEEP_BLACK);
    TE_WHITE = IGfxColor(RetroTheme::NEON_CYAN);
    TE_GRID  = IGfxColor(RetroTheme::GRID_DIM);
    TE_ACCENT = IGfxColor(RetroTheme::SELECT_BRIGHT);
    TE_DIM    = IGfxColor(RetroTheme::TEXT_DIM);
    TE_ROW_A  = IGfxColor(RetroTheme::BG_INSET);
    TE_ROW_B  = IGfxColor(RetroTheme::BG_PANEL);
    
    TE_ROW_A_DIM = TE_ROW_A.color16();
    TE_ROW_B_DIM = TE_BLACK.color16();
    TE_DIM_GRID  = TE_GRID.color16();
    TE_DIM_TEXT  = TE_DIM.color16();
    TE_DIM_DIM   = TE_BLACK.color16();

  } else if (visual_style_ == ::VisualStyle::AMBER) {
    TE_BLACK = IGfxColor(AmberTheme::BG_DEEP_BLACK);
    TE_WHITE = IGfxColor(AmberTheme::TEXT_PRIMARY);
    TE_GRID  = IGfxColor(AmberTheme::GRID_DIM);
    TE_ACCENT = IGfxColor(AmberTheme::SELECT_BRIGHT);
    TE_DIM    = IGfxColor(AmberTheme::TEXT_DIM);
    TE_ROW_A  = IGfxColor(AmberTheme::BG_INSET);
    TE_ROW_B  = IGfxColor(AmberTheme::BG_PANEL);
    
    TE_ROW_A_DIM = TE_ROW_A.color16();
    TE_ROW_B_DIM = TE_BLACK.color16();
    TE_DIM_GRID  = TE_GRID.color16();
    TE_DIM_TEXT  = TE_DIM.color16();
    TE_DIM_DIM   = TE_BLACK.color16();
  }

  gfx.fillRect(x, y, w, h, TE_BLACK);

  int header_h = 11;
  gfx.fillRect(x, y, w, header_h, TE_WHITE);
  gfx.setTextColor(TE_BLACK);

  int slot = mini_acid_.activeSongSlot();
  int playSlot = mini_acid_.songPlaybackSlot();
  bool liveMix = mini_acid_.liveMixModeEnabled();
  bool reverse = mini_acid_.isSongReverse();
  char titleBuf[40];
  if (split_compare_) {
    snprintf(titleBuf, sizeof(titleBuf), "SONG A|B%s%s", reverse ? " REV" : "", liveMix ? " LM" : "");
  } else {
    snprintf(titleBuf, sizeof(titleBuf), "SONG %c%s%s", 'A' + slot, reverse ? " REV" : "", liveMix ? " LM" : "");
  }
  gfx.drawText(x + 2, y + 2, titleBuf);

  char statusBuf[32];
  int pos = mini_acid_.currentSongPosition() + 1;
  int len = mini_acid_.songLength();
  bool playing = mini_acid_.isPlaying();
  bool songMode = mini_acid_.songModeEnabled();
  snprintf(statusBuf, sizeof(statusBuf), "%s %03d/%03d %s",
           playing ? (songMode ? ">" : "||") : "[]", pos, len, laneShortLabel());
  int statusW = textWidth(gfx, statusBuf);
  gfx.drawText(x + w - statusW - 2, y + 2, statusBuf);

  int footer_h = 11;
  int grid_y = y + header_h + 1;
  int grid_h = h - header_h - footer_h - 2;
  int cell_h = 10;
  int row_gap = 1;
  int visible_rows = grid_h / (cell_h + row_gap);
  if (visible_rows < 1) visible_rows = 1;

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

  int min_sel_row = 0, max_sel_row = -1, min_sel_track = 0, max_sel_track = -1;
  if (has_selection_) getSelectionBounds(min_sel_row, max_sel_row, min_sel_track, max_sel_track);

  auto drawPane = [=, &gfx, this](int paneX, int paneW, int paneSlot, bool editable) {
    int tracks = visibleTrackCount();
    int pos_w = 0;
    int col_gap = 1;
    int totalCellW = paneW - pos_w - 1 - col_gap * (tracks - 1);
    int cell_w = totalCellW / tracks;
    if (cell_w < 14) cell_w = 14;

    // Dim inactive pane
    uint16_t headerColor = editable ? TE_ACCENT.color16() : TE_DIM.color16();
    uint16_t sepColor = editable ? TE_GRID.color16() : TE_DIM_GRID;
    
    // Header
    gfx.setTextColor(headerColor);
    int grid_y = y + header_h + 2; 
    
    if (!split_compare_ || cell_w > 20) {
        gfx.drawText(paneX + pos_w + 2, grid_y, "A");
        gfx.drawText(paneX + pos_w + (cell_w + col_gap) + 2, grid_y, "B");
        gfx.drawText(paneX + pos_w + 2 * (cell_w + col_gap) + 2, grid_y, laneShortLabel()); 
    }

    // Slot Label in corner
    if (split_compare_) {
      char slotBuf[16];
      snprintf(slotBuf, sizeof(slotBuf), "%c", 'A' + paneSlot);
      int slotW = textWidth(gfx, slotBuf);
      gfx.setTextColor(headerColor);
      gfx.drawText(paneX + paneW - slotW - 2, grid_y, slotBuf);
      
      // Draw BOLD border for active/editable pane
      if (editable) {
          gfx.drawRect(paneX - 1, grid_y - 2, paneW + 2, h - header_h - footer_h + 2, TE_ACCENT);
          gfx.drawRect(paneX - 2, grid_y - 3, paneW + 4, h - header_h - footer_h + 4, TE_ACCENT);
      }
    }
    
    gfx.drawLine(paneX, grid_y + 9, paneX + paneW - 1, grid_y + 9, sepColor);

    int cells_y = grid_y + 11;
    for (int i = 0; i < visible_rows; ++i) {
      int row_idx = scroll_row_ + i;
      if (row_idx >= Song::kMaxPositions) break;
      int ry = cells_y + i * (cell_h + row_gap);

      // Row background
      uint16_t rowBg = (i & 1) ? TE_ROW_B.color16() : TE_ROW_A.color16();
      // Dim background if not editable
      if (!editable) {
          rowBg = (i & 1) ? TE_ROW_B_DIM : TE_ROW_A_DIM;
      }
      gfx.fillRect(paneX, ry, paneW, cell_h, rowBg);

      // Playhead (only on active slot if playing)
      if (row_idx == playhead && playing && songMode) {
        gfx.drawLine(paneX, ry + cell_h - 1, paneX + paneW - 1, ry + cell_h - 1,
                     paneSlot == playSlot ? TE_ACCENT : TE_DIM);
      }

      for (int t = 0; t < tracks; ++t) {
        int tx = paneX + pos_w + t * (cell_w + col_gap);
        bool valid = false;
        SongTrack track = trackForColumn(t, valid);
        int pattern = -1;
        if (valid) {
          pattern = this->mini_acid_.songPatternAtSlot(paneSlot, row_idx, track);
        }

        bool isSelected = false;
        if (editable) {
          if (has_selection_) {
            isSelected = row_idx >= min_sel_row && row_idx <= max_sel_row &&
                         t >= min_sel_track && t <= max_sel_track;
          } else {
            isSelected = row_idx == cursor_row && t == cursor_track_;
          }
        }

        if (isSelected) {
          gfx.fillRect(tx, ry, cell_w, cell_h, TE_ACCENT);
          gfx.setTextColor(TE_BLACK);
        } else {
          // Logic for dimmed text
          if (pattern >= 0) {
             gfx.setTextColor(editable ? TE_WHITE.color16() : TE_DIM_TEXT);
          } else {
             gfx.setTextColor(editable ? TE_DIM.color16() : TE_DIM_DIM);
          }
        }

        if (pattern >= 0) {
          char patBuf[10];
          formatSongPatternLabel(pattern, patBuf, sizeof(patBuf));
          gfx.drawText(tx + 1, ry + 1, patBuf);
        } else {
          gfx.drawText(tx + 1, ry + 1, "--");
        }
      }
    }
  };

  if (split_compare_) {
    int gap = 6; // Increased gap for border
    int paneW = (w - gap) / 2;
    int otherSlot = slot == 0 ? 1 : 0;
    
    // Draw active pane first or second? 
    // Left is always A, Right is always B.
    drawPane(x, paneW, 0, slot == 0);
    drawPane(x + paneW + gap, w - paneW - gap, 1, slot == 1);

  } else {
    drawPane(x, w, slot, true);
  }

  int footer_y = y + h - footer_h;
  gfx.drawLine(x, footer_y - 1, x + w - 1, footer_y - 1, TE_GRID);
  
  // Footer Status
  char footerBuf[64];
  // Show Edit Slot and Play Status if available
  // "EDIT:A PLAY:A" (future)
  int editSlot = mini_acid_.activeSongSlot();
  std::snprintf(footerBuf, sizeof(footerBuf), "E:%c P:%c%s  A+B:EDIT C+B:PLAY CA+X:LM",
                'A' + editSlot,
                'A' + playSlot,
                liveMix ? " LM" : "");
  
  gfx.setTextColor(TE_DIM);
  gfx.drawText(x + 2, footer_y + 2, footerBuf);

  if (mini_acid_.loopModeEnabled()) {
    int loopStart = mini_acid_.loopStartRow() + 1;
    int loopEnd = mini_acid_.loopEndRow() + 1;
    char loopBuf[16];
    snprintf(loopBuf, sizeof(loopBuf), "LP:%d-%d", loopStart, loopEnd);
    int loopW = textWidth(gfx, loopBuf);
    gfx.fillRect(x + w - loopW - 4, footer_y, loopW + 4, footer_h, TE_WHITE);
    gfx.setTextColor(TE_BLACK);
    gfx.drawText(x + w - loopW - 2, footer_y + 2, loopBuf);
  }
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
    int track_count = visibleTrackCount();
    int row_h = 10;
    int header_y = y + 26;
    int grid_y = header_y + 12;
    int pos_w = 0;
    int track_w = (w - pos_w - 4) / track_count;

    gfx.setTextColor(IGfxColor(RetroTheme::NEON_ORANGE));
    gfx.drawText(x + pos_w + 4, header_y, "A");
    gfx.setTextColor(IGfxColor(RetroTheme::NEON_MAGENTA));
    gfx.drawText(x + pos_w + track_w + 4, header_y, "B");
    gfx.setTextColor(IGfxColor(RetroTheme::NEON_CYAN));
    gfx.drawText(x + pos_w + track_w * 2 + 4, header_y, voice_lane_visible_ ? "VO" : "DR");

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
        for (int t = 0; t < track_count; ++t) {
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
                formatSongPatternLabel(pattern, buf, sizeof(buf));
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

    retro::drawFooterBar(gfx, x, y + h - 12, w, 12, "B:Flip A+B:Edit C+B:Play CA+X:LM C+R:Rev", "SONG");
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

    int track_count = visibleTrackCount();
    int row_h = 10;
    int grid_y = y + 38;
    int pos_w = 0;
    int track_w = (w - pos_w - 4) / track_count;

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
        for (int t = 0; t < track_count; ++t) {
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
                formatSongPatternLabel(pattern, buf, sizeof(buf));
                gfx.setTextColor(IGfxColor(AmberTheme::TEXT_PRIMARY));
                gfx.drawText(tx, ry, buf);
            } else {
                gfx.setTextColor(IGfxColor(AmberTheme::GRID_DIM));
                gfx.drawText(tx, ry, "--");
            }
        }
    }

    amber::drawFooterBar(gfx, x, y + h - 12, w, 12, "B:Flip A+B:Edit C+B:Play CA+X:LM C+R:Rev", "SONG");
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
        for (int col = 0; col < visibleTrackCount(); col++) {
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
