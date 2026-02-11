#pragma once

#include "../ui_core.h"
#include "../pages/help_dialog.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

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

 private:
  enum class Focus { Steps = 0, PatternRow, BankRow };

  void drawMinimalStyle(IGfx& gfx);
  void drawRetroClassicStyle(IGfx& gfx);
  void drawAmberStyle(IGfx& gfx);

  int clampCursor(int cursorIndex) const;
  int activeBankCursor() const;
  int patternIndexFromKey(char key) const;
  int bankIndexFromKey(char key) const;
  void setBankIndex(int bankIndex);
  void ensureStepFocus();
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
};
