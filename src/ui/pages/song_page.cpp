#include "song_page.h"
#include "../../../scenes.h"
#include "../../debug_log.h"
#include "../key_normalize.h"
#include "../ui_input.h"
#include "../ui_common.h"
#include "../ui_theme.h"
#include "../pattern_matrix_navigation.h"
#include "../../dsp/atlas_runtime.h"
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
  const int page = songPatternPage(pattern) + 1;
  const int bank = songPatternBank(pattern);
  const int slot = songPatternIndexInBank(pattern) + 1;
  if (bank < 0 || slot <= 0) {
    std::snprintf(out, outSize, "---");
    return;
  }
  std::snprintf(out, outSize, "%d%c%d", page, static_cast<char>('A' + bank), slot);
}

inline int songQuarterFromRow(int row) {
  if (row < 0) return 0;
  int quarter = row / 32;
  if (quarter < 0) quarter = 0;
  if (quarter > 3) quarter = 3;
  return quarter;
}

inline void drawQuarterStrip(IGfx& gfx, int x, int y, int height, int activeQuarter,
                             IGfxColor onColor, IGfxColor offColor) {
  if (height < 8) return;
  int gap = 1;
  int segH = (height - gap * 3) / 4;
  if (segH < 1) segH = 1;
  for (int q = 0; q < 4; ++q) {
    int sy = y + q * (segH + gap);
    gfx.fillRect(x, sy, 2, segH, q == activeQuarter ? onColor : offColor);
  }
}

constexpr uint32_t kCtrlRRepeatGapMs = 220;
constexpr uint32_t kCtrlRLongPressMs = 650;
constexpr bool kVoiceLaneInSongEditor = false;
constexpr int kSongSlotCount = 2;

inline IGfxColor colorForSongTrack(SongTrack track) {
  switch (track) {
    case SongTrack::SynthA: return song303Color(0);
    case SongTrack::SynthB: return song303Color(1);
    case SongTrack::Drums: return COLOR_WHITE;
    case SongTrack::Voice: return IGfxColor::Green();
    default: return COLOR_WHITE;
  }
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

// Little lock icon (5x6) for LiveMix/Edit Protection
inline void drawLockIcon(IGfx& gfx, int x, int y, IGfxColor color) {
  // Shackle (3x2)
  gfx.drawPixel(x + 1, y, color);
  gfx.drawPixel(x + 2, y, color);
  gfx.drawPixel(x + 3, y, color);
  gfx.drawPixel(x + 1, y + 1, color);
  gfx.drawPixel(x + 3, y + 1, color);
  // Body (5x4)
  gfx.fillRect(x, y + 2, 5, 4, color);
  // Keyhole (optional contrast pixel) - assuming light color on dark bg
  gfx.drawPixel(x + 2, y + 4, IGfxColor(0)); 
}

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
  assignment_bank_index_ = bankIndexForTrack(SongTrack::SynthA);
  if (assignment_bank_index_ < 0 || assignment_bank_index_ >= kBankCount) {
    assignment_bank_index_ = 0;
  }
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

bool SongPage::cursorOnModeButton() const {
  // MODE remains clickable, but it is no longer a hidden fourth keyboard column.
  return false;
}
bool SongPage::cursorOnPlayheadLabel() const { 
  return false; // Not implemented yet
}

int SongPage::logicalTrackCount() const {
  return kVoiceLaneInSongEditor ? 4 : 3;
}

int SongPage::visibleTrackCount() const {
  switch (lane_focus_mode_) {
    case LaneFocusMode::AllTracks:
      return logicalTrackCount();
    case LaneFocusMode::SynthPair:
      return 2;
    case LaneFocusMode::RhythmPair:
      return kVoiceLaneInSongEditor ? 2 : 1;
    default:
      return logicalTrackCount();
  }
}

int SongPage::maxEditableTrackColumn() const {
  return maxPatternTrackColumn();
}

int SongPage::maxPatternTrackColumn() const {
  const int count = visibleTrackCount();
  return count > 0 ? count - 1 : 0;
}

SongTrack SongPage::thirdLaneTrack() const {
  if (lane_focus_mode_ == LaneFocusMode::RhythmPair && kVoiceLaneInSongEditor) {
    return SongTrack::Voice;
  }
  return SongTrack::Drums;
}

const char* SongPage::laneShortLabel() const {
  switch (lane_focus_mode_) {
    case LaneFocusMode::AllTracks: return "ALL";
    case LaneFocusMode::SynthPair: return "AB";
    case LaneFocusMode::RhythmPair: return kVoiceLaneInSongEditor ? "DV" : "DR";
    default: return "ALL";
  }
}

const char* SongPage::trackHeaderLabel(int col) const {
  bool valid = false;
  SongTrack track = trackForColumn(col, valid);
  if (!valid) return "--";
  switch (track) {
    case SongTrack::SynthA: return "A";
    case SongTrack::SynthB: return "B";
    case SongTrack::Drums: return "DR";
    //case SongTrack::Voice: return "VO";
    default: return "--";
  }
}

int SongPage::visibleColumnForTrack(SongTrack track) const {
  switch (lane_focus_mode_) {
    case LaneFocusMode::AllTracks:
      switch (track) {
        case SongTrack::SynthA: return 0;
        case SongTrack::SynthB: return 1;
        case SongTrack::Drums: return 2;
        case SongTrack::Voice: return kVoiceLaneInSongEditor ? 3 : -1;
        default: return -1;
      }
    case LaneFocusMode::SynthPair:
      if (track == SongTrack::SynthA) return 0;
      if (track == SongTrack::SynthB) return 1;
      return -1;
    case LaneFocusMode::RhythmPair:
      if (track == SongTrack::Drums) return 0;
     // if (kVoiceLaneInSongEditor && track == SongTrack::Voice) return 1;
      return -1;
    default:
      return -1;
  }
}

void SongPage::normalizeCursorTrackAfterFocusChange(LaneFocusMode previous_mode) {
  const int oldVisibleTracks = [previous_mode]() -> int {
    switch (previous_mode) {
      case LaneFocusMode::AllTracks: return kVoiceLaneInSongEditor ? 4 : 3;
      case LaneFocusMode::SynthPair: return 2;
      case LaneFocusMode::RhythmPair: return kVoiceLaneInSongEditor ? 2 : 1;
      default: return kVoiceLaneInSongEditor ? 4 : 3; //return kVoiceLaneInSongEditor ? 4 : 3;
    }
  }();
  const bool wasModeButton = cursor_track_ == oldVisibleTracks;
  if (wasModeButton) {
    cursor_track_ = maxPatternTrackColumn();
    return;
  }

  SongTrack oldTrack = SongTrack::SynthA;
  bool oldValid = true;
  auto trackForPrevMode = [&](int col, bool& valid) -> SongTrack {
    valid = true;
    switch (previous_mode) {
      case LaneFocusMode::AllTracks:
        switch (col) {
          case 0: return SongTrack::SynthA;
          case 1: return SongTrack::SynthB;
          case 2: return SongTrack::Drums;
          case 3:
            if (kVoiceLaneInSongEditor) return SongTrack::Voice;
            valid = false;
            return SongTrack::SynthA;
          default: valid = false; return SongTrack::SynthA;
        }
      case LaneFocusMode::SynthPair:
        switch (col) {
          case 0: return SongTrack::SynthA;
          case 1: return SongTrack::SynthB;
          default: valid = false; return SongTrack::SynthA;
        }
      case LaneFocusMode::RhythmPair:
        switch (col) {
          case 0: return SongTrack::Drums;
          case 1:
            if (kVoiceLaneInSongEditor) return SongTrack::Voice;
            valid = false;
            return SongTrack::SynthA;
          default: valid = false; return SongTrack::SynthA;
        }
      default:
        valid = false;
        return SongTrack::SynthA;
    }
  };

  oldTrack = trackForPrevMode(cursor_track_, oldValid);
  if (!oldValid) {
    cursor_track_ = 0;
    return;
  }

  int nextCol = visibleColumnForTrack(oldTrack);
  if (nextCol < 0) {
    cursor_track_ = 0;
  } else {
    cursor_track_ = nextCol;
  }
}

void SongPage::cycleLaneFocusMode() {
  LaneFocusMode previous = lane_focus_mode_;
  switch (lane_focus_mode_) {
    case LaneFocusMode::AllTracks:
      lane_focus_mode_ = LaneFocusMode::SynthPair;
      break;
    case LaneFocusMode::SynthPair:
      lane_focus_mode_ = LaneFocusMode::RhythmPair;
      break;
    case LaneFocusMode::RhythmPair:
    default:
      lane_focus_mode_ = LaneFocusMode::AllTracks;
      break;
  }
  normalizeCursorTrackAfterFocusChange(previous);
  if (has_selection_) {
    clearSelection();
  }
}

void SongPage::moveCursorToRow(int row) {
  cursor_row_ = clampCursorRow(row);
  syncSongPositionToCursor();
}

void SongPage::saveMarker(int marker_index) {
  if (marker_index < 0 || marker_index >= 4) return;
  row_markers_[marker_index] = cursorRow();
  char toast[32];
  std::snprintf(toast, sizeof(toast), "Mark %d: %d", marker_index + 1, cursorRow() + 1);
  showToast(toast, 900);
}

bool SongPage::jumpToMarker(int marker_index) {
  if (marker_index < 0 || marker_index >= 4) return false;
  int row = row_markers_[marker_index];
  if (row < 0) {
    char toast[24];
    std::snprintf(toast, sizeof(toast), "Mark %d empty", marker_index + 1);
    showToast(toast, 800);
    return true;
  }
  moveCursorToRow(row);
  char toast[32];
  std::snprintf(toast, sizeof(toast), "Jump %d -> %d", marker_index + 1, row + 1);
  showToast(toast, 900);
  return true;
}
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
  int before_slot = mini_acid_.activeSongSlot();
  if (extend_selection) {
    updateSelection();
  }

  // Horizontal arranger contract:
  // - arrows move only across the visible musical columns;
  // - PAT:A/B is an independent assignment context changed with B;
  // - at the outer track edge, plain arrows cross the edit Song Slot A/B
  //   boundary instead of silently changing PAT bank.
  int track = cursorTrack();
  const int maxTrack = maxPatternTrackColumn();
  bool slotChanged = false;

  if (!extend_selection && delta > 0 && track >= maxTrack) {
    const int nextSlot = before_slot + 1;
    if (nextSlot < kSongSlotCount) {
      withAudioGuard([&]() { mini_acid_.setActiveSongSlot(nextSlot); });
      track = 0;
      slotChanged = true;
    }
  } else if (!extend_selection && delta < 0 && track <= 0) {
    const int nextSlot = before_slot - 1;
    if (nextSlot >= 0) {
      withAudioGuard([&]() { mini_acid_.setActiveSongSlot(nextSlot); });
      track = maxTrack;
      slotChanged = true;
    }
  } else {
    track += delta;
    if (track < 0) track = 0;
    if (track > maxTrack) track = maxTrack;
  }

  cursor_track_ = track;
  syncSongPositionToCursor();
  if (extend_selection) {
    updateLoopRangeFromSelection();
  }
  if (slotChanged) {
    char slotToast[20];
    std::snprintf(slotToast, sizeof(slotToast), "EDIT SLOT %c",
                  static_cast<char>('A' + mini_acid_.activeSongSlot()));
    showToast(slotToast, 650);
  }
  LOG_DEBUG_INPUT("Cursor H delta=%d extend=%d slot=%d->%d bank=%d (%d,%d)->(%d,%d) sel=%d",
                  delta, (int)extend_selection, before_slot,
                  mini_acid_.activeSongSlot(), assignment_bank_index_,
                  before_row, before_track, cursor_row_, cursor_track_,
                  (int)has_selection_);
}

