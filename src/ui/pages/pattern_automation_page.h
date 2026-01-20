#ifndef PATTERN_AUTOMATION_PAGE_H_
#define PATTERN_AUTOMATION_PAGE_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../ui_core.h"
#include "../pages/help_dialog.h"
#include "../components/automation_lane_editor.h"
#include "../components/bank_selection_bar.h"
#include "../components/combo_box.h"
#include "../components/label_component.h"
#include "../components/pattern_selection_bar.h"
#include "../../dsp/mini_dsp_params.h"
#include "../../dsp/miniacid_engine.h"

class PatternAutomationPage : public IPage, public IMultiHelpFramesProvider {
 public:
  PatternAutomationPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard, int voice_index);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string & getTitle() const override { return title_; }
  std::unique_ptr<MultiPageHelpDialog> getHelpDialog() override;
  int getHelpFrameCount() const override;
  void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const override;

 private:
  int clampCursor(int cursorIndex) const;
  int activeBankCursor() const;
  int patternIndexFromKey(char key) const;
  int bankIndexFromKey(char key) const;
  TB303ParamId selectedParamId() const;
  void setBankIndex(int bankIndex);
  void setPatternCursor(int cursorIndex);
  void withAudioGuard(const std::function<void()>& fn);
  int activePatternCursor() const;

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard& audio_guard_;
  int voice_index_;
  int pattern_row_cursor_;
  int bank_index_;
  int bank_cursor_;
  std::string title_;
  std::shared_ptr<LabelComponent> pattern_label_;
  std::shared_ptr<PatternSelectionBarComponent> pattern_bar_;
  std::shared_ptr<BankSelectionBarComponent> bank_bar_;
  std::shared_ptr<ComboBoxComponent> combo_box_;
  std::shared_ptr<AutomationLaneEditor> automation_editor_;
  std::vector<TB303ParamId> param_ids_;
};

#endif  // PATTERN_AUTOMATION_PAGE_H_
