#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/dsp/musical_development.h"
#include "src/state/material_slot_access.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

SerialMock Serial;
SDMock SD;

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "R1 FAIL: %s\n", message);
    ++g_failures;
  } else {
    std::fprintf(stdout, "R1 PASS: %s\n", message);
  }
}

PhraseRuntime::RuntimeSynthEventBuffer makeTestPhrase(uint8_t note = 48) {
  PhraseRuntime::RuntimeSynthEventBuffer buf{};
  buf.lengthTicks = PhraseRuntime::kTicksPerBar;
  buf.count = 2;
  buf.events[0].startTick = 0;
  buf.events[0].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
  buf.events[0].note = note;
  buf.events[0].velocity = 100;
  buf.events[0].probability = 100;

  buf.events[1].startTick = 24;
  buf.events[1].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
  buf.events[1].note = note + 7;
  buf.events[1].velocity = 90;
  buf.events[1].probability = 100;
  return buf;
}

// 1. Witness HOLD duration delta ordering
void testHoldDurationDelta() {
  auto phrase = makeTestPhrase();
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Hold;
  PhraseRuntime::RuntimeSynthEventBuffer candidate{};
  GroovePuterDevelopment::DevelopmentEvidence evidence{};
  GroovePuterDevelopment::transformHold(phrase, req, candidate, evidence);

  expect(evidence.bass.durationDeltaSubticks > 0,
         "HOLD evidence.bass.durationDeltaSubticks must be non-zero after duration extension");
  expect(evidence.bass.durationsExtended,
         "HOLD evidence.bass.durationsExtended must be true");
}

// 2. Witness REPEAT classification: A A must be PRESERVED, not VARIATION
void testRepeatPreserved() {
  auto phrase = makeTestPhrase();
  GroovePuterDevelopment::DevelopmentRequest req{};
  auto res = GroovePuterDevelopment::growMaterial(phrase, 2, GroovePuterDevelopment::GrowthMode::Repeat, req);
  expect(res.classification.idea == GroovePuterMaterial::IdeaClassification::Preserved,
         "REPEAT growth must classify idea as Preserved");
  expect(res.classification.genre == GroovePuterDevelopment::GenreResult::Pass,
         "REPEAT growth genre result must be Pass");
  expect(res.classification.temporalRole == GroovePuterDevelopment::TemporalRoleResult::Pass,
         "REPEAT growth temporal role must be Pass");
  expect(res.success, "REPEAT growth must succeed");
}

// 3. Witness DEVELOP growth is explicitly deferred
void testDevelopGrowthDeferred() {
  auto phrase = makeTestPhrase();
  GroovePuterDevelopment::DevelopmentRequest req{};
  auto res = GroovePuterDevelopment::growMaterial(phrase, 2, GroovePuterDevelopment::GrowthMode::Develop, req);
  expect(!res.success, "DEVELOP growth must be deferred and fail-closed");
  expect(res.classification.genre == GroovePuterDevelopment::GenreResult::Fail,
         "DEVELOP growth genre must be Fail");
  expect(res.classification.failureReason != nullptr &&
         std::strstr(res.classification.failureReason, "DEFERRED") != nullptr,
         "DEVELOP growth failure reason must state DEFERRED");
}

// 4. Witness EXTEND transformation is explicitly deferred
void testExtendDeferred() {
  auto phrase = makeTestPhrase();
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Extend;
  auto res = GroovePuterDevelopment::developCandidate(phrase, req);
  expect(!res.success, "EXTEND must be deferred without tonal root authority");
  expect(res.classification.genre == GroovePuterDevelopment::GenreResult::Fail,
         "EXTEND genre must be Fail");
  expect(res.classification.failureReason != nullptr &&
         std::strstr(res.classification.failureReason, "EXTEND DEFERRED") != nullptr,
         "EXTEND failure reason must state EXTEND DEFERRED");
}

static void setupCanonicalTestEngine(MiniAcid& engine) {
  engine.setBpm(120.0f);
  engine.rebuildPatternRuntimeEventBank();
  Scene& scene = engine.sceneManager_.currentScene();
  for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
    for (int resident = 0; resident < Scene::kMaterialSlotsPerVoice; ++resident) {
      scene.materialSlots[voice][resident].kind =
          GroovePuterMaterial::MaterialKind::Pattern;
      scene.materialSlots[voice][resident].id =
          GroovePuterMaterial::MaterialId{static_cast<uint32_t>(
              1 + voice * Scene::kMaterialSlotsPerVoice + resident)};
    }
  }
}

