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

void assertLegacyTimbre(MiniAcid& engine, int voice, int oscillatorIndex,
                        float cutoff, float resonance,
                        float envAmount, float envDecay) {
  assert(engine.parameter303(TB303ParamId::Oscillator, voice).optionIndex() ==
         oscillatorIndex);
  assert(near(engine.parameter303(TB303ParamId::Cutoff, voice).normalized(), cutoff));
  assert(near(engine.parameter303(TB303ParamId::Resonance, voice).normalized(), resonance));
  assert(near(engine.parameter303(TB303ParamId::EnvAmount, voice).normalized(), envAmount));
  assert(near(engine.parameter303(TB303ParamId::EnvDecay, voice).normalized(), envDecay));
}

void testLegacyAcidReplaysHistoricalPerVoiceTimbreWithoutGenerationMutation() {
  MiniAcid engine(44100.0f, nullptr);
  engine.modeManager().setModeLocal(GrooveboxMode::Minimal);
  engine.modeManager().setFlavorLocal(3);
  engine.set303Parameter(TB303ParamId::Oscillator, 3.0f, 0);
  engine.set303Parameter(TB303ParamId::Oscillator, 4.0f, 1);

  const GrooveboxMode modeBefore = engine.modeManager().mode();
  const int flavorBefore = engine.modeManager().flavor();

  assert(engine.modeManager().applyLegacyGenreTimbre(GenerativeMode::Acid, 0));
  assert(engine.modeManager().applyLegacyGenreTimbre(GenerativeMode::Acid, 1));

  assert(engine.modeManager().mode() == modeBefore);
  assert(engine.modeManager().flavor() == flavorBefore);

  // The historical Genre projector attempted to write Oscillator through the
  // generic uint8_t TB303 parameter interface. That interface handled only
  // indices 0..3, so Oscillator(index 4) was a production no-op. Replaying the
  // old audible behavior therefore preserves the currently selected oscillator.
  // Synth A also clamps EnvAmount to 0.55 while Synth B retains Acid 0.85.
  assertLegacyTimbre(engine, 0, 3, 0.55f, 0.35f, 0.55f, 0.35f);
  assertLegacyTimbre(engine, 1, 4, 0.55f, 0.35f, 0.85f, 0.35f);
}

