#pragma once

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class TapePage : public IPage {
 public:
  TapePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;
  void setBoundaries(const Rect& rect) override;

 private:
  class LabelValueComponent;
  class KnobComponent;
  
  void initComponents();
  void adjustFocusedElement(int direction);

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard& audio_guard_;
  
  bool initialized_ = false;
  
  // UI Components
  std::shared_ptr<KnobComponent> wow_knob_;
  std::shared_ptr<KnobComponent> flutter_knob_;
  std::shared_ptr<KnobComponent> saturation_knob_;
  
  std::shared_ptr<LabelValueComponent> rec_ctrl_;
  std::shared_ptr<LabelValueComponent> play_ctrl_;
  std::shared_ptr<LabelValueComponent> dub_ctrl_;
  std::shared_ptr<LabelValueComponent> clear_btn_;

  std::string title_ = "TAPE & LO-FI";
};
