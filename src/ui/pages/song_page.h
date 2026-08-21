#pragma once

#include <utility>

#include "../../../scenes.h"
#include "../ui_core.h"
#include "../pages/help_dialog.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include "../../dsp/pattern_generator.h"
#include "../../dsp/song_pattern_materializer.h"
#include "src/state/scene_revision.h"
#include "src/state/song_edit.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"
#include "src/generation/migration/quantized_generation_commit.h"

class SongPage : public IPage, public IMultiHelpFramesProvider {
 public:
  enum class LaneFocusMode {
    AllTracks = 0,
    SynthPair = 1,
    RhythmPair = 2,
  };

  SongPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string & getTitle() const override;
  std::unique_ptr<MultiPageHelpDialog> getHelpDialog() override;
  int getHelpFrameCount() const override;
  void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const override;

  void setScrollToPlayhead(int playhead);
  void setVisualStyle(::VisualStyle style) override { visual_style_ = style; }
  ::VisualStyle getVisualStyle() const { return visual_style_; }

 private:
  bool handleEventLegacyUnowned(UIEvent& ui_event);
  bool undoPreparedSongState();

  template <typename PrepareFn>
  bool commitSongMutation(PrepareFn&& prepare) {
    using GroovePuterUndo::SongUndoPayload;
    using GroovePuterUndo::UndoKind;
    SceneManager& manager = mini_acid_.sceneManager();
    SongUndoPayload before{};
    if (!GroovePuterUndo::captureCurrentSongUndo(manager, before)) return false;
    Song after = before.before;
    std::forward<PrepareFn>(prepare)(after);
    if (GroovePuterUndo::sameSong(before.before, after)) return false;
    if (!GroovePuterUndo::songUndoTargetAvailable(manager, before)) return false;

    auto lease =
        GroovePuterRhythm::LiveSongArrangementDetail::
            prepareSongMutationActivation(
                mini_acid_, before.songSlot, before.before, after);
    if (!lease.ok()) {
      if (lease.status ==
          GroovePuterRhythm::LiveSongArrangementDetail::SongLiveStatus::Busy) {
        UI::showToast("SONG BUSY", 900);
      }
      return false;
    }
    if (!GroovePuterRhythm::LiveSongArrangementDetail::
            songMutationTargetStillCommitSafe(mini_acid_, lease)) {
      GroovePuterRhythm::LiveSongArrangementDetail::
          abortSongMutationActivation(
              lease, GroovePuterRhythm::QuantizedGenerationStatus::
                         CancelledTargetChanged);
      return false;
    }

    const bool committed = GroovePuterUndo::undoOwner().commitPrepared(
        UndoKind::Song, before, [&]() {
          const auto apply = [&]() {
            manager.currentScene().songs[before.songSlot] = after;
          };
          if (audio_guard_) audio_guard_(apply);
          else apply();
        });
    if (!committed) {
      GroovePuterRhythm::LiveSongArrangementDetail::
          abortSongMutationActivation(lease);
      return false;
    }
    if (lease.boundaryRequired) {
      GroovePuterRhythm::LiveSongArrangementDetail::
          completeSongMutationActivation(
              lease.slot, GroovePuterUndo::undoOwner().committedRevision());
    }
    return true;
  }

  void drawMinimalStyle(IGfx& gfx);
  void drawRetroClassicStyle(IGfx& gfx);
  void drawAmberStyle(IGfx& gfx);
  void drawTEGridStyle(IGfx& gfx);
  void initModeButton(int x, int y, int w, int h);
  int clampCursorRow(int row) const;
  int cursorRow() const;
  int cursorTrack() const;
  bool cursorOnModeButton() const;
  bool cursorOnPlayheadLabel() const;
  void moveCursorHorizontal(int delta, bool extend_selection);
  void moveCursorVertical(int delta, bool extend_selection);
  void syncSongPositionToCursor();

  // Transport/navigation owns TIME. R4 persistent Song arrangement edits use
  // commitSongMutation; audio exclusion alone must not dirty or expire Undo.
  template <typename F>
  void withAudioGuard(F&& fn) {
    if (audio_guard_) audio_guard_(std::forward<F>(fn));
    else fn();
  }

  template <typename F>
  void withRuntimeAudioGuard(F&& fn) {
    if (audio_guard_) audio_guard_(std::forward<F>(fn));
    else fn();
  }
  void startSelection();
  void updateSelection();
  void clearSelection();
  bool moveSelectionFrameBy(int deltaRow, int deltaTrack);
  void updateLoopRangeFromSelection();
  void getSelectionBounds(int& min_row, int& max_row, int& min_track, int& max_track) const;
  int visibleTrackCount() const;
  int logicalTrackCount() const;
  int maxEditableTrackColumn() const;
  int maxPatternTrackColumn() const;
  const char* laneShortLabel() const;
  SongTrack thirdLaneTrack() const;
  const char* trackHeaderLabel(int col) const;
  int visibleColumnForTrack(SongTrack track) const;
  void cycleLaneFocusMode();
  void normalizeCursorTrackAfterFocusChange(LaneFocusMode previous_mode);
  void moveCursorToRow(int row);
  void saveMarker(int marker_index);
  bool jumpToMarker(int marker_index);
  bool hasVoiceDataInSlot(int slot) const;
  bool hasVoiceDataInActiveSlot() const;
  SongTrack trackForColumn(int col, bool& valid) const;
  int bankIndexForTrack(SongTrack track) const;
  int patternIndexFromKey(char key) const;
  bool adjustSongPatternAtCursor(int delta);
  bool flipSongPatternBankAtCursorOrSelection();
  bool adjustSongPlayhead(int delta);
  bool assignPattern(int patternIdx);
  bool clearPattern();
  bool toggleSongMode();
  bool toggleLoopMode();
  bool insertRowAtCursor();
  bool deleteRowAtCursor();

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  int cursor_row_;
  int cursor_track_;
  int scroll_row_;
  bool has_selection_;
  int selection_start_row_;
  int selection_start_track_;
  bool selection_locked_ = false;
  Container mode_button_container_;
  bool mode_button_initialized_ = false;

  SmartPatternGenerator::Mode gen_mode_;
  bool show_genre_hint_;
  uint32_t hint_timer_;
  uint32_t last_g_press_ = 0;
  struct PendingCellGeneration {
    bool valid = false;
    int row = 0;
    int page = 0;
    int songSlot = 0;
    SongTrack track = SongTrack::SynthA;
    int oldReference = -1;
    int generatedReference = -1;
    int oldSongLength = 1;
    GroovePuterState::SceneRevisionState revisionBefore{};
  } pending_cell_generation_;
  uint32_t last_ctrl_r_event_ms_ = 0;
  uint32_t ctrl_r_hold_start_ms_ = 0;
  bool ctrl_r_long_fired_ = false;
  LaneFocusMode lane_focus_mode_ = LaneFocusMode::AllTracks;
  int assignment_bank_index_ = 0;
  bool split_compare_ = true;
  int row_markers_[4] = {-1, -1, -1, -1};

  SongPatternMaterializer::Result materializeSongTracks(int row, uint8_t trackMask);
  bool generateCurrentCellPattern(bool rememberForDoubleTap = false);
  bool rollbackPendingCellGeneration(int row);
  bool generateEntireRow();
  void cycleGeneratorMode();
  void drawGeneratorHint(IGfx& gfx);

  ::VisualStyle visual_style_ = ::VisualStyle::MINIMAL;
};
