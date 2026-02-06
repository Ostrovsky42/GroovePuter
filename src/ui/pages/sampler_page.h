#pragma once

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class SamplerPage : public IPage {
 public:
  SamplerPage(IGfx& gfx, GroovePuter& mini_acid, AudioGuard audio_guard);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;
  void setBoundaries(const Rect& rect) override;
  template <typename F>
  void withAudioGuard(F&& fn) {
      if (audio_guard_) audio_guard_(std::forward<F>(fn));
      else fn();
  }

 private:
  class LabelValueComponent;
  
  void initComponents();
  void adjustFocusedElement(int direction);
  void prelisten();

  IGfx& gfx_;
  GroovePuter& mini_acid_;
  AudioGuard audio_guard_;
  std::string title_ = "SAMPLER";
  
  bool initialized_ = false;
  int current_pad_ = 0;
  int current_sample_idx_ = -1;
  
  // Dialog (Kit Load)
  enum class DialogType { None = 0, LoadKit };
  DialogType dialog_type_ = DialogType::None;
  std::vector<std::string> kits_;
  int list_selection_index_ = 0;
  int list_scroll_offset_ = 0;
  
  void refreshKits();
  void loadKit(const std::string& kitName);
  void openLoadKitDialog();
  void closeDialog();
  void drawDialog(IGfx& gfx);
  bool handleDialogEvent(UIEvent& ui_event);
  
  std::shared_ptr<LabelValueComponent> kit_ctrl_;
  
  // UI Components
  std::shared_ptr<LabelValueComponent> pad_ctrl_;
  std::shared_ptr<LabelValueComponent> file_ctrl_;
  std::shared_ptr<LabelValueComponent> volume_ctrl_;
  std::shared_ptr<LabelValueComponent> pitch_ctrl_;
  std::shared_ptr<LabelValueComponent> start_ctrl_;
  std::shared_ptr<LabelValueComponent> end_ctrl_;
  std::shared_ptr<LabelValueComponent> loop_ctrl_;
  std::shared_ptr<LabelValueComponent> reverse_ctrl_;
  std::shared_ptr<LabelValueComponent> choke_ctrl_;


};
