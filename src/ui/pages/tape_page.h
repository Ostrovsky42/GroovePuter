#pragma once

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include "src/dsp/miniacid_engine.h"

class TapePage : public IPage {
 public:
  TapePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
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
  class SliderComponent;
  class ModeComponent;
  class PresetComponent;
  
  void initComponents();
  void syncFromState();

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  
  bool initialized_ = false;
  
  // UI Components
  std::shared_ptr<SliderComponent> wow_slider_;
  std::shared_ptr<SliderComponent> age_slider_;
  std::shared_ptr<SliderComponent> sat_slider_;
  std::shared_ptr<SliderComponent> tone_slider_;
  std::shared_ptr<SliderComponent> crush_slider_;
  
  std::shared_ptr<FocusableComponent> mode_ctrl_;
  std::shared_ptr<FocusableComponent> preset_ctrl_;

  std::string title_ = "TAPE";
};
