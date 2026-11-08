// M0: Material Development Admission
// Chained development, runtime CURRENT admission, PreparationBasis, and failure atomicity.

#include <cassert>
#include <cstdint>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/audio/pattern_paging.h"
#include "src/input/musical_event_queue.h"
#include "src/state/material_slot_access.h"
#include "src/state/material_version.h"
#include "src/state/undo_owner.h"

SerialMock Serial;
SDMock SD;

namespace {

using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialReference;
using GroovePuterMaterial::MaterialVersionToken;
using GroovePuterMaterial::PreparationBasis;
using GroovePuterMaterial::versionForPattern;
using GroovePuterMaterial::versionForMelody;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "M0 FAIL: %s\n", message);
  ++g_failures;
}

float readEnginePhase(void* context) {
  return static_cast<MiniAcid*>(context)->transportPhaseSteps();
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

PhraseRuntime::RuntimeSynthEventBuffer invalidMelody() {
  auto melody = melodyWithNote(60);
  melody.count = PhraseRuntime::kMaxSynthEvents + 1;
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

bool samePattern(const SynthPattern& lhs, const SynthPattern& rhs) {
  return versionForPattern(lhs) == versionForPattern(rhs);
}

bool sameReference(const MaterialReference& lhs, const MaterialReference& rhs) {
  return lhs.address == rhs.address && lhs.id == rhs.id && lhs.id.valid();
}

struct Fixture {
  MiniAcid engine{44100.0f, nullptr};
  MusicalEventQueue queue{};

  static MaterialId fixtureMaterialId(int voice, int resident) {
    return MaterialId{static_cast<uint32_t>(
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
        scene.materialSlots[voice][resident].kind = MaterialKind::Pattern;
        scene.materialSlots[voice][resident].id =
            fixtureMaterialId(voice, resident);
      }
    }

    assertCanonicalReality(0);
    assertCanonicalReality(1);
  }

  MaterialReference reference(int voice) const {
    MaterialReference out{};
    const bool ok = engine.current303MaterialReference_(voice, out);
    assert(ok);
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

  void assertCanonicalReality(int voice) const {
    const MaterialReference ref = reference(voice);
    assert(ref.address.voice == static_cast<uint8_t>(voice));
    const int resident = GroovePuterMaterial::residentSlotFor(ref.address);
    assert(GroovePuterMaterial::residentSlotInRange(voice, resident));
    const Scene& scene = engine.sceneManager_.currentScene();
    const auto& descriptor = scene.materialSlots[voice][resident];
    assert(descriptor.kind == MaterialKind::Pattern);
    assert(descriptor.id.valid());
    assert(descriptor.id == ref.id);
    assert(versionForPattern(canonical(voice)).valid());
    if (engine.workingMaterial_[voice].holdsPattern()) {
      assert(engine.workingMaterial_[voice].patternMatches(ref));
    }
  }
};

}  // namespace

int main() {
  // 1. CHAINED DEVELOPMENT: A -> B -> C -> D without intermediate ACCEPT.
  {
    Fixture fixture;
    const auto origCanonical = fixture.canonical(0);
    const auto origRef = fixture.reference(0);

    // Hop 1: Pattern A -> Melody B
    const PreparationBasis basisA = fixture.engine.captureCurrentPreparationBasis(0);
    expect(basisA.valid(), "1: basisA must be valid");
    expect(basisA.kind == MaterialKind::Pattern, "1: basisA must be Pattern");
    expect(basisA.version == versionForPattern(origCanonical), "1: basisA version mismatch");

    const auto candidateB = melodyWithNote(60);
    expect(fixture.engine.prepareNextMelody(0, candidateB, basisA) ==
               MiniAcid::NextPrepareResult::Prepared,
           "1: prepare B failed");
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "1: activate B failed");
    expect(fixture.engine.workingMaterial_[0].holdsMelody(),
           "1: working must hold Melody after activating B");
    expect(sameMelody(fixture.engine.workingMaterial_[0].melody(), candidateB),
           "1: working must match candidate B");

    // Hop 2: Melody B -> Melody C (without ACCEPT)
    const PreparationBasis basisB = fixture.engine.captureCurrentPreparationBasis(0);
    expect(basisB.valid(), "1: basisB must be valid");
    expect(basisB.kind == MaterialKind::Melody, "1: basisB must be Melody");
    expect(basisB.version == versionForMelody(candidateB), "1: basisB version must match B");

    const auto candidateC = melodyWithNote(64);
    expect(fixture.engine.prepareNextMelody(0, candidateC, basisB) ==
               MiniAcid::NextPrepareResult::Prepared,
           "1: prepare C from Melody B without ACCEPT failed");
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "1: activate C failed");
    expect(fixture.engine.workingMaterial_[0].holdsMelody(),
           "1: working must hold Melody after activating C");
    expect(sameMelody(fixture.engine.workingMaterial_[0].melody(), candidateC),
           "1: working must match candidate C");

    // Hop 3: Melody C -> Melody D (without ACCEPT)
    const PreparationBasis basisC = fixture.engine.captureCurrentPreparationBasis(0);
    expect(basisC.valid(), "1: basisC must be valid");
    expect(basisC.kind == MaterialKind::Melody, "1: basisC must be Melody");
    expect(basisC.version == versionForMelody(candidateC), "1: basisC version must match C");

    const auto candidateD = melodyWithNote(67);
    expect(fixture.engine.prepareNextMelody(0, candidateD, basisC) ==
               MiniAcid::NextPrepareResult::Prepared,
           "1: prepare D from Melody C without ACCEPT failed");
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "1: activate D failed");
    expect(sameMelody(fixture.engine.workingMaterial_[0].melody(), candidateD),
           "1: working must match candidate D");

    // Canonical pattern must remain untouched through all hops
    expect(samePattern(fixture.canonical(0), origCanonical),
           "1: chained development mutated canonical pattern");
    expect(sameReference(fixture.reference(0), origRef),
           "1: chained development changed material reference");
  }

  // 2. MELODY CURRENT AS SOURCE: CURRENT holding a Melody is a legal development source.
  {
    Fixture fixture;
    const auto candidateB = melodyWithNote(61);
    const auto basisA = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, candidateB, basisA) ==
               MiniAcid::NextPrepareResult::Prepared,
           "2: prepare B failed");
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "2: activate B failed");

    // Melody B is now CURRENT. It is a legal development source for candidate C.
    const auto basisB = fixture.engine.captureCurrentPreparationBasis(0);
    expect(basisB.valid() && basisB.kind == MaterialKind::Melody,
           "2: Melody CURRENT did not yield valid Melody basis");
    const auto candidateC = melodyWithNote(65);
    expect(fixture.engine.prepareNextMelody(0, candidateC, basisB) ==
               MiniAcid::NextPrepareResult::Prepared,
           "2: Melody CURRENT rejected as development source");
    expect(fixture.engine.hasPendingMaterial(0),
           "2: NEXT not queued for candidate C");
    expect(fixture.engine.pendingMaterial_[0].melody != nullptr &&
               sameMelody(*fixture.engine.pendingMaterial_[0].melody, candidateC),
           "2: NEXT did not stage candidate C");
  }

  // 3. DIRTY CURRENT AS SOURCE: an edited Working Pattern is a legal source and untouched by prepare.
  {
    Fixture fixture;
    expect(fixture.engine.adjustWorking303StepNote(0, 0, 2),
           "3: could not create dirty working pattern");
    expect(fixture.engine.hasModifiedWorking303Pattern(0),
           "3: working pattern was not marked dirty");
    const SynthPattern dirtyPattern = *fixture.engine.currentWorking303Pattern(0);

    const PreparationBasis basis = fixture.engine.captureCurrentPreparationBasis(0);
    expect(basis.valid(), "3: dirty basis must be valid");
    expect(basis.kind == MaterialKind::Pattern, "3: dirty basis must be Pattern");
    expect(basis.version == versionForPattern(dirtyPattern),
           "3: basis version must match dirty working bytes, not accepted");

    const auto candidate = melodyWithNote(62);
    expect(fixture.engine.prepareNextMelody(0, candidate, basis) ==
               MiniAcid::NextPrepareResult::Prepared,
           "3: dirty CURRENT was rejected as development source");

    // Working pattern must remain completely untouched by prepare
    expect(fixture.engine.hasModifiedWorking303Pattern(0),
           "3: prepare cleared dirty flag");
    expect(fixture.engine.currentWorking303Pattern(0) != nullptr &&
               samePattern(*fixture.engine.currentWorking303Pattern(0), dirtyPattern),
           "3: prepare mutated dirty working pattern");

    // Activation succeeds because CURRENT is still the exact dirty pattern that was prepared
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "3: activation of candidate prepared from dirty CURRENT failed");
    expect(fixture.engine.workingMaterial_[0].holdsMelody() &&
               sameMelody(fixture.engine.workingMaterial_[0].melody(), candidate),
           "3: activation did not install melody into working");
  }

  // 4. STALE BASIS REJECTED: snapshot CURRENT, mutate CURRENT, then attempt publication -> rejected.
  {
    Fixture fixture;
    const auto priorNext = melodyWithNote(55);
    const auto initialBasis = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, priorNext, initialBasis) ==
               MiniAcid::NextPrepareResult::Prepared,
           "4: fixture could not prepare prior NEXT");

    // Snapshot CURRENT basis before mutation
    const PreparationBasis staleBasis = fixture.engine.captureCurrentPreparationBasis(0);

    // Mutate CURRENT after basis capture
    expect(fixture.engine.adjustWorking303StepNote(0, 1, 3),
           "4: could not mutate working pattern");
    const SynthPattern dirtyPattern = *fixture.engine.currentWorking303Pattern(0);

    // Attempt publication with stale basis
    const auto newCandidate = melodyWithNote(59);
    const auto result = fixture.engine.prepareNextMelody(0, newCandidate, staleBasis);
    expect(result == MiniAcid::NextPrepareResult::StalePreparationBasis,
           "4: publication with stale basis was not rejected as StalePreparationBasis");

    // CURRENT must be preserved
    expect(fixture.engine.currentWorking303Pattern(0) != nullptr &&
               samePattern(*fixture.engine.currentWorking303Pattern(0), dirtyPattern),
           "4: stale basis rejection modified CURRENT");

    // Existing NEXT must be preserved
    expect(fixture.engine.hasPendingMaterial(0) &&
               fixture.engine.pendingMaterial_[0].melody != nullptr &&
               sameMelody(*fixture.engine.pendingMaterial_[0].melody, priorNext),
           "4: stale basis rejection destroyed prior valid NEXT");
  }

  // 5. FAILURE ATOMICITY: for EVERY rejection path, verify zero state leakage.
  {
    Fixture fixture;
    const auto validBasis = fixture.engine.captureCurrentPreparationBasis(0);
    const auto priorNext = melodyWithNote(56);
    expect(fixture.engine.prepareNextMelody(0, priorNext, validBasis) ==
               MiniAcid::NextPrepareResult::Prepared,
           "5: setup prior NEXT failed");

    const auto preWorkingEmpty = fixture.engine.workingMaterial_[0].empty();
    const auto preActive = fixture.engine.activeMaterial(0);
    const auto prePendingMelody = *fixture.engine.pendingMaterial_[0].melody;
    const auto prePendingRef = fixture.engine.pendingMaterial_[0].preparedFor;
    const auto prePendingVersion = fixture.engine.pendingMaterial_[0].acceptedVersion;
    const auto prePendingQueued = fixture.engine.pendingMaterial_[0].queued;
    const auto prePendingBound = fixture.engine.pendingMaterial_[0].lifecycleBound;
    const auto preCanonical = fixture.canonical(0);
    const auto preRevision = GroovePuterUndo::undoOwner().committedRevision();

    auto assertAtomicity = [&](const char* pathName) {
      expect(fixture.engine.workingMaterial_[0].empty() == preWorkingEmpty,
             pathName);
      expect(fixture.engine.activeMaterial(0).kind == preActive.kind &&
                 fixture.engine.activeMaterial(0).slot == preActive.slot,
             pathName);
      expect(fixture.engine.pendingMaterial_[0].queued == prePendingQueued &&
                 fixture.engine.pendingMaterial_[0].lifecycleBound == prePendingBound &&
                 sameReference(fixture.engine.pendingMaterial_[0].preparedFor, prePendingRef) &&
                 fixture.engine.pendingMaterial_[0].acceptedVersion == prePendingVersion &&
                 sameMelody(*fixture.engine.pendingMaterial_[0].melody, prePendingMelody),
             pathName);
      expect(samePattern(fixture.canonical(0), preCanonical), pathName);
      expect(GroovePuterUndo::undoOwner().committedRevision() == preRevision, pathName);
    };

    // Path 5a: Invalid Voice
    expect(fixture.engine.prepareNextMelody(-1, melodyWithNote(60), validBasis) ==
               MiniAcid::NextPrepareResult::InvalidVoice,
           "5a: prepare with voice -1 did not return InvalidVoice");
    assertAtomicity("5a: invalid voice leaked state");

    // Path 5b: Invalid Candidate
    expect(fixture.engine.prepareNextMelody(0, invalidMelody(), validBasis) ==
               MiniAcid::NextPrepareResult::InvalidCandidate,
           "5b: invalid candidate did not return InvalidCandidate");
    assertAtomicity("5b: invalid candidate leaked state");

    // Path 5c: Stale Basis (wrong version)
    PreparationBasis bogusBasis = validBasis;
    bogusBasis.version.low ^= 0x12345678u;
    expect(fixture.engine.prepareNextMelody(0, melodyWithNote(60), bogusBasis) ==
               MiniAcid::NextPrepareResult::StalePreparationBasis,
           "5c: wrong version did not return StalePreparationBasis");
    assertAtomicity("5c: stale basis leaked state");

    // Path 5d: Unsupported Current State (invalid identity)
    const int resident = GroovePuterMaterial::residentSlotFor(validBasis.reference.address);
    auto& descriptor = fixture.engine.sceneManager_.currentScene().materialSlots[0][resident];
    const MaterialId savedId = descriptor.id;
    descriptor.id = MaterialId{};  // zero identity
    expect(fixture.engine.prepareNextMelody(0, melodyWithNote(60), validBasis) ==
               MiniAcid::NextPrepareResult::UnsupportedCurrentState,
           "5d: zero MaterialId did not return UnsupportedCurrentState");
    descriptor.id = savedId;
    assertAtomicity("5d: unsupported state leaked state");
  }

  // 6. VOICE ISOLATION: voice 1 completely unaffected by voice 0 operations.
  {
    Fixture fixture;
    const auto initialWorking1Empty = fixture.engine.workingMaterial_[1].empty();
    const auto initialActive1 = fixture.engine.activeMaterial(1);
    const auto initialCanonical1 = fixture.canonical(1);
    const auto initialRef1 = fixture.reference(1);

    // Chained development on voice 0
    const auto basisA = fixture.engine.captureCurrentPreparationBasis(0);
    const auto b = melodyWithNote(60);
    fixture.engine.prepareNextMelody(0, b, basisA);
    fixture.engine.activateNextMaterialAtBoundary(0);

    const auto basisB = fixture.engine.captureCurrentPreparationBasis(0);
    const auto c = melodyWithNote(64);
    fixture.engine.prepareNextMelody(0, c, basisB);
    fixture.engine.activateNextMaterialAtBoundary(0);

    // Voice 1 must be 100% identical to initial state
    expect(fixture.engine.workingMaterial_[1].empty() == initialWorking1Empty,
           "6: voice 0 operations altered voice 1 working storage");
    expect(fixture.engine.activeMaterial(1).kind == initialActive1.kind &&
               fixture.engine.activeMaterial(1).slot == initialActive1.slot,
           "6: voice 0 operations altered voice 1 active material");
    expect(!fixture.engine.hasPendingMaterial(1),
           "6: voice 0 operations queued NEXT on voice 1");
    expect(samePattern(fixture.canonical(1), initialCanonical1),
           "6: voice 0 operations mutated voice 1 canonical pattern");
    expect(sameReference(fixture.reference(1), initialRef1),
           "6: voice 0 operations altered voice 1 reference");
  }

  // 7. ACCEPT UNCHANGED: 0.9.12 canonical gate operates unchanged.
  {
    Fixture fixture;
    // Clean Pattern -> AlreadyClean
    expect(fixture.engine.acceptMaterialWorking(0) ==
               MiniAcid::AcceptResult::AlreadyClean,
           "7: clean accept did not return AlreadyClean");

    // Invalid voice -> InvalidVoice
    expect(fixture.engine.acceptMaterialWorking(-1) ==
               MiniAcid::AcceptResult::InvalidVoice,
           "7: accept with invalid voice did not return InvalidVoice");

    // Dirty Pattern -> Accepted
    expect(fixture.engine.adjustWorking303StepNote(0, 0, 1), "7: dirty failed");
    expect(fixture.engine.acceptMaterialWorking(0) ==
               MiniAcid::AcceptResult::Accepted,
           "7: dirty pattern accept did not return Accepted");

    // Melody Working -> Accepted
    const auto basis = fixture.engine.captureCurrentPreparationBasis(0);
    fixture.engine.prepareNextMelody(0, melodyWithNote(60), basis);
    fixture.engine.activateNextMaterialAtBoundary(0);
    expect(fixture.engine.workingMaterial_[0].holdsMelody(), "7: melody activate failed");
    expect(fixture.engine.acceptMaterialWorking(0) ==
               MiniAcid::AcceptResult::Accepted,
           "7: melody working accept did not return Accepted");
  }

  // 8. NO UNDO FROM M0: neither prepare nor NEXT publication creates Undo receipts.
  {
    Fixture fixture;
    const auto initialRevision = GroovePuterUndo::undoOwner().committedRevision();
    const auto initialHasUndo = GroovePuterUndo::undoOwner().hasUndo();

    const auto basis = fixture.engine.captureCurrentPreparationBasis(0);
    expect(GroovePuterUndo::undoOwner().committedRevision() == initialRevision,
           "8: captureCurrentPreparationBasis created undo receipt");

    const auto cand = melodyWithNote(60);
    fixture.engine.prepareNextMelody(0, cand, basis);
    expect(GroovePuterUndo::undoOwner().committedRevision() == initialRevision &&
               GroovePuterUndo::undoOwner().hasUndo() == initialHasUndo,
           "8: prepareNextMelody created undo receipt");

    fixture.engine.cancelNextMaterial(0);
    expect(GroovePuterUndo::undoOwner().committedRevision() == initialRevision &&
               GroovePuterUndo::undoOwner().hasUndo() == initialHasUndo,
           "8: cancelNextMaterial created undo receipt");
  }

  if (g_failures == 0) {
    std::printf("M0 development admission: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "M0 development admission: %d failure(s)\n", g_failures);
  return 1;
}
