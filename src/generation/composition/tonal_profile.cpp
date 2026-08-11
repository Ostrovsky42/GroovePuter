#include "tonal_profile.h"

#include "../../../scenes.h"
#include "../../dsp/genre_manager.h"

namespace GroovePuterRhythm {
namespace {

constexpr uint16_t bassContours(std::initializer_list<BassPitchContourId> ids) {
  uint16_t mask = 0;
  for (BassPitchContourId id : ids) mask |= bassPitchContourBit(id);
  return mask;
}

constexpr uint16_t melodicContours(std::initializer_list<MelodicContourId> ids) {
  uint16_t mask = 0;
  for (MelodicContourId id : ids) mask |= melodicContourBit(id);
  return mask;
}

constexpr BassBehaviorPolicy bassPolicy(uint16_t allowed,
                                        uint16_t preferred = 0) {
  return {allowed,
          preferred,
          bassArticulationStyleBit(BassArticulationStyleId::Plain),
          0};
}

constexpr MelodicIntentPolicy melodicPolicy(uint16_t allowedContours,
                                            uint16_t preferredContours = 0) {
  return {
      melodicRhythmOperationBit(MelodicRhythmOperationId::Preserve),
      0,
      allowedContours,
      preferredContours,
      melodicMotifOperationBit(MelodicMotifOperationId::None),
      0,
  };
}

constexpr uint16_t kBassRoot =
    bassPitchContourBit(BassPitchContourId::RootAnchor);
constexpr uint16_t kBassAcidAllowed = bassContours({
    BassPitchContourId::RootAnchor,
    BassPitchContourId::RootFifth,
    BassPitchContourId::NeighborReturn,
    BassPitchContourId::StepApproach,
    BassPitchContourId::LeapReturn,
    BassPitchContourId::RootFifthNeighbor,
    BassPitchContourId::PedalTurn,
});
constexpr uint16_t kBassAcidPreferred = bassContours({
    BassPitchContourId::StepApproach,
    BassPitchContourId::LeapReturn,
    BassPitchContourId::RootFifthNeighbor,
    BassPitchContourId::NeighborReturn,
});
constexpr uint16_t kBassSynthAllowed = bassContours({
    BassPitchContourId::RootAnchor,
    BassPitchContourId::RootFifth,
    BassPitchContourId::RootOctave,
    BassPitchContourId::NeighborReturn,
    BassPitchContourId::LeapReturn,
    BassPitchContourId::PedalTurn,
});
constexpr uint16_t kBassSynthPreferred = bassContours({
    BassPitchContourId::RootFifth,
    BassPitchContourId::RootOctave,
    BassPitchContourId::LeapReturn,
});
constexpr uint16_t kBassBrokenAllowed = bassContours({
    BassPitchContourId::RootAnchor,
    BassPitchContourId::RootFifth,
    BassPitchContourId::NeighborReturn,
    BassPitchContourId::StepApproach,
    BassPitchContourId::LeapReturn,
});
constexpr uint16_t kBassBrokenPreferred = bassContours({
    BassPitchContourId::NeighborReturn,
    BassPitchContourId::StepApproach,
    BassPitchContourId::RootFifth,
});
constexpr uint16_t kBassSlowAllowed = bassContours({
    BassPitchContourId::RootAnchor,
    BassPitchContourId::RootFifth,
    BassPitchContourId::NeighborReturn,
    BassPitchContourId::StepApproach,
    BassPitchContourId::PedalTurn,
});
constexpr uint16_t kBassSlowPreferred = bassContours({
    BassPitchContourId::PedalTurn,
    BassPitchContourId::NeighborReturn,
    BassPitchContourId::StepApproach,
});
constexpr uint16_t kBassDubAllowed = bassContours({
    BassPitchContourId::RootAnchor,
    BassPitchContourId::RootFifth,
    BassPitchContourId::NeighborReturn,
    BassPitchContourId::PedalTurn,
});
constexpr uint16_t kBassDubPreferred = bassContours({
    BassPitchContourId::RootAnchor,
    BassPitchContourId::PedalTurn,
    BassPitchContourId::RootFifth,
});

constexpr uint16_t kMelodyStatic =
    melodicContourBit(MelodicContourId::Static);
constexpr uint16_t kMelodyDriveAllowed = melodicContours({
    MelodicContourId::Static,
    MelodicContourId::StepUp,
    MelodicContourId::StepDown,
    MelodicContourId::Arch,
    MelodicContourId::InvertedArch,
    MelodicContourId::LeapReturn,
    MelodicContourId::Neighbor,
    MelodicContourId::RepeatThenUp,
    MelodicContourId::RepeatThenDown,
});
constexpr uint16_t kMelodyDrivePreferred = melodicContours({
    MelodicContourId::StepUp,
    MelodicContourId::StepDown,
    MelodicContourId::Arch,
    MelodicContourId::LeapReturn,
    MelodicContourId::Neighbor,
});
constexpr uint16_t kMelodyBrokenAllowed = melodicContours({
    MelodicContourId::Static,
    MelodicContourId::StepUp,
    MelodicContourId::StepDown,
    MelodicContourId::Neighbor,
    MelodicContourId::RepeatThenUp,
    MelodicContourId::RepeatThenDown,
});
constexpr uint16_t kMelodyBrokenPreferred = melodicContours({
    MelodicContourId::Neighbor,
    MelodicContourId::RepeatThenUp,
    MelodicContourId::RepeatThenDown,
    MelodicContourId::StepUp,
    MelodicContourId::StepDown,
});
constexpr uint16_t kMelodySlowAllowed = melodicContours({
    MelodicContourId::Static,
    MelodicContourId::Arch,
    MelodicContourId::InvertedArch,
    MelodicContourId::Neighbor,
    MelodicContourId::RepeatThenUp,
    MelodicContourId::RepeatThenDown,
});
constexpr uint16_t kMelodySlowPreferred = melodicContours({
    MelodicContourId::Static,
    MelodicContourId::Neighbor,
    MelodicContourId::Arch,
    MelodicContourId::InvertedArch,
});
constexpr uint16_t kMelodyAcidAllowed = melodicContours({
    MelodicContourId::Static,
    MelodicContourId::Neighbor,
    MelodicContourId::RepeatThenUp,
    MelodicContourId::RepeatThenDown,
});
constexpr uint16_t kMelodyAcidPreferred = melodicContours({
    MelodicContourId::Static,
    MelodicContourId::Neighbor,
});

constexpr TonalRegisterCorridor kBassRegister{24, 47, 12};
constexpr TonalRegisterCorridor kSecondaryRegister{48, 71, 16};

constexpr TonalGenerationProfile tonal(BassBehaviorPolicy bass,
                                       MelodicIntentPolicy melodic) {
  return {bass, melodic, kBassRegister, kSecondaryRegister};
}

constexpr TonalGenerationProfile kStaticProfile = tonal(
    bassPolicy(kBassRoot), melodicPolicy(kMelodyStatic));
constexpr TonalGenerationProfile kAcidProfile = tonal(
    bassPolicy(kBassAcidAllowed, kBassAcidPreferred),
    melodicPolicy(kMelodyAcidAllowed, kMelodyAcidPreferred));
constexpr TonalGenerationProfile kSynthProfile = tonal(
    bassPolicy(kBassSynthAllowed, kBassSynthPreferred),
    melodicPolicy(kMelodyDriveAllowed, kMelodyDrivePreferred));
constexpr TonalGenerationProfile kBrokenProfile = tonal(
    bassPolicy(kBassBrokenAllowed, kBassBrokenPreferred),
    melodicPolicy(kMelodyBrokenAllowed, kMelodyBrokenPreferred));
constexpr TonalGenerationProfile kSlowProfile = tonal(
    bassPolicy(kBassSlowAllowed, kBassSlowPreferred),
    melodicPolicy(kMelodySlowAllowed, kMelodySlowPreferred));
constexpr TonalGenerationProfile kDubProfile = tonal(
    bassPolicy(kBassDubAllowed, kBassDubPreferred),
    melodicPolicy(kMelodyStatic));

struct TonalProfileRow {
  uint8_t mode = 0;
  uint8_t recipe = 0;
  TonalGenerationProfile profile{};
};

constexpr TonalProfileRow row(GenerativeMode mode,
                              const TonalGenerationProfile& profile) {
  return {static_cast<uint8_t>(mode), kBaseRecipeId, profile};
}

// Current recipe variants intentionally inherit their mode's base tonal policy
// unless an exact row is added later. This keeps musical policy data-driven and
// avoids a GenerativeMode switch in roles/tonal code.
constexpr TonalProfileRow kRows[] = {
    row(GenerativeMode::Acid, kAcidProfile),
    row(GenerativeMode::Outrun, kSynthProfile),
    row(GenerativeMode::Darksynth, kSynthProfile),
    row(GenerativeMode::Electro, kBrokenProfile),
    row(GenerativeMode::Rave, kStaticProfile),
    row(GenerativeMode::Reggae, kDubProfile),
    row(GenerativeMode::TripHop, kSlowProfile),
    row(GenerativeMode::Broken, kBrokenProfile),
    row(GenerativeMode::Chip, kSynthProfile),
    row(GenerativeMode::House, kStaticProfile),
    row(GenerativeMode::Techno, kStaticProfile),
    row(GenerativeMode::HipHop, kSlowProfile),
    row(GenerativeMode::FunkSoul, kSlowProfile),
    row(GenerativeMode::UkGarage, kBrokenProfile),
    row(GenerativeMode::DrumAndBass, kBrokenProfile),
    row(GenerativeMode::LoFi, kSlowProfile),
};

}  // namespace

TonalGenerationProfile tonalGenerationProfileFor(const GenreSettings& settings) {
  const TonalProfileRow* base = nullptr;
  for (const TonalProfileRow& candidate : kRows) {
    if (candidate.mode != settings.generativeMode) continue;
    if (candidate.recipe == settings.recipe) return candidate.profile;
    if (candidate.recipe == kBaseRecipeId) base = &candidate;
  }
  return base != nullptr ? base->profile : kStaticProfile;
}

}  // namespace GroovePuterRhythm
