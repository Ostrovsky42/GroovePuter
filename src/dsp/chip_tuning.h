#pragma once

namespace ChipTuning {

// Common AY-3-8910/YM2149 clock used by 1.7734 MHz systems. The host sample
// rate is only the renderer rate; it must never be reused as the PSG clock.
inline constexpr float kAyClockHz = 1773400.0f;
inline constexpr int kAyMaxTonePeriod = 4095;

// NTSC SN76489 clock used by the existing engine.
inline constexpr float kSnClockHz = 3579545.0f;
inline constexpr int kSnMaxToneDivider = 1023;
inline constexpr float kSnMinimumToneHz =
    kSnClockHz / (32.0f * static_cast<float>(kSnMaxToneDivider));

inline int roundPositiveToInt(float value) {
  return static_cast<int>(value + 0.5f);
}

inline float quantizeAyToneFrequency(float requestedHz) {
  if (requestedHz <= 0.0f) return 0.0f;

  int period = roundPositiveToInt(kAyClockHz / (16.0f * requestedHz));
  if (period < 1) period = 1;
  if (period > kAyMaxTonePeriod) period = kAyMaxTonePeriod;
  return kAyClockHz / (16.0f * static_cast<float>(period));
}

// GroovePuter is a musical instrument first. The physical SN76489 cannot
// represent frequencies below 109.35 Hz, so unsupported notes are folded up
// by whole octaves instead of collapsing every low key to the same divider.
inline float foldSnToneFrequencyUp(float requestedHz) {
  if (requestedHz <= 0.0f) return 0.0f;
  float foldedHz = requestedHz;
  for (int octave = 0;
       octave < 8 && foldedHz < kSnMinimumToneHz;
       ++octave) {
    foldedHz *= 2.0f;
  }
  return foldedHz;
}

// Quantize a frequency that is already inside the playable register. This is
// kept separate from octave folding so a low root can be folded once before
// chord/stack intervals are applied.
inline float quantizeSnRepresentableToneFrequency(float requestedHz) {
  if (requestedHz <= 0.0f) return 0.0f;

  int divider = roundPositiveToInt(kSnClockHz / (32.0f * requestedHz));
  if (divider < 1) divider = 1;
  if (divider > kSnMaxToneDivider) divider = kSnMaxToneDivider;
  return kSnClockHz / (32.0f * static_cast<float>(divider));
}

inline float quantizeSnToneFrequency(float requestedHz) {
  return quantizeSnRepresentableToneFrequency(
      foldSnToneFrequencyUp(requestedHz));
}

inline void snStackRatios(int stackIndex, float ratios[3]) {
  ratios[0] = 1.0f;
  ratios[1] = 1.003f;
  ratios[2] = 0.997f;

  switch (stackIndex) {
    case 1:  // Oct+: root, one octave up, two octaves up.
      ratios[1] = 2.0f;
      ratios[2] = 4.0f;
      break;
    case 2:  // Fifth: root, fifth, octave.
      ratios[1] = 1.5f;
      ratios[2] = 2.0f;
      break;
    case 3:  // Chord: root, major third, fifth.
      ratios[1] = 1.25f;
      ratios[2] = 1.5f;
      break;
    default:  // Uni: slight chip-style detune.
      break;
  }
}

// Fold and quantize the root once, then build every stack voice from that
// playable root. Folding each ratio independently would collapse low Oct+
// stacks to the same pitch.
inline void snStackFrequencies(float requestedRootHz,
                               int stackIndex,
                               float frequencies[3]) {
  float ratios[3];
  snStackRatios(stackIndex, ratios);

  const float playableRootHz = quantizeSnToneFrequency(requestedRootHz);
  frequencies[0] = playableRootHz;
  for (int voice = 1; voice < 3; ++voice) {
    frequencies[voice] = quantizeSnRepresentableToneFrequency(
        playableRootHz * ratios[voice]);
  }
}

}  // namespace ChipTuning