// 5. Witness Memory Safety: default Pattern CURRENT -> DEVELOP safe
void testPatternCurrentDevelopSafe() {
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  setupCanonicalTestEngine(engine);

  // On boot, workingMaterial is empty and active material is Pattern
  expect(engine.workingMaterial_[0].empty(), "Initial working material must be empty");
  expect(engine.activeMaterial_[0].kind == GroovePuterMaterial::MaterialKind::Pattern,
         "Initial active material must be Pattern");

  // Populate active pattern with notes so working source acquisition produces valid events
  Scene& scene = engine.sceneManager_.currentScene();
  scene.synthABanks[0].patterns[0].steps[0].note = 48;
  scene.synthABanks[0].patterns[0].steps[4].note = 55;

  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Revoice;
  GroovePuterDevelopment::DevelopmentResult outResult{};

  auto prepRes = engine.developWorkingMaterial(0, req, &outResult);
  expect(prepRes == MiniAcid::NextPrepareResult::Prepared,
         "developWorkingMaterial from Pattern CURRENT must succeed safely");
  expect(outResult.candidate.count > 0,
         "Candidate count must be valid");
  expect(outResult.candidate.count <= PhraseRuntime::kMaxSynthEvents,
         "Candidate count must not exceed maximum synth events (no union tag corruption)");
  expect(engine.workingMaterial_[0].empty(),
         "CURRENT workingMaterial must remain unmutated by source acquisition");
}

// 6. Witness Malformed Source validation fail-closed
void testMalformedSourceFailClosed() {
  MiniAcid engine(44100.0f, nullptr);
  engine.init();

  // Corrupted source buffer
  PhraseRuntime::RuntimeSynthEventBuffer bad{};
  bad.count = 200; // invalid count > 64
  bad.lengthTicks = 384;

  GroovePuterDevelopment::DevelopmentRequest req{};
  auto dev = GroovePuterDevelopment::developCandidate(bad, req);
  expect(!dev.success, "developCandidate must reject malformed source fail-closed");

  auto grow = GroovePuterDevelopment::growMaterial(bad, 2, GroovePuterDevelopment::GrowthMode::Repeat, req);
  expect(!grow.success, "growMaterial must reject malformed source fail-closed");
}

// 7. Witness Real Musical Boundary GO
void testMusicalBoundaryGo() {
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  setupCanonicalTestEngine(engine);

  // Create candidate
  auto phrase = makeTestPhrase(48);
  auto basis = engine.captureCurrentPreparationBasis(0);
  auto prepRes = engine.prepareNextMelody(0, phrase, basis, GroovePuterMaterial::IdeaClassification::Variation);
  expect(prepRes == MiniAcid::NextPrepareResult::Prepared, "prepareNextMelody must succeed");
  expect(engine.hasPendingMaterial(0), "Voice 0 must have pending material");

  // Simulate playback mid-bar: playing = true, currentTick_ = 100
  engine.playing = true;
  engine.currentTick_ = 100;

  auto goRes = engine.requestGoNextMaterial(0);
  expect(goRes == MiniAcid::GoRequestResult::Queued, "requestGoNextMaterial mid-bar must be Queued");
  expect(engine.isGoQueued(0), "isGoQueued must be true");

  // Immediately after keypress: CURRENT is still Pattern, NEXT is still pending
  expect(engine.activeMaterial_[0].kind == GroovePuterMaterial::MaterialKind::Pattern,
         "CURRENT must remain unactivated mid-bar");
  expect(engine.hasPendingMaterial(0), "NEXT must still be pending before boundary");

  // Advance ticks 101 to 383: still mid-bar, no activation
  for (uint32_t t = 101; t < 384; ++t) {
    engine.currentTick_ = t;
    engine.advanceTick();
  }
  expect(engine.activeMaterial_[0].kind == GroovePuterMaterial::MaterialKind::Pattern,
         "CURRENT must remain unactivated before barTick == 0");
  expect(engine.hasPendingMaterial(0), "NEXT must still be pending at tick 383");

  // Advance to exact bar boundary: tick 384 -> barTick == 0
  engine.currentTick_ = 384;
  engine.advanceTick();

  // At boundary: NEXT committed to CURRENT!
  expect(!engine.isGoQueued(0), "isGoQueued must be cleared at boundary");
  expect(!engine.hasPendingMaterial(0), "NEXT must be empty after boundary activation");
  expect(engine.activeMaterial_[0].kind == GroovePuterMaterial::MaterialKind::Melody,
         "CURRENT must be committed to Melody at boundary");
}

