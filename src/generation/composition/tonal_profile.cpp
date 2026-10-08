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

constexpr BassBehaviorPolicy bassPolicy(
    uint16_t allowed,
    uint16_t preferred = 0,
    uint16_t allowedArticulations =
        bassArticulationStyleBit(BassArticulationStyleId::Plain),
    uint16_t preferredArticulations = 0) {
  return {allowed, preferred, allowedArticulations, preferredArticulations};
}

// 0.9.18 prototype: every genre had Plain articulation, so no generated bass
// ever slid or accented -- not even Acid, where slide and accent are the style.
constexpr uint16_t kArticulationAcidAllowed =
    bassArticulationStyleBit(BassArticulationStyleId::Plain) |
    bassArticulationStyleBit(BassArticulationStyleId::AccentPulse) |
    bassArticulationStyleBit(BassArticulationStyleId::LegatoApproach) |
    bassArticulationStyleBit(BassArticulationStyleId::Dynamic);
constexpr uint16_t kArticulationAcidPreferred =
    bassArticulationStyleBit(BassArticulationStyleId::Dynamic) |
    bassArticulationStyleBit(BassArticulationStyleId::LegatoApproach);
constexpr uint16_t kArticulationPulse =
    bassArticulationStyleBit(BassArticulationStyleId::Plain) |
    bassArticulationStyleBit(BassArticulationStyleId::AccentPulse);

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
    BassPitchContourId::NeighborReturn,
    BassPitchContourId::LeapReturn,
    BassPitchContourId::PedalTurn,
});
constexpr uint16_t kBassSynthPreferred = bassContours({
    BassPitchContourId::RootFifth,
    BassPitchContourId::LeapReturn,
    BassPitchContourId::NeighborReturn,
});
// 0.9.18 prototype: Outrun/Darksynth reference basses (Nightcall, Hotline
// Miami) sit on the chord root -- 65% repeated notes against 2-4% from the
// Synth vocabulary above. Pedal on the root, or bounce it an octave.
constexpr uint16_t kBassPedalAllowed = bassContours({
    BassPitchContourId::RootAnchor,
    BassPitchContourId::RootOctave,
    BassPitchContourId::PedalTurn,
});
constexpr uint16_t kBassPedalPreferred = bassContours({
    BassPitchContourId::RootAnchor,
    BassPitchContourId::RootOctave,
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
// 0.9.18 prototype: the Acid lead could only stay or step to a neighbour.
constexpr uint16_t kMelodyAcidAllowed = melodicContours({
    MelodicContourId::Static,
    MelodicContourId::Neighbor,
    MelodicContourId::RepeatThenUp,
    MelodicContourId::RepeatThenDown,
    MelodicContourId::StepUp,
    MelodicContourId::StepDown,
    MelodicContourId::LeapReturn,
    MelodicContourId::Arch,
});
constexpr uint16_t kMelodyAcidPreferred = melodicContours({
    MelodicContourId::Neighbor,
    MelodicContourId::RepeatThenUp,
    MelodicContourId::LeapReturn,
    MelodicContourId::StepUp,
});
// The owner preferred the wide prototype lead to a lead built mostly from
// MotifAnswer (narrower). MotifAnswer stays as one preferred contour among
// several: an occasional motif-and-answer bar, not the main voice.
constexpr uint16_t kMotifAnswerBit = melodicContourBit(MelodicContourId::MotifAnswer);

constexpr TonalRegisterCorridor kStaticBassRegister{24, 47, 12};
// Any production profile that combines a moving harmonic root with tagged
// fifth or wider degree contours must allow the full physical span of the bass
// corridor. The projector still rejects a note outside [24,47].
constexpr TonalRegisterCorridor kMovingBassRegister{24, 47, 23};
constexpr TonalRegisterCorridor kSecondaryRegister{48, 71, 16};

constexpr TonalGenerationProfile tonal(
    BassBehaviorPolicy bass,
    MelodicIntentPolicy melodic,
    TonalRegisterCorridor bassRegister = kMovingBassRegister) {
  return {bass, melodic, bassRegister, kSecondaryRegister};
}

constexpr TonalGenerationProfile kStaticProfile = {
    bassPolicy(kBassRoot), melodicPolicy(kMelodyStatic),
    kStaticBassRegister, kSecondaryRegister};
constexpr TonalGenerationProfile kAcidProfile = tonal(
    bassPolicy(kBassAcidAllowed, kBassAcidPreferred,
               kArticulationAcidAllowed, kArticulationAcidPreferred),
    melodicPolicy(static_cast<uint16_t>(kMelodyAcidAllowed | kMotifAnswerBit),
                  static_cast<uint16_t>(kMelodyAcidPreferred | kMotifAnswerBit)));
// Atlas recipes keep the prototype's tonal policy (no MotifAnswer); exact rows
// keep recipe inheritance explicit.
constexpr TonalGenerationProfile kAcidAtlasProfile = tonal(
    bassPolicy(kBassAcidAllowed, kBassAcidPreferred,
               kArticulationAcidAllowed, kArticulationAcidPreferred),
    melodicPolicy(kMelodyAcidAllowed, kMelodyAcidPreferred));
constexpr TonalGenerationProfile kSynthProfile = tonal(
    bassPolicy(kBassSynthAllowed, kBassSynthPreferred),
    melodicPolicy(kMelodyDriveAllowed, kMelodyDrivePreferred));
// 0.9.18 prototype: Synthwave bass gets accents; House leaves the static
// profile (one-pitch lead, root-only bass) for a moving bass and a Drive lead.
constexpr TonalGenerationProfile kOutrunProfile = tonal(
    bassPolicy(kBassPedalAllowed, kBassPedalPreferred, kArticulationPulse),
    melodicPolicy(static_cast<uint16_t>(kMelodyDriveAllowed | kMotifAnswerBit),
                  static_cast<uint16_t>(kMelodyDrivePreferred | kMotifAnswerBit)));
constexpr TonalGenerationProfile kHouseProfile = tonal(
    bassPolicy(kBassSynthAllowed, kBassSynthPreferred, kArticulationPulse),
    melodicPolicy(static_cast<uint16_t>(kMelodyDriveAllowed | kMotifAnswerBit),
                  static_cast<uint16_t>(kMelodyDrivePreferred | kMotifAnswerBit)));
constexpr TonalGenerationProfile kBrokenProfile = tonal(
    bassPolicy(kBassBrokenAllowed, kBassBrokenPreferred),
    melodicPolicy(kMelodyBrokenAllowed, kMelodyBrokenPreferred));
constexpr TonalGenerationProfile kSlowProfile = tonal(
    bassPolicy(kBassSlowAllowed, kBassSlowPreferred),
    melodicPolicy(kMelodySlowAllowed, kMelodySlowPreferred));
constexpr TonalGenerationProfile kDubProfile = tonal(
    bassPolicy(kBassDubAllowed, kBassDubPreferred),
    melodicPolicy(kMelodyStatic));

// 0.9.18 genre leads (recipe 0; other recipes keep their profile through
// exact rows below).
constexpr TonalGenerationProfile kDriveLeadProfile = tonal(
    bassPolicy(kBassSynthAllowed, kBassSynthPreferred, kArticulationPulse),
    melodicPolicy(static_cast<uint16_t>(kMelodyDriveAllowed | kMotifAnswerBit),
                  static_cast<uint16_t>(kMelodyDrivePreferred | kMotifAnswerBit)));
constexpr TonalGenerationProfile kDarksynthProfile = tonal(
    bassPolicy(kBassPedalAllowed, kBassPedalPreferred, kArticulationPulse),
    melodicPolicy(static_cast<uint16_t>(kMelodyDriveAllowed | kMotifAnswerBit),
                  static_cast<uint16_t>(kMelodyDrivePreferred | kMotifAnswerBit)));
// Rave: a repeated hook that jumps a fourth/fifth and comes back.
constexpr TonalGenerationProfile kRaveProfile = tonal(
    bassPolicy(kBassSynthAllowed, kBassSynthPreferred, kArticulationPulse),
    melodicPolicy(melodicContours({MelodicContourId::Static,
                                   MelodicContourId::Neighbor,
                                   MelodicContourId::LeapReturn,
                                   MelodicContourId::RepeatThenUp,
                                   MelodicContourId::RepeatThenDown}),
                  melodicContours({MelodicContourId::LeapReturn,
                                   MelodicContourId::RepeatThenUp,
                                   MelodicContourId::Neighbor})));
// Techno stays nearly static (owner): root bass, a pedal lead that may step
// to a neighbour and back.
constexpr TonalGenerationProfile kTechnoProfile = {
    bassPolicy(kBassRoot),
    melodicPolicy(melodicContours({MelodicContourId::Static,
                                   MelodicContourId::Neighbor}),
                  melodicContours({MelodicContourId::Neighbor})),
    kStaticBassRegister, kSecondaryRegister};
// Broken family: accents on the bass, the lead may also leap and answer.
constexpr TonalGenerationProfile kBrokenLiveProfile = tonal(
    bassPolicy(kBassBrokenAllowed, kBassBrokenPreferred, kArticulationPulse),
    melodicPolicy(static_cast<uint16_t>(kMelodyBrokenAllowed |
                                        melodicContourBit(MelodicContourId::LeapReturn) |
                                        kMotifAnswerBit),
                  static_cast<uint16_t>(kMelodyBrokenPreferred | kMotifAnswerBit)));

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
    {static_cast<uint8_t>(GenerativeMode::Acid), 6, kAcidAtlasProfile},
    {static_cast<uint8_t>(GenerativeMode::Acid), 7, kAcidAtlasProfile},
    row(GenerativeMode::Outrun, kOutrunProfile),
    row(GenerativeMode::Darksynth, kDarksynthProfile),
    row(GenerativeMode::Electro, kBrokenLiveProfile),
    row(GenerativeMode::Rave, kRaveProfile),
    {static_cast<uint8_t>(GenerativeMode::Rave), 4, kStaticProfile},
    row(GenerativeMode::Reggae, kDubProfile),
    row(GenerativeMode::TripHop, kSlowProfile),
    row(GenerativeMode::Broken, kBrokenLiveProfile),
    {static_cast<uint8_t>(GenerativeMode::Broken), 1, kBrokenProfile},
    {static_cast<uint8_t>(GenerativeMode::Broken), 2, kBrokenProfile},
    {static_cast<uint8_t>(GenerativeMode::Broken), 3, kBrokenProfile},
    {static_cast<uint8_t>(GenerativeMode::Broken), 8, kBrokenProfile},
    {static_cast<uint8_t>(GenerativeMode::Broken), 9, kBrokenProfile},
    row(GenerativeMode::Chip, kDriveLeadProfile),
    row(GenerativeMode::House, kHouseProfile),
    row(GenerativeMode::Techno, kTechnoProfile),
    row(GenerativeMode::HipHop, kSlowProfile),
    row(GenerativeMode::FunkSoul, kSlowProfile),
    row(GenerativeMode::UkGarage, kBrokenLiveProfile),
    row(GenerativeMode::DrumAndBass, kBrokenLiveProfile),
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
