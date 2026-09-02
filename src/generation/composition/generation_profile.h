#ifndef GROOVEPUTER_GENERATION_COMPOSITION_GENERATION_PROFILE_H
#define GROOVEPUTER_GENERATION_COMPOSITION_GENERATION_PROFILE_H

#include <cstdint>
#include <type_traits>

#include "../feel/feel_types.h"
#include "../roles/bass_rhythm.h"
#include "../roles/chord_progression.h"
#include "../roles/chord_rhythm.h"
#include "../roles/melodic_motif.h"
#include "rhythm_selection.h"

struct GenreSettings;

namespace GroovePuterRhythm {

enum class PhraseEvolutionLawId : uint8_t {
  Loop = 0,
  RepeatReply,
  DevelopReturn,
  SparseDrift,
  Count,
};

// One physical Synth B remains the destination. Hybrid means chord topology is
// primary and sparse melodic events may fill only genuinely empty secondary
// steps; it does not pretend that Cardputer has a third synth voice.
enum class CompositionSecondaryRole : uint8_t {
  Chord = 0,
  Melodic,
  ChordWithMelodicFill,
  Count,
};

struct WeightedIdentityCandidate {
  uint8_t id = 0;
  uint8_t weight = 0;
};

struct WeightedIdentityView {
  const WeightedIdentityCandidate* candidates = nullptr;
  uint8_t count = 0;
};

struct GenerationCorridor {
  uint16_t bpmMin = 0;
  uint16_t bpmMax = 0;
  uint16_t suggestedBpm = 0;
  uint8_t gridSteps = 16;
  uint8_t densityMin = 0;
  uint8_t densityMax = 16;
};

struct GenerationProfileView {
  uint8_t generativeMode = 0;
  uint8_t recipe = 0;
  RhythmCompatibilityView rhythms{};
  WeightedIdentityView feels{};
  WeightedIdentityView bassRhythms{};
  WeightedIdentityView chordRhythms{};
  WeightedIdentityView progressions{};
  WeightedIdentityView melodicRhythms{};
  WeightedIdentityView motifShapes{};
  WeightedIdentityView phraseLaws{};
  GenerationCorridor corridor{};
  CompositionSecondaryRole secondaryRole = CompositionSecondaryRole::Melodic;
};

enum class GenerationCompositionStatus : uint8_t {
  Ok = 0,
  NoProfile,
  InvalidProfile,
  NoCompatibleRhythm,
  Count,
};

struct GenerationCompositionResult {
  GenerationCompositionStatus status = GenerationCompositionStatus::NoProfile;
  RhythmSelectionMode rhythmSelectionMode = RhythmSelectionMode::Auto;
  RhythmArchetypeId rhythmArchetypeId = kNoArchetypeId;
  bool normalizedRhythmToAuto = false;
  FeelProfileId suggestedFeel = FeelProfileId::Straight;
  BassRhythmId bassRhythm = BassRhythmId::Auto;
  ChordRhythmId chordRhythm = ChordRhythmId::Auto;
  ProgressionId progression = ProgressionId::Auto;
  MelodicRhythmId melodicRhythm = MelodicRhythmId::Auto;
  MotifShapeId motifShape = MotifShapeId::Auto;
  // Planning metadata only. Production multi-bar execution remains blocked by
  // the documented Stage 6.1 physical hardware gate.
  PhraseEvolutionLawId phraseLaw = PhraseEvolutionLawId::Loop;
  uint8_t phraseBars = 1;
  GenerationCorridor corridor{};
  CompositionSecondaryRole secondaryRole = CompositionSecondaryRole::Melodic;
};

GenerationProfileView generationProfileFor(const GenreSettings& settings);
bool isValidGenerationProfile(const GenerationProfileView& profile);

bool selectWeightedIdentityFromView(
    WeightedIdentityView view,
    GenerationDomain domain,
    RhythmArchetypeId upstreamArchetype,
    uint32_t semanticSalt,
    const GenerationContext& generation,
    uint8_t& selectedId);

GenerationCompositionResult resolveGenerationComposition(
    const GenreSettings& settings,
    const GenerationContext& generation);

const char* phraseEvolutionLawName(PhraseEvolutionLawId law);

static_assert(std::is_trivially_copyable<GenerationCompositionResult>::value,
              "GenerationCompositionResult must remain fixed-capacity");
static_assert(sizeof(GenerationCompositionResult) <= 32,
              "GenerationCompositionResult exceeded its planning budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_GENERATION_PROFILE_H
