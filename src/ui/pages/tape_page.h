#pragma once

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include "src/dsp/miniacid_engine.h"
#include "waveform_page.h"

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
  
  void drawCassette(IGfx& gfx);
  void drawReel(IGfx& gfx, int x, int y, int radius, float rotation);
  void drawTape(IGfx& gfx);
  void updateAnimations();
  
  void initComponents();
  void syncFromState();

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  
  WaveformVisualization waveform_;

  // Animation state
  float left_reel_radius_ = 20.0f;
  float right_reel_radius_ = 10.0f;
  float reel_rotation_ = 0.0f;
  bool direction_ = true;
  unsigned long last_frame_time_ = 0;
  static constexpr int kFrameDelay = 50;
  
  bool initialized_ = false;
  
  // UI Components
  std::shared_ptr<SliderComponent> wow_slider_;
  std::shared_ptr<SliderComponent> age_slider_;
  std::shared_ptr<SliderComponent> sat_slider_;
  std::shared_ptr<SliderComponent> tone_slider_;
  std::shared_ptr<SliderComponent> crush_slider_;
  std::shared_ptr<SliderComponent> looper_slider_;
  
  std::shared_ptr<FocusableComponent> mode_ctrl_;
  std::shared_ptr<FocusableComponent> preset_ctrl_;

  // Performance helpers
  bool perf_wash_active_ = false;
  uint8_t perf_prev_space_ = 0;
  uint8_t perf_prev_movement_ = 0;
  uint8_t perf_prev_groove_ = 0;
  bool perf_loop_muted_ = false;
  float perf_prev_loop_volume_ = 0.55f;

  std::string title_ = "TAPE";
};
