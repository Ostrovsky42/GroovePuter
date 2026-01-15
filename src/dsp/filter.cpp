#include "filter.h"

#include <math.h>

ChamberlinFilterBase::ChamberlinFilterBase(float sampleRate) 
    : _lp(0.0f), _bp(0.0f), _hp(0.0f), _sampleRate(sampleRate) {
  if (_sampleRate <= 0.0f) _sampleRate = 44100.0f;
}

void ChamberlinFilterBase::reset() {
  _lp = 0.0f;
  _bp = 0.0f;
  _hp = 0.0f;
}

void ChamberlinFilterBase::setSampleRate(float sr) {
  if (sr <= 0.0f) sr = 44100.0f;
  _sampleRate = sr;
}

void ChamberlinFilterBase::processInternal(float input, float cutoffHz, float resonance) {
  float f = 2.0f * sinf(3.14159265f * cutoffHz / _sampleRate);
  if (!isfinite(f))
    f = 0.0f;
  float q = 1.0f / (1.0f + resonance * 4.0f);
  if (q < 0.06f)
    q = 0.06f;

  _hp = input - _lp - q * _bp;
  _bp += f * _hp;
  _lp += f * _bp;

  _bp = tanhf(_bp * 1.3f);

  // Keep states bounded to avoid numeric blowups
  const float kStateLimit = 50.0f;
  if (_lp > kStateLimit) _lp = kStateLimit;
  if (_lp < -kStateLimit) _lp = -kStateLimit;
  if (_bp > kStateLimit) _bp = kStateLimit;
  if (_bp < -kStateLimit) _bp = -kStateLimit;
  if (_hp > kStateLimit) _hp = kStateLimit;
  if (_hp < -kStateLimit) _hp = -kStateLimit;
}

float ChamberlinFilterLp::process(float input, float cutoffHz, float resonance) {
  processInternal(input, cutoffHz, resonance);
  return _lp;
}

float ChamberlinFilterBp::process(float input, float cutoffHz, float resonance) {
  processInternal(input, cutoffHz, resonance);
  return _bp;
}

float ChamberlinFilterHp::process(float input, float cutoffHz, float resonance) {
  processInternal(input, cutoffHz, resonance);
  return _hp;
}
