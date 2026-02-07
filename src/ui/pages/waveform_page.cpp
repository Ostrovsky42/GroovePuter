#include "waveform_page.h"

#include "../help_dialog_frames.h"
#include "../ui_common.h"

namespace {
constexpr IGfxColor kWaveFadeColors[] = {
  IGfxColor(0x808080),
  IGfxColor(0x404040),
  IGfxColor(0x202020),
};
constexpr int kWaveFadeColorCount =
    static_cast<int>(sizeof(kWaveFadeColors) / sizeof(kWaveFadeColors[0]));
} // namespace

WaveformPage::WaveformPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
  : gfx_(gfx),
    mini_acid_(mini_acid),
    audio_guard_(audio_guard)
{
  for (int i = 0; i < kWaveHistoryLayers; ++i) {
    wave_lengths_[i] = 0;
  }
}

void WaveformPage::draw(IGfx& gfx) {
  const Rect& bounds = getBoundaries();
  int x = bounds.x;
  int y = bounds.y;
  int w = bounds.w;
  int h = bounds.h;

  int wave_y = y + 2;
  int wave_h = h - 2;
  if (w < 4 || wave_h < 4) return;

  // Get waveform buffer (thread-safe)
  const auto& waveBuffer = mini_acid_.getWaveformBuffer();
  int mid_y = wave_y + wave_h / 2;

  gfx_.setTextColor(IGfxColor::Orange());
  const IGfxColor color = COLOR_WAVE; // Base reference line
  gfx_.drawLine(x, mid_y, x + w - 1, mid_y, color);

  int points = w;
  if (points > kMaxWavePoints) points = kMaxWavePoints;
  if (waveBuffer.count > 1 && points > 1) {
    int16_t new_wave[kMaxWavePoints];
    for (int px = 0; px < points; ++px) {
      size_t idx = static_cast<size_t>((uint64_t)px * (waveBuffer.count - 1) / (points - 1));
      new_wave[px] = waveBuffer.data[idx];
    }

    for (int layer = kWaveHistoryLayers - 1; layer > 0; --layer) {
      wave_lengths_[layer] = wave_lengths_[layer - 1];
      for (int px = 0; px < wave_lengths_[layer]; ++px) {
        wave_history_[layer][px] = wave_history_[layer - 1][px];
      }
    }

    wave_lengths_[0] = points;
    for (int px = 0; px < points; ++px) {
      wave_history_[0][px] = new_wave[px];
    }
  }

  auto drawWave = [&](const int16_t* wave, int count, IGfxColor color) {
    int drawCount = count;
    if (drawCount > w) drawCount = w;
    if (drawCount < 2) return;
    int amplitude = wave_h / 2 - 2;
    if (amplitude < 1) amplitude = 1;
    for (int px = 0; px < drawCount - 1; ++px) {
      float s0 = wave[px] / 32768.0f;
      float s1 = wave[px + 1] / 32768.0f;
      int y0 = mid_y - static_cast<int>(s0 * amplitude);
      int y1 = mid_y - static_cast<int>(s1 * amplitude);
      drawLineColored(gfx_, x + px, y0, x + px + 1, y1, color);
    }
  };

  for (int layer = kWaveHistoryLayers - 1; layer >= 1; --layer) {
    int colorIndex = layer - 1;
    if (colorIndex >= kWaveFadeColorCount) colorIndex = kWaveFadeColorCount - 1;
    drawWave(wave_history_[layer], wave_lengths_[layer], kWaveFadeColors[colorIndex]);
  }

  // Use global synchronized color
  IGfxColor waveColor = UI::kWaveColors[UI::waveformOverlay.colorIndex % UI::kNumWaveColors];
  drawWave(wave_history_[0], wave_lengths_[0], waveColor);
}

bool WaveformPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  switch (ui_event.scancode) {
    case GROOVEPUTER_UP:
    case GROOVEPUTER_DOWN:
      UI::waveformOverlay.colorIndex = (UI::waveformOverlay.colorIndex + 1) % UI::kNumWaveColors;
      return true;
    default:
      break;
  }
  return false;
}

const std::string & WaveformPage::getTitle() const {
  static std::string title = "WAVEFORM";
  return title;
}
