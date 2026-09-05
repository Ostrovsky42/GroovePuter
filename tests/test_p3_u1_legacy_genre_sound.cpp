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

bool samePattern(const SynthPattern& lhs, const SynthPattern& rhs) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& a = lhs.steps[step];
    const SynthStep& b = rhs.steps[step];
    if (a.note != b.note || a.slide != b.slide || a.accent != b.accent ||
        a.ghost != b.ghost || a.velocity != b.velocity ||
        a.timing != b.timing || a.fx != b.fx || a.fxParam != b.fxParam ||
        a.probability != b.probability) {
      return false;
    }
  }
  return true;
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

void testLegacySoundRejectsInvalidVoiceIndexInsteadOfClampingToAnotherVoice() {
  MiniAcid engine(44100.0f, nullptr);
  assert(!engine.is303DistortionEnabled(0));
  assert(!engine.is303DistortionEnabled(1));

  // MiniAcid's low-level voice helpers clamp indices for convenience. This
  // higher-level sound operation must fail closed instead of applying a preset
  // to a different synth than the caller requested.
  const bool applied = engine.modeManager().applyLegacyGenreSoundPreset(
      GenerativeMode::Acid, 1, 99);

  assert(!applied);
  assert(!engine.is303DistortionEnabled(0));
  assert(!engine.is303DistortionEnabled(1));
}

void testLegacySoundLeavesDeterministicStructuralGenerationBitForBitIdentical() {
  for (const GenerativeMode legacyGenre :
       {GenerativeMode::Acid, GenerativeMode::Techno}) {
    MiniAcid engine(44100.0f, nullptr);
    engine.modeManager().setModeLocal(GrooveboxMode::Minimal);
    engine.modeManager().setFlavorLocal(2);

    SynthPattern currentSoundMaterial{};
    engine.modeManager().setGenerationSeed(0x5A17C0DEu);
    engine.modeManager().generatePattern(currentSoundMaterial, 128.0f);

    const bool applied = engine.modeManager().applyLegacyGenreSoundPreset(
        legacyGenre, 0, 0);
    assert(applied);

    SynthPattern legacySoundMaterial{};
    engine.modeManager().setGenerationSeed(0x5A17C0DEu);
    engine.modeManager().generatePattern(legacySoundMaterial, 128.0f);

    assert(samePattern(currentSoundMaterial, legacySoundMaterial));
  }
}

}  // namespace

int main() {
  testLegacyAcidUsesOldAcidPresetWithoutChangingGenerationMode();
  testLegacyTechnoUsesOldElectroDetroitPresetWithoutChangingGenerationMode();
  testLegacySoundRejectsUnsupportedGenreAndInvalidPresetWithoutMutation();
  testLegacySoundRejectsNonTb303VoiceWithoutChangingFxOrEngine();
  testLegacySoundRejectsInvalidVoiceIndexInsteadOfClampingToAnotherVoice();
  testLegacySoundLeavesDeterministicStructuralGenerationBitForBitIdentical();
  std::puts("Legacy Acid/Techno sound profile behavior: OK");
  return 0;
}
