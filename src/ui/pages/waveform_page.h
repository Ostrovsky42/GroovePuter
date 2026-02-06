#pragma once

#include <cstdint>

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class WaveformPage : public IPage {
 public:
  WaveformPage(IGfx& gfx, GroovePuter& mini_acid, AudioGuard audio_guard);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string & getTitle() const override;

 private:
  IGfx& gfx_;
  GroovePuter& mini_acid_;
  AudioGuard audio_guard_;

  static constexpr int kWaveHistoryLayers = 4;
  static constexpr int kMaxWavePoints = 256;
  int16_t wave_history_[kWaveHistoryLayers][kMaxWavePoints];
  int wave_lengths_[kWaveHistoryLayers];
};
