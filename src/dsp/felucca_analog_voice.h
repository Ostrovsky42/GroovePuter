// SPDX-License-Identifier: MIT
// GroovePuter experiment (not the product image): Felucca ANALOG as an IMonoSynthVoice. Only in the experimental
// image (GROOVEPUTER_FELUCCA_ANALOG). The engine runs at 22050 Hz (its tables are built for that rate); any other
// sample rate makes the voice silent rather than playing at a wrong pitch.
#ifndef FELUCCA_ANALOG_VOICE_H
#define FELUCCA_ANALOG_VOICE_H

#include <stddef.h>
#include <stdint.h>

#include "mono_synth_voice.h"

struct fa_inst;

class FeluccaAnalogVoice final : public IMonoSynthVoice {
public:
  explicit FeluccaAnalogVoice(float sampleRate);
  ~FeluccaAnalogVoice() override;

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity = 100) override;
  void release() override;
  float process() override;

  uint8_t parameterCount() const override { return 6; }
  void setParameterNormalized(uint8_t index, float norm) override;
  float getParameterNormalized(uint8_t index) const override;
  const Parameter& getParameter(uint8_t index) const override;

  const char* getEngineName() const override { return "DUAL OSC"; }
  void setMode(GrooveboxMode mode) override { (void)mode; }
  void setLoFiAmount(float amount) override { (void)amount; }

private:
  void applyParameter(uint8_t index);
  void presetDefaults();

  fa_inst* inst_ = nullptr;
  bool enabled_ = false;
  float sampleRate_ = 22050.0f;
  int32_t block_[32] = {};
  uint8_t blockPos_ = 32;
  Parameter params_[6];
};

#endif  // FELUCCA_ANALOG_VOICE_H
