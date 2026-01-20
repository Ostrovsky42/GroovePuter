#pragma once
#ifndef MINIACID_DSP_FILTER_H_
#define MINIACID_DSP_FILTER_H_

class AudioFilter {
public:
  virtual ~AudioFilter() = default;

  virtual void reset() = 0;
  virtual void setSampleRate(float sr) = 0;
  virtual float process(float input, float cutoffHz, float resonance) = 0;
};

class ChamberlinFilterBase {
public:
  explicit ChamberlinFilterBase(float sampleRate);
  void reset();
  void setSampleRate(float sr);
  void processInternal(float input, float cutoffHz, float resonance);

protected:
  float _lp;
  float _bp;
  float _hp;
  float _sampleRate;
};

class ChamberlinFilterLp : public AudioFilter, protected ChamberlinFilterBase {
public:
  explicit ChamberlinFilterLp(float sampleRate) : ChamberlinFilterBase(sampleRate) {}
  void reset() override { ChamberlinFilterBase::reset(); }
  void setSampleRate(float sr) override { ChamberlinFilterBase::setSampleRate(sr); }
  float process(float input, float cutoffHz, float resonance) override;
};

class ChamberlinFilterBp : public AudioFilter, protected ChamberlinFilterBase {
public:
  explicit ChamberlinFilterBp(float sampleRate) : ChamberlinFilterBase(sampleRate) {}
  void reset() override { ChamberlinFilterBase::reset(); }
  void setSampleRate(float sr) override { ChamberlinFilterBase::setSampleRate(sr); }
  float process(float input, float cutoffHz, float resonance) override;
};

class ChamberlinFilterHp : public AudioFilter, protected ChamberlinFilterBase {
public:
  explicit ChamberlinFilterHp(float sampleRate) : ChamberlinFilterBase(sampleRate) {}
  void reset() override { ChamberlinFilterBase::reset(); }
  void setSampleRate(float sr) override { ChamberlinFilterBase::setSampleRate(sr); }
  float process(float input, float cutoffHz, float resonance) override;
};

// Legacy alias for backward compatibility
using ChamberlinFilter = ChamberlinFilterLp;

#endif  // MINIACID_DSP_FILTER_H_
