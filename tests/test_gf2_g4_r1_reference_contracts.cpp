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

bool expectedBassFamilyCompatibility(BassRhythmId bass, RhythmFamily family) {
  switch (family) {
    case RhythmFamily::FourFloor:
      return bass == BassRhythmId::KickLock ||
             bass == BassRhythmId::OffbeatPush ||
             bass == BassRhythmId::RollingDrive ||
             bass == BassRhythmId::SustainAndDrop;
    case RhythmFamily::MachineSyncopation:
      return bass == BassRhythmId::KickAnswer ||
             bass == BassRhythmId::GapFill ||
             bass == BassRhythmId::SyncopatedHook ||
             bass == BassRhythmId::RollingDrive;
    case RhythmFamily::Breakbeat:
    case RhythmFamily::UkTwoStep:
      return bass == BassRhythmId::KickAnswer ||
             bass == BassRhythmId::GapFill ||
             bass == BassRhythmId::HalfTimePocket ||
             bass == BassRhythmId::SyncopatedHook;
    case RhythmFamily::HipHopBackbeat:
      return bass == BassRhythmId::RootPulse ||
             bass == BassRhythmId::KickAnswer ||
             bass == BassRhythmId::SparseAnchor ||
             bass == BassRhythmId::HalfTimePocket ||
             bass == BassRhythmId::SustainAndDrop;
    case RhythmFamily::DubPulse:
      return bass == BassRhythmId::RootPulse ||
             bass == BassRhythmId::GapFill ||
             bass == BassRhythmId::SparseAnchor ||
             bass == BassRhythmId::SustainAndDrop;
    case RhythmFamily::Funk16:
      return bass == BassRhythmId::KickLock ||
             bass == BassRhythmId::KickAnswer ||
             bass == BassRhythmId::GapFill ||
             bass == BassRhythmId::SyncopatedHook;
    case RhythmFamily::SparsePulse:
      return bass == BassRhythmId::RootPulse ||
             bass == BassRhythmId::SparseAnchor ||
             bass == BassRhythmId::SustainAndDrop;
    case RhythmFamily::Count:
      return false;
  }
  return false;
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

bool checkExplicitBassFamilyBoundary() {
  BassRhythmRequest incompatible{};
  incompatible.requestedId = BassRhythmId::RollingDrive;
  incompatible.family = RhythmFamily::Breakbeat;
  incompatible.archetypeId = 413;
  incompatible.kickOnsets = stepBit(0) | stepBit(6) | stepBit(10);
  incompatible.generation = GenerationContext{0x47110001u, 7};

  const BassRhythmResult rejected = realizeBassRhythm(incompatible);
  if (rejected.status != BassRhythmStatus::InvalidRequest) {
    std::printf(
        "G4_R1_FAIL R1_BASS_EXPLICIT_FAMILY_BYPASS family=Breakbeat "
        "requested=RollingDrive status=%u\n",
        static_cast<unsigned>(rejected.status));
    return false;
  }

  BassRhythmRequest compatible = incompatible;
  compatible.requestedId = BassRhythmId::HalfTimePocket;
  const BassRhythmResult accepted = realizeBassRhythm(compatible);
  if (accepted.status != BassRhythmStatus::Ok ||
      accepted.plan.id != BassRhythmId::HalfTimePocket) {
    std::printf(
        "G4_R1_FAIL R1_BASS_LEGAL_EXPLICIT_REJECTED family=Breakbeat "
        "requested=HalfTimePocket status=%u\n",
        static_cast<unsigned>(accepted.status));
    return false;
  }

  std::printf("G4_R1_PASS explicit_bass_family_boundary\n");
  return true;
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
    if (definition == nullptr) {
      std::printf(
          "G4_R1_FAIL R1_DNB_ARCHETYPE identity=%u archetype=%u\n",
          identity, composition.rhythmArchetypeId);
      return false;
    }

    const uint8_t bassIndex = static_cast<uint8_t>(composition.bassRhythm);
    if (bassIndex < static_cast<uint8_t>(BassRhythmId::Count) &&
        !seenBass[bassIndex]) {
      seenBass[bassIndex] = true;
      ++distinctBass;
    }

    if (!expectedBassFamilyCompatibility(composition.bassRhythm,
                                         definition->family)) {
      if (violations < 8) {
        std::printf(
            "G4_R1_WITNESS R1_DNB_BASS_FAMILY_INCOHERENCE identity=%u "
            "archetype=%u family=%u bass=%u\n",
            identity, composition.rhythmArchetypeId,
            static_cast<unsigned>(definition->family),
            static_cast<unsigned>(composition.bassRhythm));
      }
      ++violations;
    }
  }

  std::printf(
      "G4_R1_DNB identities=%u violations=%u distinct_bass=%u\n",
      kIdentityCount, violations, distinctBass);

  if (violations != 0) {
    std::printf("G4_R1_FAIL R1_DNB_BASS_FAMILY_INCOHERENCE\n");
    return false;
  }
  if (distinctBass < 2) {
    std::printf("G4_R1_FAIL R1_DNB_BASS_COLLAPSE distinct_bass=%u\n",
                distinctBass);
    return false;
  }

  std::printf("G4_R1_PASS dnb_bass_family_coherence\n");
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

  // G4-R1 does not force every Dub Techno idea to be canonical four-floor.
  // It requires more than one structural techno-skeleton statement in the
  // candidate space so dub vocabulary is layered over a real techno choice,
  // rather than Steppers being the sole structural witness.
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

  if (acidCount < 4 || houseCount < 5) {
    std::printf("G4_R1_FAIL R1_REFERENCE_SPACE_COLLAPSE\n");
    return false;
  }

  std::printf("G4_R1_PASS reference_genre_reachability\n");
  return true;
}

}  // namespace

int main() {
  bool ok = true;
  ok = checkExplicitBassFamilyBoundary() && ok;
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
