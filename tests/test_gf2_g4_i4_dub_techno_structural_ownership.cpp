#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/composition/rhythm_selection.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint16_t kIdentityCount = 128;
constexpr StepMask kQuarterNotes =
    stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12);
constexpr StepMask kBrokenKickFrame = stepBit(0) | stepBit(10);
constexpr StepMask kBackbeatFrame = stepBit(4) | stepBit(12);
constexpr StepMask kDubOffbeats =
    stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14);

enum class TechnoSkeletonWitness : uint8_t {
  None = 0,
  QuarterPulse = 1,
  BrokenFrame = 2,
};

const char* witnessName(TechnoSkeletonWitness witness) {
  switch (witness) {
    case TechnoSkeletonWitness::QuarterPulse: return "quarter-pulse";
    case TechnoSkeletonWitness::BrokenFrame: return "broken-frame";
    case TechnoSkeletonWitness::None:
    default: return "none";
  }
}

GenreSettings dubTechnoSettings() {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::Reggae);
  settings.recipe = 5;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

const LaneGrammar* laneFor(const RhythmArchetype& archetype, RhythmRole role) {
  for (uint8_t lane = 0; lane < archetype.laneCount; ++lane) {
    if (archetype.lanes[lane].role == role) return &archetype.lanes[lane];
  }
  return nullptr;
}

StepMask structuralAnchors(const LaneGrammar* lane) {
  if (lane == nullptr) return 0;
  return static_cast<StepMask>(lane->immutableAnchors | lane->canonicalAnchors);
}

TechnoSkeletonWitness witnessForMasks(StepMask kick, StepMask backbeat) {
  if ((kick & kQuarterNotes) == kQuarterNotes) {
    return TechnoSkeletonWitness::QuarterPulse;
  }
  if ((kick & kBrokenKickFrame) == kBrokenKickFrame &&
      (backbeat & kBackbeatFrame) == kBackbeatFrame &&
      (kick & kBackbeatFrame) == 0) {
    return TechnoSkeletonWitness::BrokenFrame;
  }
  return TechnoSkeletonWitness::None;
}

TechnoSkeletonWitness witnessForArchetype(RhythmArchetypeId archetypeId) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(archetypeId);
  if (definition == nullptr) return TechnoSkeletonWitness::None;
  const RhythmArchetype* archetype =
      ReferenceVocabulary::archetypeFor(definition->key);
  if (archetype == nullptr) return TechnoSkeletonWitness::None;
  return witnessForMasks(
      structuralAnchors(laneFor(*archetype, RhythmRole::Kick)),
      structuralAnchors(laneFor(*archetype, RhythmRole::Backbeat)));
}

bool hasDubChordDialogue(RhythmArchetypeId archetypeId) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(archetypeId);
  if (definition == nullptr) return false;
  const RhythmArchetype* archetype =
      ReferenceVocabulary::archetypeFor(definition->key);
  if (archetype == nullptr) return false;
  const StepMask chord =
      structuralAnchors(laneFor(*archetype, RhythmRole::ChordRhythm));
  return (chord & kDubOffbeats) == kDubOffbeats;
}

StepMask drumOnsets(const DrumPatternSet& drums, uint8_t voice) {
  StepMask result = 0;
  for (uint8_t step = 0; step < DrumPattern::kSteps; ++step) {
    if (drums.voices[voice].steps[step].hit) {
      result = static_cast<StepMask>(result | stepBit(step));
    }
  }
  return result;
}

StrongRhythmMigrationContext contextFor(RealizationLevel level) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = 0;
  context.level = level;
  context.generationAttemptOrdinal = 0;
  context.feelProfile = FeelProfileId::Straight;
  context.feelAmount = 0;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = 0;
  context.scaleTypeValue = kScaleDorian;
  return context;
}

bool checkAdmittedIdeaOwnership() {
  const GenreSettings settings = dubTechnoSettings();
  const RhythmCompatibilityView compatibility = rhythmCompatibilityFor(settings);
  if (compatibility.candidates == nullptr || compatibility.count < 2) {
    std::printf("G4_I4_FAIL I4_DUB_EMPTY_ADMISSION count=%u\n",
                compatibility.count);
    return false;
  }

  uint8_t violations = 0;
  uint8_t witnessKinds = 0;
  uint8_t dubDialogueCandidates = 0;

  for (uint8_t index = 0; index < compatibility.count; ++index) {
    const RhythmCompatibilityCandidate candidate = compatibility.candidates[index];
    const TechnoSkeletonWitness witness =
        witnessForArchetype(candidate.archetypeId);
    const bool dubDialogue = hasDubChordDialogue(candidate.archetypeId);
    if (witness == TechnoSkeletonWitness::None) ++violations;
    if (witness != TechnoSkeletonWitness::None) {
      witnessKinds = static_cast<uint8_t>(
          witnessKinds | (1u << static_cast<uint8_t>(witness)));
    }
    if (dubDialogue) ++dubDialogueCandidates;

    std::printf(
        "G4_I4_DUB_CANDIDATE archetype=%u weight=%u witness=%s dub_chord_dialogue=%u\n",
        candidate.archetypeId, candidate.weight, witnessName(witness),
        dubDialogue ? 1u : 0u);
  }

  const bool hasQuarter =
      (witnessKinds & (1u << static_cast<uint8_t>(
                           TechnoSkeletonWitness::QuarterPulse))) != 0;
  const bool hasBroken =
      (witnessKinds & (1u << static_cast<uint8_t>(
                           TechnoSkeletonWitness::BrokenFrame))) != 0;

  std::printf(
      "G4_I4_DUB_ADMISSION candidates=%u violations=%u quarter=%u broken=%u dub_dialogue=%u\n",
      compatibility.count, violations, hasQuarter ? 1u : 0u,
      hasBroken ? 1u : 0u, dubDialogueCandidates);

  if (violations != 0) {
    std::puts("G4_I4_FAIL I4_DUB_UNOWNED_IDEA");
    return false;
  }
  if (!hasQuarter || !hasBroken) {
    std::puts("G4_I4_FAIL I4_DUB_TECHNO_SKELETON_COLLAPSE");
    return false;
  }
  if (dubDialogueCandidates == 0) {
    std::puts("G4_I4_FAIL I4_DUB_RELATIONSHIP_COLLAPSE");
    return false;
  }

  std::puts("G4_I4_PASS dub_techno_admission_ownership");
  return true;
}

