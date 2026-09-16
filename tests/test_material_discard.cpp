// 0.9.12 Material DISCARD: restore CURRENT from exact accepted Pattern truth.
//
// First slice is intentionally RAM-only. Accepted Pattern is resolvable without
// I/O; accepted Melody is fail-closed until durable Melody resolution exists.

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
using GroovePuterMaterial::MaterialVersionToken;
using GroovePuterMaterial::versionForPattern;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "DISCARD FAIL: %s\n", message);
  ++g_failures;
}

bool samePattern(const SynthPattern& lhs, const SynthPattern& rhs) {
  return versionForPattern(lhs) == versionForPattern(rhs);
}

bool sameReference(const MaterialReference& lhs, const MaterialReference& rhs) {
  return lhs.address == rhs.address && lhs.id == rhs.id && lhs.id.valid();
}

bool sameMelody(const PhraseRuntime::RuntimeSynthEventBuffer& lhs,
                const PhraseRuntime::RuntimeSynthEventBuffer& rhs) {
  if (lhs.count != rhs.count || lhs.lengthTicks != rhs.lengthTicks) return false;
  for (uint16_t i = 0; i < lhs.count; ++i) {
    const auto& a = lhs.events[i];
    const auto& b = rhs.events[i];
    if (a.startTick != b.startTick ||
        a.durationSubticks != b.durationSubticks || a.note != b.note ||
        a.velocity != b.velocity || a.probability != b.probability ||
        a.flags != b.flags || a.fx != b.fx || a.fxParam != b.fxParam) {
      return false;
    }
  }
  return true;
}

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

struct Fixture {
  MiniAcid engine{44100.0f, nullptr};

  static MaterialId fixtureMaterialId(int voice, int resident) {
    return MaterialId{static_cast<uint32_t>(
        1001 + voice * Scene::kMaterialSlotsPerVoice + resident)};
  }

  Fixture() {
    engine.setBpm(120.0f);
    Scene& scene = engine.sceneManager_.currentScene();
    for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
      for (int resident = 0; resident < Scene::kMaterialSlotsPerVoice;
           ++resident) {
        scene.materialSlots[voice][resident].kind = MaterialKind::Pattern;
        scene.materialSlots[voice][resident].id =
            fixtureMaterialId(voice, resident);
      }
    }

    canonical(0).steps[0].note = 60;
    canonical(1).steps[0].note = 48;
    assert(engine.rebuildPatternRuntimeEventBank());
    assertCanonicalReality(0);
    assertCanonicalReality(1);
  }

  MaterialReference reference(int voice) const {
    MaterialReference out{};
    assert(engine.current303MaterialReference_(voice, out));
    return out;
  }

  SynthPattern& canonical(int voice) {
    const int bank = engine.current303BankIndex(voice);
    const int pattern = engine.display303LocalPatternIndex(voice);
    Scene& scene = engine.sceneManager_.currentScene();
    return voice == 0 ? scene.synthABanks[bank].patterns[pattern]
                      : scene.synthBBanks[bank].patterns[pattern];
  }

  const SynthPattern& canonical(int voice) const {
    const int bank = engine.current303BankIndex(voice);
    const int pattern = engine.display303LocalPatternIndex(voice);
    const Scene& scene = engine.sceneManager_.currentScene();
    return voice == 0 ? scene.synthABanks[bank].patterns[pattern]
                      : scene.synthBBanks[bank].patterns[pattern];
  }

  const PhraseRuntime::RuntimeSynthEvent* runtimeStep0(int voice) const {
    const int bank = engine.current303BankIndex(voice);
    const int pattern = engine.display303LocalPatternIndex(voice);
    return engine.patternRuntimeBank_
        .select(static_cast<uint8_t>(voice), static_cast<uint8_t>(bank),
                static_cast<uint8_t>(pattern))
        .eventForSourceStep(0);
  }

  void assertCanonicalReality(int voice) const {
    const MaterialReference ref = reference(voice);
    const int resident = GroovePuterMaterial::residentSlotFor(ref.address);
    assert(GroovePuterMaterial::residentSlotInRange(voice, resident));
    const auto& descriptor =
        engine.sceneManager_.currentScene().materialSlots[voice][resident];
    assert(descriptor.kind == MaterialKind::Pattern);
    assert(descriptor.id.valid());
    assert(descriptor.id == ref.id);
    assert(versionForPattern(canonical(voice)).valid());
  }

  int currentResident(int voice) const {
    return GroovePuterMaterial::residentSlotFor(reference(voice).address);
  }
};

}  // namespace

