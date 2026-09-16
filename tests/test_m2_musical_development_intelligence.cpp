#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#define private public
#include "src/dsp/miniacid_engine.h"
#include "src/dsp/musical_development.h"
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

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "M2 FAIL: %s\n", message);
    ++g_failures;
  }
}

float readEnginePhase(void* context) {
  return static_cast<MiniAcid*>(context)->transportPhaseSteps();
}

PhraseRuntime::RuntimeSynthEventBuffer melodyWithTwoNotes(uint8_t note1, uint8_t note2) {
  PhraseRuntime::RuntimeSynthEventBuffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 2;

  // The One on step 0
  melody.events[0].startTick = 0;
  melody.events[0].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
  melody.events[0].note = note1;
  melody.events[0].velocity = 100;
  melody.events[0].probability = 100;

  // Second note on step 4 (tick 96)
  melody.events[1].startTick = 96;
  melody.events[1].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
  melody.events[1].note = note2;
  melody.events[1].velocity = 90;
  melody.events[1].probability = 100;

  return melody;
}

PhraseRuntime::RuntimeSynthEventBuffer melodyWithFourNotes() {
  PhraseRuntime::RuntimeSynthEventBuffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 4;

  for (uint16_t i = 0; i < 4; ++i) {
    melody.events[i].startTick = i * 96; // Steps 0, 4, 8, 12
    melody.events[i].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
    melody.events[i].note = static_cast<uint8_t>(36 + i * 2);
    melody.events[i].velocity = 100;
    melody.events[i].probability = 100;
  }
  return melody;
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
  using namespace GroovePuterDevelopment;
  using GroovePuterMaterial::IdeaClassification;

  // -------------------------------------------------------------------------
  // 1. HARMONY VERTICAL: REVOICE
  // -------------------------------------------------------------------------
  {
    const auto source = melodyWithTwoNotes(36, 40); // C2, E2
    DevelopmentRequest req{};
    req.transformation = TransformationKind::Revoice;
    req.octaveShift = 1; // register shift up an octave

    const auto dev = developCandidate(source, req);
    expect(dev.success, "1.1: Revoice development must succeed");
    expect(dev.evidence.harmony.pitchesChanged, "1.1: Revoice must change pitches");
    expect(dev.evidence.harmony.pitchClassesPreserved, "1.1: Revoice must preserve harmonic pitch classes");
    expect(dev.evidence.harmony.rootPreserved, "1.1: Revoice must preserve root");
    expect(!dev.evidence.rhythm.onsetsChanged, "1.1: Revoice must not change onsets");
    expect(!dev.evidence.bass.durationsExtended, "1.1: Revoice must not extend durations");
    expect(dev.classification.genre == GenreResult::Pass, "1.1: Genre must pass");
    expect(dev.classification.idea == IdeaClassification::Variation, "1.1: Revoice must be Variation");
    expect(dev.candidate.events[0].startTick == 0, "1.1: Step 0 onset must be preserved");
    expect(dev.candidate.events[0].note == 48, "1.1: Note 0 must be revoiced to C3 (36 + 12)");
    expect(dev.candidate.events[0].note % 12 == 36 % 12, "1.1: Pitch class modulo 12 must be strictly preserved");
  }

  // -------------------------------------------------------------------------
  // 2. HARMONY VERTICAL: EXTEND
  // -------------------------------------------------------------------------
  {
    const auto source = melodyWithTwoNotes(36, 40);
    DevelopmentRequest req{};
    req.transformation = TransformationKind::Extend;
    req.rootKey = 0; // C root

    const auto dev = developCandidate(source, req);
    expect(dev.success, "1.2: Extend development must succeed");
    expect(dev.candidate.count == 3, "1.2: Extend must add an extension event");
    expect(dev.evidence.rhythm.densityDelta == 1, "1.2: Density delta must be +1");
    expect(dev.evidence.harmony.pitchesChanged, "1.2: Pitches changed must be true");
    expect(dev.evidence.harmony.extensionsAdded, "1.2: Extensions added must be true");
    expect(dev.classification.genre == GenreResult::Pass, "1.2: Genre must pass");
    expect(dev.classification.idea == IdeaClassification::NewIdea, "1.2: Extend must classify as NewIdea");
  }

  // -------------------------------------------------------------------------
  // 3. MANDATORY HARMONIC REJECTION WITNESS:
  //    Harmonic evidence is acceptable, BUT G4 / genre structural constraint fails.
  //    Candidate must be rejected fail-closed and must not reach NEXT.
  // -------------------------------------------------------------------------
  {
    Fixture fixture;
    const auto a0Basis = fixture.basis(0);

    const auto source = melodyWithTwoNotes(36, 40);
    DevelopmentRequest req{};
    req.transformation = TransformationKind::Revoice;
    req.genreId = static_cast<uint8_t>(GenerativeMode::FunkSoul);
    req.requireTheOne = true;
    req.forceDisplaceTheOne = true; // Simulates an edit that broke The One in Funk

    const auto dev = developCandidate(source, req);
    // Structural invariant: Harmonic transform succeeded, but G4 genre constraint failed!
    expect(dev.evidence.harmony.pitchesChanged, "1.3: Harmonic revoice succeeded in isolation");
    expect(dev.classification.genre == GenreResult::Fail,
           "1.3: G4 must reject candidate when The One is broken in FunkSoul");
    expect(!dev.success, "1.3: Overall development result must be failed");

    // Fail-closed admission policy: Candidate is NOT staged to NEXT
    if (dev.success) {
      fixture.engine.prepareNextMelody(0, dev.candidate, a0Basis, dev.classification.idea);
    }
    expect(!fixture.engine.hasPendingMaterial(0),
           "1.3: Failed candidate must NOT be staged to NEXT");
    expect(fixture.basis(0) == a0Basis,
           "1.3: CURRENT basis must remain untouched after rejection");
  }

  // -------------------------------------------------------------------------
  // 4. RHYTHM VERTICAL: DISPLACE
  // -------------------------------------------------------------------------
  {
    const auto source = melodyWithTwoNotes(36, 40);
    DevelopmentRequest req{};
    req.transformation = TransformationKind::Displace;
    req.displaceTicks = 24; // 1 step displacement

    const auto dev = developCandidate(source, req);
    expect(dev.success, "2.1: Displace development must succeed");
    expect(dev.evidence.rhythm.onsetsChanged, "2.1: Onsets must change");
    expect(!dev.evidence.harmony.pitchesChanged, "2.1: Pitches must be preserved");
    expect(dev.classification.genre == GenreResult::Pass, "2.1: Genre must pass");
    expect(dev.classification.idea == IdeaClassification::Variation, "2.1: Displace must be Variation");
    expect(dev.candidate.events[0].note == 36, "2.1: Pitch 0 preserved");
    expect(dev.candidate.events[1].note == 40, "2.1: Pitch 1 preserved");
  }

  // -------------------------------------------------------------------------
  // 5. RHYTHM VERTICAL: THIN
  // -------------------------------------------------------------------------
  {
    const auto source = melodyWithFourNotes();
    DevelopmentRequest req{};
    req.transformation = TransformationKind::Thin;

    const auto dev = developCandidate(source, req);
    expect(dev.success, "2.2: Thin development must succeed");
    expect(dev.candidate.count < source.count, "2.2: Note count must decrease");
    expect(dev.evidence.rhythm.densityDelta < 0, "2.2: Density delta must be negative");
    expect(!dev.evidence.harmony.pitchesChanged, "2.2: Pitches must not change");
    expect(dev.evidence.rhythm.theOnePreserved, "2.2: The One must be preserved");
    expect(dev.classification.genre == GenreResult::Pass, "2.2: Genre must pass");
  }

  // -------------------------------------------------------------------------
  // 6. MANDATORY RHYTHM REJECTION WITNESS:
  //    Candidate harmonically sound, but rhythmically / metrically genre-invalid.
  // -------------------------------------------------------------------------
  {
    // Case A: Funk requires The One; displacing The One causes G4 failure
    const auto source = melodyWithTwoNotes(36, 40);
    DevelopmentRequest req{};
    req.transformation = TransformationKind::Displace;
    req.genreId = static_cast<uint8_t>(GenerativeMode::FunkSoul);
    req.forceDisplaceTheOne = true;

    const auto dev = developCandidate(source, req);
    expect(dev.classification.genre == GenreResult::Fail,
           "2.3: G4 must reject rhythmic displacement that removes The One in Funk");
    expect(!dev.success, "2.3: Development result must be false");

    // Case B: Excessive density drop (e.g. maxDensityDrop exceeded)
    DevelopmentRequest reqDrop{};
    reqDrop.transformation = TransformationKind::Thin;
    reqDrop.maxDensityDrop = 0; // Strict zero tolerance for drops

    const auto devDrop = developCandidate(source, reqDrop);
    expect(devDrop.classification.genre == GenreResult::Fail,
           "2.3: G4 must reject rhythm candidate when density drop exceeds threshold");
  }

  // -------------------------------------------------------------------------
  // 7. BASS + ARTICULATION VERTICAL: HOLD
  // -------------------------------------------------------------------------
  {
    const auto source = melodyWithTwoNotes(36, 40);
    DevelopmentRequest req{};
    req.transformation = TransformationKind::Hold;

    const auto dev = developCandidate(source, req);
    expect(dev.success, "3.1: Hold development must succeed");
    expect(dev.evidence.bass.durationsExtended, "3.1: Durations must be extended");
    expect(dev.evidence.bass.articulationChanged, "3.1: Articulation changed must be true");
    expect(!dev.evidence.harmony.pitchesChanged, "3.1: Pitches must NOT change in Hold");
    expect(!dev.evidence.rhythm.onsetsChanged, "3.1: Onsets must NOT change in Hold");
    expect(dev.classification.genre == GenreResult::Pass, "3.1: Genre must pass");
    expect(dev.classification.idea == IdeaClassification::Variation, "3.1: Hold must be Variation");
  }

  // -------------------------------------------------------------------------
  // 8. BASS + ARTICULATION VERTICAL: CONNECT
  // -------------------------------------------------------------------------
  {
    const auto source = melodyWithTwoNotes(36, 40);
    DevelopmentRequest req{};
    req.transformation = TransformationKind::Connect;

    const auto dev = developCandidate(source, req);
    expect(dev.success, "3.2: Connect development must succeed");
    expect(dev.evidence.bass.slidesAdded, "3.2: Slides must be added");
    expect(dev.evidence.bass.articulationChanged, "3.2: Articulation changed must be true");
    expect(!dev.evidence.harmony.pitchesChanged, "3.2: Pitches must NOT change in Connect");
    expect(!dev.evidence.rhythm.onsetsChanged, "3.2: Onsets must NOT change in Connect");
    expect((dev.candidate.events[0].flags & PhraseRuntime::kEventSlide) != 0,
           "3.2: Slide flag must be set on event 0");
  }

  // -------------------------------------------------------------------------
  // 9. BASS + ARTICULATION VERTICAL: MOVE
  // -------------------------------------------------------------------------
  {
    const auto source = melodyWithTwoNotes(36, 40);
    DevelopmentRequest req{};
    req.transformation = TransformationKind::Move;
    req.octaveShift = 1;

    const auto dev = developCandidate(source, req);
    expect(dev.success, "3.3: Move development must succeed");
    expect(dev.evidence.harmony.pitchesChanged, "3.3: Pitch must change in Move");
    expect(dev.evidence.bass.articulationChanged, "3.3: Articulation/contour changed");
    expect(!dev.evidence.rhythm.onsetsChanged, "3.3: Onsets must NOT change in Move");
  }

  // -------------------------------------------------------------------------
  // 10. BASS / ARTICULATION WITNESS:
  //     Harmony unchanged, Bass/Articulation structurally changes,
  //     G4 still produces meaningful judgment.
  // -------------------------------------------------------------------------
  {
    const auto source = melodyWithTwoNotes(36, 40);
    DevelopmentRequest req{};
    req.transformation = TransformationKind::Connect;

    const auto dev = developCandidate(source, req);
    expect(!dev.evidence.harmony.pitchesChanged,
           "3.4: Harmony is unchanged (witness condition 1)");
    expect(dev.evidence.bass.articulationChanged,
           "3.4: Bass/Articulation structurally changes (witness condition 2)");
    expect(dev.classification.genre == GenreResult::Pass,
           "3.4: G4 produces meaningful PASS judgment (witness condition 3)");
  }

  // -------------------------------------------------------------------------
  // 11. END-TO-END FRACTAL PIPELINE INTEGRATION:
  //     CURRENT -> REQUEST -> PRIVATE CANDIDATE -> TRANSFORMATION ->
  //     EVIDENCE -> CLASSIFICATION & G4 -> NEXT -> GO -> CURRENT -> DEVELOP AGAIN
  // -------------------------------------------------------------------------
  {
    Fixture fixture;
    const auto a0Basis = fixture.basis(0);

    // Initial melody
    const auto initialMelody = melodyWithTwoNotes(36, 40);
    fixture.engine.prepareNextMelody(0, initialMelody, a0Basis, IdeaClassification::Variation);
    fixture.engine.activateNextMaterialAtBoundary(0);

    const auto a1Basis = fixture.basis(0);
    expect(a1Basis.valid() && a1Basis != a0Basis, "4.1: Initial activation to A1 valid");

    // Cycle 1: Develop A1 via REVOICE -> A2
    DevelopmentRequest req1{};
    req1.transformation = TransformationKind::Revoice;
    req1.degreeShift = 2;
    const auto dev1 = developCandidate(fixture.engine.currentPhraseBuffer(0), req1);
    expect(dev1.success, "4.1: Develop A1 via Revoice succeeded");

    expect(fixture.engine.prepareNextMelody(
               0, dev1.candidate, a1Basis, dev1.classification.idea) ==
               MiniAcid::NextPrepareResult::Prepared,
           "4.1: Prepare A2 as NEXT succeeded");
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "4.1: GO A2 activated");

    const auto a2Basis = fixture.basis(0);
    expect(a2Basis.valid() && a2Basis != a1Basis, "4.1: A2 basis valid and fresh");
    expect(fixture.predecessor(0) == a1Basis, "4.1: Predecessor is A1");
    expect(fixture.sourceAnchor(0) == a0Basis, "4.1: Variation preserves sourceAnchor A0");

    // Cycle 2: Develop A2 via HOLD -> A3
    DevelopmentRequest req2{};
    req2.transformation = TransformationKind::Hold;
    const auto dev2 = developCandidate(fixture.engine.currentPhraseBuffer(0), req2);
    expect(dev2.success, "4.2: Develop A2 via Hold succeeded");

    expect(fixture.engine.prepareNextMelody(
               0, dev2.candidate, a2Basis, dev2.classification.idea) ==
               MiniAcid::NextPrepareResult::Prepared,
           "4.2: Prepare A3 as NEXT succeeded");
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "4.2: GO A3 activated");

    const auto a3Basis = fixture.basis(0);
    expect(fixture.predecessor(0) == a2Basis, "4.2: Predecessor is A2");

    // Cycle 3: Single-slot Undo restores A2
    expect(fixture.engine.undoMaterialWorking(0), "4.3: Undo A3 restores A2");
    expect(fixture.basis(0) == a2Basis, "4.3: Basis restored to A2");

    // Cycle 4: DISCARD restores canonical Pattern A0
    expect(fixture.engine.discardCurrentMaterial(0) == MiniAcid::DiscardResult::Discarded,
           "4.4: Discard restores canonical Pattern");
    expect(fixture.sourceAnchor(0) == a0Basis, "4.4: sourceAnchor reset to canonical A0");
  }

  // -------------------------------------------------------------------------
  // 12. FRACTAL NODE D — MATERIAL GROWTH (REPEAT / DEVELOP)
  // -------------------------------------------------------------------------
  {
    const auto source = melodyWithTwoNotes(36, 40); // 1 bar (384 ticks), 2 notes

    // REPEAT: A A (2 bars, identical content repeated)
    DevelopmentRequest reqRepeat{};
    const auto resRepeat = growMaterial(source, 2, GrowthMode::Repeat, reqRepeat);
    expect(resRepeat.success, "5.1: Grow Repeat 2 bars must succeed");
    expect(resRepeat.candidate.lengthTicks == 2 * PhraseRuntime::kTicksPerBar,
           "5.1: Target length must be 2 bars (768 ticks)");
    expect(resRepeat.candidate.count == 4, "5.1: Repeat must duplicate events to 4");
    expect(resRepeat.candidate.events[0].startTick == 0, "5.1: Bar 0 event 0 at tick 0");
    expect(resRepeat.candidate.events[2].startTick == 384, "5.1: Bar 1 event 0 at tick 384");
    expect(resRepeat.candidate.events[2].note == source.events[0].note,
           "5.1: Repeated note must match source note");

    // DEVELOP: A A' (2 bars, Bar 1 is developed variation)
    DevelopmentRequest reqDevelop{};
    reqDevelop.transformation = TransformationKind::Revoice;
    reqDevelop.degreeShift = 2;
    const auto resDevelop = growMaterial(source, 2, GrowthMode::Develop, reqDevelop);
    expect(resDevelop.success, "5.2: Grow Develop 2 bars must succeed");
    expect(resDevelop.candidate.lengthTicks == 2 * PhraseRuntime::kTicksPerBar,
           "5.2: Target length must be 2 bars");
    expect(resDevelop.candidate.count == 4, "5.2: Develop must produce 4 events");
    expect(resDevelop.candidate.events[0].note == source.events[0].note,
           "5.2: Bar 0 must retain original note");
    expect(resDevelop.candidate.events[2].note != source.events[0].note,
           "5.2: Bar 1 must contain developed variation A'");
    expect(resDevelop.candidate.events[2].startTick == 384,
           "5.2: Bar 1 event starts at tick 384");

    // Negative witness: G4 rejection during develop growth fails closed
    DevelopmentRequest reqG4Fail{};
    reqG4Fail.transformation = TransformationKind::Displace;
    reqG4Fail.genreId = static_cast<uint8_t>(GenerativeMode::FunkSoul);
    reqG4Fail.forceDisplaceTheOne = true;
    const auto resFail = growMaterial(source, 2, GrowthMode::Develop, reqG4Fail);
    expect(!resFail.success, "5.3: Growth must fail closed if variation violates G4");
    expect(resFail.classification.genre == GenreResult::Fail,
           "5.3: G4 failure must be recorded");

    // Invalid bar length: 3 bars is not supported (only 1, 2, 4, 8)
    const auto resInvalid = growMaterial(source, 3, GrowthMode::Repeat, reqRepeat);
    expect(!resInvalid.success, "5.4: Non-power-of-two bar length must be rejected");
  }

  // -------------------------------------------------------------------------
  // 13. FRACTAL NODE E — PROVENANCE / EXPLANATION
  // -------------------------------------------------------------------------
  {
    Fixture fixture;
    const auto a0Basis = fixture.basis(0);

    const auto source = melodyWithTwoNotes(36, 40);
    DevelopmentRequest req{};
    req.transformation = TransformationKind::Revoice;
    req.degreeShift = 2;

    const auto dev = developCandidate(source, req);
    expect(dev.success, "6.1: Develop candidate succeeded");

    DevelopmentProvenance prov{};
    prov.requestKind = req.transformation;
    prov.growthMode = GrowthMode::None;
    prov.sourceBasis = a0Basis;
    prov.evidence = dev.evidence;
    prov.classification = dev.classification;

    expect(prov.valid(), "6.1: Provenance must be valid");

    char buf[512]{};
    formatProvenance(prov, buf, sizeof(buf));
    expect(std::strstr(buf, "REQUEST: REVOICE") != nullptr, "6.2: Must show requested transformation");
    expect(std::strstr(buf, "pitch=CHANGED") != nullptr, "6.2: Must show pitch changed");
    expect(std::strstr(buf, "onset=UNCHANGED") != nullptr, "6.2: Must show onset unchanged");
    expect(std::strstr(buf, "idea=VARIATION") != nullptr, "6.2: Must show idea classification");
    expect(std::strstr(buf, "genre=PASS") != nullptr, "6.2: Must show genre result");
    expect(std::strstr(buf, "temporal=UNKNOWN") != nullptr, "6.2: Must preserve temporal=UNKNOWN");
  }

  // -------------------------------------------------------------------------
  // 14. PRODUCTION ENTRYPOINTS: developWorkingMaterial & growWorkingMaterial
  // -------------------------------------------------------------------------
  {
    Fixture fixture;
    // Set up voice 0 with a working melody
    const auto source = melodyWithTwoNotes(36, 40);
    const auto a0Basis = fixture.basis(0);
    fixture.engine.prepareNextMelody(0, source, a0Basis, IdeaClassification::Variation);
    fixture.engine.activateNextMaterialAtBoundary(0);

    // 14.1 developWorkingMaterial directly from production engine API
    DevelopmentRequest req{};
    req.transformation = TransformationKind::Revoice;
    req.octaveShift = 1;

    DevelopmentResult devResult{};
    const auto prep = fixture.engine.developWorkingMaterial(0, req, &devResult);
    expect(prep == MiniAcid::NextPrepareResult::Prepared, "7.1: developWorkingMaterial must prepare NEXT");
    expect(fixture.engine.hasPendingMaterial(0), "7.1: Voice 0 must have pending material");
    expect(devResult.success, "7.1: Development result must be success");
    expect(devResult.classification.idea == IdeaClassification::Variation, "7.1: Idea must be Variation");

    // Activate developed candidate at boundary (GO)
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "7.1: Activate developed candidate must succeed");
    expect(!fixture.engine.hasPendingMaterial(0), "7.1: Pending must be cleared after GO");

    // 14.2 growWorkingMaterial directly from production engine API
    const auto growPrep = fixture.engine.growWorkingMaterial(
        0, 2, GrowthMode::Develop, req, &devResult);
    expect(growPrep == MiniAcid::NextPrepareResult::Prepared, "7.2: growWorkingMaterial must prepare NEXT 2B");
    expect(fixture.engine.hasPendingMaterial(0), "7.2: Voice 0 must have pending grown material");
    expect(fixture.engine.activateNextMaterialAtBoundary(0) ==
               MiniAcid::NextActivationResult::Activated,
           "7.2: Activate grown candidate must succeed");
  }

  if (g_failures == 0) {
    std::printf("M2 musical development intelligence: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "M2 musical development intelligence: %d failure(s)\n", g_failures);
  return 1;
}
