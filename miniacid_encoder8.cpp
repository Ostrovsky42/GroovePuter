#include <algorithm>
#include <cmath>

#include <M5Cardputer.h>

#include "miniacid_encoder8.h"
#include "display.h"

namespace {

static constexpr uint32_t LED_COLOR_CUTOFF = IGfxColor::Orange().color24();
static constexpr uint32_t LED_COLOR_RESONANCE = IGfxColor::Cyan().color24();
static constexpr uint32_t LED_COLOR_ENV_AMOUNT = IGfxColor::Magenta().color24();
static constexpr uint32_t LED_COLOR_ENV_DECAY = IGfxColor::Green().color24();

}  // namespace

const Encoder8Miniacid::EncoderParam Encoder8Miniacid::kEncoderParams[Encoder8Miniacid::kEncoderCount] = {
    {TB303ParamId::Cutoff,    0, LED_COLOR_CUTOFF},
    {TB303ParamId::Resonance, 0, LED_COLOR_RESONANCE},
    {TB303ParamId::EnvAmount, 0, LED_COLOR_ENV_AMOUNT},
    {TB303ParamId::EnvDecay,  0, LED_COLOR_ENV_DECAY},
    {TB303ParamId::Cutoff,    1, LED_COLOR_CUTOFF},
    {TB303ParamId::Resonance, 1, LED_COLOR_RESONANCE},
    {TB303ParamId::EnvAmount, 1, LED_COLOR_ENV_AMOUNT},
    {TB303ParamId::EnvDecay,  1, LED_COLOR_ENV_DECAY},
};

Encoder8Miniacid::Encoder8Miniacid(MiniAcid& miniAcid)
    : sensor_(),
      sensor_initialized_(false),
      initial_values_sent_(false),
      miniAcid_(miniAcid) {}

void Encoder8Miniacid::initialize() {
  // sensor_.begin(&Wire, ENCODER_ADDR, G2, G1, 100000UL);
  sensor_initialized_ = false; // Disabled until pins are properly defined for Cardputer
  initial_values_sent_ = false;
}

void Encoder8Miniacid::update() {
  if (!sensor_initialized_) return;

  if (!initial_values_sent_) {
    setInitialColors();
    initial_values_sent_ = true;
  }

  for (int i = 0; i < kEncoderCount; i++) {
    int inc_value = sensor_.getIncrementValue(i);
    if (inc_value != 0) {
      const EncoderParam& enc = kEncoderParams[i];
      miniAcid_.adjust303Parameter(enc.param, inc_value, enc.voice);
      setLedFromParam(i);
    }
  }
}

void Encoder8Miniacid::setInitialColors() {
  for (int i = 0; i < kEncoderCount; i++) {
    setLedFromParam(i);
  }
}

void Encoder8Miniacid::setLedFromParam(int encoderIndex) {
  if (encoderIndex < 0 || encoderIndex >= kEncoderCount) return;
  const EncoderParam& enc = kEncoderParams[encoderIndex];
  float norm = miniAcid_.parameter303(enc.param, enc.voice).normalized();
  uint32_t color = applyBrightness(enc.baseColor, norm);
  sensor_.setLEDColor(encoderIndex, color);
}

uint32_t Encoder8Miniacid::applyBrightness(uint32_t baseColor, float normalized) const {
  float clamped = std::clamp(normalized, 0.0f, 1.0f);
  static const float kLogNorm = 1.0f / static_cast<float>(std::log1p(9.0));
  float brightness = static_cast<float>(std::log1p(clamped * 9.0f)) * kLogNorm;
  uint8_t r = static_cast<uint8_t>(std::round(((baseColor >> 16) & 0xff) * brightness));
  uint8_t g = static_cast<uint8_t>(std::round(((baseColor >> 8) & 0xff) * brightness));
  uint8_t b = static_cast<uint8_t>(std::round((baseColor & 0xff) * brightness));
  return (static_cast<uint32_t>(r) << 16) |
         (static_cast<uint32_t>(g) << 8) |
         static_cast<uint32_t>(b);
}
