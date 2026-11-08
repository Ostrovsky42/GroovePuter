#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/audio/pattern_paging.h"
#include "src/input/musical_event_queue.h"
#include "src/state/material_slot_access.h"
#include "src/state/material_version.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

SerialMock Serial;
SDMock SD;

namespace {

static_assert(sizeof(MiniAcid::PendingMaterial) == 32,
              "PendingMaterial must remain 32 bytes");
static_assert(sizeof(GroovePuterMaterial::DevelopmentLineage) == 40,
              "DevelopmentLineage must remain 40 bytes");

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "M1 FAIL: %s\n", message);
    ++g_failures;
  }
}

float readEnginePhase(void* context) {
  return static_cast<MiniAcid*>(context)->transportPhaseSteps();
}

PhraseRuntime::RuntimeSynthEventBuffer melodyWithNote(uint8_t note, uint16_t lengthTicks = PhraseRuntime::kTicksPerBar) {
  PhraseRuntime::RuntimeSynthEventBuffer melody{};
  melody.lengthTicks = lengthTicks;
  melody.count = 1;
  melody.events[0].startTick = 0;
  melody.events[0].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
  melody.events[0].note = note;
  melody.events[0].velocity = 100;
  melody.events[0].probability = 100;
  return melody;
}

bool sameMelody(const PhraseRuntime::RuntimeSynthEventBuffer& lhs,
                const PhraseRuntime::RuntimeSynthEventBuffer& rhs) {
  if (lhs.count != rhs.count || lhs.lengthTicks != rhs.lengthTicks) return false;
  const uint16_t count = lhs.count <= PhraseRuntime::kMaxSynthEvents
                             ? lhs.count
                             : PhraseRuntime::kMaxSynthEvents;
  for (uint16_t i = 0; i < count; ++i) {
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

struct Fixture {
  MiniAcid engine{44100.0f, nullptr};
  MusicalEventQueue queue{};

  static GroovePuterMaterial::MaterialId fixtureMaterialId(int voice, int resident) {
    return GroovePuterMaterial::MaterialId{static_cast<uint32_t>(
        1 + voice * Scene::kMaterialSlotsPerVoice + resident)};
  }

  Fixture() {
    engine.setBpm(120.0f);
    assert(engine.rebuildPatternRuntimeEventBank());
    engine.setPatternEventQueue(&queue);
    queue.setPhaseReader(readEnginePhase, &engine);
    engine.playing = true;
    engine.tickPhaseAccum_ = 0;

    Scene& scene = engine.sceneManager_.currentScene();
    for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
      for (int resident = 0; resident < Scene::kMaterialSlotsPerVoice;
           ++resident) {
        scene.materialSlots[voice][resident].kind =
            GroovePuterMaterial::MaterialKind::Pattern;
        scene.materialSlots[voice][resident].id =
            fixtureMaterialId(voice, resident);
      }
    }
  }

  MiniAcid::PreparationBasis basis(int voice) const {
    return engine.captureCurrentPreparationBasis(voice);
  }

  MiniAcid::PreparationBasis sourceAnchor(int voice) const {
    return engine.sourceAnchor(voice);
  }

  MiniAcid::PreparationBasis predecessor(int voice) const {
    return engine.predecessor(voice);
  }
};

}  // namespace

