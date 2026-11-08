#ifndef GROOVEPUTER_GENERATION_PHRASE_PHRASE_EVOLUTION_H
#define GROOVEPUTER_GENERATION_PHRASE_PHRASE_EVOLUTION_H

#include <cstdint>
#include <type_traits>

#include "../rhythm/bar_evolution.h"
#include "../roles/bass_rhythm.h"
#include "../roles/chord_rhythm.h"
#include "../roles/melodic_motif.h"

namespace GroovePuterRhythm {

constexpr uint8_t kMaxProductionPhraseBars = 8;

enum class PhraseEvolutionStatus : uint8_t {
  Ok = 0,
  InvalidRequest,
  CoreEvolutionFailed,
  Count,
};

struct PhraseRoleIdentity {
  BassRhythmId bass = BassRhythmId::Auto;
  ChordRhythmId chord = ChordRhythmId::Auto;
  MelodicRhythmId melodic = MelodicRhythmId::Auto;
  MotifShapeId motif = MotifShapeId::Auto;
};

struct PhraseEvolutionRequest {
  const RhythmCatalogView* catalog = nullptr;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  uint8_t phraseBars = 0;
  RealizationLevel level = RealizationLevel::P1Canonical;
  GenerationContext generation{};
  // GF2-I4: resolved upstream once; PhraseEvolution forwards the value and
  // remains solely an orchestration layer over authoritative BarEvolution.
  uint8_t structuralDensityTarget = kNoStructuralDensityTarget;
  TrajectoryId requestedTrajectoryId = kNoTrajectoryId;
  const PhraseRhythmIdentity* reuseIdentity = nullptr;
  PhraseRoleIdentity roleIdentity{};
};

struct PhraseEvolutionResult {
  PhraseEvolutionStatus status = PhraseEvolutionStatus::InvalidRequest;
  BarEvolutionStatus coreStatus = BarEvolutionStatus::InvalidRequest;
  uint8_t barCount = 0;
  uint8_t segmentCount = 0;
  TrajectoryId segmentTrajectories[2]{};
  uint8_t variationHistoryMask = 0;
  PhraseRhythmIdentity rhythmIdentity{};
  PhraseRoleIdentity roleIdentity{};
  RhythmBarPlan bars[kMaxProductionPhraseBars]{};
};

// Fixed-capacity orchestration over the existing BarEvolution owner. 1/2/4
// bars use one core call. 8 bars use two deterministic 4-bar segments while
// reusing one PhraseRhythmIdentity. This function owns no Scene/Song/PhraseCore
// destination. This is an API-only, fixture-capable layer: the shipped
// ReferenceVocabulary currently supports one bar only. Production wiring also
// remains blocked until the Stage 6.1 ESP32-S3 task high-water gate is recorded.
PhraseEvolutionResult evolveMultiBarPhrase(
    const PhraseEvolutionRequest& request);

static_assert(std::is_trivially_copyable<PhraseRoleIdentity>::value,
              "PhraseRoleIdentity must remain fixed-capacity");
static_assert(std::is_trivially_copyable<PhraseEvolutionResult>::value,
              "PhraseEvolutionResult must remain fixed-capacity");
static_assert(sizeof(PhraseEvolutionResult) <= 1408,
              "PhraseEvolutionResult exceeded its command-time RAM budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_PHRASE_PHRASE_EVOLUTION_H
