// FS2A regression: the lower M4 staging primitive may replace the NEXT payload,
// but it must never inherit an upper lifecycle causal stamp from a previous
// prepareNextMelody() call.

#include <cassert>
#include <cstdint>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/state/material_slot_access.h"
#include "src/state/material_version.h"

SerialMock Serial;
SDMock SD;

namespace {

using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialReference;
using GroovePuterMaterial::versionForPattern;

PhraseRuntime::RuntimeSynthEventBuffer melodyWithNote(uint8_t note) {
  PhraseRuntime::RuntimeSynthEventBuffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 1;
  melody.events[0].startTick = 0;
  melody.events[0].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
  melody.events[0].note = note;
  melody.events[0].velocity = 100;
  melody.events[0].probability = 100;
  return melody;
}

bool samePattern(const SynthPattern& lhs, const SynthPattern& rhs) {
  return versionForPattern(lhs) == versionForPattern(rhs);
}

void seedCanonicalIdentity(MiniAcid& engine) {
  Scene& scene = engine.sceneManager_.currentScene();
  for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
    for (int resident = 0; resident < Scene::kMaterialSlotsPerVoice;
         ++resident) {
      scene.materialSlots[voice][resident].kind = MaterialKind::Pattern;
      scene.materialSlots[voice][resident].id = MaterialId{
          static_cast<uint32_t>(1 + voice * Scene::kMaterialSlotsPerVoice +
                                resident)};
    }
  }
}

}  // namespace

int main() {
  MiniAcid engine{44100.0f, nullptr};
  engine.setBpm(120.0f);
  assert(engine.rebuildPatternRuntimeEventBank());
  seedCanonicalIdentity(engine);

  MaterialReference reference{};
  assert(engine.current303MaterialReference_(0, reference));
  assert(reference.id.valid());

  const int bank = engine.current303BankIndex(0);
  const int pattern = engine.display303LocalPatternIndex(0);
  const SynthPattern accepted =
      engine.sceneManager_.currentScene().synthABanks[bank].patterns[pattern];
  const auto activeBefore = engine.activeMaterial(0);

  const auto lifecycleCandidate = melodyWithNote(60);
  assert(engine.prepareNextMelody(0, lifecycleCandidate) ==
         MiniAcid::NextPrepareResult::Prepared);
  assert(engine.pendingMaterial_[0].lifecycleBound);

  const auto rawReplacement = melodyWithNote(72);
  assert(engine.stagePendingMaterial(
      0, static_cast<uint16_t>(reference.address.globalSlot),
      MaterialKind::Melody, &rawReplacement));

  if (engine.pendingMaterial_[0].lifecycleBound) {
    std::fprintf(stderr,
                 "FS2A RAW-STAGE FAIL: raw M4 replacement inherited stale lifecycle stamp\n");
    return 1;
  }

  if (engine.activateNextMaterialAtBoundary(0) !=
      MiniAcid::NextActivationResult::UnboundPending) {
    std::fprintf(stderr,
                 "FS2A RAW-STAGE FAIL: unbound raw replacement crossed lifecycle boundary\n");
    return 1;
  }

  if (!engine.workingMaterial_[0].empty() ||
      engine.activeMaterial(0).kind != activeBefore.kind ||
      engine.activeMaterial(0).slot != activeBefore.slot ||
      !samePattern(engine.sceneManager_.currentScene()
                       .synthABanks[bank]
                       .patterns[pattern],
                   accepted)) {
    std::fprintf(stderr,
                 "FS2A RAW-STAGE FAIL: rejection mutated CURRENT/runtime/canonical\n");
    return 1;
  }

  std::printf("FS2A raw-stage causal unbind: PASS\n");
  return 0;
}
