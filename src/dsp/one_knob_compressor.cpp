#include "one_knob_compressor.h"

#include <math.h>

OneKnobCompressor::OneKnobCompressor()
  : amount_(0.0f),
    mix_(1.0f),
    enabled_(false),
    envelope_(0.0f) {}
    
void OneKnobCompressor::reset() {
  envelope_ = 0.0f;
}

void OneKnobCompressor::setAmount(float amount) {
  if (amount < 0.0f)
    amount = 0.0f;
  if (amount > 1.0f)
    amount = 1.0f;
  amount_ = amount;
}

void OneKnobCompressor::setMix(float mix) {
  if (mix < 0.0f)
    mix = 0.0f;
  if (mix > 1.0f)
    mix = 1.0f;
  mix_ = mix;
}

void OneKnobCompressor::setEnabled(bool on) {
  enabled_ = on;
  if (!enabled_) {
    envelope_ = 0.0f;
  }
}

bool OneKnobCompressor::isEnabled() const { return enabled_; }

float OneKnobCompressor::process(float input) {
  if (!enabled_) {
    return input;
  }

  // Drive the input harder at higher amounts so compression actually engages.
  float drive = 1.0f + amount_ * 2.0f;
  float driven = input * drive;
  float level = fabsf(driven);
  if (level > envelope_) {
    envelope_ += (level - envelope_) * 0.25f;
  } else {
    envelope_ += (level - envelope_) * 0.02f;
  }

  float threshold = 0.45f - 0.40f * amount_;
  float ratio = 1.0f + amount_ * 19.0f;
  float gain = 1.0f;
  if (envelope_ > threshold) {
    float compressed = threshold + (envelope_ - threshold) / ratio;
    gain = compressed / (envelope_ + 0.000001f);
  }

  float makeup = 1.0f + amount_ * 1.0f;
  float wet = driven * gain * makeup;
  return input * (1.0f - mix_) + wet * mix_;
}
