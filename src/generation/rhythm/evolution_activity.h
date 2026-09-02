#ifndef GROOVEPUTER_GENERATION_RHYTHM_EVOLUTION_ACTIVITY_H
#define GROOVEPUTER_GENERATION_RHYTHM_EVOLUTION_ACTIVITY_H

#include <cstdint>
#include <type_traits>

#include "../generation_context.h"

namespace GroovePuterRhythm {

constexpr uint8_t kEvolutionCadenceSegmentBars = 4;

enum class EvolutionActivity : uint8_t {
  Off = 0,
  Low,
  Medium,
  High,
  Count,
};

enum class EvolutionCadenceDecision : uint8_t {
  Hold = 0,
  Attempt,
  Count,
};

constexpr bool isValidEvolutionActivity(EvolutionActivity activity) {
  return static_cast<uint8_t>(activity) <
         static_cast<uint8_t>(EvolutionActivity::Count);
}

constexpr uint8_t evolutionCadenceThreshold(EvolutionActivity activity) {
  switch (activity) {
    case EvolutionActivity::Low:
      return 1;
    case EvolutionActivity::Medium:
      return 2;
    case EvolutionActivity::High:
      return 4;
    case EvolutionActivity::Off:
    default:
      return 0;
  }
}

inline EvolutionCadenceDecision evolutionCadenceDecision(
    EvolutionActivity activity,
    uint8_t phraseBarOrdinal,
    uint8_t evolutionOrdinal,
    const GenerationContext& generation) {
  if (!isValidEvolutionActivity(activity) ||
      phraseBarOrdinal % kEvolutionCadenceSegmentBars != 0u ||
      phraseBarOrdinal / kEvolutionCadenceSegmentBars != evolutionOrdinal) {
    return EvolutionCadenceDecision::Hold;
  }

  const uint8_t threshold = evolutionCadenceThreshold(activity);
  if (threshold == 0u) {
    return EvolutionCadenceDecision::Hold;
  }
  if (threshold == 4u) {
    return EvolutionCadenceDecision::Attempt;
  }

  constexpr uint32_t kCadenceSeedSalt = 0x45325443u;  // "E2TC"
  const uint32_t phraseCadenceSeed = deterministicValue(
      generation.projectSeed ^ kCadenceSeedSalt,
      static_cast<uint32_t>(generation.phraseOrdinal));
  const uint8_t bucket = static_cast<uint8_t>(
      deterministicValue(phraseCadenceSeed, evolutionOrdinal) & 0x03u);
  return bucket < threshold ? EvolutionCadenceDecision::Attempt
                            : EvolutionCadenceDecision::Hold;
}

static_assert(kEvolutionCadenceSegmentBars == 4,
              "evolution cadence must preserve the E0a four-bar segment bound");
static_assert(std::is_enum<EvolutionActivity>::value,
              "EvolutionActivity must remain a bounded enum");
static_assert(std::is_enum<EvolutionCadenceDecision>::value,
              "EvolutionCadenceDecision must remain a bounded enum");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_RHYTHM_EVOLUTION_ACTIVITY_H
