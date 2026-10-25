// SPDX-License-Identifier: MIT
// GroovePuter experiment (not the product image): Felucca ANALOG as an IMonoSynthVoice. See felucca_analog_voice.h.
#if defined(GROOVEPUTER_FELUCCA_ANALOG)
#include "felucca_analog_voice.h"

#include "dual_osc_presets.h"

constexpr uint8_t kPresetIndex = 0;   // the preset is applied first, so saved knob values override it on restore

#include <math.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
size_t fa_inst_bytes(void);
void fa_init(struct fa_inst* f);
void fa_set_param(struct fa_inst* f, int idx, int value);
void fa_preset(struct fa_inst* f, int id);
void fa_note_on(struct fa_inst* f, float freq_hz, int accent, int slide, int velocity);
void fa_note_off(struct fa_inst* f);
void fa_render_block(struct fa_inst* f, int32_t* out);
void fa_set_decay_s(struct fa_inst* f, float seconds);
int fa_preset_count(void);
void fa_load_preset(struct fa_inst* f, int idx);
int fa_get_param(const struct fa_inst* f, int idx);
float fa_get_decay_s(const struct fa_inst* f);
}

namespace {
constexpr float kEngineRate = 22050.0f;  // the rate the engine's tables are built for (felucca/fe_tables.h)
constexpr int kWaveCount = 7;
const char* const kWaveNames[kWaveCount] = {"Saw", "Square", "Tri", "Sine", "PWM", "Sync", "Sub"};
}  // namespace

FeluccaAnalogVoice::FeluccaAnalogVoice(float sampleRate)
    // Layout: 0-3 are the main tab's four knobs (Preset, Cutoff, Reso, Decay); 4-5 are the MORE tab's two (Wave, Drive).
    // Mix is not a knob: the preset sets it. Preset is index 0 so that a restored scene applies it before the knobs.
    : params_{Parameter("Preset", "", kDualOscPresetNames, kDualOscPresetCount, 0),
              Parameter("Cutoff", "", 0.0f, 127.0f, 61.0f, 1.0f),
              Parameter("Reso", "", 0.0f, 127.0f, 83.0f, 1.0f),
              Parameter("Decay", "ms", 30.0f, 2400.0f, 250.0f, 10.0f),
              Parameter("Wave", "", kWaveNames, kWaveCount, 0),
              Parameter("Drive", "", 0.0f, 127.0f, 0.0f, 1.0f)} {
  inst_ = static_cast<fa_inst*>(calloc(1, fa_inst_bytes()));
  setSampleRate(sampleRate);
  reset();
}

FeluccaAnalogVoice::~FeluccaAnalogVoice() { free(inst_); }

void FeluccaAnalogVoice::setSampleRate(float sampleRate) {
  sampleRate_ = sampleRate;
  enabled_ = inst_ != nullptr && fabsf(sampleRate - kEngineRate) < 0.5f;
}

void FeluccaAnalogVoice::presetDefaults() {
  if (!inst_) return;
  fa_init(inst_);
  fa_preset(inst_, 0);                 // INIT: one saw
  for (uint8_t i = 0; i < parameterCount(); ++i)
    if (i != kPresetIndex) applyParameter(i);
}

void FeluccaAnalogVoice::reset() {
  presetDefaults();
  blockPos_ = 32;
  memset(block_, 0, sizeof(block_));
}

void FeluccaAnalogVoice::startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity) {
  if (!enabled_ || freqHz <= 0.0f) return;
  fa_note_on(inst_, freqHz, accent ? 1 : 0, slideFlag ? 1 : 0, velocity);
}

void FeluccaAnalogVoice::release() {
  if (enabled_) fa_note_off(inst_);
}

float FeluccaAnalogVoice::process() {
  if (!enabled_) return 0.0f;
  if (blockPos_ >= 32) {
    fa_render_block(inst_, block_);
    blockPos_ = 0;
  }
  return static_cast<float>(block_[blockPos_++]) / 32768.0f;
}

void FeluccaAnalogVoice::applyParameter(uint8_t index) {
  if (!inst_) return;
  const int v = static_cast<int>(params_[index].value());
  switch (index) {
    case 1: fa_set_param(inst_, 4, v); break;   // CUT   (P_E4)
    case 2: fa_set_param(inst_, 5, v); break;   // RES   (P_E5)
    case 3: fa_set_decay_s(inst_, params_[3].value() / 1000.0f); break;   // DECAY ms
    case 4: fa_set_param(inst_, 0, v); break;   // WAVE  (P_E0)
    case 5: fa_set_param(inst_, 6, v); break;   // DRV   (P_E6)
    case kPresetIndex: {  // PRESET: INIT, or one of the library's 12; the knobs take the preset's values
      if (v <= 0) {
        fa_init(inst_);
        fa_preset(inst_, 0);
      } else {
        fa_load_preset(inst_, v - 1);
      }
      params_[1].setValue(static_cast<float>(fa_get_param(inst_, 4)));
      params_[2].setValue(static_cast<float>(fa_get_param(inst_, 5)));
      params_[3].setValue(fa_get_decay_s(inst_) * 1000.0f);
      params_[4].setValue(static_cast<float>(fa_get_param(inst_, 0)));
      params_[5].setValue(static_cast<float>(fa_get_param(inst_, 6)));
      for (uint8_t k = 1; k < parameterCount(); ++k) applyParameter(k);
      break;
    }
    default: break;
  }
}

void FeluccaAnalogVoice::setParameterNormalized(uint8_t index, float norm) {
  if (index >= parameterCount()) return;
  if (norm < 0.0f) norm = 0.0f;
  if (norm > 1.0f) norm = 1.0f;
  params_[index].setNormalized(norm);
  applyParameter(index);
}

float FeluccaAnalogVoice::getParameterNormalized(uint8_t index) const {
  if (index >= parameterCount()) return 0.0f;
  return params_[index].normalized();
}

const Parameter& FeluccaAnalogVoice::getParameter(uint8_t index) const {
  return params_[index < parameterCount() ? index : 0];
}

#endif  // GROOVEPUTER_FELUCCA_ANALOG
