#ifndef DRUM_PATTERN_AUTOMATION_PAGE_H_
#define DRUM_PATTERN_AUTOMATION_PAGE_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../ui_core.h"
#include "../components/drum_automation_lane_editor.h"
#include "../components/bank_selection_bar.h"
#include "../components/combo_box.h"
#include "../components/label_component.h"
#include "../components/pattern_selection_bar.h"
#include "../components/drum_automation_lane_label.h"
#include "../../dsp/mini_dsp_params.h"
#include "../../dsp/miniacid_engine.h"

class DrumPatternAutomationPage : public IPage {
 public:
  DrumPatternAutomationPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string & getTitle() const override { return title_; }

 private:
  int clampCursor(int cursorIndex) const;
  int activeBankCursor() const;
  int patternIndexFromKey(char key) const;
  int bankIndexFromKey(char key) const;
  DrumAutomationParamId selectedParamId() const;
  void setBankIndex(int bankIndex);
  void setPatternCursor(int cursorIndex);
  void withAudioGuard(const std::function<void()>& fn);
  int activePatternCursor() const;

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard& audio_guard_;
  int pattern_row_cursor_;
  int bank_index_;
  int bank_cursor_;
  std::string title_;
  std::shared_ptr<LabelComponent> pattern_label_;
  std::shared_ptr<PatternSelectionBarComponent> pattern_bar_;
  std::shared_ptr<BankSelectionBarComponent> bank_bar_;
  std::shared_ptr<ComboBoxComponent> combo_box_;
  std::shared_ptr<DrumAutomationLaneEditor> automation_editor_;
  std::vector<DrumAutomationParamId> param_ids_;
};

#endif  // DRUM_PATTERN_AUTOMATION_PAGE_H_
