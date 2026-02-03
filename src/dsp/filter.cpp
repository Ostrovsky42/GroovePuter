#include "filter.h"

#include <math.h>

ChamberlinFilter::ChamberlinFilter(float sampleRate) : _lp(0.0f), _bp(0.0f), _sampleRate(sampleRate) {
  if (_sampleRate <= 0.0f) _sampleRate = 44100.0f;
}

void ChamberlinFilter::reset() {
  _lp = 0.0f;
  _bp = 0.0f;
}

void ChamberlinFilter::setSampleRate(float sr) {
  if (sr <= 0.0f) sr = 44100.0f;
  _sampleRate = sr;
}

float ChamberlinFilter::process(float input, float cutoffHz, float resonance) {
  float f = 2.0f * sinf(3.14159265f * cutoffHz / _sampleRate);
  if (!isfinite(f))
    f = 0.0f;
  float q = 1.0f / (1.0f + resonance * 4.0f);
  if (q < 0.06f)
    q = 0.06f;

  float hp = input - _lp - q * _bp;
  _bp += f * hp;
  _lp += f * _bp;

  _bp = tanhf(_bp * 1.3f);

  // Keep states bounded to avoid numeric blowups
  const float kStateLimit = 50.0f;
  if (_lp > kStateLimit) _lp = kStateLimit;
  if (_lp < -kStateLimit) _lp = -kStateLimit;
  if (_bp > kStateLimit) _bp = kStateLimit;
  if (_bp < -kStateLimit) _bp = -kStateLimit;

  return _lp;
}

// === DIODE FILTER (Classic Acid) ===
DiodeFilter::DiodeFilter(float sampleRate) : _sampleRate(sampleRate) { reset(); }
void DiodeFilter::reset() { for (int i=0; i<4; ++i) _s[i] = 0; }
void DiodeFilter::setSampleRate(float sr) { _sampleRate = sr; }
float DiodeFilter::process(float input, float cutoffHz, float resonance) {
  float f = (cutoffHz * 2.0f) / _sampleRate;
  if (f > 0.95f) f = 0.95f;
  float k = resonance * 17.0f; // Scale resonance to diode ranges
  for (int i=0; i<4; ++i) {
    float prev = (i == 0) ? (input - k * _s[3]) : _s[i-1];
    _s[i] += f * (tanhf(prev) - tanhf(_s[i]));
  }
  return _s[3];
}

// === LADDER FILTER (Moog Style) ===
LadderFilter::LadderFilter(float sampleRate) : _sampleRate(sampleRate) { reset(); }
void LadderFilter::reset() { for (int i=0; i<4; ++i) _s[i] = 0; }
void LadderFilter::setSampleRate(float sr) { _sampleRate = sr; }
float LadderFilter::process(float input, float cutoffHz, float resonance) {
  float f = (cutoffHz * 2.0f) / _sampleRate;
  if (f > 0.95f) f = 0.95f;
  float k = resonance * 4.0f;
  float sum = input - k * _s[3];
  for (int i=0; i<4; ++i) {
    _s[i] += f * (sum - _s[i]);
    sum = _s[i];
  }
  return _s[3];
}