// 8. Witness Queued GO Cancel before boundary
void testQueuedGoCancelBeforeBoundary() {
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  setupCanonicalTestEngine(engine);

  auto phrase = makeTestPhrase(48);
  auto basis = engine.captureCurrentPreparationBasis(0);
  engine.prepareNextMelody(0, phrase, basis, GroovePuterMaterial::IdeaClassification::Variation);

  engine.playing = true;
  engine.currentTick_ = 100;
  auto goRes = engine.requestGoNextMaterial(0);
  expect(goRes == MiniAcid::GoRequestResult::Queued, "requestGoNextMaterial must queue");
  expect(engine.isGoQueued(0), "GO must be queued");

  // Cancel before boundary
  engine.cancelNextMaterial(0);
  expect(!engine.isGoQueued(0), "cancelNextMaterial must clear queued GO");
  expect(!engine.hasPendingMaterial(0), "NEXT must be cleared");

  // Advance to boundary
  engine.currentTick_ = 384;
  engine.advanceTick();
  expect(engine.activeMaterial_[0].kind == GroovePuterMaterial::MaterialKind::Pattern,
         "Cancelled candidate must NEVER activate at boundary");
}

// 9. Witness Queued GO replaced before boundary
void testQueuedGoReplacedBeforeBoundary() {
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  setupCanonicalTestEngine(engine);

  auto phraseB = makeTestPhrase(48);
  auto basis = engine.captureCurrentPreparationBasis(0);
  engine.prepareNextMelody(0, phraseB, basis, GroovePuterMaterial::IdeaClassification::Variation);

  engine.playing = true;
  engine.currentTick_ = 100;
  auto goRes = engine.requestGoNextMaterial(0);
  expect(goRes == MiniAcid::GoRequestResult::Queued, "requestGoNextMaterial must queue");
  expect(engine.isGoQueued(0), "GO(B) must be queued");

  // Replace B with C before boundary
  auto phraseC = makeTestPhrase(55);
  engine.prepareNextMelody(0, phraseC, basis, GroovePuterMaterial::IdeaClassification::Variation);

  // The old GO request must be invalidated!
  expect(!engine.isGoQueued(0), "Replacing candidate must invalidate old queued GO");

  // Advance to boundary
  engine.currentTick_ = 384;
  engine.advanceTick();
  expect(engine.hasPendingMaterial(0), "C must NOT activate on stale GO(B)");
  expect(engine.activeMaterial_[0].kind == GroovePuterMaterial::MaterialKind::Pattern,
         "CURRENT must remain unactivated on stale GO");
}

