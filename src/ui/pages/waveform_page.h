#pragma once

#include "../ui_core.h"

class WaveformVisualization {
 public:
  explicit WaveformVisualization(IGfx& gfx);
  void setWaveData(const int16_t* data, int len);
  void drawWaveformInRegion(const Rect& region, IGfxColor color);

 private:
  IGfx& gfx_;
  static constexpr int kMaxWavePoints = 128;
  int16_t wave_data_[kMaxWavePoints];
  int wave_len_ = 0;
};
