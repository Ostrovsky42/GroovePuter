#pragma once

#include <array>
#include <cstdint>

class DrumReverb {
public:
  DrumReverb();

  void reset();
  void setSampleRate(float sr);
  void setMix(float mix);
  void setDecay(float decay);

  float process(float input);

private:
  struct OnePoleLP {
    float z = 0.0f;
    float a = 0.0f;

    void reset() { z = 0.0f; }
    void setCutoff(float cutoffHz, float sampleRate);
    float process(float input) {
      z += a * (input - z);
      return z;
    }
  };

  struct OnePoleHPF {
    float y = 0.0f;
    float x1 = 0.0f;
    float a = 0.0f;

    void reset() {
      y = 0.0f;
      x1 = 0.0f;
    }
    void setCutoff(float cutoffHz, float sampleRate);
    float process(float input) {
      float out = a * (y + input - x1);
      x1 = input;
      y = out;
      return out;
    }
  };

  struct DelayLine {
    int16_t* buffer = nullptr;
    int size_ = 0;
    int index = 0;

    void setBuffer(int16_t* data, int size);
    void reset();
    int size() const { return size_; }
    float read() const;
    void write(float value);
  };

  struct Allpass {
    DelayLine delay;

    void reset() { delay.reset(); }
    float process(float input, float k) {
      float buf = delay.read();
      float y = -k * input + buf;
      delay.write(input + k * y);
      return y;
    }
  };

  void updateMix();
  void updateDecay();

  static constexpr int kCombDelaySamples[4] = {326, 392, 465, 529};
  static constexpr int kAllpassDelaySamples[2] = {52, 79};
  static constexpr int kPredelaySamples = 176;
  static constexpr int kTotalDelaySamples =
      kCombDelaySamples[0] + kCombDelaySamples[1] + kCombDelaySamples[2] + kCombDelaySamples[3] +
      kAllpassDelaySamples[0] + kAllpassDelaySamples[1] + kPredelaySamples;

  float sampleRate_ = 44100.0f;
  float mix_ = 0.0f;
  float decay_ = 0.3f;
  float wet_ = 0.0f;
  float dry_ = 1.0f;
  float combFeedback_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  OnePoleLP combDamp_[4];
  DelayLine combDelay_[4];
  Allpass allpass_[2];
  float allpassK_ = 0.7f;
  OnePoleHPF inputHpf_;
  OnePoleHPF outputHpf_;
  OnePoleLP outputLpf_;
  DelayLine predelay_;
  bool hasPredelay_ = true;
  std::array<int16_t, kTotalDelaySamples> delayMemory_{};
};
