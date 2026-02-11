#include "waveform_page.h"

WaveformVisualization::WaveformVisualization(IGfx& gfx) : gfx_(gfx) {
    for (int i = 0; i < kMaxWavePoints; ++i) wave_data_[i] = 0;
}

void WaveformVisualization::setWaveData(const int16_t* data, int len) {
  int copyLen = len > kMaxWavePoints ? kMaxWavePoints : len;
  for (int i = 0; i < copyLen; ++i) {
    wave_data_[i] = data[i];
  }
  wave_len_ = copyLen;
}

void WaveformVisualization::drawWaveformInRegion(const Rect& region, IGfxColor color) {
  if (wave_len_ < 2) return;

  int mid_y = region.y + region.h / 2;
  int amplitude = (region.h / 2) - 1;
  if (amplitude < 1) amplitude = 1;

  int step = region.w / (wave_len_ - 1);
  if (step == 0) step = 1;

  int16_t prev_y = mid_y - (static_cast<int32_t>(wave_data_[0]) * amplitude) / 32768;

  for (int i = 1; i < wave_len_; ++i) {
    int16_t curr_y = mid_y - (static_cast<int32_t>(wave_data_[i]) * amplitude) / 32768;
    int x0 = region.x + (i - 1) * step;
    int x1 = region.x + i * step;
    
    // Safety check to stay within region
    if (x1 >= region.x + region.w) x1 = region.x + region.w - 1;
    
    gfx_.drawLine(x0, prev_y, x1, curr_y, color);
    prev_y = curr_y;
  }
}