bool checkMaterializedOwnership() {
  const GenreSettings settings = dubTechnoSettings();
  constexpr RealizationLevel levels[] = {
      RealizationLevel::P1Canonical,
      RealizationLevel::P2Variation,
      RealizationLevel::P3Transformation,
  };

  uint16_t ready = 0;
  uint16_t violations = 0;
  uint8_t observedWitnessKinds = 0;

  for (uint16_t identity = 1; identity <= kIdentityCount; ++identity) {
    for (RealizationLevel level : levels) {
      const StrongRhythmMigrationContext context = contextFor(level);
      StrongRhythmFrozenSelection selection{};
      const StrongRhythmMigrationResult selected =
          resolveStrongRhythmFrozenSelection(
              settings, context, identity, selection);
      if (selected.status != StrongRhythmMigrationStatus::Applied ||
          !selection.resolved) {
        std::printf(
            "G4_I4_FAIL I4_DUB_RESOLUTION identity=%u level=%u status=%u\n",
            identity, static_cast<unsigned>(level),
            static_cast<unsigned>(selected.status));
        return false;
      }

      DrumPatternSet drums{};
      SynthPattern synthA{};
      SynthPattern synthB{};
      const StrongRhythmMigrationResult materialized =
          migrateStrongRhythmFrozenMaterial(
              settings, selection, context, drums, synthA, synthB);
      if (materialized.status != StrongRhythmMigrationStatus::Applied) {
        std::printf(
            "G4_I4_FAIL I4_DUB_MATERIALIZE identity=%u level=%u status=%u\n",
            identity, static_cast<unsigned>(level),
            static_cast<unsigned>(materialized.status));
        return false;
      }
      ++ready;

      const TechnoSkeletonWitness expected =
          witnessForArchetype(selection.composition.rhythmArchetypeId);
      const TechnoSkeletonWitness actual = witnessForMasks(
          drumOnsets(drums, KICK), drumOnsets(drums, SNARE));
      if (actual != TechnoSkeletonWitness::None) {
        observedWitnessKinds = static_cast<uint8_t>(
            observedWitnessKinds | (1u << static_cast<uint8_t>(actual)));
      }

      if (expected == TechnoSkeletonWitness::None || actual != expected) {
        if (violations < 12) {
          std::printf(
              "G4_I4_WITNESS I4_DUB_MATERIALIZED_OWNERSHIP identity=%u level=%u archetype=%u expected=%s actual=%s kick=%04x backbeat=%04x\n",
              identity, static_cast<unsigned>(level),
              selection.composition.rhythmArchetypeId,
              witnessName(expected), witnessName(actual),
              drumOnsets(drums, KICK), drumOnsets(drums, SNARE));
        }
        ++violations;
      }
    }
  }

  const bool observedQuarter =
      (observedWitnessKinds & (1u << static_cast<uint8_t>(
                                   TechnoSkeletonWitness::QuarterPulse))) != 0;
  const bool observedBroken =
      (observedWitnessKinds & (1u << static_cast<uint8_t>(
                                   TechnoSkeletonWitness::BrokenFrame))) != 0;

  std::printf(
      "G4_I4_DUB_MATERIALIZED ready=%u violations=%u quarter=%u broken=%u\n",
      ready, violations, observedQuarter ? 1u : 0u,
      observedBroken ? 1u : 0u);

  if (ready != static_cast<uint16_t>(kIdentityCount * 3u) ||
      violations != 0) {
    std::puts("G4_I4_FAIL I4_DUB_MATERIALIZED_OWNERSHIP");
    return false;
  }
  if (!observedQuarter || !observedBroken) {
    std::puts("G4_I4_FAIL I4_DUB_MATERIALIZED_SKELETON_COLLAPSE");
    return false;
  }

  std::puts("G4_I4_PASS dub_techno_materialized_ownership");
  return true;
}

}  // namespace

int main() {
  bool ok = true;
  ok = checkAdmittedIdeaOwnership() && ok;
  ok = checkMaterializedOwnership() && ok;

  if (!ok) {
    std::puts("G4-I4 Dub Techno structural ownership: FAIL");
    return 1;
  }

  std::puts("G4-I4 Dub Techno structural ownership: PASS");
  return 0;
}
