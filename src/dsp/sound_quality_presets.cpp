#include "sound_quality_presets.h"
#include "src/dsp/mini_tb303.h"
#include "src/dsp/wave_morph_synth_voice.h"

namespace SoundQualityPresets {
bool apply(TB303Voice& voice, Tb303 preset) {
  const auto index = static_cast<uint8_t>(preset);
  if (index >= static_cast<uint8_t>(Tb303::Count)) return false;
  const auto& p = tb303[index];
  voice.setParameter(TB303ParamId::Oscillator, p.oscillator);
  voice.setParameter(TB303ParamId::FilterType, p.filter);
  voice.setParameter(TB303ParamId::Cutoff, p.cutoff);
  voice.setParameter(TB303ParamId::Resonance, p.resonance);
  voice.setParameter(TB303ParamId::EnvAmount, p.envelopeAmount);
  voice.setParameter(TB303ParamId::EnvDecay, p.decayMs);
  return true;
}
bool apply(WaveMorphSynthVoice& voice, WaveMorph preset) {
  const auto index = static_cast<uint8_t>(preset);
  if (index >= static_cast<uint8_t>(WaveMorph::Count)) return false;
  const auto& p = waveMorph[index];
  const float values[] = {static_cast<float>(p.wave), p.morph, p.sub, p.cutoff, p.resonance, p.decayMs};
  for (uint8_t i = 0; i < 6; ++i) {
    const auto& parameter = voice.getParameter(i);
    const float range = parameter.max() - parameter.min();
    voice.setParameterNormalized(i, range > 0 ? (values[i] - parameter.min()) / range : 0);
  }
  return true;
}
}  // namespace SoundQualityPresets
