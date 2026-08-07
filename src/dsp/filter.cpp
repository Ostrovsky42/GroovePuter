#include "filter.h"

#include <math.h>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kHalfPi = 1.57079632679489661923f;

// Fast soft saturation to avoid expensive tanhf() in hot DSP paths.
inline float fastSaturate(float x) {
  return x / (1.0f + fabsf(x));
}

// 7th-order sine approximation for the Chamberlin coefficient hot path.
// cutoff/sampleRate maps to [0, pi/2] for the useful low-pass range. The
// approximation stays within about 0.001% of sinf() at the TB303 8 kHz cap
// while avoiding a libm transcendental call for every rendered sample.
inline float fastSinForChamberlin(float x) {
  if (x < 0.0f) x = 0.0f;
  if (x > kHalfPi) x = kHalfPi;
  const float x2 = x * x;
  return x *
         (1.0f + x2 *
                     (-1.0f / 6.0f +
                      x2 * (1.0f / 120.0f + x2 * (-1.0f / 5040.0f))));
}
}  // namespace

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
  const float phase = kPi * cutoffHz / _sampleRate;
  float f = 2.0f * fastSinForChamberlin(phase);
  if (!isfinite(f))
    f = 0.0f;
  float q = 1.0f / (1.0f + resonance * 4.0f);
  if (q < 0.06f)
    q = 0.06f;

  float hp = input - _lp - q * _bp;
  _bp += f * hp;
  _lp += f * _bp;

  _bp = fastSaturate(_bp * 1.3f);

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
    _s[i] += f * (fastSaturate(prev) - fastSaturate(_s[i]));
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
