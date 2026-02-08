#pragma once

#include <algorithm>

class TransientShaper {
public:
  TransientShaper();

  void reset();
  void setSampleRate(float sr);
  void setAttackAmount(float amount);
  void setSustainAmount(float amount);

  float process(float input);

private:
  struct EnvelopeFollower {
    void reset();
    void setSampleRate(float sr);
    void setTimes(float attackMs, float releaseMs);
    float process(float input);

    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float env = 0.0f;
    float sampleRate = 44100.0f;
  };

  void updateEnvelopeTimes();

  EnvelopeFollower fastEnv_;
  EnvelopeFollower slowEnv_;
  float sampleRate_ = 44100.0f;
  float attackAmount_ = 0.0f;
  float sustainAmount_ = 0.0f;
};
