#pragma once

#include <utility>

#include "../ui_core.h"
#include "../pages/help_dialog.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include "../../dsp/miniacid_engine.h"
#include "../../state/scene_revision.h"
#include "src/state/synth_pattern_edit.h"
#include "../../state/song_edit.h"
#include "../../state/undo_owner.h"
#include "../../state/undo_receipts.h"

class BankSelectionBarComponent;
class PatternSelectionBarComponent;

class PatternEditPage : public IPage, public IMultiHelpFramesProvider {
 public:
  PatternEditPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard, int voice_index);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  void tick() override;
  const std::string & getTitle() const override;
  void setContext(int context) override; // context = step index to focus
  std::unique_ptr<MultiPageHelpDialog> getHelpDialog() override;
  int getHelpFrameCount() const override;
  void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const override;

  int activePatternCursor() const;
  int activePatternStep() const;
  void setPatternCursor(int cursorIndex);
  void focusPatternRow();
  void focusPatternSteps();
  bool patternRowFocused() const;
  void movePatternCursor(int delta);
  void movePatternCursorVertical(int delta);
  void startSelection();
  void updateSelection();
  void clearSelection();
  bool hasSelection() const;
  void getSelectionBounds(int& min_row, int& max_row, int& min_col, int& max_col) const;
  bool isStepSelected(int stepIndex) const;
  bool moveSelectionFrameBy(int deltaRow, int deltaCol);
  int voiceIndex() const { return voice_index_; }

  // I1 display projection only. Song transport remains the owner of the
  // physical pattern selection; the NOTES editor mirrors that selection so
  // every visual style shows the pattern that is actually sounding. STOP does
  // not rewind Song selection, so this also preserves the last played pattern
  // as the immediate edit target.
  void syncSongPatternContext() {
    if (!mini_acid_.songModeEnabled()) return;
    pattern_row_cursor_ = mini_acid_.current303PatternIndex(voice_index_);
    bank_index_ = mini_acid_.current303BankIndex(voice_index_);
    bank_cursor_ = bank_index_;
  }

 private:
  enum class Focus { Steps = 0, PatternRow, BankRow };
  enum class PatternMutationResult { Invalid = 0, NoChange, Committed };

  // R3 inserts a narrow persistent-mutation owner in front of the retained
  // legacy event implementation. The unowned handler keeps the old routing;
  // manual Pattern writes are intercepted by handleEventLegacy().
  bool handleEventLegacy(UIEvent& ui_event);
  bool handleEventLegacyUnowned(UIEvent& ui_event);

  void drawMinimalStyle(IGfx& gfx);
  void drawRetroClassicStyle(IGfx& gfx);
  void drawAmberStyle(IGfx& gfx);

  int clampCursor(int cursorIndex) const;
  int activeBankCursor() const;
  int patternIndexFromKey(char key) const;
  int bankIndexFromKey(char key) const;
  void setBankIndex(int bankIndex);
  void ensureStepFocus();
  int noteForEntryKey(char key) const;
  void resetNoteHoldTracking();
  void advanceNoteEntryCursor();
  void writeNoteEntryStep(int step, int note, bool continuation);
  bool handleNoteEntryKey(char key);

  template <typename PrepareFn>
  PatternMutationResult commitPatternMutation(PrepareFn&& prepare) {
    using GroovePuterUndo::SynthPatternUndoPayload;
    using GroovePuterUndo::UndoKind;

    SceneManager& manager = mini_acid_.sceneManager();
    SynthPatternUndoPayload before{};
    if (!GroovePuterUndo::captureCurrentSynthPatternUndo(
            manager, voice_index_, before)) {
      return PatternMutationResult::Invalid;
    }

    SynthPattern after = before.before;
    std::forward<PrepareFn>(prepare)(after);
    if (GroovePuterUndo::PatternEdit::samePattern(before.before, after)) {
      return PatternMutationResult::NoChange;
    }

    // Page residency is validated before owner publication. COMMIT itself is a
    // single bounded in-memory assignment under the existing audio guard.
    if (!GroovePuterUndo::synthPatternUndoTargetAvailable(manager, before)) {
      return PatternMutationResult::Invalid;
    }

    SynthPatternUndoPayload prepared = before;
    prepared.before = after;
    const bool committed = GroovePuterUndo::undoOwner().commitPrepared(
        UndoKind::Pattern, before, [&]() {
          const auto apply = [&]() {
            GroovePuterUndo::restoreSynthPatternUndo(manager, prepared);
          };
          if (audio_guard_) audio_guard_(apply);
          else apply();
        });
    return committed ? PatternMutationResult::Committed
                     : PatternMutationResult::Invalid;
  }

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
    return GroovePuterUndo::undoOwner().commitPrepared(
        UndoKind::Song, before, [&]() {
          const auto apply = [&]() {
            manager.currentScene().songs[before.songSlot] = after;
          };
          if (audio_guard_) audio_guard_(apply);
          else apply();
        });
  }

  // Audio exclusion and persistent-revision ownership are deliberately
  // separate in R3. Runtime pattern/bank selection uses this guard but must not
  // dirty the Scene or expire a valid retained Undo receipt.
  template <typename F>
  void withAudioGuard(F&& fn) {
      if (audio_guard_) audio_guard_(std::forward<F>(fn));
      else fn();
  }

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  int voice_index_;
  int pattern_edit_cursor_;
  int pattern_row_cursor_;
  int bank_index_;
  int bank_cursor_;
  Focus focus_;
  std::string title_;
  std::shared_ptr<PatternSelectionBarComponent> pattern_bar_;
  std::shared_ptr<BankSelectionBarComponent> bank_bar_;
  bool chaining_mode_ = false;
  bool has_selection_ = false;
  int selection_start_step_ = 0;
  bool selection_locked_ = false;
  int last_page_ = -1;

  // Optional fast step-entry layer. It is local to NOTES and leaves legacy
  // editing bindings untouched while disabled.
  bool note_entry_mode_ = false;
  char last_note_key_ = 0;
  int last_entered_note_ = -1;
  int last_entered_step_ = -1;
  unsigned long last_note_key_ms_ = 0;
  bool note_hold_active_ = false;
};
