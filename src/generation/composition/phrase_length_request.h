#ifndef GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_LENGTH_REQUEST_H
#define GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_LENGTH_REQUEST_H

#include <cstdint>
#include <type_traits>

#include "generation_profile.h"

struct GenreSettings;

namespace GroovePuterRhythm {

constexpr uint8_t kPhraseLength1Bit = 1u << 0u;
constexpr uint8_t kPhraseLength2Bit = 1u << 1u;
constexpr uint8_t kPhraseLength4Bit = 1u << 2u;
constexpr uint8_t kPhraseLength8Bit = 1u << 3u;

struct PhraseLengthAdmissibility {
  uint8_t mask = 0;
};

enum class PhraseLengthRequestStatus : uint8_t {
  Accepted = 0,
  Rejected,
  Count,
};

enum class PhraseLengthRejectReason : uint8_t {
  None = 0,
  InvalidPhraseLengthDomain,
  NoAdmissibleLawForRequestedLength,
  CompositionResolutionFailed,
  Count,
};

struct PhraseLengthRequestResult {
  PhraseLengthRequestStatus status = PhraseLengthRequestStatus::Rejected;
  PhraseLengthRejectReason rejectReason =
      PhraseLengthRejectReason::CompositionResolutionFailed;
  uint8_t requestedPhraseBars = 0;
  uint8_t effectivePhraseBars = 0;
  GenerationCompositionResult composition{};
};

constexpr bool isSupportedPhraseLength(uint8_t phraseBars) {
  return phraseBars == 1 || phraseBars == 2 || phraseBars == 4 || phraseBars == 8;
}

constexpr uint8_t phraseLengthBit(uint8_t phraseBars) {
  return phraseBars == 1 ? kPhraseLength1Bit
       : phraseBars == 2 ? kPhraseLength2Bit
       : phraseBars == 4 ? kPhraseLength4Bit
       : phraseBars == 8 ? kPhraseLength8Bit
                         : 0;
}

constexpr bool phraseLengthAdmits(const PhraseLengthAdmissibility& admissibility,
                                  uint8_t phraseBars) {
  const uint8_t bit = phraseLengthBit(phraseBars);
  return bit != 0 && (admissibility.mask & bit) != 0;
}

PhraseLengthAdmissibility phraseLengthAdmissibilityFor(
    const GenreSettings& settings,
    const GenerationContext& generation);

PhraseLengthRequestResult resolveGenerationCompositionForPhraseBars(
    const GenreSettings& settings,
    const GenerationContext& generation,
    uint8_t requestedPhraseBars);

static_assert(std::is_trivially_copyable<PhraseLengthAdmissibility>::value,
              "phrase admissibility must remain fixed-capacity");
static_assert(std::is_trivially_copyable<PhraseLengthRequestResult>::value,
              "phrase length result must remain fixed-capacity");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_LENGTH_REQUEST_H