// 10. Witness Lineage Undo atomicity: A0 -> A1 -> A2 -> B(NewIdea) -> Undo restores A0 sourceAnchor
void testLineageUndoAtomicity() {
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  setupCanonicalTestEngine(engine);

  // Establish A0 as initial idea
  auto a0 = makeTestPhrase(48);
  auto basis0 = engine.captureCurrentPreparationBasis(0);
  auto prep0 = engine.prepareNextMelody(0, a0, basis0, GroovePuterMaterial::IdeaClassification::NewIdea);
  expect(prep0 == MiniAcid::NextPrepareResult::Prepared, "prepare A0 must succeed");
  auto act0 = engine.activateNextMaterialAtBoundary(0);
  expect(act0 == MiniAcid::NextActivationResult::Activated, "activate A0 must succeed");
  auto anchorA0 = engine.sourceAnchor(0);
  const auto* snap0 = engine.sourceAnchorSnapshot(0);
  expect(snap0 != nullptr && snap0->events[0].note == 48, "A0 snapshot must be captured");

  // A0 -> A1 (Variation)
  auto a1 = makeTestPhrase(50);
  auto basis1 = engine.captureCurrentPreparationBasis(0);
  auto prep1 = engine.prepareNextMelody(0, a1, basis1, GroovePuterMaterial::IdeaClassification::Variation);
  expect(prep1 == MiniAcid::NextPrepareResult::Prepared, "prepare A1 must succeed");
  auto act1 = engine.activateNextMaterialAtBoundary(0);
  expect(act1 == MiniAcid::NextActivationResult::Activated, "activate A1 must succeed");

  // A1 -> A2 (Variation)
  auto a2 = makeTestPhrase(52);
  auto basis2 = engine.captureCurrentPreparationBasis(0);
  auto prep2 = engine.prepareNextMelody(0, a2, basis2, GroovePuterMaterial::IdeaClassification::Variation);
  expect(prep2 == MiniAcid::NextPrepareResult::Prepared, "prepare A2 must succeed");
  auto act2 = engine.activateNextMaterialAtBoundary(0);
  expect(act2 == MiniAcid::NextActivationResult::Activated, "activate A2 must succeed");

  expect(engine.sourceAnchor(0).version == anchorA0.version,
         "Before B, sourceAnchor must still be A0");

  // A2 -> B (NewIdea)
  auto b = makeTestPhrase(60);
  auto basisB = engine.captureCurrentPreparationBasis(0);
  auto prepB = engine.prepareNextMelody(0, b, basisB, GroovePuterMaterial::IdeaClassification::NewIdea);
  expect(prepB == MiniAcid::NextPrepareResult::Prepared, "prepare B must succeed");
  auto actB = engine.activateNextMaterialAtBoundary(0);
  expect(actB == MiniAcid::NextActivationResult::Activated, "activate B must succeed");

  expect(engine.sourceAnchor(0).version != anchorA0.version,
         "After GO(B), sourceAnchor is switched to B");

  // UNDO GO(B) -> MUST restore A0 as sourceAnchor!
  bool undid = engine.undoMaterialWorking(0);
  expect(undid, "Undo must succeed");
  expect(engine.sourceAnchor(0).version == anchorA0.version,
         "After Undo, sourceAnchor must be restored to A0");
  const auto* restoredSnap = engine.sourceAnchorSnapshot(0);
  expect(restoredSnap != nullptr && restoredSnap->events[0].note == 48,
         "After Undo, sourceAnchorSnapshot note must be restored to A0");
}

// 11. Witness Default Epistemic UNKNOWN / No Default Variation
void testDefaultUnknown() {
  MiniAcid::PendingMaterial pending{};
  expect(pending.ideaClassification == GroovePuterMaterial::IdeaClassification::Unknown,
         "PendingMaterial::ideaClassification must default to Unknown");

  auto phrase = makeTestPhrase();
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::None;
  auto res = GroovePuterDevelopment::developCandidate(phrase, req);
  expect(res.classification.idea == GroovePuterMaterial::IdeaClassification::Preserved,
         "Unmutated candidate must be Preserved, not Variation");
}

// 12. Witness REVOICE pitch class invariance and evidence derived from buffer
void testRevoiceEvidenceTruth() {
  auto phrase = makeTestPhrase(48);
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = GroovePuterDevelopment::TransformationKind::Revoice;
  req.octaveShift = 1;

  auto res = GroovePuterDevelopment::developCandidate(phrase, req);
  expect(res.success, "REVOICE must succeed");
  expect(res.evidence.harmony.pitchClassesPreserved,
         "REVOICE evidence must report pitchClassesPreserved = true");
  expect(res.evidence.harmony.pitchesChanged,
         "REVOICE evidence must report pitchesChanged = true");

  // Check actual candidate musical bytes
  for (uint16_t i = 0; i < res.candidate.count; ++i) {
    expect((res.candidate.events[i].note % 12) == (phrase.events[i].note % 12),
           "Every note in REVOICE must have pitch class equal to source");
    expect(res.candidate.events[i].note != phrase.events[i].note,
           "Every note in REVOICE must have transposed register");
  }
}

}  // namespace

int main() {
  testHoldDurationDelta();
  testRepeatPreserved();
  testDevelopGrowthDeferred();
  testExtendDeferred();
  testPatternCurrentDevelopSafe();
  testMalformedSourceFailClosed();
  testMusicalBoundaryGo();
  testQueuedGoCancelBeforeBoundary();
  testQueuedGoReplacedBeforeBoundary();
  testLineageUndoAtomicity();
  testDefaultUnknown();
  testRevoiceEvidenceTruth();

  std::printf("\n=== R1 Remediation Gate: %d failures ===\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
