#include "filter.h"

#include <math.h>

namespace {
constexpr float kPi = 3.14159265358979323846f;

// Fast soft saturation to avoid expensive tanhf() in hot DSP paths.
inline float fastSaturate(float x) {
  return x / (1.0f + fabsf(x));
}

// Rational tan approximation for the bounded [0, .45*pi] coefficient.
// At the product's 8 kHz / 22050 Hz limit its relative error is <0.008%.
// Avoid a per-sample transcendental while the filter envelope moves cutoff.
inline float fastTanForSvf(float x) {
  const float x2 = x * x;
  return x * (105.0f - 10.0f * x2) /
         (105.0f - 45.0f * x2 + x2 * x2);
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
  // Topology-preserving state-variable low-pass. The retained class/API and
  // two states serve the same LP1/Warm/Retro profiles. The old explicit update
  // became unstable at reachable accented cutoffs; multiplying bandpass state
  // by 1.3 also created autonomous oscillation at zero resonance.
  const float cutoff = fminf(fmaxf(cutoffHz, 0.0f), _sampleRate * 0.45f);
  const float boundedResonance = fminf(fmaxf(resonance, 0.0f), 0.95f);
  const float g = fastTanForSvf(kPi * cutoff / _sampleRate);
  const float damping = 1.0f / (1.0f + boundedResonance * 4.0f);
  const float normalization = 1.0f / (1.0f + g * (g + damping));
  const float band = (_bp + g * (input - _lp)) * normalization;
  const float low = _lp + g * band;
  _bp = 2.0f * band - _bp;
  _lp = 2.0f * low - _lp;
  return low;
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
