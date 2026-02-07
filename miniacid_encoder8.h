#pragma once

#include "src/dsp/miniacid_engine.h"
#include "UNIT_8ENCODER.h"

class Encoder8Miniacid {
public:
  explicit Encoder8Miniacid(MiniAcid& miniAcid);
  void initialize();
  void update();

private:
  struct EncoderParam {
    TB303ParamId param;
    int voice;
    uint32_t baseColor;
  };

  static constexpr int kEncoderCount = 8;
  static const EncoderParam kEncoderParams[kEncoderCount];

  void setInitialColors();
  void setLedFromParam(int encoderIndex);
  uint32_t applyBrightness(uint32_t baseColor, float normalized) const;

  UNIT_8ENCODER sensor_;
  bool sensor_initialized_;
  bool initial_values_sent_;
  MiniAcid& miniAcid_;
};
