#pragma once

class AudioFilter {
public:
  virtual ~AudioFilter() = default;

  virtual void reset() = 0;
  virtual void setSampleRate(float sr) = 0;
  virtual float process(float input, float cutoffHz, float resonance) = 0;
};

class ChamberlinFilter : public AudioFilter {
public:
  explicit ChamberlinFilter(float sampleRate);
  void reset() override;
  void setSampleRate(float sr) override;
  float process(float input, float cutoffHz, float resonance) override;

private:
  float _lp;
  float _bp;
  float _sampleRate;
};


class DiodeFilter : public AudioFilter {
public:
  explicit DiodeFilter(float sampleRate);
  void reset() override;
  void setSampleRate(float sr) override;
  float process(float input, float cutoffHz, float resonance) override;

private:
  float _s[4];
  float _sampleRate;
};

class LadderFilter : public AudioFilter {
public:
  explicit LadderFilter(float sampleRate);
  void reset() override;
  void setSampleRate(float sr) override;
  float process(float input, float cutoffHz, float resonance) override;

private:
  float _s[4];
  float _sampleRate;
};