int main() {
  // 1. Dirty Pattern Working is discarded back to exact accepted Pattern.
  {
    Fixture fixture;
    const auto acceptedA = fixture.canonical(0);
    const auto acceptedB = fixture.canonical(1);
    const auto refA = fixture.reference(0);
    const auto versionA = versionForPattern(acceptedA);
    const auto activeB = fixture.engine.activeMaterial(1);

    expect(fixture.engine.adjustWorking303StepNote(0, 0, 1),
           "fixture must create dirty Pattern Working");
    expect(fixture.engine.workingMaterial_[0].holdsPattern(),
           "dirty Pattern must be owned by Working");
    const auto* dirtyRuntime = fixture.runtimeStep0(0);
    expect(dirtyRuntime != nullptr && dirtyRuntime->note == 61,
           "dirty Pattern must be audible before DISCARD");

    expect(fixture.engine.discardCurrentMaterial(0) ==
               MiniAcid::DiscardResult::Discarded,
           "dirty Pattern DISCARD must succeed");
    expect(fixture.engine.workingMaterial_[0].empty(),
           "DISCARD must clear session Working owner");
    expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Pattern,
           "DISCARD must publish accepted Pattern as CURRENT source");
    expect(samePattern(fixture.canonical(0), acceptedA),
           "DISCARD must not mutate accepted Pattern bytes");
    expect(sameReference(fixture.reference(0), refA),
           "DISCARD must preserve MaterialId/address");
    expect(versionForPattern(fixture.canonical(0)) == versionA,
           "DISCARD must preserve accepted version token");
    const auto* restoredRuntime = fixture.runtimeStep0(0);
    expect(restoredRuntime != nullptr && restoredRuntime->note == 60,
           "DISCARD must restore accepted Pattern into audible runtime");
    expect(samePattern(fixture.canonical(1), acceptedB) &&
               fixture.engine.workingMaterial_[1].empty() &&
               fixture.engine.activeMaterial(1).kind == activeB.kind &&
               fixture.engine.activeMaterial(1).slot == activeB.slot,
           "DISCARD A must not mutate voice B");
  }

  // 2. Melody CURRENT created by FS2A activation discards back to accepted
  //    Pattern without changing canonical identity/version.
  {
    Fixture fixture;
    const auto accepted = fixture.canonical(0);
    const auto ref = fixture.reference(0);
    const auto version = versionForPattern(accepted);
    const auto melody = melodyWithNote(72);
    const auto basis = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, melody, basis) ==
               MiniAcid::NextPrepareResult::Prepared,
           "fixture NEXT prepare must succeed");
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "fixture NEXT activation must create Melody CURRENT");
    expect(fixture.engine.workingMaterial_[0].holdsMelody() &&
               fixture.engine.activeMaterial(0).kind == MaterialKind::Melody,
           "Melody must be CURRENT before DISCARD");

    expect(fixture.engine.discardCurrentMaterial(0) ==
               MiniAcid::DiscardResult::Discarded,
           "Melody CURRENT DISCARD must restore accepted Pattern");
    expect(fixture.engine.workingMaterial_[0].empty(),
           "Melody Working must be cleared by DISCARD");
    expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Pattern,
           "accepted Pattern must become active after Melody DISCARD");
    expect(sameReference(fixture.reference(0), ref) &&
               samePattern(fixture.canonical(0), accepted) &&
               versionForPattern(fixture.canonical(0)) == version,
           "Melody DISCARD must preserve accepted truth");
    const auto* restoredRuntime = fixture.runtimeStep0(0);
    expect(restoredRuntime != nullptr && restoredRuntime->note == 60,
           "Melody DISCARD must leave accepted Pattern runtime ready");
  }

  // 3. Clean accepted Pattern is explicitly idempotent.
  {
    Fixture fixture;
    const auto accepted = fixture.canonical(0);
    const auto ref = fixture.reference(0);
    const auto active = fixture.engine.activeMaterial(0);

    expect(fixture.engine.discardCurrentMaterial(0) ==
               MiniAcid::DiscardResult::AlreadyClean,
           "clean Pattern DISCARD must report AlreadyClean");
    expect(fixture.engine.workingMaterial_[0].empty() &&
               samePattern(fixture.canonical(0), accepted) &&
               sameReference(fixture.reference(0), ref) &&
               fixture.engine.activeMaterial(0).kind == active.kind &&
               fixture.engine.activeMaterial(0).slot == active.slot,
           "AlreadyClean must be a no-op");
  }

  // 4. Invalid canonical identity fails closed and preserves dirty CURRENT.
  {
    Fixture fixture;
    expect(fixture.engine.adjustWorking303StepNote(0, 0, 1),
           "fixture must create dirty Pattern before invalid-ID case");
    const SynthPattern dirty = fixture.engine.workingMaterial_[0].pattern();
    const auto accepted = fixture.canonical(0);
    const auto* dirtyRuntimeBefore = fixture.runtimeStep0(0);
    expect(dirtyRuntimeBefore != nullptr && dirtyRuntimeBefore->note == 61,
           "invalid-ID fixture must begin with dirty audible runtime");

    const int resident = fixture.currentResident(0);
    fixture.engine.sceneManager_.currentScene().materialSlots[0][resident].id =
        MaterialId{};

    expect(fixture.engine.discardCurrentMaterial(0) ==
               MiniAcid::DiscardResult::UnsupportedCurrentState,
           "invalid canonical ID must reject DISCARD");
    expect(fixture.engine.workingMaterial_[0].holdsPattern() &&
               samePattern(fixture.engine.workingMaterial_[0].pattern(), dirty),
           "invalid-ID rejection must preserve Working");
    const auto* dirtyRuntimeAfter = fixture.runtimeStep0(0);
    expect(dirtyRuntimeAfter != nullptr && dirtyRuntimeAfter->note == 61,
           "invalid-ID rejection must preserve audible CURRENT");
    expect(samePattern(fixture.canonical(0), accepted),
           "invalid-ID rejection must not mutate canonical bytes");
  }

  // 5. Accepted Melody is deliberately unsupported in the RAM-only first
  //    slice; rejection preserves CURRENT instead of performing filesystem I/O.
  {
    Fixture fixture;
    expect(fixture.engine.adjustWorking303StepNote(0, 0, 1),
           "fixture must create dirty CURRENT before unsupported-kind case");
    const SynthPattern dirty = fixture.engine.workingMaterial_[0].pattern();
    const auto accepted = fixture.canonical(0);
    const int resident = fixture.currentResident(0);
    fixture.engine.sceneManager_.currentScene().materialSlots[0][resident].kind =
        MaterialKind::Melody;

    expect(fixture.engine.discardCurrentMaterial(0) ==
               MiniAcid::DiscardResult::UnsupportedCurrentState,
           "accepted Melody must fail closed in DISCARD first slice");
    expect(fixture.engine.workingMaterial_[0].holdsPattern() &&
               samePattern(fixture.engine.workingMaterial_[0].pattern(), dirty),
           "unsupported accepted kind must preserve Working");
    const auto* runtime = fixture.runtimeStep0(0);
    expect(runtime != nullptr && runtime->note == 61,
           "unsupported accepted kind must preserve audible CURRENT");
    expect(samePattern(fixture.canonical(0), accepted),
           "unsupported accepted kind must not mutate canonical bytes");
  }

  // 6. DISCARD restores the exact causal basis of an already-prepared NEXT.
  //    It must not silently cancel or rewrite that independent candidate.
  {
    Fixture fixture;
    const auto next = melodyWithNote(67);
    const auto basis = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, next, basis) ==
               MiniAcid::NextPrepareResult::Prepared,
           "pending-preservation fixture NEXT prepare must succeed");
    const auto pendingRef = fixture.engine.pendingMaterial_[0].preparedFor;
    const auto pendingVersion = fixture.engine.pendingMaterial_[0].acceptedVersion;
    const auto pendingPayload = *fixture.engine.pendingMaterial_[0].melody;

    expect(fixture.engine.adjustWorking303StepNote(0, 0, 1),
           "pending-preservation fixture must dirty CURRENT");
    expect(fixture.engine.discardCurrentMaterial(0) ==
               MiniAcid::DiscardResult::Discarded,
           "DISCARD must restore CURRENT while NEXT exists");
    expect(fixture.engine.pendingMaterial_[0].queued &&
               fixture.engine.pendingMaterial_[0].lifecycleBound &&
               sameReference(fixture.engine.pendingMaterial_[0].preparedFor,
                             pendingRef) &&
               fixture.engine.pendingMaterial_[0].acceptedVersion ==
                   pendingVersion &&
               sameMelody(*fixture.engine.pendingMaterial_[0].melody,
                          pendingPayload),
           "DISCARD must preserve independent NEXT candidate exactly");
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "preserved NEXT must activate after DISCARD restores causal basis");
  }

  // 7. Invalid voice is rejected without clamping into a real voice.
  {
    Fixture fixture;
    expect(fixture.engine.discardCurrentMaterial(-1) ==
               MiniAcid::DiscardResult::InvalidVoice &&
               fixture.engine.discardCurrentMaterial(NUM_303_VOICES) ==
                   MiniAcid::DiscardResult::InvalidVoice,
           "invalid voice must fail without aliasing voice A/B");
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "DISCARD contract: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("Material DISCARD contract: PASS\n");
  return 0;
}