void SongPage::moveCursorVertical(int delta, bool extend_selection) {
  if (delta == 0) return;
  int before_row = cursor_row_;
  int before_track = cursor_track_;
  if (cursorOnModeButton()) {
    // If on sidebar, Up/Down does nothing or scrolls? 
    // Let's allow vertical movement within the grid but keep focus on sidebar if it was meant to be a vertical list.
    // However, SongPage mode button is one giant vertical bar.
    // For now, let it fall through or just return true to consume.
    return;
  }
  if (cursorOnPlayheadLabel()) {
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
  switch (lane_focus_mode_) {
    case LaneFocusMode::AllTracks:
      switch (col) {
        case 0: return SongTrack::SynthA;
        case 1: return SongTrack::SynthB;
        case 2: return SongTrack::Drums;
        case 3:
          if (kVoiceLaneInSongEditor) return SongTrack::Voice;
          valid = false;
          return SongTrack::SynthA;
        default: valid = false; return SongTrack::SynthA;
      }
    case LaneFocusMode::SynthPair:
      switch (col) {
        case 0: return SongTrack::SynthA;
        case 1: return SongTrack::SynthB;
        default: valid = false; return SongTrack::SynthA;
      }
    case LaneFocusMode::RhythmPair:
      switch (col) {
        case 0: return SongTrack::Drums;
        case 1:
          if (kVoiceLaneInSongEditor) return SongTrack::Voice;
          valid = false;
          return SongTrack::SynthA;
        default: valid = false; return SongTrack::SynthA;
      }
    default:
      valid = false;
      return SongTrack::SynthA;
  }
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
  int maxPattern = kMaxGlobalPatterns - 1;
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
    int page = songPatternPage(pattern);
    return songPatternFromPageBankIndex(page, flippedBank, slot);
  };

  if (has_selection_) {
    int min_row, max_row, min_track, max_track;
    getSelectionBounds(min_row, max_row, min_track, max_track);
    if (min_track < 0) min_track = 0;
    int maxCol = maxPatternTrackColumn();
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
    int maxCol = maxPatternTrackColumn();
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
          int bankIndex = assignment_bank_index_;
          int combined = songPatternFromPageBankIndex(mini_acid_.currentPageIndex(), bankIndex, patternIdx);
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
  int bankIndex = assignment_bank_index_;
  int combined = songPatternFromPageBankIndex(mini_acid_.currentPageIndex(), bankIndex, patternIdx);
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
    int maxCol = maxPatternTrackColumn();
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

bool SongPage::insertRowAtCursor() {
  int row = cursorRow();
  withAudioGuard([&]() {
    mini_acid_.insertSongRow(row);
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      mini_acid_.setSongPosition(row);
    }
  });
  char toast[32];
  std::snprintf(toast, sizeof(toast), "INS row %d", row + 1);
  showToast(toast, 900);
  return true;
}

bool SongPage::deleteRowAtCursor() {
  int row = cursorRow();
  withAudioGuard([&]() {
    mini_acid_.deleteSongRow(row);
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      int newPos = row;
      int len = mini_acid_.songLength();
      if (newPos >= len) newPos = std::max(0, len - 1);
      mini_acid_.setSongPosition(newPos);
    }
  });
  cursor_row_ = clampCursorRow(cursor_row_);
  char toast[32];
  std::snprintf(toast, sizeof(toast), "DEL row %d", row + 1);
  showToast(toast, 900);
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

bool SongPage::handleEventLegacyUnowned(UIEvent& ui_event) {
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
          
          int maxCol = maxPatternTrackColumn();
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
          
          int maxCol = maxPatternTrackColumn();
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
          int maxCol = maxPatternTrackColumn();
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
    case GROOVEPUTER_TAB:
      // Cycle focus or toggle lane? 
      // For now, let's allow it to fall through or just be handled.
      break;
    default:
      // Custom expert shortcuts for long songs
      if (ui_event.ctrl) {
        if (ui_event.key == 'w' || ui_event.key == 'W') {
           int step = (ui_event.alt) ? 32 : 8;
           moveCursorVertical(-step, extend_selection);
           return true;
        }
        if (ui_event.key == 's' || ui_event.key == 'S') {
           int step = (ui_event.alt) ? 32 : 8;
           moveCursorVertical(step, extend_selection);
           return true;
        }
      }
      break;
  }
  
  // Home/End via Alt + < / > (prevent accidental jumps with plain comma/dot)
  if (ui_event.alt && (ui_event.key == '<' || ui_event.key == ',')) {
     moveCursorToRow(0);
     showToast("Top", 500);
     return true;
  }
  if (ui_event.alt && (ui_event.key == '>' || ui_event.key == '.')) {
     moveCursorToRow(mini_acid_.songLength() - 1);
     showToast("End", 500);
     return true;
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

  auto markerFromLetter = [&](char ch) -> int {
    switch (ch) {
      case 'q': return 0;
      case 'e': return 1;
      case 'r': return 2;
      case 't': return 3;
      default: return -1;
    }
  };
  int markerIdx = markerFromLetter(lowerKey);
  if (ui_event.alt && !ui_event.ctrl && markerIdx >= 0) {
    saveMarker(markerIdx);
    return true;
  }
  if (ui_event.ctrl && ui_event.alt && markerIdx >= 0) {
    return jumpToMarker(markerIdx);
  }

  // alt + 1-4: Jump + Lock (Loop current section)
  if (ui_event.alt && !ui_event.ctrl && markerIdx >= 0 && markerIdx < 4) {
    int target = row_markers_[markerIdx];
    if (target >= 0) {
      int sectionLen = 16; // Default 16 bar loop
      withAudioGuard([&]() {
        mini_acid_.setSongPosition(target);
        mini_acid_.setLoopRange(target, std::min(Song::kMaxPositions - 1, target + sectionLen - 1));
        mini_acid_.setLoopMode(true);
      });
      moveCursorToRow(target);
      char buf[32];
      snprintf(buf, sizeof(buf), "JUMP+LOCK: Mark %d", markerIdx + 1);
      showToast(buf, 1000);
      return true;
    }
  }

  const int targetPage = UI::songPatternPageShortcut(
      key, ui_event.ctrl, ui_event.meta, ui_event.alt);
  if (targetPage >= 0) {
    mini_acid_.requestPageSwitch(targetPage);
    char buf[20];
    std::snprintf(buf, sizeof(buf), "Page: %d", targetPage + 1);
    showToast(buf, 800);
    return true;
  }

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

  if ((ui_event.ctrl || ui_event.meta) && key_l) {
    LOG_INFO_UI("Toggle loop mode");
    return toggleLoopMode();
  }

  // L (plain): Loop Lock (loop ±4 bars around playhead)
  if (!ui_event.ctrl && !ui_event.alt && !ui_event.meta && lowerKey == 'l') {
    int ph = mini_acid_.songPlayheadPosition();
    int lockRadius = 4;
    int len = mini_acid_.songLength();
    withAudioGuard([&]() {
      mini_acid_.setLoopRange(
        std::max(0, ph - lockRadius),
        std::min(len - 1, ph + lockRadius)
      );
      mini_acid_.setLoopMode(true);
    });
    showToast(("LOCK @bar " + std::to_string(ph + 1)).c_str(), 900);
    return true;
  }

  if (ui_event.alt && key_x) {// отображать замочек если заблокированно
    bool enable = !mini_acid_.liveMixModeEnabled();
    withRuntimeAudioGuard([&]() { mini_acid_.setLiveMixMode(enable); });
    showToast(enable ? "LiveMix: ON" : "LiveMix: OFF", 900);
    return true;
  }

  if (ui_event.ctrl && key_b) {
    if (!mini_acid_.liveMixModeEnabled()) {
      withRuntimeAudioGuard([&]() { mini_acid_.setLiveMixMode(true); });
    }
    int nextPlaySlot = mini_acid_.songPlaybackSlot() == 0 ? 1 : 0;
    withRuntimeAudioGuard([&]() { mini_acid_.setSongPlaybackSlot(nextPlaySlot); });
    showToast(nextPlaySlot == 0 ? "Play: A" : "Play: B", 900);
    return true;
  }

  if (ui_event.alt && !ui_event.ctrl && key_b) {
    // Alt+B keeps the explicit destructive operation: flip the bank of the
    // already stored Song reference (or selection). Edit-slot switching now
    // belongs to the plain Left/Right outer boundary.
    return flipSongPatternBankAtCursorOrSelection();
  }

  if (!ui_event.alt && !ui_event.ctrl && key_b) {
    assignment_bank_index_ = assignment_bank_index_ == 0 ? 1 : 0;
    char bankToast[20];
    std::snprintf(bankToast, sizeof(bankToast), "PAT BANK %c",
                  static_cast<char>('A' + assignment_bank_index_));
    showToast(bankToast, 650);
    return true;
  }

  // Song operations (Ctrl held)
  if (ui_event.ctrl && key_r) {
    uint32_t now = millis();
    bool sameHoldStream = (last_ctrl_r_event_ms_ != 0) &&
                          (now - last_ctrl_r_event_ms_ <= kCtrlRRepeatGapMs);
    last_ctrl_r_event_ms_ = now;

    if (sameHoldStream) {
      if (!ctrl_r_long_fired_ && (now - ctrl_r_hold_start_ms_ >= kCtrlRLongPressMs)) {
        ctrl_r_long_fired_ = true;
        withAudioGuard([&]() { mini_acid_.setSongPosition(0); });
        moveCursorToRow(0);
        setScrollToPlayhead(0);
        showToast("Song: START", 1000);
      }
      return true;
    }

    ctrl_r_hold_start_ms_ = now;
    ctrl_r_long_fired_ = false;

    bool wasSongMode = mini_acid_.songModeEnabled();
    bool newReverse = !mini_acid_.isSongReverse();
    int len = mini_acid_.songLength();
    LOG_INFO_UI("Song reverse: %s (songMode=%d len=%d pos=%d)",
                newReverse ? "ON" : "OFF",
                (int)wasSongMode, len, mini_acid_.currentSongPosition());
    withAudioGuard([&]() {
      if (!mini_acid_.songModeEnabled()) {
        // Keep reverse toggle anchored to the row user is editing.
        mini_acid_.setSongPosition(cursorRow());
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

  /*
  if (ui_event.meta && key_m) { 
    LOG_INFO_UI("Merge songs (other -> active slot %d)", mini_acid_.activeSongSlot());
    withAudioGuard([&]() { mini_acid_.mergeSongs(); });
    showToast("Merge: done", 1000);
    return true;
  }
  if (ui_event.meta && key_n) { 
    LOG_INFO_UI("Alternate songs (interleave slots)");
    withAudioGuard([&]() { mini_acid_.alternateSongs(); });
    showToast("Alternate: done", 1000);
    return true;
  }
  */
  
  if (cursorOnModeButton() && (key == '\n' || key == '\r')) {
    return toggleSongMode();
  }

  // ENTER: Acknowledge Rehearsal Pause or jump to pattern editor
  if (!ui_event.ctrl && !ui_event.alt && (key == '\n' || key == '\r')) {
    if (mini_acid_.isWaitingForRehearsal()) {
      withAudioGuard([&]() { mini_acid_.acknowledgeRehearsal(); });
      showToast("RESUME", 800);
      return true;
    }

    // Quick jump to pattern editor
    bool validTrk = false;
    SongTrack trk = trackForColumn(cursorTrack(), validTrk);
    if (validTrk) {
        int patIndex = mini_acid_.songPatternAt(cursorRow(), trk);
        int targetPage = -1;
        char patLabel[16] = "---";
        
        if (patIndex >= 0) {
            formatSongPatternLabel(patIndex, patLabel, sizeof(patLabel));
        }

        if (trk == SongTrack::SynthA) {
            targetPage = 1;
            if (patIndex >= 0) withAudioGuard([&]() { mini_acid_.set303PatternIndex(0, patIndex); });
        } else if (trk == SongTrack::SynthB) {
            targetPage = 2;
            if (patIndex >= 0) withAudioGuard([&]() { mini_acid_.set303PatternIndex(1, patIndex); });
        } else if (trk == SongTrack::Drums) {
            targetPage = 5;
            if (patIndex >= 0) withAudioGuard([&]() { mini_acid_.setDrumPatternIndex(patIndex); });
        }

        if (targetPage >= 0) {
            char toast[32];
            snprintf(toast, sizeof(toast), "Edit: %s", patLabel);
            showToast(toast, 800);
            requestPageTransition(targetPage);
            return true;
        }
    }
  }

  // Alt+M: Toggle song mode (M alone is now Delete Row)
  if (ui_event.alt && !ui_event.ctrl && key_m) {
    LOG_INFO_UI("Toggle song mode (Alt+M)");
    return toggleSongMode();
  }

  // ctrl + M: Delete Row at cursor
  if (ui_event.ctrl && !ui_event.alt && key_m) {
    LOG_INFO_UI("Delete row at cursor");
    return deleteRowAtCursor();
  }

  // ctrl+ N: Insert Row at cursor
  if (ui_event.ctrl && !ui_event.alt && key_n) {
    LOG_INFO_UI("Insert row at cursor");
    return insertRowAtCursor();
  }

  if (!ui_event.ctrl && !ui_event.alt && key_v) {
    cycleLaneFocusMode();
    switch (lane_focus_mode_) {
      case LaneFocusMode::AllTracks:
        showToast("Lane: ALL", 900);
        break;
      case LaneFocusMode::SynthPair:
        showToast("Lane: AB", 900);
        break;
      case LaneFocusMode::RhythmPair:
      default:
        showToast(kVoiceLaneInSongEditor ? "Lane: DR+VO" : "Lane: DR", 900);
        break;
    }
    return true;
  }

  if (!ui_event.ctrl && !ui_event.alt &&
      (lowerKey == 'p' || ui_event.scancode == GROOVEPUTER_P)) {
    int playhead = mini_acid_.songPlayheadPosition();
    moveCursorToRow(playhead);
    setScrollToPlayhead(playhead);
    showToast("Cursor -> Playhead", 900);
    return true;
  }

  if (!ui_event.ctrl && !ui_event.alt && (lowerKey == 'x' || ui_event.scancode == GROOVEPUTER_X)) {
    split_compare_ = !split_compare_;
    showToast(split_compare_ ? "Split: ON" : "Split: OFF", 900);
    return true;
  }

  int patternIdx = patternIndexFromKey(lowerKey);
  if (patternIdx < 0) patternIdx = scancodeToPatternIndex(ui_event.scancode);
  
  // Pattern assignment: only if NO modifiers (prevent Ctrl+R/Ctrl+C overlap)
  if (patternIdx >= 0 && !ui_event.ctrl && !ui_event.alt) {
    if (cursorOnModeButton()) return false;
    return assignPattern(patternIdx);
  }

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
        int maxCol = maxPatternTrackColumn();
        if (max_track > maxCol) max_track = maxCol;
        for (int r = min_row; r <= max_row; ++r) {
            for (int t = min_track; t <= max_track; ++t) {
                bool valid = false;
                SongTrack track = trackForColumn(t, valid);
                if (!valid) continue;
                // Each cell remains an independent copy-on-write mutation.
                int savedRow = cursor_row_;
                int savedTrack = cursor_track_;
                cursor_row_ = r;
                cursor_track_ = t;
                generateCurrentCellPattern();
                cursor_row_ = savedRow;
                cursor_track_ = savedTrack;
            }
        }
        return true;
    } else {
        // G - Generate
        // Check for double tap
        uint32_t now = millis();
        if (last_g_press_ != 0 && now - last_g_press_ < 300) {
            // Double tap is one logical row mutation. Undo the provisional
            // single-cell result before preparing the row transaction.
            if (rollbackPendingCellGeneration(cursorRow())) {
                generateEntireRow();
            } else {
                showToast("GENERATION FAILED", 1200);
            }
            last_g_press_ = 0;
        } else {
            // The first tap is committed immediately for responsive hardware
            // feedback, but its receipt allows an exact revision/data rollback
            // if a second tap turns the gesture into row generation.
            if (generateCurrentCellPattern(true)) {
                last_g_press_ = now;
            }
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

void SongPage::drawMinimalStyle(IGfx &gfx) {
  const Rect &bounds = getBoundaries();
  const int x = bounds.x;
  const int y = bounds.y;
  const int w = bounds.w;
  const int h = bounds.h;
  if (w <= 0 || h <= 0)
    return;

  const UI::ThemePalette palette = UI::themePalette(::VisualStyle::MINIMAL);
  const int activeSlot = mini_acid_.activeSongSlot();
  const int playSlot = mini_acid_.songPlaybackSlot();
  const bool liveMix = mini_acid_.liveMixModeEnabled();
  const bool songMode = mini_acid_.songModeEnabled();
  const bool playing = mini_acid_.isPlaying();
  const int playhead = mini_acid_.songPlayheadPosition();
  const int songLength = mini_acid_.songLength();

  const int statusH = 12;
  const int footerH = 11;
  const int footerY = y + h - footerH;
  gfx.fillRect(x, y, w, statusH, palette.panel);
  gfx.drawLine(x, y + statusH - 1, x + w - 1, y + statusH - 1, palette.dim);

  char stateBuf[40];
  std::snprintf(stateBuf, sizeof(stateBuf), "E:%c P:%c PAT:%c%s %s", 'A' + activeSlot,
                'A' + playSlot, 'A' + assignment_bank_index_,
                liveMix ? " LM" : "", laneShortLabel());
  gfx.setTextColor(palette.secondary);
  gfx.drawText(x + 2, y + 2, stateBuf);

  const char *modeText = songMode ? "PLAY" : "EDIT";
  const int modeW = textWidth(gfx, modeText) + 8;
  const int modeX = x + w - modeW - 2;

  char barBuf[20];
  std::snprintf(barBuf, sizeof(barBuf), "B%02d/%02d", playhead + 1, songLength);
  const int barW = textWidth(gfx, barBuf);
  int barX = x + 2 + textWidth(gfx, stateBuf) + 5;
  const int maxBarX = modeX - barW - 3;
  if (barX > maxBarX) barX = maxBarX;
  if (barX < x + 2) barX = x + 2;
  gfx.setTextColor(playing && songMode ? palette.active : palette.dim);
  gfx.drawText(barX, y + 2, barBuf);
  gfx.fillRect(modeX, y + 1, modeW, statusH - 3, palette.inset);
  gfx.drawRect(modeX, y + 1, modeW, statusH - 3,
               cursorOnModeButton() ? palette.focus : palette.dim);
  gfx.setTextColor(cursorOnModeButton() ? palette.focus : palette.text);
  gfx.drawText(modeX + 4, y + 2, modeText);

  if (liveMix) {
    drawLockIcon(gfx, modeX - 8, y + 3, palette.warning);
  }

  const int gridTop = y + statusH + 1;
  const int paneHeaderH = 10;
  const int rowH = 10;
  const int trackCount = visibleTrackCount();
  const int cellsTop = gridTop + paneHeaderH;
  int visibleRows = (footerY - cellsTop) / rowH;
  if (visibleRows < 1)
    visibleRows = 1;

  const int cursorRowValue = cursorRow();
  if (playing && songMode) {
    const int minTarget = std::min(cursorRowValue, playhead);
    const int maxTarget = std::max(cursorRowValue, playhead);
    if (minTarget < scroll_row_)
      scroll_row_ = minTarget;
    if (maxTarget >= scroll_row_ + visibleRows)
      scroll_row_ = maxTarget - visibleRows + 1;
  } else {
    if (cursorRowValue < scroll_row_)
      scroll_row_ = cursorRowValue;
    if (cursorRowValue >= scroll_row_ + visibleRows)
      scroll_row_ = cursorRowValue - visibleRows + 1;
  }
  if (scroll_row_ < 0)
    scroll_row_ = 0;
  const int maxStart = std::max(0, Song::kMaxPositions - visibleRows);
  if (scroll_row_ > maxStart)
    scroll_row_ = maxStart;

  int minSelRow = 0;
  int maxSelRow = -1;
  int minSelTrack = 0;
  int maxSelTrack = -1;
  if (has_selection_) {
    getSelectionBounds(minSelRow, maxSelRow, minSelTrack, maxSelTrack);
  }

  auto trackColor = [&](SongTrack track) -> IGfxColor {
    switch (track) {
    case SongTrack::SynthA:
      return palette.accent;
    case SongTrack::SynthB:
      return palette.accent2;
    case SongTrack::Drums:
      return palette.warning;
    case SongTrack::Voice:
      return palette.active;
    default:
      return palette.text;
    }
  };

  auto drawPane = [&](int paneX, int paneW, int paneSlot, bool editable) {
    const int rightInset = 4;
    const int barW = 24;
    const int usableW = paneW - rightInset;
    int cellW = (usableW - barW) / trackCount;
    if (cellW < 12)
      cellW = 12;

    const IGfxColor frame = editable ? palette.focus : palette.dim;
    gfx.fillRect(paneX, gridTop, usableW, paneHeaderH, palette.inset);
    gfx.drawRect(paneX, gridTop, usableW, footerY - gridTop, frame);

    char paneLabel[8];
    if (split_compare_)
      std::snprintf(paneLabel, sizeof(paneLabel), "%c", 'A' + paneSlot);
    else
      std::snprintf(paneLabel, sizeof(paneLabel), "BAR");
    gfx.setTextColor(editable ? palette.text : palette.dim);
    gfx.drawText(paneX + 2, gridTop + 1, paneLabel);

    for (int t = 0; t < trackCount; ++t) {
      bool valid = false;
      const SongTrack track = trackForColumn(t, valid);
      if (!valid)
        continue;
      const int tx = paneX + barW + t * cellW;
      const IGfxColor color = editable ? trackColor(track) : palette.dim;
      gfx.fillRect(tx, gridTop, cellW - 1, 1, color);
      gfx.setTextColor(color);
      gfx.drawText(tx + 2, gridTop + 1, trackHeaderLabel(t));
    }

    if (mini_acid_.isWaitingForRehearsal()) {
      gfx.setTextColor(palette.warning);
      gfx.drawText(paneX + usableW - 36, gridTop + 1, "PAUSE");
    }

    for (int i = 0; i < visibleRows; ++i) {
      const int row = scroll_row_ + i;
      if (row >= Song::kMaxPositions)
        break;
      const int rowY = cellsTop + i * rowH;
      gfx.fillRect(paneX + 1, rowY, usableW - 2, rowH,
                   (i & 1) ? palette.panel : palette.background);

      char rowBuf[8];
      std::snprintf(rowBuf, sizeof(rowBuf), "%d", row + 1);
      gfx.setTextColor(((row + 1) % 8) == 0 ? palette.warning : palette.dim);
      gfx.drawText(paneX + 2, rowY + 1, rowBuf);

      if (row == playhead) {
        const bool activePlay = playing && songMode && paneSlot == playSlot;
        if (activePlay) {
          gfx.fillRect(paneX + 1, rowY, usableW - 2, rowH, palette.inset);
          gfx.drawLine(paneX + 1, rowY, paneX + usableW - 2, rowY, palette.active);
          gfx.drawLine(paneX + 1, rowY + rowH - 1, paneX + usableW - 2,
                       rowY + rowH - 1, palette.active);
          char playRowBuf[8];
          std::snprintf(playRowBuf, sizeof(playRowBuf), ">%d", row + 1);
          gfx.setTextColor(palette.active);
          gfx.drawText(paneX + 2, rowY + 1, playRowBuf);
        } else {
          gfx.fillRect(paneX + barW, rowY, usableW - barW - 1, rowH,
                       palette.panel);
          gfx.fillRect(paneX + 1, rowY, 2, rowH, palette.secondary);
        }
      }

      if (mini_acid_.loopModeEnabled() && (row == mini_acid_.loopStartRow() ||
                                           row == mini_acid_.loopEndRow())) {
        gfx.fillRect(paneX + 1, rowY, 2, rowH, palette.warning);
      }

      for (int t = 0; t < trackCount; ++t) {
        bool valid = false;
        const SongTrack track = trackForColumn(t, valid);
        const int pattern =
            valid ? mini_acid_.songPatternAtSlot(paneSlot, row, track) : -1;
        const int tx = paneX + barW + t * cellW;
        const bool selected =
            editable &&
            (has_selection_ ? (row >= minSelRow && row <= maxSelRow &&
                               t >= minSelTrack && t <= maxSelTrack)
                            : (row == cursorRowValue && t == cursor_track_));

        if (selected && has_selection_) {
          gfx.fillRect(tx, rowY, cellW - 1, rowH, palette.focus);
        } else if (pattern >= 0 || pattern == -2) {
          gfx.fillRect(tx, rowY, cellW - 1, rowH, palette.inset);
        }

        if (pattern == -2) {
          gfx.setTextColor(palette.warning);
          gfx.drawText(tx + 2, rowY + 1, "WAIT");
        } else if (pattern >= 0) {
          char patternBuf[10];
          formatSongPatternLabel(pattern, patternBuf, sizeof(patternBuf));
          gfx.setTextColor(selected && has_selection_
                               ? palette.invert
                               : (editable ? trackColor(track) : palette.dim));
          gfx.drawText(tx + 2, rowY + 1, patternBuf);
        } else if (!selected) {
          gfx.setTextColor(palette.dim);
          gfx.drawText(tx + 2, rowY + 1, "--");
        }

        if (selected && !has_selection_) {
          gfx.drawRect(tx, rowY, cellW - 1, rowH, palette.focus);
        }
      }

      // Final playhead overlay: draw after cells so populated patterns cannot
      // erase the row outline or marker.
      if (row == playhead && playing && songMode && paneSlot == playSlot) {
        gfx.drawRect(paneX + 1, rowY, usableW - 2, rowH, palette.active);
        char playRowBuf[8];
        std::snprintf(playRowBuf, sizeof(playRowBuf), ">%d", row + 1);
        gfx.setTextColor(palette.active);
        gfx.drawText(paneX + 2, rowY + 1, playRowBuf);
      }
    }

    drawQuarterStrip(gfx, paneX + paneW - 3, cellsTop, visibleRows * rowH,
                     songQuarterFromRow(cursorRowValue),
                     editable ? palette.focus : palette.dim, palette.dim);
  };

  if (split_compare_) {
    const int gap = 4;
    const int leftW = (w - gap) / 2;
    drawPane(x, leftW, 0, activeSlot == 0);
    drawPane(x + leftW + gap, w - leftW - gap, 1, activeSlot == 1);
  } else {
    drawPane(x, w, activeSlot, true);
  }

  gfx.fillRect(x, footerY, w, footerH, palette.panel);
  gfx.drawLine(x, footerY, x + w - 1, footerY, palette.dim);
  gfx.setTextColor(palette.secondary);
  gfx.drawText(x + 2, footerY + 2, "Q-I:P B:PAT <>:TRK/SLOT V:LANE X:SPL");

  if (mini_acid_.isPageLoading()) {
    char loadBuf[32];
    std::snprintf(loadBuf, sizeof(loadBuf), "LOADING PAGE %d...",
                  mini_acid_.targetPageIndex() + 1);
    const int textW = textWidth(gfx, loadBuf);
    const int boxX = x + (w - textW) / 2 - 6;
    const int boxY = y + (h - 18) / 2;
    gfx.fillRect(boxX, boxY, textW + 12, 16, palette.background);
    gfx.drawRect(boxX, boxY, textW + 12, 16, palette.focus);
    gfx.setTextColor(palette.focus);
    gfx.drawText(boxX + 6, boxY + 3, loadBuf);
  }

  drawGeneratorHint(gfx);
}

void SongPage::drawTEGridStyle(IGfx &gfx) {
  const Rect &bounds = getBoundaries();
  int x = bounds.x;
  int y = bounds.y;
  int w = bounds.w;
  int h = bounds.h;

  // Base Colors
  IGfxColor TE_BLACK = IGfxColor::Black();
  IGfxColor TE_WHITE = IGfxColor::White();
  IGfxColor TE_GRID = IGfxColor(0x353535);
  IGfxColor TE_ACCENT = IGfxColor(0xD8D8D8);
  IGfxColor TE_DIM = IGfxColor(0x8A8A8A);
  IGfxColor TE_ROW_A = IGfxColor(0x101010);
  IGfxColor TE_ROW_B = IGfxColor(0x151515);

  // Dimmed Colors
  uint16_t TE_ROW_A_DIM = 0x1010;
  uint16_t TE_ROW_B_DIM = 0x0000;
  uint16_t TE_DIM_GRID = 0x2020;
  uint16_t TE_DIM_TEXT = 0x6060;
  uint16_t TE_DIM_DIM = 0x3030;

  // Apply Theme
  if (visual_style_ == ::VisualStyle::RETRO_CLASSIC) {
    TE_BLACK = IGfxColor(RetroTheme::BG_DEEP_BLACK);
    TE_WHITE = IGfxColor(RetroTheme::NEON_CYAN);
    TE_GRID = IGfxColor(RetroTheme::GRID_DIM);
    TE_ACCENT = IGfxColor(RetroTheme::SELECT_BRIGHT);
    TE_DIM = IGfxColor(RetroTheme::TEXT_DIM);
    TE_ROW_A = IGfxColor(RetroTheme::BG_INSET);
    TE_ROW_B = IGfxColor(RetroTheme::BG_PANEL);

    TE_ROW_A_DIM = TE_ROW_A.color16();
    TE_ROW_B_DIM = TE_BLACK.color16();
    TE_DIM_GRID = TE_GRID.color16();
    TE_DIM_TEXT = TE_DIM.color16();
    TE_DIM_DIM = TE_BLACK.color16();

  } else if (visual_style_ == ::VisualStyle::AMBER) {
    TE_BLACK = IGfxColor(AmberTheme::BG_DEEP_BLACK);
    TE_WHITE = IGfxColor(AmberTheme::TEXT_PRIMARY);
    TE_GRID = IGfxColor(AmberTheme::GRID_DIM);
    TE_ACCENT = IGfxColor(AmberTheme::SELECT_BRIGHT);
    TE_DIM = IGfxColor(AmberTheme::TEXT_DIM);
    TE_ROW_A = IGfxColor(AmberTheme::BG_INSET);
    TE_ROW_B = IGfxColor(AmberTheme::BG_PANEL);

    TE_ROW_A_DIM = TE_ROW_A.color16();
    TE_ROW_B_DIM = TE_BLACK.color16();
    TE_DIM_GRID = TE_GRID.color16();
    TE_DIM_TEXT = TE_DIM.color16();
    TE_DIM_DIM = TE_BLACK.color16();
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
    snprintf(titleBuf, sizeof(titleBuf), "SONG A|B%s", reverse ? " REV" : "");
  } else {
    snprintf(titleBuf, sizeof(titleBuf), "SONG %c%s", 'A' + slot,
             reverse ? " REV" : "");
  }
  gfx.drawText(x + 2, y + 2, titleBuf);

  char statusBuf[32];
  int pos = mini_acid_.currentSongPosition() + 1;
  int len = mini_acid_.songLength();
  bool playing = mini_acid_.isPlaying();
  bool songMode = mini_acid_.songModeEnabled();
  snprintf(statusBuf, sizeof(statusBuf), "%s %d/%d PAT:%c %s",
           playing ? (songMode ? ">" : "||") : "[]", pos, len,
           'A' + assignment_bank_index_, laneShortLabel());
  int statusW = textWidth(gfx, statusBuf);
  if (playing && songMode) {
    // Dynamic pulsing indicator in header
    bool pulse = (millis() % 800) < 400;
    if (pulse)
      gfx.drawRect(x + w - statusW - 4, y + 1, statusW + 4, header_h - 2,
                   TE_BLACK);
  }
  gfx.drawText(x + w - statusW - 2, y + 2, statusBuf);

  // LiveMix Lock Icon
  if (liveMix) {
    drawLockIcon(gfx, x + w - statusW - 10, y + 3, TE_BLACK);
  }

  int footer_h = 11;
  int grid_y = y + header_h + 1;
  int grid_h = h - header_h - footer_h - 2;
  int cell_h = 10;
  int row_gap = 1;
  int visible_rows = grid_h / (cell_h + row_gap);
  if (visible_rows < 1)
    visible_rows = 1;

  int cursor_row = cursorRow();
  int playhead = mini_acid_.songPlayheadPosition();
  if (playing && songMode) {
    int minTarget = std::min(cursor_row, playhead);
    int maxTarget = std::max(cursor_row, playhead);
    if (minTarget < scroll_row_)
      scroll_row_ = minTarget;
    if (maxTarget >= scroll_row_ + visible_rows)
      scroll_row_ = maxTarget - visible_rows + 1;
  } else {
    if (cursor_row < scroll_row_)
      scroll_row_ = cursor_row;
    if (cursor_row >= scroll_row_ + visible_rows)
      scroll_row_ = cursor_row - visible_rows + 1;
  }
  if (scroll_row_ < 0)
    scroll_row_ = 0;

  int min_sel_row = 0, max_sel_row = -1, min_sel_track = 0, max_sel_track = -1;
  if (has_selection_)
    getSelectionBounds(min_sel_row, max_sel_row, min_sel_track, max_sel_track);

  auto drawPane = [=, &gfx, this](int paneX, int paneW, int paneSlot,
                                  bool editable) {
    int tracks = visibleTrackCount();
    const int pos_w = 24;
    int col_gap = 1;
    int totalCellW = paneW - pos_w - 1 - col_gap * (tracks - 1);
    int cell_w = totalCellW / tracks;
    if (cell_w < 14)
      cell_w = 14;

    // Dim inactive pane
    uint16_t headerColor = editable ? TE_ACCENT.color16() : TE_DIM.color16();
    uint16_t sepColor = editable ? TE_GRID.color16() : TE_DIM_GRID;

    // Header
    gfx.setTextColor(headerColor);
    int grid_y = y + header_h + 2;

    if (!split_compare_ || cell_w > 18) {
      for (int t = 0; t < tracks; ++t) {
        bool valid = false;
        SongTrack headerTrack = trackForColumn(t, valid);
        if (!valid)
          continue;
        gfx.setTextColor(editable ? colorForSongTrack(headerTrack).color16()
                                  : TE_DIM.color16());
        gfx.drawText(paneX + pos_w + t * (cell_w + col_gap) + 2, grid_y,
                     trackHeaderLabel(t));
      }
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
        gfx.drawRect(paneX - 1, grid_y - 2, paneW + 2,
                     h - header_h - footer_h + 2, TE_ACCENT);
        gfx.drawRect(paneX - 2, grid_y - 3, paneW + 4,
                     h - header_h - footer_h + 4, TE_ACCENT);
      }
    }

    gfx.drawLine(paneX, grid_y + 9, paneX + paneW - 1, grid_y + 9, sepColor);

    int cells_y = grid_y + 11;
    for (int i = 0; i < visible_rows; ++i) {
      int row_idx = scroll_row_ + i;
      if (row_idx >= Song::kMaxPositions)
        break;
      int ry = cells_y + i * (cell_h + row_gap);

      // Row background
      uint16_t rowBg = (i & 1) ? TE_ROW_B.color16() : TE_ROW_A.color16();
      // Dim background if not editable
      if (!editable) {
        rowBg = (i & 1) ? TE_ROW_B_DIM : TE_ROW_A_DIM;
      }
      gfx.fillRect(paneX, ry, paneW, cell_h, rowBg);

      const bool activePlayRow =
          row_idx == playhead && playing && songMode && paneSlot == playSlot;
      if (row_idx == playhead) {
        if (activePlayRow) {
          gfx.fillRect(paneX, ry, paneW, cell_h, IGfxColor(0x303030));
          gfx.drawLine(paneX, ry, paneX + paneW - 1, ry, TE_ACCENT);
          gfx.drawLine(paneX, ry + cell_h - 1, paneX + paneW - 1,
                       ry + cell_h - 1, TE_ACCENT);
        } else {
          gfx.fillRect(paneX + pos_w, ry, paneW - pos_w, cell_h,
                       IGfxColor(0x1A1A1A));
        }
      }
      char teRowBuf[8];
      std::snprintf(teRowBuf, sizeof(teRowBuf), activePlayRow ? ">%d" : "%d",
                    row_idx + 1);
      gfx.setTextColor(activePlayRow ? TE_ACCENT : TE_DIM);
      gfx.drawText(paneX + 1, ry + 1, teRowBuf);

      for (int t = 0; t < tracks; ++t) {
        int tx = paneX + pos_w + t * (cell_w + col_gap);
        bool valid = false;
        SongTrack track = trackForColumn(t, valid);
        int pattern = -1;
        if (valid) {
          pattern =
              this->mini_acid_.songPatternAtSlot(paneSlot, row_idx, track);
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
          if (has_selection_) {
            gfx.fillRect(tx, ry, cell_w, cell_h, TE_ACCENT);
            gfx.setTextColor(TE_BLACK);
          } else {
            // Hollow cursor for single selection
            gfx.drawRect(tx - 1, ry - 1, cell_w + 2, cell_h + 2, TE_ACCENT);
            gfx.setTextColor(pattern >= 0 ? TE_WHITE.color16()
                                          : TE_DIM.color16());
          }
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
          IGfxColor patColor = colorForSongTrack(track);
          gfx.setTextColor(patColor.color16());
          gfx.drawText(tx + 1, ry + 1, patBuf);
        } else {
          gfx.drawText(tx + 1, ry + 1, "--");
        }
      }

      // Final playhead overlay must remain visible above populated cells.
      if (activePlayRow) {
        gfx.drawRect(paneX, ry, paneW, cell_h, TE_ACCENT);
        gfx.setTextColor(TE_ACCENT);
        gfx.drawText(paneX + 1, ry + 1, teRowBuf);
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

  int quarterTop = grid_y + 11;
  int quarterH = visible_rows * (cell_h + row_gap) - row_gap;
  if (quarterH > 0) {
    drawQuarterStrip(gfx, x + w - 3, quarterTop, quarterH,
                     songQuarterFromRow(cursor_row), TE_ACCENT, TE_DIM);
  }

  int footer_y = y + h - footer_h;
  gfx.drawLine(x, footer_y - 1, x + w - 1, footer_y - 1, TE_GRID);

  // Footer Status
  char footerBuf[64];
  // Show Edit Slot and Play Status if available
  // "EDIT:A PLAY:A" (future)
  int editSlot = mini_acid_.activeSongSlot();
  std::snprintf(footerBuf, sizeof(footerBuf),
                "E:%c P:%c PAT:%c%s C+N/M:row P:PH",
                'A' + editSlot, 'A' + playSlot, 'A' + assignment_bank_index_,
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
  if (playing && songMode) {
    // Running playhead glow at the bottom
    int animX = (millis() / 10) % w;
    gfx.fillRect(x + animX, footer_y - 1, 20, 1, TE_ACCENT);
  }
  drawGeneratorHint(gfx);
}

void SongPage::drawRetroClassicStyle(IGfx &gfx) {
  const Rect &bounds = getBoundaries();
  const int x = bounds.x;
  const int y = bounds.y;
  const int w = bounds.w;
  const int h = bounds.h;
  const int activeSlot = mini_acid_.activeSongSlot();
  const int playSlot = mini_acid_.songPlaybackSlot();
  const bool liveMix = mini_acid_.liveMixModeEnabled();
  const bool songMode = mini_acid_.songModeEnabled();
  const bool reverse = mini_acid_.isSongReverse();
  const bool playing = mini_acid_.isPlaying();

  char titleBuf[32];
  if (split_compare_) {
    std::snprintf(titleBuf, sizeof(titleBuf), "SONG A|B%s",
                  reverse ? " REV" : "");
  } else {
    std::snprintf(titleBuf, sizeof(titleBuf), "SONG %c%s", 'A' + activeSlot,
                  reverse ? " REV" : "");
  }
  char modeBuf[16];
  std::snprintf(modeBuf, sizeof(modeBuf), "%s", songMode ? "PLAY" : "EDIT");
  retro::drawHeaderBar(gfx, x, y, w, 12, titleBuf, modeBuf, playing,
                       static_cast<int>(mini_acid_.bpm()),
                       mini_acid_.currentSongPosition());

  const int statusY = y + 14;
  const int statusH = 10;
  gfx.fillRect(x, statusY, w, statusH, IGfxColor(RetroTheme::BG_PANEL));
  gfx.drawLine(x, statusY + statusH, x + w - 1, statusY + statusH,
               IGfxColor(RetroTheme::GRID_DIM));
  char statusBuf[48];
  std::snprintf(statusBuf, sizeof(statusBuf), "EDIT:%c PLAY:%c PAT:%c%s %s",
                'A' + activeSlot, 'A' + playSlot, 'A' + assignment_bank_index_,
                liveMix ? " LM" : "", laneShortLabel());
  gfx.setTextColor(IGfxColor(RetroTheme::TEXT_SECONDARY));
  gfx.drawText(x + 3, statusY + 1, statusBuf);
  if (liveMix) {
    drawLockIcon(gfx, x + w - 8, statusY + 2,
                 IGfxColor(RetroTheme::SELECT_BRIGHT));
  }

  const int footerY = y + h - 12;
  const int gridTop = statusY + statusH + 2;
  const int paneHeaderH = 10;
  const int rowH = 10;
  const int trackCount = visibleTrackCount();
  const int cellsTop = gridTop + paneHeaderH;
  int visibleRows = (footerY - cellsTop) / rowH;
  if (visibleRows < 1)
    visibleRows = 1;

  const int cursorRowValue = cursorRow();
  const int playhead = mini_acid_.songPlayheadPosition();
  if (playing && songMode) {
    const int minTarget = std::min(cursorRowValue, playhead);
    const int maxTarget = std::max(cursorRowValue, playhead);
    if (minTarget < scroll_row_)
      scroll_row_ = minTarget;
    if (maxTarget >= scroll_row_ + visibleRows)
      scroll_row_ = maxTarget - visibleRows + 1;
  } else {
    if (cursorRowValue < scroll_row_)
      scroll_row_ = cursorRowValue;
    if (cursorRowValue >= scroll_row_ + visibleRows)
      scroll_row_ = cursorRowValue - visibleRows + 1;
  }
  if (scroll_row_ < 0)
    scroll_row_ = 0;
  const int maxStart = std::max(0, Song::kMaxPositions - visibleRows);
  if (scroll_row_ > maxStart)
    scroll_row_ = maxStart;

  int minSelRow = 0, maxSelRow = -1, minSelTrack = 0, maxSelTrack = -1;
  if (has_selection_) {
    getSelectionBounds(minSelRow, maxSelRow, minSelTrack, maxSelTrack);
  }

  auto themedTrackColor = [](SongTrack track) -> IGfxColor {
    if (track == SongTrack::SynthA)
      return IGfxColor(RetroTheme::NEON_CYAN);
    if (track == SongTrack::SynthB)
      return IGfxColor(RetroTheme::NEON_MAGENTA);
    return IGfxColor(RetroTheme::NEON_YELLOW);
  };

  auto drawPane = [&](int paneX, int paneW, int paneSlot, bool editable) {
    const int rightInset = 4;
    const int barW = 24;
    const int usableW = paneW - rightInset;
    int cellW = (usableW - barW) / trackCount;
    if (cellW < 12)
      cellW = 12;

    const IGfxColor frameColor = editable ? IGfxColor(RetroTheme::SELECT_BRIGHT)
                                          : IGfxColor(RetroTheme::GRID_DIM);
    gfx.fillRect(paneX, gridTop, usableW, paneHeaderH,
                 IGfxColor(RetroTheme::BG_INSET));
    gfx.drawRect(paneX, gridTop, usableW, footerY - gridTop, frameColor);

    char paneLabel[8];
    if (split_compare_)
      std::snprintf(paneLabel, sizeof(paneLabel), "%c", 'A' + paneSlot);
    else
      std::snprintf(paneLabel, sizeof(paneLabel), "BAR");
    gfx.setTextColor(editable ? IGfxColor(RetroTheme::TEXT_PRIMARY)
                              : IGfxColor(RetroTheme::TEXT_DIM));
    gfx.drawText(paneX + 2, gridTop + 1, paneLabel);

    for (int t = 0; t < trackCount; ++t) {
      bool valid = false;
      const SongTrack track = trackForColumn(t, valid);
      if (!valid)
        continue;
      const int tx = paneX + barW + t * cellW;
      const IGfxColor color =
          editable ? themedTrackColor(track) : IGfxColor(RetroTheme::TEXT_DIM);
      gfx.fillRect(tx, gridTop, cellW - 1, 1, color);
      gfx.setTextColor(color);
      gfx.drawText(tx + 2, gridTop + 1, trackHeaderLabel(t));
    }

    if (mini_acid_.isWaitingForRehearsal()) {
      gfx.setTextColor(IGfxColor(RetroTheme::NEON_ORANGE));
      gfx.drawText(paneX + usableW - 36, gridTop + 1, "PAUSE");
    }

    for (int i = 0; i < visibleRows; ++i) {
      const int row = scroll_row_ + i;
      if (row >= Song::kMaxPositions)
        break;
      const int ry = cellsTop + i * rowH;
      const IGfxColor rowBg =
          (i & 1) ? IGfxColor(0x0E1319) : IGfxColor(0x090D12);
      gfx.fillRect(paneX + 1, ry, usableW - 2, rowH, rowBg);

      char barBuf[8];
      std::snprintf(barBuf, sizeof(barBuf), "%d", row + 1);
      gfx.setTextColor(((row + 1) % 8) == 0 ? IGfxColor(RetroTheme::NEON_YELLOW)
                                            : IGfxColor(RetroTheme::TEXT_DIM));
      gfx.drawText(paneX + 2, ry + 1, barBuf);

      if (row == playhead) {
        const bool activePlay = playing && songMode && paneSlot == playSlot;
        if (activePlay) {
          gfx.fillRect(paneX + 1, ry, usableW - 2, rowH, IGfxColor(0x13261E));
          gfx.drawLine(paneX + 1, ry, paneX + usableW - 2, ry,
                       IGfxColor(RetroTheme::NEON_CYAN));
          gfx.drawLine(paneX + 1, ry + rowH - 1, paneX + usableW - 2,
                       ry + rowH - 1, IGfxColor(RetroTheme::NEON_CYAN));
          char playRowBuf[8];
          std::snprintf(playRowBuf, sizeof(playRowBuf), ">%d", row + 1);
          gfx.setTextColor(IGfxColor(RetroTheme::NEON_CYAN));
          gfx.drawText(paneX + 2, ry + 1, playRowBuf);
        } else {
          gfx.fillRect(paneX + barW, ry, usableW - barW - 1, rowH,
                       IGfxColor(0x11161B));
          gfx.fillRect(paneX + 1, ry, 2, rowH,
                       IGfxColor(RetroTheme::TEXT_DIM));
        }
      }

      if (mini_acid_.loopModeEnabled() && (row == mini_acid_.loopStartRow() ||
                                           row == mini_acid_.loopEndRow())) {
        gfx.fillRect(paneX + 1, ry, 2, rowH,
                     themedTrackColor(SongTrack::Drums));
      }

      for (int t = 0; t < trackCount; ++t) {
        bool valid = false;
        const SongTrack track = trackForColumn(t, valid);
        const int pattern =
            valid ? mini_acid_.songPatternAtSlot(paneSlot, row, track) : -1;
        const int tx = paneX + barW + t * cellW;
        const bool selected =
            editable &&
            (has_selection_ ? (row >= minSelRow && row <= maxSelRow &&
                               t >= minSelTrack && t <= maxSelTrack)
                            : (row == cursorRowValue && t == cursor_track_));

        if (selected && has_selection_) {
          gfx.fillRect(tx, ry, cellW - 1, rowH,
                       IGfxColor(RetroTheme::SELECT_BRIGHT));
        } else if (pattern >= 0 || pattern == -2) {
          const uint32_t bg = track == SongTrack::SynthA   ? 0x071510
                              : track == SongTrack::SynthB ? 0x100718
                                                           : 0x151007;
          gfx.fillRect(tx, ry, cellW - 1, rowH, IGfxColor(bg));
        }

        if (pattern == -2) {
          gfx.setTextColor(IGfxColor(RetroTheme::NEON_ORANGE));
          gfx.drawText(tx + 2, ry + 1, "WAIT");
        } else if (pattern >= 0) {
          char patBuf[10];
          formatSongPatternLabel(pattern, patBuf, sizeof(patBuf));
          gfx.setTextColor(selected && has_selection_
                               ? IGfxColor(RetroTheme::BG_DEEP_BLACK)
                               : (editable ? themedTrackColor(track)
                                           : IGfxColor(RetroTheme::TEXT_DIM)));
          gfx.drawText(tx + 2, ry + 1, patBuf);
        } else if (!selected) {
          gfx.setTextColor(IGfxColor(RetroTheme::GRID_DIM));
          gfx.drawText(tx + 2, ry + 1, "--");
        }

        if (selected && !has_selection_) {
          gfx.drawRect(tx, ry, cellW - 1, rowH, themedTrackColor(track));
        }
      }

      // Final playhead overlay must remain visible above populated cells.
      if (row == playhead && playing && songMode && paneSlot == playSlot) {
        gfx.drawRect(paneX + 1, ry, usableW - 2, rowH,
                     IGfxColor(RetroTheme::NEON_CYAN));
        char playRowBuf[8];
        std::snprintf(playRowBuf, sizeof(playRowBuf), ">%d", row + 1);
        gfx.setTextColor(IGfxColor(RetroTheme::NEON_CYAN));
        gfx.drawText(paneX + 2, ry + 1, playRowBuf);
      }
    }

    drawQuarterStrip(gfx, paneX + paneW - 3, cellsTop, visibleRows * rowH,
                     songQuarterFromRow(cursorRowValue),
                     editable ? IGfxColor(RetroTheme::SELECT_BRIGHT)
                              : IGfxColor(RetroTheme::TEXT_DIM),
                     IGfxColor(RetroTheme::GRID_DIM));
  };

  if (split_compare_) {
    const int gap = 4;
    const int leftW = (w - gap) / 2;
    drawPane(x, leftW, 0, activeSlot == 0);
    drawPane(x + leftW + gap, w - leftW - gap, 1, activeSlot == 1);
  } else {
    drawPane(x, w, activeSlot, true);
  }

  if (mini_acid_.isPageLoading()) {
    char loadBuf[32];
    std::snprintf(loadBuf, sizeof(loadBuf), "LOADING PAGE %d...",
                  mini_acid_.targetPageIndex() + 1);
    const int textW = textWidth(gfx, loadBuf);
    const int boxX = x + (w - textW) / 2 - 6;
    const int boxY = y + (h - 18) / 2;
    gfx.fillRect(boxX, boxY, textW + 12, 16,
                 IGfxColor(RetroTheme::BG_DEEP_BLACK));
    gfx.drawRect(boxX, boxY, textW + 12, 16,
                 IGfxColor(RetroTheme::NEON_YELLOW));
    gfx.setTextColor(IGfxColor(RetroTheme::NEON_YELLOW));
    gfx.drawText(boxX + 6, boxY + 3, loadBuf);
  }

  retro::drawFooterBar(gfx, x, footerY, w, 12,
                       "Q-I:Pat  <-/->:Track/Bank  C+N/M:Row", "SONG");
}

void SongPage::drawAmberStyle(IGfx &gfx) {
  const Rect &bounds = getBoundaries();
  const int x = bounds.x;
  const int y = bounds.y;
  const int w = bounds.w;
  const int h = bounds.h;
  const int activeSlot = mini_acid_.activeSongSlot();
  const int playSlot = mini_acid_.songPlaybackSlot();
  const bool liveMix = mini_acid_.liveMixModeEnabled();
  const bool songMode = mini_acid_.songModeEnabled();
  const bool reverse = mini_acid_.isSongReverse();
  const bool playing = mini_acid_.isPlaying();

  char titleBuf[32];
  if (split_compare_) {
    std::snprintf(titleBuf, sizeof(titleBuf), "SONG A|B%s",
                  reverse ? " REV" : "");
  } else {
    std::snprintf(titleBuf, sizeof(titleBuf), "SONG %c%s", 'A' + activeSlot,
                  reverse ? " REV" : "");
  }
  char modeBuf[16];
  std::snprintf(modeBuf, sizeof(modeBuf), "%s", songMode ? "PLAY" : "EDIT");
  amber::drawHeaderBar(gfx, x, y, w, 12, titleBuf, modeBuf, playing,
                       static_cast<int>(mini_acid_.bpm()),
                       mini_acid_.currentSongPosition());

  const int statusY = y + 14;
  const int statusH = 10;
  gfx.fillRect(x, statusY, w, statusH, IGfxColor(AmberTheme::BG_PANEL));
  gfx.drawLine(x, statusY + statusH, x + w - 1, statusY + statusH,
               IGfxColor(AmberTheme::GRID_DIM));
  char statusBuf[48];
  std::snprintf(statusBuf, sizeof(statusBuf), "EDIT:%c PLAY:%c PAT:%c%s %s",
                'A' + activeSlot, 'A' + playSlot, 'A' + assignment_bank_index_,
                liveMix ? " LM" : "", laneShortLabel());
  gfx.setTextColor(IGfxColor(AmberTheme::TEXT_SECONDARY));
  gfx.drawText(x + 3, statusY + 1, statusBuf);
  if (liveMix) {
    drawLockIcon(gfx, x + w - 8, statusY + 2,
                 IGfxColor(AmberTheme::SELECT_BRIGHT));
  }

  const int footerY = y + h - 12;
  const int gridTop = statusY + statusH + 2;
  const int paneHeaderH = 10;
  const int rowH = 10;
  const int trackCount = visibleTrackCount();
  const int cellsTop = gridTop + paneHeaderH;
  int visibleRows = (footerY - cellsTop) / rowH;
  if (visibleRows < 1)
    visibleRows = 1;

  const int cursorRowValue = cursorRow();
  const int playhead = mini_acid_.songPlayheadPosition();
  if (playing && songMode) {
    const int minTarget = std::min(cursorRowValue, playhead);
    const int maxTarget = std::max(cursorRowValue, playhead);
    if (minTarget < scroll_row_)
      scroll_row_ = minTarget;
    if (maxTarget >= scroll_row_ + visibleRows)
      scroll_row_ = maxTarget - visibleRows + 1;
  } else {
    if (cursorRowValue < scroll_row_)
      scroll_row_ = cursorRowValue;
    if (cursorRowValue >= scroll_row_ + visibleRows)
      scroll_row_ = cursorRowValue - visibleRows + 1;
  }
  if (scroll_row_ < 0)
    scroll_row_ = 0;
  const int maxStart = std::max(0, Song::kMaxPositions - visibleRows);
  if (scroll_row_ > maxStart)
    scroll_row_ = maxStart;

  int minSelRow = 0, maxSelRow = -1, minSelTrack = 0, maxSelTrack = -1;
  if (has_selection_) {
    getSelectionBounds(minSelRow, maxSelRow, minSelTrack, maxSelTrack);
  }

  auto themedTrackColor = [](SongTrack track) -> IGfxColor {
    if (track == SongTrack::SynthA)
      return IGfxColor(AmberTheme::NEON_CYAN);
    if (track == SongTrack::SynthB)
      return IGfxColor(AmberTheme::NEON_MAGENTA);
    return IGfxColor(AmberTheme::NEON_ORANGE);
  };

  auto drawPane = [&](int paneX, int paneW, int paneSlot, bool editable) {
    const int rightInset = 4;
    const int barW = 24;
    const int usableW = paneW - rightInset;
    int cellW = (usableW - barW) / trackCount;
    if (cellW < 12)
      cellW = 12;

    const IGfxColor frameColor = editable ? IGfxColor(AmberTheme::SELECT_BRIGHT)
                                          : IGfxColor(AmberTheme::GRID_DIM);
    gfx.fillRect(paneX, gridTop, usableW, paneHeaderH,
                 IGfxColor(AmberTheme::BG_INSET));
    gfx.drawRect(paneX, gridTop, usableW, footerY - gridTop, frameColor);

    char paneLabel[8];
    if (split_compare_)
      std::snprintf(paneLabel, sizeof(paneLabel), "%c", 'A' + paneSlot);
    else
      std::snprintf(paneLabel, sizeof(paneLabel), "BAR");
    gfx.setTextColor(editable ? IGfxColor(AmberTheme::TEXT_PRIMARY)
                              : IGfxColor(AmberTheme::TEXT_DIM));
    gfx.drawText(paneX + 2, gridTop + 1, paneLabel);

    for (int t = 0; t < trackCount; ++t) {
      bool valid = false;
      const SongTrack track = trackForColumn(t, valid);
      if (!valid)
        continue;
      const int tx = paneX + barW + t * cellW;
      const IGfxColor color =
          editable ? themedTrackColor(track) : IGfxColor(AmberTheme::TEXT_DIM);
      gfx.fillRect(tx, gridTop, cellW - 1, 1, color);
      gfx.setTextColor(color);
      gfx.drawText(tx + 2, gridTop + 1, trackHeaderLabel(t));
    }

    if (mini_acid_.isWaitingForRehearsal()) {
      gfx.setTextColor(IGfxColor(AmberTheme::NEON_ORANGE));
      gfx.drawText(paneX + usableW - 36, gridTop + 1, "PAUSE");
    }

    for (int i = 0; i < visibleRows; ++i) {
      const int row = scroll_row_ + i;
      if (row >= Song::kMaxPositions)
        break;
      const int ry = cellsTop + i * rowH;
      const IGfxColor rowBg = (i & 1) ? IGfxColor(AmberTheme::BG_PANEL)
                                      : IGfxColor(AmberTheme::BG_DEEP_BLACK);
      gfx.fillRect(paneX + 1, ry, usableW - 2, rowH, rowBg);

      char barBuf[8];
      std::snprintf(barBuf, sizeof(barBuf), "%d", row + 1);
      gfx.setTextColor(((row + 1) % 8) == 0 ? IGfxColor(AmberTheme::NEON_ORANGE)
                                            : IGfxColor(AmberTheme::TEXT_DIM));
      gfx.drawText(paneX + 2, ry + 1, barBuf);

      if (row == playhead) {
        const bool activePlay = playing && songMode && paneSlot == playSlot;
        if (activePlay) {
          gfx.fillRect(paneX + 1, ry, usableW - 2, rowH,
                       IGfxColor(AmberTheme::BG_INSET));
          gfx.drawLine(paneX + 1, ry, paneX + usableW - 2, ry,
                       IGfxColor(AmberTheme::SELECT_BRIGHT));
          gfx.drawLine(paneX + 1, ry + rowH - 1, paneX + usableW - 2,
                       ry + rowH - 1, IGfxColor(AmberTheme::SELECT_BRIGHT));
          char playRowBuf[8];
          std::snprintf(playRowBuf, sizeof(playRowBuf), ">%d", row + 1);
          gfx.setTextColor(IGfxColor(AmberTheme::SELECT_BRIGHT));
          gfx.drawText(paneX + 2, ry + 1, playRowBuf);
        } else {
          gfx.fillRect(paneX + barW, ry, usableW - barW - 1, rowH,
                       IGfxColor(AmberTheme::BG_PANEL));
          gfx.fillRect(paneX + 1, ry, 2, rowH,
                       IGfxColor(AmberTheme::TEXT_DIM));
        }
      }

      if (mini_acid_.loopModeEnabled() && (row == mini_acid_.loopStartRow() ||
                                           row == mini_acid_.loopEndRow())) {
        gfx.fillRect(paneX + 1, ry, 2, rowH,
                     themedTrackColor(SongTrack::Drums));
      }

      for (int t = 0; t < trackCount; ++t) {
        bool valid = false;
        const SongTrack track = trackForColumn(t, valid);
        const int pattern =
            valid ? mini_acid_.songPatternAtSlot(paneSlot, row, track) : -1;
        const int tx = paneX + barW + t * cellW;
        const bool selected =
            editable &&
            (has_selection_ ? (row >= minSelRow && row <= maxSelRow &&
                               t >= minSelTrack && t <= maxSelTrack)
                            : (row == cursorRowValue && t == cursor_track_));

        if (selected && has_selection_) {
          gfx.fillRect(tx, ry, cellW - 1, rowH,
                       IGfxColor(AmberTheme::SELECT_BRIGHT));
        } else if (pattern >= 0 || pattern == -2) {
          const uint32_t bg = track == SongTrack::SynthA   ? 0x071510
                              : track == SongTrack::SynthB ? 0x100718
                                                           : 0x171008;
          gfx.fillRect(tx, ry, cellW - 1, rowH, IGfxColor(bg));
        }

        if (pattern == -2) {
          gfx.setTextColor(IGfxColor(AmberTheme::NEON_ORANGE));
          gfx.drawText(tx + 2, ry + 1, "WAIT");
        } else if (pattern >= 0) {
          char patBuf[10];
          formatSongPatternLabel(pattern, patBuf, sizeof(patBuf));
          gfx.setTextColor(selected && has_selection_
                               ? IGfxColor(AmberTheme::BG_DEEP_BLACK)
                               : (editable ? themedTrackColor(track)
                                           : IGfxColor(AmberTheme::TEXT_DIM)));
          gfx.drawText(tx + 2, ry + 1, patBuf);
        } else if (!selected) {
          gfx.setTextColor(IGfxColor(AmberTheme::GRID_DIM));
          gfx.drawText(tx + 2, ry + 1, "--");
        }

        if (selected && !has_selection_) {
          gfx.drawRect(tx, ry, cellW - 1, rowH, themedTrackColor(track));
        }
      }

      // Final playhead overlay must remain visible above populated cells.
      if (row == playhead && playing && songMode && paneSlot == playSlot) {
        gfx.drawRect(paneX + 1, ry, usableW - 2, rowH,
                     IGfxColor(AmberTheme::SELECT_BRIGHT));
        char playRowBuf[8];
        std::snprintf(playRowBuf, sizeof(playRowBuf), ">%d", row + 1);
        gfx.setTextColor(IGfxColor(AmberTheme::SELECT_BRIGHT));
        gfx.drawText(paneX + 2, ry + 1, playRowBuf);
      }
    }

    drawQuarterStrip(gfx, paneX + paneW - 3, cellsTop, visibleRows * rowH,
                     songQuarterFromRow(cursorRowValue),
                     editable ? IGfxColor(AmberTheme::SELECT_BRIGHT)
                              : IGfxColor(AmberTheme::TEXT_DIM),
                     IGfxColor(AmberTheme::GRID_DIM));
  };

  if (split_compare_) {
    const int gap = 4;
    const int leftW = (w - gap) / 2;
    drawPane(x, leftW, 0, activeSlot == 0);
    drawPane(x + leftW + gap, w - leftW - gap, 1, activeSlot == 1);
  } else {
    drawPane(x, w, activeSlot, true);
  }

  if (mini_acid_.isPageLoading()) {
    char loadBuf[32];
    std::snprintf(loadBuf, sizeof(loadBuf), "LOADING PAGE %d...",
                  mini_acid_.targetPageIndex() + 1);
    const int textW = textWidth(gfx, loadBuf);
    const int boxX = x + (w - textW) / 2 - 6;
    const int boxY = y + (h - 18) / 2;
    gfx.fillRect(boxX, boxY, textW + 12, 16,
                 IGfxColor(AmberTheme::BG_DEEP_BLACK));
    gfx.drawRect(boxX, boxY, textW + 12, 16,
                 IGfxColor(AmberTheme::SELECT_BRIGHT));
    gfx.setTextColor(IGfxColor(AmberTheme::SELECT_BRIGHT));
    gfx.drawText(boxX + 6, boxY + 3, loadBuf);
  }

  amber::drawFooterBar(gfx, x, footerY, w, 12,
                       "Q-I:Pat  <-/->:Track/Bank  C+N/M:Row", "SONG");
}

std::unique_ptr<MultiPageHelpDialog> SongPage::getHelpDialog() {
  return std::make_unique<MultiPageHelpDialog>(*this);
}

int SongPage::getHelpFrameCount() const {
  return 3;
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
    case 2:
      drawHelpPageSongSelectionLoop(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
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

SongPatternMaterializer::Result SongPage::materializeSongTracks(
        int row, uint8_t trackMask) {
    SongPatternMaterializer::Request request{};
    request.row = row;
    request.pageIndex = mini_acid_.currentPageIndex();
    request.seed = mini_acid_.modeManager().generationSeed();
    const uint8_t genreTag = static_cast<uint8_t>(
        mini_acid_.genreManager().generativeMode());
    const uint8_t recipeTag = static_cast<uint8_t>(
        mini_acid_.genreManager().recipe());
    request.modeTag = static_cast<uint8_t>(
        genreTag * 17u + recipeTag * 5u +
        static_cast<uint8_t>(gen_mode_));
    request.trackMask = trackMask;
    request.preferredLocalSlot[0] = mini_acid_.current303BankIndex(0) * 8;
    request.preferredLocalSlot[1] = mini_acid_.current303BankIndex(1) * 8;
    request.preferredLocalSlot[2] = mini_acid_.currentDrumBankIndex() * 8;

    Scene& scene = mini_acid_.sceneManager().currentScene();
    auto& genreManager = mini_acid_.genreManager();
    const GenerativeMode activeGenre = genreManager.generativeMode();
    const GenreRecipeId activeRecipe = genreManager.recipe();
    const GenerativeParams& params =
        genreManager.getCompiledGenerativeParams();
    const GenreBehavior behavior = genreManager.getBehavior();
    const GrooveboxMode mappedMode = GenreManager::grooveboxModeForRecipe(
        activeRecipe, activeGenre);

    SynthPattern atlasA{};
    SynthPattern atlasB{};
    DrumPatternSet atlasDrums{};
    const uint8_t variationCount = AtlasRuntime::variationCount(activeRecipe);
    const uint8_t variation = variationCount == 0
        ? 0
        : static_cast<uint8_t>(std::min(
              static_cast<int>(variationCount) - 1,
              static_cast<int>(gen_mode_)));
    const bool atlasReady = AtlasRuntime::hasRecipe(activeRecipe) &&
        AtlasRuntime::applyRecipe(activeRecipe, variation,
                                  atlasA, atlasB, atlasDrums, nullptr);

    auto generateTrack = [&](SongTrack track,
                             uint32_t seed,
                             SynthPattern& synth,
                             DrumPatternSet& drums) {
        if (atlasReady) {
            switch (track) {
                case SongTrack::SynthA:
                    synth = atlasA;
                    return !SongPatternMaterializer::synthPatternIsStrictlyEmpty(synth);
                case SongTrack::SynthB:
                    synth = atlasB;
                    return !SongPatternMaterializer::synthPatternIsStrictlyEmpty(synth);
                case SongTrack::Drums:
                    drums = atlasDrums;
                    return !SongPatternMaterializer::drumPatternSetIsStrictlyEmpty(drums);
                case SongTrack::Voice:
                    return false;
            }
        }

        GrooveboxModeManager generator(mini_acid_);
        generator.setModeLocal(mappedMode);
        generator.setFlavorLocal(0);
        generator.setGenerationSeed(seed);

        switch (track) {
            case SongTrack::SynthA:
                generator.generatePattern(
                    synth, mini_acid_.bpm(), params, behavior, 0);
                return !SongPatternMaterializer::synthPatternIsStrictlyEmpty(
                    synth);
            case SongTrack::SynthB:
                generator.generatePattern(
                    synth, mini_acid_.bpm(), params, behavior, 1);
                return !SongPatternMaterializer::synthPatternIsStrictlyEmpty(
                    synth);
            case SongTrack::Drums:
                generator.generateDrumPattern(drums, params, behavior);
                return !SongPatternMaterializer::drumPatternSetIsStrictlyEmpty(
                    drums);
            case SongTrack::Voice:
                return false;
        }
        return false;
    };

    auto commit = [&](auto&& applyPrepared) {
        withRuntimeAudioGuard([&]() {
            applyPrepared();
            if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
                mini_acid_.setSongPosition(row);
            }
        });
    };

    return SongPatternMaterializer::generate(
        scene, request, generateTrack, commit);
}

bool SongPage::generateCurrentCellPattern(bool rememberForDoubleTap) {
    const int row = cursorRow();
    bool valid = false;
    const SongTrack track = trackForColumn(cursorTrack(), valid);
    const uint8_t trackMask = valid
        ? SongPatternMaterializer::maskForTrack(track)
        : 0;
    if (trackMask == 0) {
        showToast("GENERATION FAILED", 1000);
        return false;
    }

    Scene& scene = mini_acid_.sceneManager().currentScene();
    const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);
    const int trackIndex =
        SongPatternMaterializer::editableTrackIndex(track);
    const int oldReference =
        scene.songs[songSlot].positions[row].patterns[trackIndex];
    const int oldSongLength = scene.songs[songSlot].length;
    const GroovePuterState::SceneRevisionState revisionBefore =
        GroovePuterState::sceneRevisionSnapshot();

    const SongPatternMaterializer::Result result =
        materializeSongTracks(row, trackMask);
    if (!result) {
        showToast(
            result.error == SongPatternMaterializer::Error::NoEmptyPatternSlots
                ? "NO EMPTY PATTERN SLOTS"
                : "GENERATION FAILED",
            1200);
        return false;
    }

    if (rememberForDoubleTap) {
        pending_cell_generation_.valid = true;
        pending_cell_generation_.row = row;
        pending_cell_generation_.page = mini_acid_.currentPageIndex();
        pending_cell_generation_.songSlot = songSlot;
        pending_cell_generation_.track = track;
        pending_cell_generation_.oldReference = oldReference;
        pending_cell_generation_.generatedReference =
            result.globalPattern[trackIndex];
        pending_cell_generation_.oldSongLength = oldSongLength;
        pending_cell_generation_.revisionBefore = revisionBefore;
    }

    char patternLabel[12];
    formatSongPatternLabel(
        result.globalPattern[trackIndex], patternLabel, sizeof(patternLabel));
    const char* trackLabel = track == SongTrack::SynthA
        ? "A"
        : track == SongTrack::SynthB ? "B" : "DR";
    char message[96];
    std::snprintf(
        message, sizeof(message), "GEN %s -> %s %s/%s",
        trackLabel, patternLabel,
        GenreManager::generativeModeName(
            mini_acid_.genreManager().generativeMode()),
        GenreManager::recipeName(mini_acid_.genreManager().recipe()));
    showToast(message, 1400);
    return true;
}


bool SongPage::rollbackPendingCellGeneration(int row) {
    if (!pending_cell_generation_.valid ||
        pending_cell_generation_.row != row ||
        pending_cell_generation_.page != mini_acid_.currentPageIndex()) {
        pending_cell_generation_.valid = false;
        return false;
    }

    Scene& scene = mini_acid_.sceneManager().currentScene();
    const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);
    if (songSlot != pending_cell_generation_.songSlot) {
        pending_cell_generation_.valid = false;
        return false;
    }

    const SongTrack track = pending_cell_generation_.track;
    const int trackIndex =
        SongPatternMaterializer::editableTrackIndex(track);
    Song& song = scene.songs[songSlot];
    if (trackIndex < 0 ||
        song.positions[row].patterns[trackIndex] !=
            pending_cell_generation_.generatedReference) {
        pending_cell_generation_.valid = false;
        return false;
    }

    int referenceCount = 0;
    for (int slot = 0; slot < 2; ++slot) {
        for (int songRow = 0; songRow < Song::kMaxPositions; ++songRow) {
            if (scene.songs[slot].positions[songRow].patterns[trackIndex] ==
                pending_cell_generation_.generatedReference) {
                ++referenceCount;
            }
        }
    }
    if (referenceCount != 1) {
        pending_cell_generation_.valid = false;
        return false;
    }

    const int localSlot =
        (songPatternBank(pending_cell_generation_.generatedReference) *
             Bank<SynthPattern>::kPatterns) +
        songPatternIndexInBank(
            pending_cell_generation_.generatedReference);
    if (localSlot < 0 || localSlot >= kPatternsPerPage) {
        pending_cell_generation_.valid = false;
        return false;
    }
    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;

    withRuntimeAudioGuard([&]() {
        switch (track) {
            case SongTrack::SynthA:
                scene.synthABanks[bank].patterns[index] = SynthPattern{};
                break;
            case SongTrack::SynthB:
                scene.synthBBanks[bank].patterns[index] = SynthPattern{};
                break;
            case SongTrack::Drums:
                scene.drumBanks[bank].patterns[index] = DrumPatternSet{};
                break;
            case SongTrack::Voice:
                break;
        }
        song.positions[row].patterns[trackIndex] =
            static_cast<int16_t>(pending_cell_generation_.oldReference);
        song.length = pending_cell_generation_.oldSongLength;
    });
    GroovePuterState::restoreSceneRevision(
        pending_cell_generation_.revisionBefore);
    pending_cell_generation_.valid = false;
    return true;
}

bool SongPage::generateEntireRow() {
    const int row = cursorRow();
    const SongPatternMaterializer::Result result = materializeSongTracks(
        row, SongPatternMaterializer::kEditableTrackMask);
    if (!result) {
        showToast(
            result.error == SongPatternMaterializer::Error::NoEmptyPatternSlots
                ? "NO EMPTY PATTERN SLOTS"
                : "GENERATION FAILED",
            1200);
        return false;
    }

    char message[96];
    std::snprintf(message, sizeof(message), "GENERATED ROW %d %s/%s",
                  row + 1,
                  GenreManager::generativeModeName(
                      mini_acid_.genreManager().generativeMode()),
                  GenreManager::recipeName(
                      mini_acid_.genreManager().recipe()));
    showToast(message, 1100);
    return true;
}

#include "song_page_r4_owner.inc"
