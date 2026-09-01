#include "generation_profile.h"

#include <cstddef>

#include "../../../scenes.h"
#include "../../dsp/genre_manager.h"

namespace GroovePuterRhythm {
namespace {

constexpr uint8_t kMaxWeightedCandidates = 16;

constexpr WeightedIdentityCandidate weighted(uint8_t id, uint8_t weight) {
  return WeightedIdentityCandidate{id, weight};
}

template <typename Id>
constexpr WeightedIdentityCandidate weighted(Id id, uint8_t weight) {
  return weighted(static_cast<uint8_t>(id), weight);
}

constexpr uint8_t phraseIdentity(PhraseEvolutionLawId law, uint8_t bars) {
  return static_cast<uint8_t>((static_cast<uint8_t>(law) << 4u) | bars);
}

constexpr WeightedIdentityCandidate phrase(PhraseEvolutionLawId law,
                                             uint8_t bars,
                                             uint8_t weight) {
  return weighted(phraseIdentity(law, bars), weight);
}

template <size_t N>
constexpr WeightedIdentityView view(
    const WeightedIdentityCandidate (&candidates)[N]) {
  return {candidates, static_cast<uint8_t>(N)};
}

constexpr WeightedIdentityCandidate kFeelStraightDrive[] = {
    weighted(FeelProfileId::Straight, 120),
    weighted(FeelProfileId::PushPullControlled, 45),
};
constexpr WeightedIdentityCandidate kFeelSwingDrive[] = {
    weighted(FeelProfileId::Straight, 45),
    weighted(FeelProfileId::SwingCompatible, 110),
    weighted(FeelProfileId::PushPullControlled, 70),
};
constexpr WeightedIdentityCandidate kFeelSlowPocket[] = {
    weighted(FeelProfileId::Straight, 25),
    weighted(FeelProfileId::LaidBack, 120),
    weighted(FeelProfileId::PushPullControlled, 65),
};
constexpr WeightedIdentityCandidate kFeelDubPocket[] = {
    weighted(FeelProfileId::SwingCompatible, 60),
    weighted(FeelProfileId::LaidBack, 120),
    weighted(FeelProfileId::PushPullControlled, 40),
};
constexpr WeightedIdentityCandidate kFeelLoFiPocket[] = {
    weighted(FeelProfileId::Straight, 20),
    weighted(FeelProfileId::SwingCompatible, 90),
    weighted(FeelProfileId::LaidBack, 150),
    weighted(FeelProfileId::PushPullControlled, 120),
};
constexpr WeightedIdentityCandidate kFeelDrunkenPocket[] = {
    weighted(FeelProfileId::SwingCompatible, 120),
    weighted(FeelProfileId::LaidBack, 125),
    weighted(FeelProfileId::PushPullControlled, 150),
};
constexpr WeightedIdentityCandidate kFeelLoFiHouse[] = {
    weighted(FeelProfileId::Straight, 55),
    weighted(FeelProfileId::SwingCompatible, 110),
    weighted(FeelProfileId::LaidBack, 80),
    weighted(FeelProfileId::PushPullControlled, 90),
};

constexpr WeightedIdentityCandidate kBassDrive[] = {
    weighted(BassRhythmId::KickLock, 70),
    weighted(BassRhythmId::OffbeatPush, 90),
    weighted(BassRhythmId::RollingDrive, 110),
    weighted(BassRhythmId::SyncopatedHook, 75),
};
constexpr WeightedIdentityCandidate kBassMachine[] = {
    weighted(BassRhythmId::KickAnswer, 90),
    weighted(BassRhythmId::GapFill, 90),
    weighted(BassRhythmId::OffbeatPush, 70),
    weighted(BassRhythmId::SyncopatedHook, 120),
};
constexpr WeightedIdentityCandidate kBassSlow[] = {
    weighted(BassRhythmId::KickAnswer, 75),
    weighted(BassRhythmId::GapFill, 45),
    weighted(BassRhythmId::SparseAnchor, 120),
    weighted(BassRhythmId::HalfTimePocket, 100),
    weighted(BassRhythmId::SustainAndDrop, 80),
};
constexpr WeightedIdentityCandidate kBassDub[] = {
    weighted(BassRhythmId::KickAnswer, 85),
    weighted(BassRhythmId::GapFill, 75),
    weighted(BassRhythmId::SparseAnchor, 120),
    weighted(BassRhythmId::SustainAndDrop, 100),
};
constexpr WeightedIdentityCandidate kBassChip[] = {
    weighted(BassRhythmId::RootPulse, 100),
    weighted(BassRhythmId::RollingDrive, 95),
    weighted(BassRhythmId::SyncopatedHook, 110),
};
constexpr WeightedIdentityCandidate kBassLoFi[] = {
    weighted(BassRhythmId::KickAnswer, 60),
    weighted(BassRhythmId::GapFill, 40),
    weighted(BassRhythmId::SparseAnchor, 150),
    weighted(BassRhythmId::HalfTimePocket, 135),
    weighted(BassRhythmId::SustainAndDrop, 115),
};
constexpr WeightedIdentityCandidate kBassBoomBap[] = {
    weighted(BassRhythmId::KickAnswer, 110),
    weighted(BassRhythmId::GapFill, 70),
    weighted(BassRhythmId::SparseAnchor, 100),
    weighted(BassRhythmId::HalfTimePocket, 120),
    weighted(BassRhythmId::SyncopatedHook, 70),
};

constexpr WeightedIdentityCandidate kChordDrive[] = {
    weighted(ChordRhythmId::HalfBarChange, 75),
    weighted(ChordRhythmId::OffbeatStab, 110),
    weighted(ChordRhythmId::AnticipatedChange, 85),
    weighted(ChordRhythmId::SyncopatedComp, 70),
};
constexpr WeightedIdentityCandidate kChordBroken[] = {
    weighted(ChordRhythmId::HeldPad, 55),
    weighted(ChordRhythmId::BackbeatStab, 80),
    weighted(ChordRhythmId::AnticipatedChange, 100),
    weighted(ChordRhythmId::SparseChordReply, 90),
};
constexpr WeightedIdentityCandidate kChordSlow[] = {
    weighted(ChordRhythmId::HeldPad, 110),
    weighted(ChordRhythmId::WholeBarHold, 100),
    weighted(ChordRhythmId::BackbeatStab, 35),
    weighted(ChordRhythmId::SparseChordReply, 120),
};
constexpr WeightedIdentityCandidate kChordDub[] = {
    weighted(ChordRhythmId::WholeBarHold, 55),
    weighted(ChordRhythmId::OffbeatStab, 85),
    weighted(ChordRhythmId::SparseChordReply, 95),
    weighted(ChordRhythmId::DubChordSpace, 120),
};
constexpr WeightedIdentityCandidate kChordLoFi[] = {
    weighted(ChordRhythmId::HeldPad, 125),
    weighted(ChordRhythmId::WholeBarHold, 150),
    weighted(ChordRhythmId::SparseChordReply, 115),
    weighted(ChordRhythmId::BackbeatStab, 55),
};
constexpr WeightedIdentityCandidate kChordLoFiHouse[] = {
    weighted(ChordRhythmId::HeldPad, 70),
    weighted(ChordRhythmId::WholeBarHold, 80),
    weighted(ChordRhythmId::OffbeatStab, 120),
    weighted(ChordRhythmId::SparseChordReply, 90),
};

// Stage 15 harmony choices are editorial data. The progression module remains
// genre-agnostic and consumes only the selected identity plus event count.
constexpr WeightedIdentityCandidate kProgressionStatic[] = {
    weighted(ProgressionId::StaticModal, 190),
    weighted(ProgressionId::PedalDrone, 35),
};
constexpr WeightedIdentityCandidate kProgressionPop[] = {
    weighted(ProgressionId::PopCycle, 150),
    weighted(ProgressionId::StaticModal, 50),
    weighted(ProgressionId::BorrowedLift, 45),
};
constexpr WeightedIdentityCandidate kProgressionDark[] = {
    weighted(ProgressionId::MinorFall, 140),
    weighted(ProgressionId::ParallelShift, 85),
    weighted(ProgressionId::StaticModal, 55),
};
constexpr WeightedIdentityCandidate kProgressionBroken[] = {
    weighted(ProgressionId::StaticModal, 85),
    weighted(ProgressionId::BorrowedLift, 80),
    weighted(ProgressionId::ParallelShift, 65),
    weighted(ProgressionId::MinorFall, 45),
};
constexpr WeightedIdentityCandidate kProgressionDub[] = {
    weighted(ProgressionId::PedalDrone, 150),
    weighted(ProgressionId::StaticModal, 80),
    weighted(ProgressionId::BorrowedLift, 55),
};
constexpr WeightedIdentityCandidate kProgressionTrip[] = {
    weighted(ProgressionId::TwoFiveOne, 150),
    weighted(ProgressionId::ParallelShift, 100),
    weighted(ProgressionId::PedalDrone, 70),
};
constexpr WeightedIdentityCandidate kProgressionHipHop[] = {
    weighted(ProgressionId::TwoFiveOne, 135),
    weighted(ProgressionId::ParallelShift, 110),
    weighted(ProgressionId::PedalDrone, 85),
};
constexpr WeightedIdentityCandidate kProgressionFunk[] = {
    weighted(ProgressionId::TwoFiveOne, 140),
    weighted(ProgressionId::PopCycle, 90),
    weighted(ProgressionId::BorrowedLift, 85),
};
constexpr WeightedIdentityCandidate kProgressionLoFi[] = {
    weighted(ProgressionId::TwoFiveOne, 130),
    weighted(ProgressionId::ParallelShift, 135),
    weighted(ProgressionId::PedalDrone, 100),
    weighted(ProgressionId::BorrowedLift, 70),
};
constexpr WeightedIdentityCandidate kProgressionChip[] = {
    weighted(ProgressionId::PopCycle, 120),
    weighted(ProgressionId::StaticModal, 90),
    weighted(ProgressionId::MinorFall, 60),
};

constexpr WeightedIdentityCandidate kMelodicDrive[] = {
    weighted(MelodicRhythmId::TwoNoteHook, 80),
    weighted(MelodicRhythmId::PickupPhrase, 85),
    weighted(MelodicRhythmId::SyncopatedMotif, 110),
    weighted(MelodicRhythmId::RepeatedCell, 100),
};
constexpr WeightedIdentityCandidate kMelodicBroken[] = {
    weighted(MelodicRhythmId::SparseCall, 55),
    weighted(MelodicRhythmId::DelayedAnswer, 110),
    weighted(MelodicRhythmId::PickupPhrase, 85),
    weighted(MelodicRhythmId::BarEndResponse, 100),
};
constexpr WeightedIdentityCandidate kMelodicSlow[] = {
    weighted(MelodicRhythmId::SparseCall, 85),
    weighted(MelodicRhythmId::DelayedAnswer, 110),
    weighted(MelodicRhythmId::TwoNoteHook, 70),
    weighted(MelodicRhythmId::LongTone, 75),
    weighted(MelodicRhythmId::RestHeavy, 120),
};
constexpr WeightedIdentityCandidate kMelodicDub[] = {
    weighted(MelodicRhythmId::SparseCall, 100),
    weighted(MelodicRhythmId::LongTone, 70),
    weighted(MelodicRhythmId::RestHeavy, 120),
    weighted(MelodicRhythmId::DriftPhrase, 90),
};
// Every Lo-Fi melodic identity is 0..3 onsets/bar before chord blocking.
constexpr WeightedIdentityCandidate kMelodicLoFi[] = {
    weighted(MelodicRhythmId::SparseCall, 110),
    weighted(MelodicRhythmId::DelayedAnswer, 105),
    weighted(MelodicRhythmId::TwoNoteHook, 100),
    weighted(MelodicRhythmId::PickupPhrase, 60),
    weighted(MelodicRhythmId::LongTone, 95),
    weighted(MelodicRhythmId::RestHeavy, 160),
    weighted(MelodicRhythmId::BarEndResponse, 90),
    weighted(MelodicRhythmId::DriftPhrase, 105),
};

constexpr WeightedIdentityCandidate kMotifDrive[] = {
    weighted(MotifShapeId::SourceOrder, 70),
    weighted(MotifShapeId::TwoNoteCell, 110),
    weighted(MotifShapeId::Mirror, 65),
    weighted(MotifShapeId::Pivot, 90),
};
constexpr WeightedIdentityCandidate kMotifAnswer[] = {
    weighted(MotifShapeId::SourceOrder, 55),
    weighted(MotifShapeId::Mirror, 75),
    weighted(MotifShapeId::CallResponse, 120),
    weighted(MotifShapeId::Pivot, 80),
};
constexpr WeightedIdentityCandidate kMotifSparse[] = {
    weighted(MotifShapeId::SourceOrder, 100),
    weighted(MotifShapeId::TwoNoteCell, 65),
    weighted(MotifShapeId::CallResponse, 110),
};
constexpr WeightedIdentityCandidate kMotifLoFi[] = {
    weighted(MotifShapeId::SourceOrder, 75),
    weighted(MotifShapeId::TwoNoteCell, 125),
    weighted(MotifShapeId::CallResponse, 105),
    weighted(MotifShapeId::Pivot, 55),
};

// Stage 12 phrase choices are planning metadata only until its physical gate.
constexpr WeightedIdentityCandidate kPhraseDrive[] = {
    phrase(PhraseEvolutionLawId::Loop, 2, 55),
    phrase(PhraseEvolutionLawId::RepeatReply, 4, 100),
    phrase(PhraseEvolutionLawId::DevelopReturn, 4, 120),
};
constexpr WeightedIdentityCandidate kPhraseBroken[] = {
    phrase(PhraseEvolutionLawId::RepeatReply, 2, 55),
    phrase(PhraseEvolutionLawId::RepeatReply, 4, 110),
    phrase(PhraseEvolutionLawId::DevelopReturn, 4, 95),
};
constexpr WeightedIdentityCandidate kPhraseSlow[] = {
    phrase(PhraseEvolutionLawId::RepeatReply, 4, 100),
    phrase(PhraseEvolutionLawId::DevelopReturn, 4, 70),
    phrase(PhraseEvolutionLawId::SparseDrift, 8, 120),
};
constexpr WeightedIdentityCandidate kPhraseCompact[] = {
    phrase(PhraseEvolutionLawId::Loop, 1, 65),
    phrase(PhraseEvolutionLawId::Loop, 2, 110),
    phrase(PhraseEvolutionLawId::RepeatReply, 4, 70),
};

struct ProfileDefinition {
  uint8_t generativeMode;
  uint8_t recipe;
  WeightedIdentityView feels;
  WeightedIdentityView bass;
  WeightedIdentityView chord;
  WeightedIdentityView progression;
  WeightedIdentityView melodic;
  WeightedIdentityView motif;
  WeightedIdentityView phraseLaw;
  GenerationCorridor corridor;
  CompositionSecondaryRole secondaryRole;
};

constexpr ProfileDefinition profile(
    GenerativeMode mode, uint8_t recipe, WeightedIdentityView feels,
    WeightedIdentityView bass, WeightedIdentityView chord,
    WeightedIdentityView progression, WeightedIdentityView melodic,
    WeightedIdentityView motif, WeightedIdentityView phraseLaw,
    GenerationCorridor corridor,
    CompositionSecondaryRole secondaryRole) {
  return {static_cast<uint8_t>(mode), recipe, feels, bass, chord, progression,
          melodic, motif, phraseLaw, corridor, secondaryRole};
}

constexpr ProfileDefinition kProfiles[] = {
    profile(GenerativeMode::Acid, 0, view(kFeelStraightDrive), view(kBassDrive), view(kChordDrive), view(kProgressionStatic), view(kMelodicDrive), view(kMotifDrive), view(kPhraseDrive), {118,150,132,16,6,14}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Outrun, 0, view(kFeelStraightDrive), view(kBassChip), view(kChordSlow), view(kProgressionPop), view(kMelodicDrive), view(kMotifDrive), view(kPhraseDrive), {88,125,108,16,4,11}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Darksynth, 0, view(kFeelStraightDrive), view(kBassDrive), view(kChordDrive), view(kProgressionDark), view(kMelodicDrive), view(kMotifDrive), view(kPhraseDrive), {122,148,134,16,5,13}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Electro, 0, view(kFeelSwingDrive), view(kBassMachine), view(kChordBroken), view(kProgressionBroken), view(kMelodicBroken), view(kMotifAnswer), view(kPhraseBroken), {102,132,116,16,4,12}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Rave, 0, view(kFeelStraightDrive), view(kBassDrive), view(kChordDrive), view(kProgressionStatic), view(kMelodicDrive), view(kMotifDrive), view(kPhraseCompact), {132,160,145,16,7,15}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Reggae, 0, view(kFeelDubPocket), view(kBassDub), view(kChordDub), view(kProgressionDub), view(kMelodicDub), view(kMotifSparse), view(kPhraseSlow), {68,105,82,16,2,9}, CompositionSecondaryRole::Chord),
    profile(GenerativeMode::TripHop, 0, view(kFeelSlowPocket), view(kBassSlow), view(kChordSlow), view(kProgressionTrip), view(kMelodicSlow), view(kMotifSparse), view(kPhraseSlow), {66,98,80,16,2,9}, CompositionSecondaryRole::Chord),
    profile(GenerativeMode::Broken, 0, view(kFeelSwingDrive), view(kBassMachine), view(kChordBroken), view(kProgressionBroken), view(kMelodicBroken), view(kMotifAnswer), view(kPhraseBroken), {118,148,132,16,5,14}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Chip, 0, view(kFeelStraightDrive), view(kBassChip), view(kChordDrive), view(kProgressionChip), view(kMelodicDrive), view(kMotifDrive), view(kPhraseCompact), {96,170,128,16,5,15}, CompositionSecondaryRole::Melodic),

    profile(GenerativeMode::House, 0, view(kFeelSwingDrive), view(kBassDrive), view(kChordDrive), view(kProgressionPop), view(kMelodicDrive), view(kMotifDrive), view(kPhraseDrive), {112,128,122,16,5,13}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Techno, 0, view(kFeelStraightDrive), view(kBassDrive), view(kChordDrive), view(kProgressionStatic), view(kMelodicDrive), view(kMotifDrive), view(kPhraseDrive), {124,146,134,16,5,14}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::HipHop, 0, view(kFeelLoFiPocket), view(kBassBoomBap), view(kChordLoFi), view(kProgressionHipHop), view(kMelodicLoFi), view(kMotifLoFi), view(kPhraseBroken), {76,104,90,16,3,10}, CompositionSecondaryRole::ChordWithMelodicFill),
    profile(GenerativeMode::FunkSoul, 0, view(kFeelSwingDrive), view(kBassBoomBap), view(kChordLoFi), view(kProgressionFunk), view(kMelodicLoFi), view(kMotifLoFi), view(kPhraseBroken), {88,116,102,16,4,11}, CompositionSecondaryRole::ChordWithMelodicFill),
    profile(GenerativeMode::UkGarage, 0, view(kFeelSwingDrive), view(kBassMachine), view(kChordBroken), view(kProgressionBroken), view(kMelodicBroken), view(kMotifAnswer), view(kPhraseBroken), {126,140,132,16,5,13}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::DrumAndBass, 0, view(kFeelStraightDrive), view(kBassDrive), view(kChordBroken), view(kProgressionBroken), view(kMelodicBroken), view(kMotifAnswer), view(kPhraseCompact), {160,180,174,16,7,15}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::LoFi, 0, view(kFeelLoFiPocket), view(kBassLoFi), view(kChordLoFi), view(kProgressionLoFi), view(kMelodicLoFi), view(kMotifLoFi), view(kPhraseSlow), {54,90,72,16,2,8}, CompositionSecondaryRole::ChordWithMelodicFill),

    profile(GenerativeMode::Broken, 1, view(kFeelSwingDrive), view(kBassMachine), view(kChordBroken), view(kProgressionBroken), view(kMelodicBroken), view(kMotifAnswer), view(kPhraseBroken), {125,138,132,16,5,13}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Broken, 2, view(kFeelStraightDrive), view(kBassDrive), view(kChordBroken), view(kProgressionBroken), view(kMelodicBroken), view(kMotifAnswer), view(kPhraseCompact), {160,180,174,16,7,15}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Broken, 3, view(kFeelSwingDrive), view(kBassMachine), view(kChordBroken), view(kProgressionBroken), view(kMelodicBroken), view(kMotifAnswer), view(kPhraseBroken), {145,165,158,16,6,15}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Rave, 4, view(kFeelStraightDrive), view(kBassDrive), view(kChordDrive), view(kProgressionStatic), view(kMelodicDrive), view(kMotifDrive), view(kPhraseCompact), {138,150,145,16,8,15}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Reggae, 5, view(kFeelDubPocket), view(kBassDub), view(kChordDub), view(kProgressionDub), view(kMelodicDub), view(kMotifSparse), view(kPhraseSlow), {112,128,120,16,2,8}, CompositionSecondaryRole::Chord),
    profile(GenerativeMode::Acid, 6, view(kFeelStraightDrive), view(kBassDrive), view(kChordDrive), view(kProgressionStatic), view(kMelodicDrive), view(kMotifDrive), view(kPhraseCompact), {118,132,124,16,6,13}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Acid, 7, view(kFeelStraightDrive), view(kBassDrive), view(kChordDrive), view(kProgressionStatic), view(kMelodicDrive), view(kMotifDrive), view(kPhraseDrive), {126,145,136,16,8,15}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Broken, 8, view(kFeelSwingDrive), view(kBassMachine), view(kChordBroken), view(kProgressionBroken), view(kMelodicBroken), view(kMotifAnswer), view(kPhraseBroken), {126,136,132,16,5,12}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Broken, 9, view(kFeelSwingDrive), view(kBassSlow), view(kChordBroken), view(kProgressionBroken), view(kMelodicBroken), view(kMotifAnswer), view(kPhraseSlow), {128,140,134,16,3,11}, CompositionSecondaryRole::Melodic),
    profile(GenerativeMode::Reggae, 10, view(kFeelSlowPocket), view(kBassDub), view(kChordDub), view(kProgressionDub), view(kMelodicDub), view(kMotifSparse), view(kPhraseSlow), {108,124,116,16,2,8}, CompositionSecondaryRole::Chord),
    profile(GenerativeMode::Reggae, 11, view(kFeelDubPocket), view(kBassDub), view(kChordDub), view(kProgressionDub), view(kMelodicDub), view(kMotifSparse), view(kPhraseSlow), {72,102,86,16,1,7}, CompositionSecondaryRole::Chord),

    profile(GenerativeMode::LoFi, kClassicChillRecipeId, view(kFeelLoFiPocket), view(kBassLoFi), view(kChordLoFi), view(kProgressionLoFi), view(kMelodicLoFi), view(kMotifLoFi), view(kPhraseSlow), {58,82,72,16,2,7}, CompositionSecondaryRole::ChordWithMelodicFill),
    profile(GenerativeMode::LoFi, kDrunkenGrooveRecipeId, view(kFeelDrunkenPocket), view(kBassBoomBap), view(kChordLoFi), view(kProgressionLoFi), view(kMelodicLoFi), view(kMotifLoFi), view(kPhraseBroken), {66,92,82,16,3,9}, CompositionSecondaryRole::ChordWithMelodicFill),
    profile(GenerativeMode::LoFi, kLoFiHouseRecipeId, view(kFeelLoFiHouse), view(kBassDrive), view(kChordLoFiHouse), view(kProgressionPop), view(kMelodicLoFi), view(kMotifLoFi), view(kPhraseDrive), {92,118,106,16,4,11}, CompositionSecondaryRole::ChordWithMelodicFill),
    profile(GenerativeMode::LoFi, kMinimalSleepRecipeId, view(kFeelLoFiPocket), view(kBassLoFi), view(kChordLoFi), view(kProgressionLoFi), view(kMelodicLoFi), view(kMotifLoFi), view(kPhraseSlow), {42,66,54,16,1,5}, CompositionSecondaryRole::ChordWithMelodicFill),
    profile(GenerativeMode::HipHop, kGoldenEraRecipeId, view(kFeelLoFiPocket), view(kBassBoomBap), view(kChordLoFi), view(kProgressionHipHop), view(kMelodicLoFi), view(kMotifLoFi), view(kPhraseBroken), {82,100,92,16,4,10}, CompositionSecondaryRole::ChordWithMelodicFill),
    profile(GenerativeMode::HipHop, kDustyJazzRecipeId, view(kFeelDrunkenPocket), view(kBassBoomBap), view(kChordLoFi), view(kProgressionHipHop), view(kMelodicLoFi), view(kMotifLoFi), view(kPhraseSlow), {70,94,84,16,3,9}, CompositionSecondaryRole::ChordWithMelodicFill),
};

const ProfileDefinition* definitionFor(const GenreSettings& settings) {
  if (settings.generativeMode >= kGenerativeModeCount) return nullptr;
  for (const ProfileDefinition& p : kProfiles) {
    if (p.generativeMode == settings.generativeMode && p.recipe == settings.recipe) return &p;
  }
  for (const ProfileDefinition& p : kProfiles) {
    if (p.generativeMode == settings.generativeMode && p.recipe == kBaseRecipeId) return &p;
  }
  return nullptr;
}

bool validView(WeightedIdentityView value, bool (*validId)(uint8_t)) {
  if (value.candidates == nullptr || value.count == 0 || value.count > kMaxWeightedCandidates) return false;
  for (uint8_t index = 0; index < value.count; ++index) {
    if (value.candidates[index].weight == 0 || !validId(value.candidates[index].id)) return false;
  }
  return true;
}

bool validFeel(uint8_t id) { return isValidFeelProfile(static_cast<FeelProfileId>(id)); }
bool validBass(uint8_t id) { return isValidBassRhythmId(static_cast<BassRhythmId>(id), false); }
bool validChord(uint8_t id) { return isValidChordRhythmId(static_cast<ChordRhythmId>(id), false); }
bool validProgression(uint8_t id) { return isValidProgressionId(static_cast<ProgressionId>(id), false); }
bool validMelodic(uint8_t id) { return isValidMelodicRhythmId(static_cast<MelodicRhythmId>(id), false); }
bool validMotif(uint8_t id) { return isValidMotifShapeId(static_cast<MotifShapeId>(id), false); }
bool validPhrase(uint8_t id) {
  const uint8_t bars = static_cast<uint8_t>(id & 0x0Fu);
  const uint8_t law = static_cast<uint8_t>(id >> 4u);
  return law < static_cast<uint8_t>(PhraseEvolutionLawId::Count) &&
         (bars == 1 || bars == 2 || bars == 4 || bars == 8);
}

uint32_t profileSalt(const GenerationProfileView& profile) {
  return (static_cast<uint32_t>(profile.generativeMode) << 24u) |
         (static_cast<uint32_t>(profile.recipe) << 16u);
}

}  // namespace

uint8_t availableRecipeCount(GenerativeMode genre) {
  const uint8_t genreId = static_cast<uint8_t>(genre);
  if (genreId >= kGenerativeModeCount) return 0;
  uint8_t count = 0;
  for (const ProfileDefinition& profile : kProfiles) {
    if (profile.generativeMode == genreId) ++count;
  }
  return count;
}

bool availableRecipeAt(GenerativeMode genre, uint8_t ordinal,
                       GenreRecipeId& recipe) {
  const uint8_t genreId = static_cast<uint8_t>(genre);
  if (genreId >= kGenerativeModeCount) return false;
  for (const ProfileDefinition& profile : kProfiles) {
    if (profile.generativeMode != genreId) continue;
    if (ordinal == 0) {
      recipe = profile.recipe;
      return true;
    }
    --ordinal;
  }
  return false;
}

bool isRecipeAvailable(GenerativeMode genre, GenreRecipeId recipe) {
  const uint8_t genreId = static_cast<uint8_t>(genre);
  if (genreId >= kGenerativeModeCount) return false;
  for (const ProfileDefinition& profile : kProfiles) {
    if (profile.generativeMode == genreId && profile.recipe == recipe) {
      return true;
    }
  }
  return false;
}

GenerationProfileView generationProfileFor(const GenreSettings& settings) {
  const ProfileDefinition* definition = definitionFor(settings);
  if (definition == nullptr) return {};
  GenerationProfileView result{};
  result.generativeMode = definition->generativeMode;
  result.recipe = definition->recipe;
  result.rhythms = rhythmCompatibilityFor(settings);
  result.feels = definition->feels;
  result.bassRhythms = definition->bass;
  result.chordRhythms = definition->chord;
  result.progressions = definition->progression;
  result.melodicRhythms = definition->melodic;
  result.motifShapes = definition->motif;
  result.phraseLaws = definition->phraseLaw;
  result.corridor = definition->corridor;
  result.secondaryRole = definition->secondaryRole;
  return result;
}

bool isValidGenerationProfile(const GenerationProfileView& profile) {
  if (profile.generativeMode >= kGenerativeModeCount ||
      profile.rhythms.candidates == nullptr || profile.rhythms.count == 0 ||
      profile.corridor.bpmMin < 30 ||
      profile.corridor.bpmMin > profile.corridor.suggestedBpm ||
      profile.corridor.suggestedBpm > profile.corridor.bpmMax ||
      (profile.corridor.gridSteps != 8 && profile.corridor.gridSteps != 16 &&
       profile.corridor.gridSteps != 32) ||
      profile.corridor.densityMin > profile.corridor.densityMax ||
      profile.corridor.densityMax > 16 ||
      static_cast<uint8_t>(profile.secondaryRole) >= static_cast<uint8_t>(CompositionSecondaryRole::Count)) {
    return false;
  }
  for (uint8_t index = 0; index < profile.rhythms.count; ++index) {
    const RhythmCompatibilityCandidate& c = profile.rhythms.candidates[index];
    if (c.weight == 0 || ReferenceVocabulary::definitionForId(c.archetypeId) == nullptr) return false;
  }
  return validView(profile.feels, validFeel) && validView(profile.bassRhythms, validBass) &&
         validView(profile.chordRhythms, validChord) && validView(profile.progressions, validProgression) &&
         validView(profile.melodicRhythms, validMelodic) && validView(profile.motifShapes, validMotif) &&
         validView(profile.phraseLaws, validPhrase);
}

bool selectWeightedIdentityFromView(
    WeightedIdentityView input, GenerationDomain domain,
    RhythmArchetypeId upstreamArchetype, uint32_t semanticSalt,
    const GenerationContext& generation, uint8_t& selectedId) {
  WeightedIdentityCandidate canonical[kMaxWeightedCandidates]{};
  uint8_t count = 0;
  if (input.candidates == nullptr || input.count == 0 || input.count > kMaxWeightedCandidates ||
      static_cast<uint8_t>(domain) >= static_cast<uint8_t>(GenerationDomain::Count)) return false;
  for (uint8_t index = 0; index < input.count; ++index) {
    const WeightedIdentityCandidate candidate = input.candidates[index];
    if (candidate.weight == 0) continue;
    uint8_t insertion = 0;
    while (insertion < count && canonical[insertion].id < candidate.id) ++insertion;
    if (insertion < count && canonical[insertion].id == candidate.id) {
      const uint16_t combined = static_cast<uint16_t>(canonical[insertion].weight) + candidate.weight;
      canonical[insertion].weight = static_cast<uint8_t>(combined > 255 ? 255 : combined);
      continue;
    }
    for (uint8_t move = count; move > insertion; --move) canonical[move] = canonical[move - 1u];
    canonical[insertion] = candidate;
    ++count;
  }
  uint16_t totalWeight = 0;
  for (uint8_t index = 0; index < count; ++index)
    totalWeight = static_cast<uint16_t>(totalWeight + canonical[index].weight);
  if (totalWeight == 0) return false;
  const uint32_t seed = deriveGenerationSeed(generation, upstreamArchetype, domain, semanticSalt);
  uint16_t coordinate = static_cast<uint16_t>(deterministicValue(seed, 0) % totalWeight);
  for (uint8_t index = 0; index < count; ++index) {
    if (coordinate < canonical[index].weight) {
      selectedId = canonical[index].id;
      return true;
    }
    coordinate = static_cast<uint16_t>(coordinate - canonical[index].weight);
  }
  return false;
}

GenerationCompositionResult resolveGenerationComposition(
    const GenreSettings& settings, const GenerationContext& generation) {
  GenerationCompositionResult result{};
  const GenerationProfileView profile = generationProfileFor(settings);
  if (profile.rhythms.candidates == nullptr) return result;
  if (!isValidGenerationProfile(profile)) {
    result.status = GenerationCompositionStatus::InvalidProfile;
    return result;
  }
  const RhythmSelectionResult rhythm = resolveRhythmSelection(settings, generation);
  if (rhythm.status != RhythmSelectionStatus::Ok) {
    result.status = GenerationCompositionStatus::NoCompatibleRhythm;
    return result;
  }
  result.rhythmSelectionMode = rhythm.mode;
  result.rhythmArchetypeId = rhythm.archetypeId;
  result.normalizedRhythmToAuto = rhythm.normalizedToAuto;
  result.corridor = profile.corridor;
  result.secondaryRole = profile.secondaryRole;

  const uint32_t baseSalt = profileSalt(profile);
  uint8_t feel=0,bass=0,chord=0,progression=0,melodic=0,motif=0,phraseChoice=0;
  if (!selectWeightedIdentityFromView(profile.feels, GenerationDomain::FeelProfileSelection, rhythm.archetypeId, baseSalt, generation, feel) ||
      !selectWeightedIdentityFromView(profile.bassRhythms, GenerationDomain::BassRhythmSelection, rhythm.archetypeId, baseSalt, generation, bass) ||
      !selectWeightedIdentityFromView(profile.chordRhythms, GenerationDomain::ChordRhythmSelection, rhythm.archetypeId, baseSalt | bass, generation, chord) ||
      !selectWeightedIdentityFromView(profile.progressions, GenerationDomain::ChordPitch, rhythm.archetypeId, static_cast<uint8_t>(ProgressionId::Auto), generation, progression) ||
      !selectWeightedIdentityFromView(profile.melodicRhythms, GenerationDomain::MelodicRhythmSelection, rhythm.archetypeId, baseSalt | (static_cast<uint32_t>(bass) << 8u) | chord, generation, melodic) ||
      !selectWeightedIdentityFromView(profile.motifShapes, GenerationDomain::MotifSelection, rhythm.archetypeId, baseSalt | melodic, generation, motif) ||
      !selectWeightedIdentityFromView(profile.phraseLaws, GenerationDomain::PhraseLawSelection, rhythm.archetypeId, baseSalt, generation, phraseChoice)) {
    result.status = GenerationCompositionStatus::InvalidProfile;
    return result;
  }
  result.suggestedFeel = static_cast<FeelProfileId>(feel);
  result.bassRhythm = static_cast<BassRhythmId>(bass);
  result.chordRhythm = static_cast<ChordRhythmId>(chord);
  result.progression = static_cast<ProgressionId>(progression);
  result.melodicRhythm = static_cast<MelodicRhythmId>(melodic);
  result.motifShape = static_cast<MotifShapeId>(motif);
  result.phraseLaw = static_cast<PhraseEvolutionLawId>(phraseChoice >> 4u);
  result.phraseBars = static_cast<uint8_t>(phraseChoice & 0x0Fu);
  result.status = GenerationCompositionStatus::Ok;
  return result;
}

const char* phraseEvolutionLawName(PhraseEvolutionLawId law) {
  switch (law) {
    case PhraseEvolutionLawId::Loop: return "LOOP";
    case PhraseEvolutionLawId::RepeatReply: return "REPEAT/REPLY";
    case PhraseEvolutionLawId::DevelopReturn: return "DEVELOP/RETURN";
    case PhraseEvolutionLawId::SparseDrift: return "SPARSE DRIFT";
    case PhraseEvolutionLawId::Count: break;
  }
  return "INVALID";
}

}  // namespace GroovePuterRhythm
