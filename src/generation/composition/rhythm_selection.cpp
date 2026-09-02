#include "rhythm_selection.h"

#include <cstddef>

#include "../../../scenes.h"
#include "../../dsp/genre_manager.h"

namespace GroovePuterRhythm {
namespace {

using Archetype = ReferenceVocabulary::Archetype;
constexpr uint8_t kMaxCandidates =
    static_cast<uint8_t>(ReferenceVocabulary::Archetype::Count);

constexpr RhythmArchetypeId id(Archetype archetype) {
  switch (archetype) {
    case Archetype::StraightDrive: return 401;
    case Archetype::OffbeatOpenHat: return 402;
    case Archetype::HypnoticSparse: return 403;
    case Archetype::BrokenTechno: return 404;
    case Archetype::StraightAcid: return 405;
    case Archetype::RollingAcid: return 406;
    case Archetype::SyncopatedAcid: return 407;
    case Archetype::SparseAcid: return 408;
    case Archetype::OneDropSpace: return 409;
    case Archetype::Steppers: return 410;
    case Archetype::SparseSkank: return 411;
    case Archetype::ChordResponse: return 412;
    case Archetype::TwoStepRoll: return 413;
    case Archetype::GhostedRoll: return 414;
    case Archetype::SparseFastBreak: return 415;
    case Archetype::HalftimeSwitch: return 416;
    case Archetype::ClassicTwoStep: return 417;
    case Archetype::SkippyTwoStep: return 418;
    case Archetype::ShuffledFourFour: return 419;
    case Archetype::MachineSyncopation: return 420;
    case Archetype::StackedQuarters: return 711;
    case Archetype::ElectroBackskip: return 712;
    case Archetype::FunkHouseBridge: return 713;
    case Archetype::ElectroGapPush: return 714;
    case Archetype::Count:
    default: return kNoArchetypeId;
  }
}

constexpr RhythmCompatibilityCandidate candidate(Archetype archetype,
                                                  uint8_t weight = 100) {
  return RhythmCompatibilityCandidate{id(archetype), weight};
}

template <size_t N>
constexpr RhythmCompatibilityView view(
    const RhythmCompatibilityCandidate (&candidates)[N]) {
  return RhythmCompatibilityView{candidates, static_cast<uint8_t>(N)};
}

constexpr RhythmCompatibilityCandidate kAcidBase[] = {
    candidate(Archetype::StraightAcid, 120), candidate(Archetype::RollingAcid, 110),
    candidate(Archetype::SyncopatedAcid, 100), candidate(Archetype::SparseAcid, 90),
};
constexpr RhythmCompatibilityCandidate kSynthwaveBase[] = {
    candidate(Archetype::StraightDrive, 90), candidate(Archetype::HypnoticSparse, 120),
    candidate(Archetype::StackedQuarters, 110), candidate(Archetype::FunkHouseBridge, 100),
};
constexpr RhythmCompatibilityCandidate kDarksynthBase[] = {
    candidate(Archetype::StraightDrive, 120), candidate(Archetype::OffbeatOpenHat, 110),
    candidate(Archetype::HypnoticSparse, 100), candidate(Archetype::BrokenTechno, 100),
    candidate(Archetype::StackedQuarters, 80),
};
constexpr RhythmCompatibilityCandidate kElectroBase[] = {
    candidate(Archetype::BrokenTechno, 80), candidate(Archetype::MachineSyncopation, 120),
    candidate(Archetype::ElectroBackskip, 120), candidate(Archetype::ElectroGapPush, 110),
};
constexpr RhythmCompatibilityCandidate kRaveBase[] = {
    candidate(Archetype::StraightDrive, 120), candidate(Archetype::OffbeatOpenHat, 110),
    candidate(Archetype::BrokenTechno, 90), candidate(Archetype::ShuffledFourFour, 80),
};
constexpr RhythmCompatibilityCandidate kReggaeBase[] = {
    candidate(Archetype::OneDropSpace, 120), candidate(Archetype::Steppers, 110),
    candidate(Archetype::SparseSkank, 110), candidate(Archetype::ChordResponse, 100),
};
constexpr RhythmCompatibilityCandidate kTripHopBase[] = {
    candidate(Archetype::SparseFastBreak, 120), candidate(Archetype::HalftimeSwitch, 130),
    candidate(Archetype::FunkHouseBridge, 90), candidate(Archetype::ElectroGapPush, 75),
};
constexpr RhythmCompatibilityCandidate kBrokenBase[] = {
    candidate(Archetype::TwoStepRoll, 100), candidate(Archetype::GhostedRoll, 100),
    candidate(Archetype::SparseFastBreak, 90), candidate(Archetype::HalftimeSwitch, 90),
    candidate(Archetype::ClassicTwoStep, 110), candidate(Archetype::SkippyTwoStep, 110),
    candidate(Archetype::ShuffledFourFour, 90), candidate(Archetype::MachineSyncopation, 80),
};
constexpr RhythmCompatibilityCandidate kChipBase[] = {
    candidate(Archetype::MachineSyncopation, 120), candidate(Archetype::ElectroBackskip, 100),
    candidate(Archetype::ElectroGapPush, 90), candidate(Archetype::StackedQuarters, 80),
};

// Stage 14 uses only repository-approved production identities. Pending
// HARD_02/HARD_04/HARD_05 never appear in these compatibility edges.
constexpr RhythmCompatibilityCandidate kHouseBase[] = {
    candidate(Archetype::StraightDrive, 90), candidate(Archetype::OffbeatOpenHat, 90),
    candidate(Archetype::StackedQuarters, 125), candidate(Archetype::FunkHouseBridge, 130),
    candidate(Archetype::ShuffledFourFour, 70),
};
constexpr RhythmCompatibilityCandidate kTechnoBase[] = {
    candidate(Archetype::StraightDrive, 120), candidate(Archetype::OffbeatOpenHat, 110),
    candidate(Archetype::HypnoticSparse, 105), candidate(Archetype::BrokenTechno, 105),
    candidate(Archetype::MachineSyncopation, 90),
};
constexpr RhythmCompatibilityCandidate kHipHopBase[] = {
    candidate(Archetype::HalftimeSwitch, 125), candidate(Archetype::SparseFastBreak, 105),
    candidate(Archetype::ElectroBackskip, 90), candidate(Archetype::ElectroGapPush, 110),
    candidate(Archetype::FunkHouseBridge, 85),
};
constexpr RhythmCompatibilityCandidate kFunkSoulBase[] = {
    candidate(Archetype::FunkHouseBridge, 150), candidate(Archetype::HalftimeSwitch, 90),
    candidate(Archetype::SparseFastBreak, 75),
};
constexpr RhythmCompatibilityCandidate kUkGarageBase[] = {
    candidate(Archetype::ClassicTwoStep, 125), candidate(Archetype::SkippyTwoStep, 120),
    candidate(Archetype::ShuffledFourFour, 105), candidate(Archetype::MachineSyncopation, 75),
};
constexpr RhythmCompatibilityCandidate kDrumAndBassBase[] = {
    candidate(Archetype::TwoStepRoll, 125), candidate(Archetype::GhostedRoll, 115),
    candidate(Archetype::SparseFastBreak, 105), candidate(Archetype::HalftimeSwitch, 95),
};
constexpr RhythmCompatibilityCandidate kLoFiBase[] = {
    candidate(Archetype::HalftimeSwitch, 135), candidate(Archetype::SparseFastBreak, 115),
    candidate(Archetype::FunkHouseBridge, 95), candidate(Archetype::ElectroGapPush, 65),
    candidate(Archetype::HypnoticSparse, 55),
};

constexpr RhythmCompatibilityCandidate kUkGarage[] = {
    candidate(Archetype::ClassicTwoStep, 120), candidate(Archetype::SkippyTwoStep, 110),
    candidate(Archetype::ShuffledFourFour, 100), candidate(Archetype::MachineSyncopation, 80),
};
constexpr RhythmCompatibilityCandidate kDrumAndBass[] = {
    candidate(Archetype::TwoStepRoll, 120), candidate(Archetype::GhostedRoll, 110),
    candidate(Archetype::SparseFastBreak, 100), candidate(Archetype::HalftimeSwitch, 90),
};
constexpr RhythmCompatibilityCandidate kFootwork[] = {
    candidate(Archetype::SparseFastBreak, 90), candidate(Archetype::HalftimeSwitch, 100),
    candidate(Archetype::MachineSyncopation, 120), candidate(Archetype::ElectroGapPush, 110),
};
constexpr RhythmCompatibilityCandidate kPsytrance[] = {
    candidate(Archetype::StraightDrive, 120), candidate(Archetype::OffbeatOpenHat, 110),
    candidate(Archetype::RollingAcid, 100),
};
constexpr RhythmCompatibilityCandidate kDubTechno[] = {
    candidate(Archetype::OneDropSpace, 110), candidate(Archetype::Steppers, 100),
    candidate(Archetype::SparseSkank, 110), candidate(Archetype::ChordResponse, 120),
};
constexpr RhythmCompatibilityCandidate kChicagoJack[] = {
    candidate(Archetype::StraightAcid, 120), candidate(Archetype::SparseAcid, 100),
};
constexpr RhythmCompatibilityCandidate kRollingAcid[] = {
    candidate(Archetype::RollingAcid, 120), candidate(Archetype::SyncopatedAcid, 110),
};
constexpr RhythmCompatibilityCandidate kClassicTwoStep[] = {
    candidate(Archetype::ClassicTwoStep, 120), candidate(Archetype::ShuffledFourFour, 90),
};
constexpr RhythmCompatibilityCandidate kDarkSkippy[] = {
    candidate(Archetype::SkippyTwoStep, 120), candidate(Archetype::MachineSyncopation, 100),
};
constexpr RhythmCompatibilityCandidate kDeepChord[] = {candidate(Archetype::ChordResponse, 120)};
constexpr RhythmCompatibilityCandidate kMinimalSpace[] = {
    candidate(Archetype::OneDropSpace, 100), candidate(Archetype::SparseSkank, 120),
    candidate(Archetype::ChordResponse, 90),
};

constexpr RhythmCompatibilityCandidate kClassicChill[] = {
    candidate(Archetype::HalftimeSwitch, 145), candidate(Archetype::SparseFastBreak, 115),
    candidate(Archetype::FunkHouseBridge, 85),
};
constexpr RhythmCompatibilityCandidate kDrunkenGroove[] = {
    candidate(Archetype::FunkHouseBridge, 130), candidate(Archetype::HalftimeSwitch, 105),
    candidate(Archetype::ElectroGapPush, 90), candidate(Archetype::SparseFastBreak, 70),
};
constexpr RhythmCompatibilityCandidate kLoFiHouse[] = {
    candidate(Archetype::StackedQuarters, 115), candidate(Archetype::FunkHouseBridge, 140),
    candidate(Archetype::ShuffledFourFour, 85), candidate(Archetype::OffbeatOpenHat, 65),
};
constexpr RhythmCompatibilityCandidate kMinimalSleep[] = {
    candidate(Archetype::HalftimeSwitch, 155), candidate(Archetype::HypnoticSparse, 85),
    candidate(Archetype::SparseFastBreak, 70),
};
constexpr RhythmCompatibilityCandidate kGoldenEra[] = {
    candidate(Archetype::HalftimeSwitch, 135), candidate(Archetype::ElectroBackskip, 90),
    candidate(Archetype::ElectroGapPush, 100), candidate(Archetype::FunkHouseBridge, 85),
};
constexpr RhythmCompatibilityCandidate kDustyJazz[] = {
    candidate(Archetype::FunkHouseBridge, 135), candidate(Archetype::HalftimeSwitch, 115),
    candidate(Archetype::SparseFastBreak, 85),
};

struct CanonicalCandidates {
  RhythmCompatibilityCandidate values[kMaxCandidates]{};
  uint8_t count = 0;
};

bool validReferenceId(RhythmArchetypeId archetypeId) {
  return rhythmSelectionName(archetypeId) != nullptr;
}

CanonicalCandidates canonicalize(RhythmCompatibilityView compatibility) {
  CanonicalCandidates result{};
  if (compatibility.candidates == nullptr) return result;
  for (uint8_t index = 0; index < compatibility.count && result.count < kMaxCandidates; ++index) {
    const RhythmCompatibilityCandidate input = compatibility.candidates[index];
    if (input.archetypeId == kNoArchetypeId || input.weight == 0 || !validReferenceId(input.archetypeId)) continue;
    uint8_t insertion = 0;
    while (insertion < result.count && result.values[insertion].archetypeId < input.archetypeId) ++insertion;
    if (insertion < result.count && result.values[insertion].archetypeId == input.archetypeId) {
      const uint16_t combined = static_cast<uint16_t>(result.values[insertion].weight) + input.weight;
      result.values[insertion].weight = static_cast<uint8_t>(combined > 255u ? 255u : combined);
      continue;
    }
    for (uint8_t move = result.count; move > insertion; --move) result.values[move] = result.values[move - 1u];
    result.values[insertion] = input;
    ++result.count;
  }
  return result;
}

}  // namespace

RhythmCompatibilityView rhythmCompatibilityFor(const GenreSettings& settings) {
  const GenerativeMode genre = static_cast<GenerativeMode>(
      settings.generativeMode < kGenerativeModeCount ? settings.generativeMode
          : static_cast<uint8_t>(GenerativeMode::Acid));
  switch (settings.recipe) {
    case 1: if (genre == GenerativeMode::Broken) return view(kUkGarage); break;
    case 2: if (genre == GenerativeMode::Broken) return view(kDrumAndBass); break;
    case 3: if (genre == GenerativeMode::Broken) return view(kFootwork); break;
    case 4: if (genre == GenerativeMode::Rave) return view(kPsytrance); break;
    case 5: if (genre == GenerativeMode::Reggae) return view(kDubTechno); break;
    case 6: if (genre == GenerativeMode::Acid) return view(kChicagoJack); break;
    case 7: if (genre == GenerativeMode::Acid) return view(kRollingAcid); break;
    case 8: if (genre == GenerativeMode::Broken) return view(kClassicTwoStep); break;
    case 9: if (genre == GenerativeMode::Broken) return view(kDarkSkippy); break;
    case 10: if (genre == GenerativeMode::Reggae) return view(kDeepChord); break;
    case 11: if (genre == GenerativeMode::Reggae) return view(kMinimalSpace); break;
    case kClassicChillRecipeId: if (genre == GenerativeMode::LoFi) return view(kClassicChill); break;
    case kDrunkenGrooveRecipeId: if (genre == GenerativeMode::LoFi) return view(kDrunkenGroove); break;
    case kLoFiHouseRecipeId: if (genre == GenerativeMode::LoFi) return view(kLoFiHouse); break;
    case kMinimalSleepRecipeId: if (genre == GenerativeMode::LoFi) return view(kMinimalSleep); break;
    case kGoldenEraRecipeId: if (genre == GenerativeMode::HipHop) return view(kGoldenEra); break;
    case kDustyJazzRecipeId: if (genre == GenerativeMode::HipHop) return view(kDustyJazz); break;
    case kBaseRecipeId:
    default: break;
  }

  switch (genre) {
    case GenerativeMode::Acid: return view(kAcidBase);
    case GenerativeMode::Outrun: return view(kSynthwaveBase);
    case GenerativeMode::Darksynth: return view(kDarksynthBase);
    case GenerativeMode::Electro: return view(kElectroBase);
    case GenerativeMode::Rave: return view(kRaveBase);
    case GenerativeMode::Reggae: return view(kReggaeBase);
    case GenerativeMode::TripHop: return view(kTripHopBase);
    case GenerativeMode::Broken: return view(kBrokenBase);
    case GenerativeMode::Chip: return view(kChipBase);
    case GenerativeMode::House: return view(kHouseBase);
    case GenerativeMode::Techno: return view(kTechnoBase);
    case GenerativeMode::HipHop: return view(kHipHopBase);
    case GenerativeMode::FunkSoul: return view(kFunkSoulBase);
    case GenerativeMode::UkGarage: return view(kUkGarageBase);
    case GenerativeMode::DrumAndBass: return view(kDrumAndBassBase);
    case GenerativeMode::LoFi: return view(kLoFiBase);
    default: return {};
  }
}

RhythmSelectionIntent rhythmSelectionIntent(const GenreSettings& settings) {
  RhythmSelectionIntent intent{};
  intent.mode = settings.rhythmSelectionMode == static_cast<uint8_t>(RhythmSelectionMode::Manual)
                    ? RhythmSelectionMode::Manual : RhythmSelectionMode::Auto;
  intent.manualArchetypeId = settings.rhythmArchetypeId;
  return intent;
}

bool isRhythmCompatible(const GenreSettings& settings, RhythmArchetypeId archetypeId) {
  const CanonicalCandidates candidates = canonicalize(rhythmCompatibilityFor(settings));
  for (uint8_t index = 0; index < candidates.count; ++index) {
    if (candidates.values[index].archetypeId == archetypeId) return true;
  }
  return false;
}

uint8_t compatibleRhythmCount(const GenreSettings& settings) {
  return canonicalize(rhythmCompatibilityFor(settings)).count;
}

RhythmArchetypeId compatibleRhythmId(const GenreSettings& settings, uint8_t canonicalIndex) {
  const CanonicalCandidates candidates = canonicalize(rhythmCompatibilityFor(settings));
  return canonicalIndex < candidates.count ? candidates.values[canonicalIndex].archetypeId : kNoArchetypeId;
}

RhythmSelectionResult resolveRhythmSelectionFromView(
    RhythmCompatibilityView compatibility,
    const RhythmSelectionIntent& intent,
    const GenerationContext& generation) {
  RhythmSelectionResult result{};
  const CanonicalCandidates candidates = canonicalize(compatibility);
  if (candidates.count == 0) return result;
  if (intent.mode == RhythmSelectionMode::Manual) {
    for (uint8_t index = 0; index < candidates.count; ++index) {
      if (candidates.values[index].archetypeId == intent.manualArchetypeId) {
        result.status = RhythmSelectionStatus::Ok;
        result.mode = RhythmSelectionMode::Manual;
        result.archetypeId = intent.manualArchetypeId;
        return result;
      }
    }
    result.normalizedToAuto = true;
  }
  uint16_t totalWeight = 0;
  for (uint8_t index = 0; index < candidates.count; ++index)
    totalWeight = static_cast<uint16_t>(totalWeight + candidates.values[index].weight);
  if (totalWeight == 0) return result;
  const uint32_t selectionSeed = deriveGenerationSeed(
      generation, kNoArchetypeId, GenerationDomain::ArchetypeSelection);
  uint16_t coordinate = static_cast<uint16_t>(deterministicValue(selectionSeed, 0) % totalWeight);
  for (uint8_t index = 0; index < candidates.count; ++index) {
    if (coordinate < candidates.values[index].weight) {
      result.status = RhythmSelectionStatus::Ok;
      result.mode = RhythmSelectionMode::Auto;
      result.archetypeId = candidates.values[index].archetypeId;
      return result;
    }
    coordinate = static_cast<uint16_t>(coordinate - candidates.values[index].weight);
  }
  return result;
}

RhythmSelectionResult resolveRhythmSelection(
    const GenreSettings& settings, const GenerationContext& generation) {
  return resolveRhythmSelectionFromView(
      rhythmCompatibilityFor(settings), rhythmSelectionIntent(settings), generation);
}

const char* rhythmSelectionName(RhythmArchetypeId archetypeId) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(archetypeId);
  return definition == nullptr ? nullptr : definition->name;
}

}  // namespace GroovePuterRhythm
