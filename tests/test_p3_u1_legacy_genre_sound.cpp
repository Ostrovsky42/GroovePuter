#include <cassert>
#include <cmath>
#include <cstdio>

#include "src/dsp/miniacid_engine.h"

SerialMock Serial;
SDMock SD;

namespace {

bool near(float lhs, float rhs, float epsilon = 0.015f) {
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

void assertLegacyTimbreNormalized(MiniAcid& engine, int voice,
                                  float osc, float cutoff, float resonance,
                                  float envAmount, float envDecay) {
  assert(near(engine.parameter303(TB303ParamId::Oscillator, voice).normalized(), osc));
  assert(near(engine.parameter303(TB303ParamId::Cutoff, voice).normalized(), cutoff));
  assert(near(engine.parameter303(TB303ParamId::Resonance, voice).normalized(), resonance));
  assert(near(engine.parameter303(TB303ParamId::EnvAmount, voice).normalized(), envAmount));
  assert(near(engine.parameter303(TB303ParamId::EnvDecay, voice).normalized(), envDecay));
}

void testLegacyAcidReplaysHistoricalPerVoiceTimbreWithoutGenerationMutation() {
  MiniAcid engine(44100.0f, nullptr);
  engine.modeManager().setModeLocal(GrooveboxMode::Minimal);
  engine.modeManager().setFlavorLocal(3);

  const GrooveboxMode modeBefore = engine.modeManager().mode();
  const int flavorBefore = engine.modeManager().flavor();

  assert(engine.modeManager().applyLegacyGenreTimbre(GenerativeMode::Acid, 0));
  assert(engine.modeManager().applyLegacyGenreTimbre(GenerativeMode::Acid, 1));

  assert(engine.modeManager().mode() == modeBefore);
  assert(engine.modeManager().flavor() == flavorBefore);

  // Historical applyGenreTimbre() clamped Synth A EnvAmount to 0.55 while
  // Synth B retained the Acid value 0.85.
  assertLegacyTimbreNormalized(engine, 0, 0.0f, 0.55f, 0.35f, 0.55f, 0.35f);
  assertLegacyTimbreNormalized(engine, 1, 0.0f, 0.55f, 0.35f, 0.85f, 0.35f);
}

void testLegacyTechnoReplaysHistoricalDetroitElectroTimbre() {
  MiniAcid engine(44100.0f, nullptr);
  engine.modeManager().setModeLocal(GrooveboxMode::Acid);
  engine.modeManager().setFlavorLocal(4);

  const GrooveboxMode modeBefore = engine.modeManager().mode();
  const int flavorBefore = engine.modeManager().flavor();

  assert(engine.modeManager().applyLegacyGenreTimbre(GenerativeMode::Techno, 0));
  assert(engine.modeManager().applyLegacyGenreTimbre(GenerativeMode::Techno, 1));

  assert(engine.modeManager().mode() == modeBefore);
  assert(engine.modeManager().flavor() == flavorBefore);

  // Techno did not exist when the old projector was removed. Legacy Techno is
  // deliberately the old Electro/Detroit behavior, with the same A/B clamps.
  assertLegacyTimbreNormalized(engine, 0, 0.20f, 0.60f, 0.30f, 0.55f, 0.20f);
  assertLegacyTimbreNormalized(engine, 1, 0.20f, 0.60f, 0.30f, 0.75f, 0.20f);
}

void testLegacyTimbreDoesNotOwnDelayOrDistortion() {
  MiniAcid engine(44100.0f, nullptr);
  engine.set303DistortionEnabled(0, true);
  engine.set303DelayEnabled(0, true);

  assert(engine.modeManager().applyLegacyGenreTimbre(GenerativeMode::Acid, 0));
  assert(engine.is303DistortionEnabled(0));
  assert(engine.is303DelayEnabled(0));
}

void testLegacyTimbreRejectsUnsupportedGenreWithoutMutation() {
  MiniAcid engine(44100.0f, nullptr);
  const float cutoffBefore =
      engine.parameter303(TB303ParamId::Cutoff, 0).normalized();
  const GrooveboxMode modeBefore = engine.modeManager().mode();
  const int flavorBefore = engine.modeManager().flavor();

  assert(!engine.modeManager().applyLegacyGenreTimbre(GenerativeMode::House, 0));

  assert(near(engine.parameter303(TB303ParamId::Cutoff, 0).normalized(), cutoffBefore));
  assert(engine.modeManager().mode() == modeBefore);
  assert(engine.modeManager().flavor() == flavorBefore);
}

void testLegacyTimbreRejectsNonTb303VoiceWithoutChangingFxOrEngine() {
  MiniAcid engine(44100.0f, nullptr);
  engine.set303DistortionEnabled(0, true);
  engine.set303DelayEnabled(0, true);
  engine.setSynthEngine(0, "SID");
  assert(engine.currentSynthEngineName(0) == "SID");

  const bool applied =
      engine.modeManager().applyLegacyGenreTimbre(GenerativeMode::Acid, 0);

  assert(!applied);
  assert(engine.currentSynthEngineName(0) == "SID");
  assert(engine.is303DistortionEnabled(0));
  assert(engine.is303DelayEnabled(0));
}

void testLegacyTimbreRejectsInvalidVoiceIndexInsteadOfClamping() {
  MiniAcid engine(44100.0f, nullptr);
  const float synthBCutoffBefore =
      engine.parameter303(TB303ParamId::Cutoff, 1).normalized();

  const bool applied =
      engine.modeManager().applyLegacyGenreTimbre(GenerativeMode::Acid, 99);

  assert(!applied);
  assert(near(engine.parameter303(TB303ParamId::Cutoff, 1).normalized(),
              synthBCutoffBefore));
}

void testLegacyTimbreLeavesDeterministicStructuralGenerationBitForBitIdentical() {
  for (const GenerativeMode legacyGenre :
       {GenerativeMode::Acid, GenerativeMode::Techno}) {
    MiniAcid engine(44100.0f, nullptr);
    engine.modeManager().setModeLocal(GrooveboxMode::Minimal);
    engine.modeManager().setFlavorLocal(2);

    SynthPattern currentSoundMaterial{};
    engine.modeManager().setGenerationSeed(0x5A17C0DEu);
    engine.modeManager().generatePattern(currentSoundMaterial, 128.0f);

    assert(engine.modeManager().applyLegacyGenreTimbre(legacyGenre, 0));
    assert(engine.modeManager().applyLegacyGenreTimbre(legacyGenre, 1));

    SynthPattern legacySoundMaterial{};
    engine.modeManager().setGenerationSeed(0x5A17C0DEu);
    engine.modeManager().generatePattern(legacySoundMaterial, 128.0f);

    assert(samePattern(currentSoundMaterial, legacySoundMaterial));
  }
}

}  // namespace

int main() {
  testLegacyAcidReplaysHistoricalPerVoiceTimbreWithoutGenerationMutation();
  testLegacyTechnoReplaysHistoricalDetroitElectroTimbre();
  testLegacyTimbreDoesNotOwnDelayOrDistortion();
  testLegacyTimbreRejectsUnsupportedGenreWithoutMutation();
  testLegacyTimbreRejectsNonTb303VoiceWithoutChangingFxOrEngine();
  testLegacyTimbreRejectsInvalidVoiceIndexInsteadOfClamping();
  testLegacyTimbreLeavesDeterministicStructuralGenerationBitForBitIdentical();
  std::puts("Legacy Acid/Techno historical timbre behavior: OK");
  return 0;
}
