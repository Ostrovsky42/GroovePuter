// FS2A: CURRENT / NEXT session causality.
//
// ACCEPT is deliberately outside this fixture. NEXT is prepared session state,
// inaudible until ACTIVATE, and activation may only replace a CURRENT that is
// still the exact clean material against which NEXT was prepared.

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
  std::fprintf(stderr, "FS2A FAIL: %s\n", message);
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

    // A host fixture has no project bootstrap/migration step, so construct the
    // same canonical identity reality production requires before asking for a
    // trusted MaterialReference. Every resident slot is a Pattern with a
    // stable, non-zero identity; retarget tests can therefore move to another
    // resident slot without falling back into legacy/unassigned identity.
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

  void setAcceptedKind(int voice, MaterialKind kind) {
    const auto ref = reference(voice);
    const int resident = GroovePuterMaterial::residentSlotFor(ref.address);
    assert(GroovePuterMaterial::setResidentKind(
        engine.sceneManager_.currentScene(), voice, resident, kind));
  }
};

}  // namespace

int main() {
  // 1. Clean prepare is session-only, inaudible, and bound to exact accepted
  //    identity/version. Voice B is not collateral state.
  {
    Fixture fixture;
    const SynthPattern acceptedA = fixture.canonical(0);
    const SynthPattern acceptedB = fixture.canonical(1);
    const auto refA = fixture.reference(0);
    const auto versionA = versionForPattern(acceptedA);
    const auto activeA = fixture.engine.activeMaterial(0);
    const auto activeB = fixture.engine.activeMaterial(1);
    const auto candidate = melodyWithNote(60);
    const auto basis = fixture.engine.captureCurrentPreparationBasis(0);

    expect(fixture.engine.prepareNextMelody(0, candidate, basis) ==
               MiniAcid::NextPrepareResult::Prepared,
           "clean prepare did not return Prepared");
    expect(fixture.engine.hasPendingMaterial(0),
           "clean prepare did not queue NEXT");
    expect(fixture.engine.activeMaterial(0).kind == activeA.kind &&
               fixture.engine.activeMaterial(0).slot == activeA.slot,
           "prepare changed audible CURRENT");
    expect(samePattern(fixture.canonical(0), acceptedA),
           "prepare mutated canonical Pattern");
    expect(fixture.engine.pendingMaterial_[0].lifecycleBound,
           "prepared NEXT is not lifecycle-bound");
    expect(sameReference(fixture.engine.pendingMaterial_[0].preparedFor, refA),
           "prepared NEXT captured wrong MaterialReference");
    expect(fixture.engine.pendingMaterial_[0].acceptedVersion == versionA,
           "prepared NEXT captured wrong canonical VersionToken");
    expect(fixture.engine.activeMaterial(1).kind == activeB.kind &&
               fixture.engine.activeMaterial(1).slot == activeB.slot &&
               samePattern(fixture.canonical(1), acceptedB) &&
               !fixture.engine.hasPendingMaterial(1),
           "prepare A changed voice B");
  }

  // 2. Valid replacement is latest-wins, but only after the replacement has
  //    been fully validated/staged.
  {
    Fixture fixture;
    const auto a1 = melodyWithNote(61);
    const auto a2 = melodyWithNote(65);
    const auto basis1 = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, a1, basis1) ==
               MiniAcid::NextPrepareResult::Prepared,
           "first prepare failed");
    const auto basis2 = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, a2, basis2) ==
               MiniAcid::NextPrepareResult::Replaced,
           "valid replacement did not return Replaced");
    expect(fixture.engine.pendingMaterial_[0].melody != nullptr &&
               sameMelody(*fixture.engine.pendingMaterial_[0].melody, a2),
           "valid replacement did not replace payload");
  }

  // 3. Invalid replacement cannot destroy the last successfully prepared idea
  //    or its causal stamp.
  {
    Fixture fixture;
    const auto a1 = melodyWithNote(62);
    const auto bad = invalidMelody();
    const auto basis1 = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, a1, basis1) ==
               MiniAcid::NextPrepareResult::Prepared,
           "fixture could not prepare prior NEXT");
    const auto priorPayload = *fixture.engine.pendingMaterial_[0].melody;
    const auto priorReference = fixture.engine.pendingMaterial_[0].preparedFor;
    const auto priorVersion = fixture.engine.pendingMaterial_[0].acceptedVersion;

    const auto basis2 = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, bad, basis2) ==
               MiniAcid::NextPrepareResult::InvalidCandidate,
           "invalid replacement was not rejected");
    expect(fixture.engine.hasPendingMaterial(0) &&
               fixture.engine.pendingMaterial_[0].lifecycleBound &&
               sameMelody(*fixture.engine.pendingMaterial_[0].melody,
                          priorPayload) &&
               sameReference(fixture.engine.pendingMaterial_[0].preparedFor,
                             priorReference) &&
               fixture.engine.pendingMaterial_[0].acceptedVersion == priorVersion,
           "failed replacement destroyed prior valid NEXT or binding");
  }

  // 4. Dirty CURRENT rejects a stale request prepared from before the edit,
  //    preserving both the edit and an already-valid NEXT.
  {
    Fixture fixture;
    const auto next = melodyWithNote(63);
    const auto basisBeforeEdit = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, next, basisBeforeEdit) ==
               MiniAcid::NextPrepareResult::Prepared,
           "fixture could not prepare prior NEXT");
    const auto priorNext = *fixture.engine.pendingMaterial_[0].melody;
    const auto priorStamp = fixture.engine.pendingMaterial_[0].acceptedVersion;
    expect(fixture.engine.adjustWorking303StepNote(0, 0, 1),
           "could not create identity-bound Working Pattern edit");
    expect(fixture.engine.hasModifiedWorking303Pattern(0),
           "Working Pattern edit was not dirty");
    const SynthPattern dirty = *fixture.engine.currentWorking303Pattern(0);

    // 0.9.12 invariant: rejected any prepare when CURRENT was dirty (RejectedCurrentDirty).
    // 0.9.13 M0 invariant: dirty CURRENT is a valid preparation basis; but a candidate
    // prepared against the pre-edit basis is now stale and rejected with StalePreparationBasis.
    // Why safe: the dirty Working Pattern edit is preserved, existing NEXT is preserved,
    // and canonical commit state is unchanged.
    expect(fixture.engine.prepareNextMelody(0, melodyWithNote(66), basisBeforeEdit) ==
               MiniAcid::NextPrepareResult::StalePreparationBasis,
           "stale basis prepare did not reject with StalePreparationBasis");
    expect(fixture.engine.currentWorking303Pattern(0) != nullptr &&
               samePattern(*fixture.engine.currentWorking303Pattern(0), dirty),
           "dirty CURRENT was changed by rejected prepare");
    expect(fixture.engine.hasPendingMaterial(0) &&
               sameMelody(*fixture.engine.pendingMaterial_[0].melody, priorNext) &&
               fixture.engine.pendingMaterial_[0].acceptedVersion == priorStamp,
           "dirty rejection destroyed existing NEXT");
  }

  // 5. Cancel is strictly per voice and does not touch CURRENT/canonical.
  {
    Fixture fixture;
    const SynthPattern acceptedA = fixture.canonical(0);
    const SynthPattern acceptedB = fixture.canonical(1);
    const auto b = melodyWithNote(69);
    const auto basisA = fixture.engine.captureCurrentPreparationBasis(0);
    const auto basisB = fixture.engine.captureCurrentPreparationBasis(1);
    expect(fixture.engine.prepareNextMelody(0, melodyWithNote(64), basisA) ==
               MiniAcid::NextPrepareResult::Prepared,
           "prepare A failed before cancel test");
    expect(fixture.engine.prepareNextMelody(1, b, basisB) ==
               MiniAcid::NextPrepareResult::Prepared,
           "prepare B failed before cancel test");
    const auto bStamp = fixture.engine.pendingMaterial_[1].acceptedVersion;

    expect(fixture.engine.cancelNextMaterial(0),
           "cancel A did not report a removed candidate");
    expect(!fixture.engine.hasPendingMaterial(0) &&
               !fixture.engine.pendingMaterial_[0].lifecycleBound,
           "cancel A left lifecycle NEXT queued");
    expect(fixture.engine.hasPendingMaterial(1) &&
               fixture.engine.pendingMaterial_[1].lifecycleBound &&
               fixture.engine.pendingMaterial_[1].acceptedVersion == bStamp &&
               sameMelody(*fixture.engine.pendingMaterial_[1].melody, b),
           "cancel A changed voice B NEXT");
    expect(samePattern(fixture.canonical(0), acceptedA) &&
               samePattern(fixture.canonical(1), acceptedB),
           "cancel mutated canonical material");
  }

  // 6. ACTIVATE moves NEXT to CURRENT/runtime only. Canonical identity/version
  //    remain the accepted Pattern truth.
  {
    Fixture fixture;
    const auto candidate = melodyWithNote(70);
    const auto ref = fixture.reference(0);
    const SynthPattern accepted = fixture.canonical(0);
    const MaterialVersionToken acceptedVersion = versionForPattern(accepted);
    const auto basis = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, candidate, basis) ==
               MiniAcid::NextPrepareResult::Prepared,
           "prepare failed before activation");

    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "valid NEXT did not activate");
    expect(fixture.engine.workingMaterial_[0].holdsMelody() &&
               sameMelody(fixture.engine.workingMaterial_[0].melody(), candidate),
           "activation did not make Melody CURRENT");
    expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Melody &&
               sameMelody(fixture.engine.currentPhraseBuffer(0), candidate),
           "activation did not publish Melody runtime");
    expect(samePattern(fixture.canonical(0), accepted) &&
               sameReference(fixture.reference(0), ref) &&
               versionForPattern(fixture.canonical(0)) == acceptedVersion,
           "activation mutated canonical payload/identity/version");
    expect(!fixture.engine.hasPendingMaterial(0),
           "activated NEXT remained queued");
  }

  // 7. A CURRENT edit after prepare invalidates activation, but does not erase
  //    either the edit or NEXT. The other voice remains independently usable.
  {
    Fixture fixture;
    const auto a = melodyWithNote(60);
    const auto b = melodyWithNote(67);
    const auto basisA = fixture.engine.captureCurrentPreparationBasis(0);
    const auto basisB = fixture.engine.captureCurrentPreparationBasis(1);
    expect(fixture.engine.prepareNextMelody(0, a, basisA) ==
               MiniAcid::NextPrepareResult::Prepared &&
           fixture.engine.prepareNextMelody(1, b, basisB) ==
               MiniAcid::NextPrepareResult::Prepared,
           "prepare failed before dirty-at-boundary race");
    expect(fixture.engine.adjustWorking303StepNote(0, 1, 1),
           "could not dirty A after prepare");
    const SynthPattern dirtyA = *fixture.engine.currentWorking303Pattern(0);

    // 0.9.12 invariant: any dirty CURRENT at boundary was rejected as RejectedCurrentDirty.
    // 0.9.13 M0 invariant: boundary activation checks basis equality; CURRENT edit causes
    // basis mismatch which returns RejectedCanonicalChanged (stale basis rejected).
    // Why safe: edit is preserved, candidate remains queued, uncommitted.
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::RejectedCanonicalChanged,
           "dirty-at-boundary A was activated");
    expect(fixture.engine.hasPendingMaterial(0) &&
               samePattern(*fixture.engine.currentWorking303Pattern(0), dirtyA),
           "dirty rejection did not preserve CURRENT and NEXT");
    expect(fixture.engine.activateNextMaterialAtBoundary(1) ==
               MiniAcid::NextActivationResult::Activated,
           "clean B could not activate independently");
  }

  // 8. A candidate is tied to the exact reference it was prepared for.
  {
    Fixture fixture;
    const auto preparedFor = fixture.reference(0);
    const auto basis = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, melodyWithNote(65), basis) ==
               MiniAcid::NextPrepareResult::Prepared,
           "prepare failed before retarget race");
    const int currentPattern = fixture.engine.display303LocalPatternIndex(0);
    const int nextPattern = (currentPattern + 1) % Bank<SynthPattern>::kPatterns;
    expect(fixture.engine.tryManual303TargetSwitch(0, nextPattern),
           "could not retarget to a clean resident Pattern");
    const auto afterRetarget = fixture.reference(0);
    expect(!sameReference(preparedFor, afterRetarget),
           "retarget test did not change MaterialReference");

    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::RejectedReferenceMismatch,
           "stale reference candidate activated");
    expect(fixture.engine.hasPendingMaterial(0),
           "reference rejection destroyed NEXT");
  }

  // 9. Same identity is still not enough: changing accepted bytes changes the
  //    canonical VersionToken and rejects the stale candidate without
  //    destroying it or publishing anything.
  {
    Fixture fixture;
    const auto ref = fixture.reference(0);
    const SynthPattern acceptedB = fixture.canonical(1);
    const auto refB = fixture.reference(1);
    const auto activeA = fixture.engine.activeMaterial(0);
    const auto activeB = fixture.engine.activeMaterial(1);
    const auto basis = fixture.engine.captureCurrentPreparationBasis(0);
    // 0.9.12 invariant: prepare NextMelody captured CURRENT silently at publication.
    // 0.9.13 M0 invariant: basis is captured before candidate preparation and passed explicitly;
    // changing canonical pattern bytes changes current basis version and rejects activation with RejectedCanonicalChanged.
    // Why safe: candidate remains queued and uncommitted, canonical pattern and CURRENT are preserved.
    expect(fixture.engine.prepareNextMelody(0, melodyWithNote(66), basis) ==
               MiniAcid::NextPrepareResult::Prepared,
           "prepare failed before canonical-version race");
    const auto priorPayload = *fixture.engine.pendingMaterial_[0].melody;
    const auto priorReference = fixture.engine.pendingMaterial_[0].preparedFor;
    const auto oldVersion = fixture.engine.pendingMaterial_[0].acceptedVersion;
    SynthPattern& accepted = fixture.canonical(0);
    accepted.steps[0].accent = !accepted.steps[0].accent;
    const SynthPattern changedAccepted = accepted;
    expect(sameReference(fixture.reference(0), ref),
           "canonical-version test accidentally changed identity");
    expect(versionForPattern(accepted) != oldVersion,
           "canonical-version test did not change VersionToken");

    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::RejectedCanonicalChanged,
           "stale canonical-version candidate activated");
    expect(fixture.engine.hasPendingMaterial(0) &&
               fixture.engine.pendingMaterial_[0].lifecycleBound &&
               sameMelody(*fixture.engine.pendingMaterial_[0].melody,
                          priorPayload) &&
               sameReference(fixture.engine.pendingMaterial_[0].preparedFor,
                             priorReference) &&
               fixture.engine.pendingMaterial_[0].acceptedVersion == oldVersion,
           "canonical-version rejection destroyed NEXT or causal stamp");
    expect(fixture.engine.workingMaterial_[0].empty() &&
               fixture.engine.activeMaterial(0).kind == activeA.kind &&
               fixture.engine.activeMaterial(0).slot == activeA.slot &&
               samePattern(fixture.canonical(0), changedAccepted),
           "canonical-version rejection published or changed CURRENT");
    expect(sameReference(fixture.reference(1), refB) &&
               samePattern(fixture.canonical(1), acceptedB) &&
               fixture.engine.activeMaterial(1).kind == activeB.kind &&
               fixture.engine.activeMaterial(1).slot == activeB.slot &&
               !fixture.engine.hasPendingMaterial(1),
           "canonical-version rejection changed voice B");
  }

  // 10. Accepted Melody is explicitly unsupported in this slice. No SD lookup
  //     is smuggled into prepare/activation to prove its canonical bytes.
  {
    Fixture fixture;
    const auto basis1 = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, melodyWithNote(68), basis1) ==
               MiniAcid::NextPrepareResult::Prepared,
           "fixture could not create prior NEXT");
    const auto prior = *fixture.engine.pendingMaterial_[0].melody;
    const auto priorReference = fixture.engine.pendingMaterial_[0].preparedFor;
    const auto priorVersion = fixture.engine.pendingMaterial_[0].acceptedVersion;
    fixture.setAcceptedKind(0, MaterialKind::Melody);

    // 0.9.12 invariant: accepted (resident) Melody failed closed as UnsupportedCurrentState.
    // 0.9.13 M0 invariant: accepted resident Melody still fails closed as UnsupportedCurrentState
    // because resident Melody requires filesystem resolution (no SD I/O in NEXT lifecycle);
    // however runtime CURRENT Melody in Working is supported as a valid preparation basis.
    // Why safe: preserved fail-closed behavior for unsupported resident Melody state.
    const auto basis2 = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, melodyWithNote(71), basis2) ==
               MiniAcid::NextPrepareResult::UnsupportedCurrentState,
           "accepted Melody did not fail closed as unsupported");
    expect(fixture.engine.hasPendingMaterial(0) &&
               sameMelody(*fixture.engine.pendingMaterial_[0].melody, prior) &&
               sameReference(fixture.engine.pendingMaterial_[0].preparedFor,
                             priorReference) &&
               fixture.engine.pendingMaterial_[0].acceptedVersion == priorVersion,
           "unsupported state destroyed previous NEXT");
  }

  // 11. Zero is not a weak identity. If canonical identity becomes invalid,
  //     a request fails closed before it can replace CURRENT or an existing
  //     valid NEXT.
  {
    Fixture fixture;
    const SynthPattern acceptedA = fixture.canonical(0);
    const SynthPattern acceptedB = fixture.canonical(1);
    const auto refA = fixture.reference(0);
    const auto refB = fixture.reference(1);
    const auto activeA = fixture.engine.activeMaterial(0);
    const auto activeB = fixture.engine.activeMaterial(1);
    const auto basis1 = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, melodyWithNote(72), basis1) ==
               MiniAcid::NextPrepareResult::Prepared,
           "fixture could not create prior NEXT for invalid-ID test");
    const auto priorPayload = *fixture.engine.pendingMaterial_[0].melody;
    const auto priorReference = fixture.engine.pendingMaterial_[0].preparedFor;
    const auto priorVersion = fixture.engine.pendingMaterial_[0].acceptedVersion;

    const int resident = GroovePuterMaterial::residentSlotFor(refA.address);
    auto& descriptor =
        fixture.engine.sceneManager_.currentScene().materialSlots[0][resident];
    descriptor.id = MaterialId{};
    expect(!descriptor.id.valid(),
           "invalid-ID fixture did not install zero MaterialId");

    const auto basis2 = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, melodyWithNote(73), basis2) ==
               MiniAcid::NextPrepareResult::UnsupportedCurrentState,
           "invalid canonical identity did not fail closed");
    expect(fixture.engine.workingMaterial_[0].empty() &&
               fixture.engine.activeMaterial(0).kind == activeA.kind &&
               fixture.engine.activeMaterial(0).slot == activeA.slot,
           "invalid-ID rejection changed CURRENT/runtime");
    expect(samePattern(fixture.canonical(0), acceptedA) &&
               !descriptor.id.valid(),
           "invalid-ID rejection mutated canonical payload/identity");
    expect(fixture.engine.hasPendingMaterial(0) &&
               fixture.engine.pendingMaterial_[0].lifecycleBound &&
               sameMelody(*fixture.engine.pendingMaterial_[0].melody,
                          priorPayload) &&
               sameReference(fixture.engine.pendingMaterial_[0].preparedFor,
                             priorReference) &&
               fixture.engine.pendingMaterial_[0].acceptedVersion == priorVersion,
           "invalid-ID rejection destroyed existing NEXT");
    expect(sameReference(fixture.reference(1), refB) &&
               samePattern(fixture.canonical(1), acceptedB) &&
               fixture.engine.activeMaterial(1).kind == activeB.kind &&
               fixture.engine.activeMaterial(1).slot == activeB.slot &&
               !fixture.engine.hasPendingMaterial(1),
           "invalid-ID rejection changed voice B");
  }

  // 12. Address reuse is not identity reuse. A prepared OLD_ID candidate must
  //     not publish if the same address now names NEW_ID (ABA/stale identity).
  {
    Fixture fixture;
    const SynthPattern acceptedA = fixture.canonical(0);
    const SynthPattern acceptedB = fixture.canonical(1);
    const auto oldRef = fixture.reference(0);
    const auto refB = fixture.reference(1);
    const auto activeA = fixture.engine.activeMaterial(0);
    const auto activeB = fixture.engine.activeMaterial(1);
    const auto basis = fixture.engine.captureCurrentPreparationBasis(0);
    expect(fixture.engine.prepareNextMelody(0, melodyWithNote(74), basis) ==
               MiniAcid::NextPrepareResult::Prepared,
           "prepare failed before ABA identity race");
    const auto priorPayload = *fixture.engine.pendingMaterial_[0].melody;
    const auto priorReference = fixture.engine.pendingMaterial_[0].preparedFor;
    const auto priorVersion = fixture.engine.pendingMaterial_[0].acceptedVersion;

    const int resident = GroovePuterMaterial::residentSlotFor(oldRef.address);
    auto& descriptor =
        fixture.engine.sceneManager_.currentScene().materialSlots[0][resident];
    const MaterialId replacementId{0xF0000001u};
    assert(replacementId.valid() && replacementId != oldRef.id);
    descriptor.id = replacementId;

    const auto newRef = fixture.reference(0);
    expect(newRef.address == oldRef.address && newRef.id == replacementId,
           "ABA fixture did not preserve address while replacing identity");
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::RejectedReferenceMismatch,
           "stale-ID candidate published after address reuse");
    expect(fixture.engine.workingMaterial_[0].empty() &&
               fixture.engine.activeMaterial(0).kind == activeA.kind &&
               fixture.engine.activeMaterial(0).slot == activeA.slot,
           "stale-ID rejection changed CURRENT/runtime");
    expect(samePattern(fixture.canonical(0), acceptedA) &&
               descriptor.id == replacementId,
           "stale-ID rejection mutated canonical payload/new identity");
    expect(fixture.engine.hasPendingMaterial(0) &&
               fixture.engine.pendingMaterial_[0].lifecycleBound &&
               sameMelody(*fixture.engine.pendingMaterial_[0].melody,
                          priorPayload) &&
               sameReference(fixture.engine.pendingMaterial_[0].preparedFor,
                             priorReference) &&
               fixture.engine.pendingMaterial_[0].acceptedVersion == priorVersion,
           "stale-ID rejection destroyed NEXT or causal stamp");
    expect(sameReference(fixture.reference(1), refB) &&
               samePattern(fixture.canonical(1), acceptedB) &&
               fixture.engine.activeMaterial(1).kind == activeB.kind &&
               fixture.engine.activeMaterial(1).slot == activeB.slot &&
               !fixture.engine.hasPendingMaterial(1),
           "stale-ID rejection changed voice B");
  }

  if (g_failures == 0) {
    std::printf("FS2A current-next causality: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "FS2A current-next causality: %d failure(s)\n", g_failures);
  return 1;
}