int main() {
  using GroovePuterMaterial::IdeaClassification;

  // 1. Initial clean state establishes sourceAnchor = A0, predecessor = A0.
  //    Activation of VARIATION A1 transitions predecessor to A0, but keeps sourceAnchor = A0.
  {
    Fixture fixture;
    const auto a0Basis = fixture.basis(0);
    expect(a0Basis.valid(), "1: a0 basis must be valid");
    expect(fixture.sourceAnchor(0) == a0Basis, "1: initial sourceAnchor must be A0");
    expect(fixture.predecessor(0) == a0Basis, "1: initial predecessor must be A0");

    const auto candidateA1 = melodyWithNote(60);
    expect(fixture.engine.prepareNextMelody(0, candidateA1, a0Basis, IdeaClassification::Variation) ==
               MiniAcid::NextPrepareResult::Prepared,
           "1: prepare A1 failed");

    // While A1 is NEXT: sourceAnchor must remain A0
    expect(fixture.sourceAnchor(0) == a0Basis, "1: sourceAnchor must remain A0 while A1 is NEXT");

    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "1: activate A1 failed");

    const auto a1Basis = fixture.basis(0);
    expect(a1Basis.valid(), "1: a1 basis must be valid");
    expect(fixture.sourceAnchor(0) == a0Basis, "1: sourceAnchor must remain A0 after Variation A1 activated");
    expect(fixture.predecessor(0) == a0Basis, "1: predecessor must be A0 after A1 activated");
  }

  // 2. Chained Variation A1 -> A2: predecessor becomes A1, sourceAnchor still A0.
  {
    Fixture fixture;
    const auto a0Basis = fixture.basis(0);
    const auto candA1 = melodyWithNote(60);
    fixture.engine.prepareNextMelody(0, candA1, a0Basis, IdeaClassification::Variation);
    fixture.engine.activateNextMaterialAtBoundary(0);
    const auto a1Basis = fixture.basis(0);

    const auto candA2 = melodyWithNote(62);
    expect(fixture.engine.prepareNextMelody(0, candA2, a1Basis, IdeaClassification::Variation) ==
               MiniAcid::NextPrepareResult::Prepared,
           "2: prepare A2 failed");

    // While A2 is NEXT: sourceAnchor remains A0
    expect(fixture.sourceAnchor(0) == a0Basis, "2: sourceAnchor must remain A0 while A2 is NEXT");

    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "2: activate A2 failed");

    const auto a2Basis = fixture.basis(0);
    expect(fixture.sourceAnchor(0) == a0Basis, "2: sourceAnchor must remain A0 after Variation A2 activated");
    expect(fixture.predecessor(0) == a1Basis, "2: predecessor must be A1 after A2 activated");
  }

  // 3. NEW IDEA B prepared from A2:
  //    - While PRIVATE / NEXT: sourceAnchor remains A0.
  //    - After GO / activation: CURRENT becomes B, predecessor remains A2,
  //      and active sourceAnchor BECOMES B (without ACCEPT!).
  {
    Fixture fixture;
    const auto a0Basis = fixture.basis(0);
    fixture.engine.prepareNextMelody(0, melodyWithNote(60), a0Basis, IdeaClassification::Variation);
    fixture.engine.activateNextMaterialAtBoundary(0);
    const auto a1Basis = fixture.basis(0);

    fixture.engine.prepareNextMelody(0, melodyWithNote(62), a1Basis, IdeaClassification::Variation);
    fixture.engine.activateNextMaterialAtBoundary(0);
    const auto a2Basis = fixture.basis(0);

    // Prepare B with NEW_IDEA classification
    const auto candB = melodyWithNote(72);
    expect(fixture.engine.prepareNextMelody(0, candB, a2Basis, IdeaClassification::NewIdea) ==
               MiniAcid::NextPrepareResult::Prepared,
           "3: prepare NEW_IDEA B failed");

    // Invariant: While NEXT, active sourceAnchor must remain A0!
    expect(fixture.sourceAnchor(0) == a0Basis,
           "3: active sourceAnchor must remain A0 while B is NEXT");

    // Activate B at boundary (GO)
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "3: activate B failed");

    const auto bBasis = fixture.basis(0);
    expect(bBasis.valid() && bBasis != a2Basis, "3: B basis must be fresh valid basis");
    expect(fixture.predecessor(0) == a2Basis, "3: historical predecessor must remain A2");
    expect(fixture.sourceAnchor(0) == bBasis, "3: runtime sourceAnchor must transition to B upon activation of NEW_IDEA");

    // Verify subsequent derivation from B inherits B as sourceAnchor
    const auto candC = melodyWithNote(74);
    expect(fixture.engine.prepareNextMelody(0, candC, bBasis, IdeaClassification::Variation) ==
               MiniAcid::NextPrepareResult::Prepared,
           "3: prepare C from B failed");
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "3: activate C from B failed");
    expect(fixture.sourceAnchor(0) == bBasis, "3: C variation must retain sourceAnchor B");
    expect(fixture.predecessor(0) == bBasis, "3: C predecessor must be B");
  }

  // 4. GO writes single-slot Undo receipt; Undo restores prior state.
  {
    Fixture fixture;
    const auto a0Basis = fixture.basis(0);
    const auto candA1 = melodyWithNote(60);

    auto& undoOwner = GroovePuterUndo::undoOwner();
    undoOwner.clear();
    expect(!undoOwner.hasUndo(), "4: undo should be clean initially");

    fixture.engine.prepareNextMelody(0, candA1, a0Basis, IdeaClassification::Variation);
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "4: activate A1 failed");

    // GO A1 must write an activation Undo receipt
    expect(undoOwner.hasUndo(), "4: GO A1 must write Undo receipt");
    expect(undoOwner.kind() == GroovePuterUndo::UndoKind::RuntimePhrase,
           "4: receipt kind must be RuntimePhrase");

    // GO A2 replaces receipt in the single slot
    const auto a1Basis = fixture.basis(0);
    const auto candA2 = melodyWithNote(64);
    fixture.engine.prepareNextMelody(0, candA2, a1Basis, IdeaClassification::Variation);
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "4: activate A2 failed");

    expect(undoOwner.hasUndo(), "4: GO A2 must have Undo receipt");

    // Calling undoMaterialWorking(0) restores A1
    expect(fixture.engine.undoMaterialWorking(0), "4: undoMaterialWorking must succeed");
    expect(sameMelody(fixture.engine.currentPhraseBuffer(0), candA1),
           "4: undo must restore A1 melody");
    expect(!undoOwner.hasUndo(), "4: single undo slot must be cleared after undo");
    expect(!fixture.engine.undoMaterialWorking(0), "4: second undo must return false");
  }

  // 5. Replace NEXT: staging candidate B2 replaces B1 as NEXT.
  {
    Fixture fixture;
    const auto basis = fixture.basis(0);
    const auto b1 = melodyWithNote(65);
    const auto b2 = melodyWithNote(67);
    expect(fixture.engine.prepareNextMelody(0, b1, basis) ==
               MiniAcid::NextPrepareResult::Prepared,
           "5: prepare b1 failed");
    expect(fixture.engine.prepareNextMelody(0, b2, basis) ==
               MiniAcid::NextPrepareResult::Replaced,
           "5: prepare b2 did not return Replaced");
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "5: activate b2 failed");
    expect(sameMelody(fixture.engine.currentPhraseBuffer(0), b2),
           "5: activated melody must be b2");
  }

  // 6. CANCEL NEXT: candidate cleared, CURRENT and Undo untouched.
  {
    Fixture fixture;
    const auto basis = fixture.basis(0);
    const auto cand = melodyWithNote(69);
    fixture.engine.prepareNextMelody(0, cand, basis);
    expect(fixture.engine.hasPendingMaterial(0), "6: candidate not queued");

    auto& undoOwner = GroovePuterUndo::undoOwner();
    undoOwner.clear();

    expect(fixture.engine.cancelNextMaterial(0), "6: cancel failed");
    expect(!fixture.engine.hasPendingMaterial(0), "6: candidate still queued");
    expect(!undoOwner.hasUndo(), "6: cancel must not create undo");
  }

  // 7. DISCARD restores accepted canonical Pattern and resets sourceAnchor to canonical basis.
  {
    Fixture fixture;
    const auto a0Basis = fixture.basis(0);
    const auto cand = melodyWithNote(71);
    fixture.engine.prepareNextMelody(0, cand, a0Basis, IdeaClassification::NewIdea);
    fixture.engine.activateNextMaterialAtBoundary(0);

    // CURRENT is now Melody, sourceAnchor transitioned to Melody
    expect(fixture.engine.activeMaterial(0).kind == GroovePuterMaterial::MaterialKind::Melody,
           "7: active material must be Melody");

    // DISCARD restores accepted canonical Pattern
    expect(fixture.engine.discardCurrentMaterial(0) == MiniAcid::DiscardResult::Discarded,
           "7: discard failed");
    expect(fixture.engine.activeMaterial(0).kind == GroovePuterMaterial::MaterialKind::Pattern,
           "7: active material must be restored to Pattern");
    expect(fixture.sourceAnchor(0) == a0Basis,
           "7: sourceAnchor must be reset to accepted canonical basis after DISCARD");
  }

  if (g_failures == 0) {
    std::printf("M1 development lineage and lifecycle: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "M1 development lineage and lifecycle: %d failure(s)\n", g_failures);
  return 1;
}
