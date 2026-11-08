#pragma once
#ifndef GROOVEPUTER_DSP_SOUND_QUALITY_PRESETS_H
#define GROOVEPUTER_DSP_SOUND_QUALITY_PRESETS_H
#include <cstdint>
class TB303Voice;
class WaveMorphSynthVoice;

// Explicit control-thread recipes. Applying one never changes global/track
// gain, engine identity, parameter layout or persisted defaults automatically.
namespace SoundQualityPresets {
enum class Tb303 : uint8_t { ClassicAcid, DeepBass, AggressiveAcid, Count };
enum class WaveMorph : uint8_t { WarmPadLikeLead, DigitalBass, BrightLead, Count };
struct Tb303Recipe {
  const char* name;
  uint8_t oscillator, filter;
  float cutoff, resonance, envelopeAmount, decayMs;
};
struct WaveMorphRecipe {
  const char* name;
  uint8_t wave;
  float morph, sub, cutoff, resonance, decayMs;
};
inline constexpr Tb303Recipe tb303[] = {
  {"Classic Acid", 0, 1, 700, .60f, 1400, 260},
  {"Deep Bass", 4, 4, 650, .20f, 500, 650},
  {"Aggressive Acid", 0, 6, 1100, .55f, 1800, 180},
};
inline constexpr WaveMorphRecipe waveMorph[] = {
  {"Warm Pad-like Lead", 4, .15f, .10f, 1400, .10f, 1800},
  {"Digital Bass", 7, .15f, .65f, 1100, .20f, 300},
  {"Bright Lead", 5, .30f, .10f, 5000, .12f, 750},
};
bool apply(TB303Voice& voice, Tb303 preset);
bool apply(WaveMorphSynthVoice& voice, WaveMorph preset);
}  // namespace SoundQualityPresets

#endif  // GROOVEPUTER_DSP_SOUND_QUALITY_PRESETS_H