void testLegacyTechnoReplaysHistoricalDetroitElectroTimbre() {
  MiniAcid engine(44100.0f, nullptr);
  engine.modeManager().setModeLocal(GrooveboxMode::Acid);
  engine.modeManager().setFlavorLocal(4);
  engine.set303Parameter(TB303ParamId::Oscillator, 2.0f, 0);
  engine.set303Parameter(TB303ParamId::Oscillator, 1.0f, 1);

  const GrooveboxMode modeBefore = engine.modeManager().mode();
  const int flavorBefore = engine.modeManager().flavor();

  assert(engine.modeManager().applyLegacyGenreTimbre(GenerativeMode::Techno, 0));
  assert(engine.modeManager().applyLegacyGenreTimbre(GenerativeMode::Techno, 1));

  assert(engine.modeManager().mode() == modeBefore);
  assert(engine.modeManager().flavor() == flavorBefore);

  // Techno did not exist when the old projector was removed. Legacy Techno is
  // deliberately the old Electro/Detroit timbre, including the historical
  // Oscillator write being ineffective through the generic parameter seam.
  assertLegacyTimbre(engine, 0, 2, 0.60f, 0.30f, 0.55f, 0.20f);
  assertLegacyTimbre(engine, 1, 1, 0.60f, 0.30f, 0.75f, 0.20f);
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

void testLegacySoundToggleRoundTripsCurrentPatchExactly() {
  MiniAcid engine(44100.0f, nullptr);

  engine.set303ParameterNormalized(TB303ParamId::Cutoff, 0.24f, 0);
  engine.set303ParameterNormalized(TB303ParamId::Resonance, 0.73f, 0);
  engine.set303ParameterNormalized(TB303ParamId::EnvAmount, 0.41f, 0);
  engine.set303ParameterNormalized(TB303ParamId::EnvDecay, 0.66f, 0);
  engine.set303ParameterNormalized(TB303ParamId::Cutoff, 0.82f, 1);
  engine.set303ParameterNormalized(TB303ParamId::Resonance, 0.18f, 1);
  engine.set303ParameterNormalized(TB303ParamId::EnvAmount, 0.29f, 1);
  engine.set303ParameterNormalized(TB303ParamId::EnvDecay, 0.57f, 1);

  const float before[2][4] = {
      {engine.parameter303(TB303ParamId::Cutoff, 0).normalized(),
       engine.parameter303(TB303ParamId::Resonance, 0).normalized(),
       engine.parameter303(TB303ParamId::EnvAmount, 0).normalized(),
       engine.parameter303(TB303ParamId::EnvDecay, 0).normalized()},
      {engine.parameter303(TB303ParamId::Cutoff, 1).normalized(),
       engine.parameter303(TB303ParamId::Resonance, 1).normalized(),
       engine.parameter303(TB303ParamId::EnvAmount, 1).normalized(),
       engine.parameter303(TB303ParamId::EnvDecay, 1).normalized()},
  };

  assert(engine.modeManager().setLegacyGenreSoundEnabled(
      true, GenerativeMode::Acid));
  assert(engine.modeManager().legacyGenreSoundEnabled());
  assertLegacyTimbre(engine, 0, 0, 0.55f, 0.35f, 0.55f, 0.35f);
  assertLegacyTimbre(engine, 1, 0, 0.55f, 0.35f, 0.85f, 0.35f);

  assert(engine.modeManager().toggleLegacyGenreSound(GenerativeMode::Acid));
  assert(!engine.modeManager().legacyGenreSoundEnabled());

  const TB303ParamId ids[4] = {
      TB303ParamId::Cutoff,
      TB303ParamId::Resonance,
      TB303ParamId::EnvAmount,
      TB303ParamId::EnvDecay,
  };
  for (int voice = 0; voice < 2; ++voice) {
    for (int param = 0; param < 4; ++param) {
      assert(near(engine.parameter303(ids[param], voice).normalized(),
                  before[voice][param], 0.0001f));
    }
  }
}

void testLegacySoundToggleRejectsUnsupportedGenreWithoutEnabling() {
  MiniAcid engine(44100.0f, nullptr);
  const float cutoffBefore =
      engine.parameter303(TB303ParamId::Cutoff, 0).normalized();

  assert(!engine.modeManager().toggleLegacyGenreSound(GenerativeMode::House));
  assert(!engine.modeManager().legacyGenreSoundEnabled());
  assert(near(engine.parameter303(TB303ParamId::Cutoff, 0).normalized(),
              cutoffBefore));
}

void testLegacySoundToggleRequiresAtLeastOneTb303Voice() {
  MiniAcid engine(44100.0f, nullptr);
  engine.setSynthEngine(0, "SID");
  engine.setSynthEngine(1, "AY");

  assert(!engine.modeManager().toggleLegacyGenreSound(GenerativeMode::Techno));
  assert(!engine.modeManager().legacyGenreSoundEnabled());
  assert(engine.currentSynthEngineName(0) == "SID");
  assert(engine.currentSynthEngineName(1) == "AY");
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
  testLegacySoundToggleRoundTripsCurrentPatchExactly();
  testLegacySoundToggleRejectsUnsupportedGenreWithoutEnabling();
  testLegacySoundToggleRequiresAtLeastOneTb303Voice();
  testLegacyTimbreLeavesDeterministicStructuralGenerationBitForBitIdentical();
  std::puts("Legacy Acid/Techno historical timbre + sound toggle behavior: OK");
  return 0;
}
