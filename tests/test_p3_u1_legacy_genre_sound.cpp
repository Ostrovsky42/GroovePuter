#include <cassert>
#include <cmath>
#include <cstdio>

#include "src/dsp/miniacid_engine.h"

SerialMock Serial;
SDMock SD;

namespace {

bool near(float lhs, float rhs, float epsilon = 0.01f) {
  return std::fabs(lhs - rhs) <= epsilon;
}

void testLegacyAcidUsesOldAcidPresetWithoutChangingGenerationMode() {
  MiniAcid engine(44100.0f, nullptr);
  engine.modeManager().setModeLocal(GrooveboxMode::Minimal);
  engine.modeManager().setFlavorLocal(3);

  const GrooveboxMode modeBefore = engine.modeManager().mode();
  const int flavorBefore = engine.modeManager().flavor();

  const bool applied = engine.modeManager().applyLegacyGenreSoundPreset(
      GenerativeMode::Acid, 0, 0);

  assert(applied);
  assert(engine.modeManager().mode() == modeBefore);
  assert(engine.modeManager().flavor() == flavorBefore);
  assert(near(engine.parameter303(TB303ParamId::Cutoff, 0).value(), 820.0f));
  assert(near(engine.parameter303(TB303ParamId::Resonance, 0).value(), 0.66f));
  assert(near(engine.parameter303(TB303ParamId::EnvAmount, 0).value(), 460.0f));
  assert(near(engine.parameter303(TB303ParamId::EnvDecay, 0).value(), 1001.0f));
  assert(!engine.is303DistortionEnabled(0));
  assert(!engine.is303DelayEnabled(0));
}

void testLegacyTechnoUsesOldElectroDetroitPresetWithoutChangingGenerationMode() {
  MiniAcid engine(44100.0f, nullptr);
  engine.modeManager().setModeLocal(GrooveboxMode::Acid);
  engine.modeManager().setFlavorLocal(4);

  const GrooveboxMode modeBefore = engine.modeManager().mode();
  const int flavorBefore = engine.modeManager().flavor();

  const bool applied = engine.modeManager().applyLegacyGenreSoundPreset(
      GenerativeMode::Techno, 0, 0);

  assert(applied);
  assert(engine.modeManager().mode() == modeBefore);
  assert(engine.modeManager().flavor() == flavorBefore);
  assert(near(engine.parameter303(TB303ParamId::Cutoff, 0).value(), 900.0f));
  assert(near(engine.parameter303(TB303ParamId::Resonance, 0).value(), 0.46f));
  assert(near(engine.parameter303(TB303ParamId::EnvAmount, 0).value(), 280.0f));
  assert(near(engine.parameter303(TB303ParamId::EnvDecay, 0).value(), 543.2f));
  assert(!engine.is303DistortionEnabled(0));
  assert(!engine.is303DelayEnabled(0));
}

void testLegacySoundRejectsUnsupportedGenreAndInvalidPresetWithoutMutation() {
  MiniAcid engine(44100.0f, nullptr);
  const float cutoffBefore = engine.parameter303(TB303ParamId::Cutoff, 0).value();
  const GrooveboxMode modeBefore = engine.modeManager().mode();
  const int flavorBefore = engine.modeManager().flavor();

  assert(!engine.modeManager().applyLegacyGenreSoundPreset(
      GenerativeMode::House, 0, 0));
  assert(!engine.modeManager().applyLegacyGenreSoundPreset(
      GenerativeMode::Acid, 99, 0));

  assert(near(engine.parameter303(TB303ParamId::Cutoff, 0).value(), cutoffBefore));
  assert(engine.modeManager().mode() == modeBefore);
  assert(engine.modeManager().flavor() == flavorBefore);
}

void testLegacySoundRejectsNonTb303VoiceWithoutChangingFxOrEngine() {
  MiniAcid engine(44100.0f, nullptr);
  engine.setSynthEngine(0, "SID");
  assert(engine.currentSynthEngineName(0) == "SID");
  assert(!engine.is303DistortionEnabled(0));
  assert(!engine.is303DelayEnabled(0));

  // Acid preset 1 (SHARP) enables distortion. Applying it to a SID voice must
  // be rejected rather than mutating hidden TB303/FX state behind the active
  // engine.
  const bool applied = engine.modeManager().applyLegacyGenreSoundPreset(
      GenerativeMode::Acid, 1, 0);

  assert(!applied);
  assert(engine.currentSynthEngineName(0) == "SID");
  assert(!engine.is303DistortionEnabled(0));
  assert(!engine.is303DelayEnabled(0));
}

}  // namespace

int main() {
  testLegacyAcidUsesOldAcidPresetWithoutChangingGenerationMode();
  testLegacyTechnoUsesOldElectroDetroitPresetWithoutChangingGenerationMode();
  testLegacySoundRejectsUnsupportedGenreAndInvalidPresetWithoutMutation();
  testLegacySoundRejectsNonTb303VoiceWithoutChangingFxOrEngine();
  std::puts("Legacy Acid/Techno sound profile behavior: OK");
  return 0;
}
