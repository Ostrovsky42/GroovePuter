#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/composition/rhythm_selection.h"
#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/roles/bass_rhythm.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint16_t kIdentityCount = 128;
constexpr StepMask kQuarterNotes =
    stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12);

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = recipe;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

// G4-R1: this is a DnB genre-identity contract, not a universal statement that
// every explicit bass request must belong to a RhythmFamily's Auto vocabulary.
// Acid is the counterexample: its profile intentionally has independent bass
// motion across FourFloor / MachineSyncopation / SparsePulse archetypes.
bool dnbBassIdentityAllowed(BassRhythmId bass) {
  return bass == BassRhythmId::KickAnswer ||
         bass == BassRhythmId::GapFill ||
         bass == BassRhythmId::HalfTimePocket ||
         bass == BassRhythmId::SyncopatedHook;
}

const LaneGrammar* kickLane(const RhythmArchetype& archetype) {
  for (uint8_t lane = 0; lane < archetype.laneCount; ++lane) {
    if (archetype.lanes[lane].role == RhythmRole::Kick) {
      return &archetype.lanes[lane];
    }
  }
  return nullptr;
}

bool hasQuarterNoteKickSkeleton(RhythmArchetypeId archetypeId) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(archetypeId);
  if (definition == nullptr) return false;
  const RhythmArchetype* archetype =
      ReferenceVocabulary::archetypeFor(definition->key);
  if (archetype == nullptr) return false;
  const LaneGrammar* kick = kickLane(*archetype);
  if (kick == nullptr) return false;
  const StepMask structuralAnchors = static_cast<StepMask>(
      kick->immutableAnchors | kick->canonicalAnchors);
  return (structuralAnchors & kQuarterNotes) == kQuarterNotes;
}

bool checkDnbBassCoherence() {
  const GenreSettings settings =
      settingsFor(GenerativeMode::DrumAndBass, kBaseRecipeId);
  bool seenBass[static_cast<uint8_t>(BassRhythmId::Count)]{};
  uint8_t distinctBass = 0;
  uint16_t violations = 0;

  for (uint16_t identity = 1; identity <= kIdentityCount; ++identity) {
    const GenerationContext generation{0x47112000u, identity};
    const GenerationCompositionResult composition =
        resolveGenerationComposition(settings, generation);
    if (composition.status != GenerationCompositionStatus::Ok) {
      std::printf(
          "G4_R1_FAIL R1_DNB_RESOLUTION identity=%u status=%u\n",
          identity, static_cast<unsigned>(composition.status));
      return false;
    }

    const ReferenceVocabulary::Definition* definition =
        ReferenceVocabulary::definitionForId(composition.rhythmArchetypeId);
    if (definition == nullptr || definition->family != RhythmFamily::Breakbeat) {
      std::printf(
          "G4_R1_FAIL R1_DNB_ARCHETYPE identity=%u archetype=%u family=%u\n",
          identity, composition.rhythmArchetypeId,
          definition == nullptr ? 255u
                                : static_cast<unsigned>(definition->family));
      return false;
    }

    const uint8_t bassIndex = static_cast<uint8_t>(composition.bassRhythm);
    if (bassIndex < static_cast<uint8_t>(BassRhythmId::Count) &&
        !seenBass[bassIndex]) {
      seenBass[bassIndex] = true;
      ++distinctBass;
    }

    if (!dnbBassIdentityAllowed(composition.bassRhythm)) {
      if (violations < 8) {
        std::printf(
            "G4_R1_WITNESS R1_DNB_BASS_IDENTITY_INCOHERENCE identity=%u "
            "archetype=%u bass=%u\n",
            identity, composition.rhythmArchetypeId,
            static_cast<unsigned>(composition.bassRhythm));
      }
      ++violations;
    }
  }

  std::printf(
      "G4_R1_DNB identities=%u violations=%u distinct_bass=%u\n",
      kIdentityCount, violations, distinctBass);

  if (violations != 0) {
    std::printf("G4_R1_FAIL R1_DNB_BASS_IDENTITY_INCOHERENCE\n");
    return false;
  }
  if (distinctBass < 2) {
    std::printf("G4_R1_FAIL R1_DNB_BASS_COLLAPSE distinct_bass=%u\n",
                distinctBass);
    return false;
  }

  std::printf("G4_R1_PASS dnb_bass_identity_coherence\n");
  return true;
}

bool checkDubTechnoCandidateSpace() {
  const GenreSettings settings = settingsFor(GenerativeMode::Reggae, 5);
  const RhythmCompatibilityView compatibility = rhythmCompatibilityFor(settings);
  if (compatibility.candidates == nullptr || compatibility.count < 2) {
    std::printf("G4_R1_FAIL R1_DUB_TECHNO_EMPTY_SPACE count=%u\n",
                compatibility.count);
    return false;
  }

  uint8_t technoSkeletonCandidates = 0;
  for (uint8_t index = 0; index < compatibility.count; ++index) {
    const RhythmCompatibilityCandidate candidate = compatibility.candidates[index];
    const bool technoSkeleton = hasQuarterNoteKickSkeleton(candidate.archetypeId);
    if (technoSkeleton) ++technoSkeletonCandidates;
    std::printf(
        "G4_R1_DUB_CANDIDATE archetype=%u weight=%u quarter_skeleton=%u\n",
        candidate.archetypeId, candidate.weight, technoSkeleton ? 1u : 0u);
  }

  std::printf(
      "G4_R1_DUB candidates=%u quarter_skeleton_candidates=%u\n",
      compatibility.count, technoSkeletonCandidates);

  // Do not canonicalize Dub Techno into one four-floor pattern. The first G4
  // contract merely requires a plural techno-skeleton choice in the candidate
  // space; the existing dub/space archetypes remain available for evaluation.
  if (technoSkeletonCandidates < 2) {
    std::printf("G4_R1_FAIL R1_DUB_TECHNO_SKELETON\n");
    return false;
  }

  std::printf("G4_R1_PASS dub_techno_candidate_space\n");
  return true;
}

bool checkReferenceGenreReachability() {
  const GenreSettings acid = settingsFor(GenerativeMode::Acid, kBaseRecipeId);
  const GenreSettings house = settingsFor(GenerativeMode::House, kBaseRecipeId);
  const uint8_t acidCount = compatibleRhythmCount(acid);
  const uint8_t houseCount = compatibleRhythmCount(house);

  std::printf("G4_R1_REFERENCE acid_archetypes=%u house_archetypes=%u\n",
              acidCount, houseCount);

  // G4-I5 removes one unowned House idea while preserving four distinct
  // quarter-pulse archetypes. Reachability protects plurality, not the obsolete
  // pre-ownership cardinality of five candidates.
  if (acidCount < 4 || houseCount < 4) {
    std::printf("G4_R1_FAIL R1_REFERENCE_SPACE_COLLAPSE\n");
    return false;
  }

  std::printf("G4_R1_PASS reference_genre_reachability\n");
  return true;
}

}  // namespace

int main() {
  bool ok = true;
  ok = checkDnbBassCoherence() && ok;
  ok = checkDubTechnoCandidateSpace() && ok;
  ok = checkReferenceGenreReachability() && ok;

  if (!ok) {
    std::puts("G4-R1 reference genre contracts: FAIL");
    return 1;
  }

  std::puts("G4-R1 reference genre contracts: PASS");
  return 0;
}
