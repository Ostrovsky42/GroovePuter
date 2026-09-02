#include "phrase_length_request.h"

namespace GroovePuterRhythm {
namespace {

constexpr uint8_t kMaxExactPhraseLawCandidates = 16;

constexpr uint8_t phraseBarsFromCandidate(uint8_t id) {
  return static_cast<uint8_t>(id & 0x0Fu);
}

constexpr PhraseEvolutionLawId phraseLawFromCandidate(uint8_t id) {
  return static_cast<PhraseEvolutionLawId>(id >> 4u);
}

uint32_t phraseSelectionSalt(const GenerationProfileView& profile) {
  // Keep the existing GenerationProfile phrase-selection routing context.
  // This is deterministic selection plumbing, not a new musical policy.
  return (static_cast<uint32_t>(profile.generativeMode) << 24u) |
         (static_cast<uint32_t>(profile.recipe) << 16u);
}

PhraseLengthAdmissibility phraseLengthAdmissibilityFromProfile(
    const GenerationProfileView& profile) {
  PhraseLengthAdmissibility result{};
  if (!isValidGenerationProfile(profile)) return result;
  for (uint8_t index = 0; index < profile.phraseLaws.count; ++index) {
    const WeightedIdentityCandidate candidate = profile.phraseLaws.candidates[index];
    if (candidate.weight == 0) continue;
    result.mask = static_cast<uint8_t>(
        result.mask | phraseLengthBit(phraseBarsFromCandidate(candidate.id)));
  }
  return result;
}

}  // namespace

PhraseLengthAdmissibility phraseLengthAdmissibilityFor(
    const GenreSettings& settings,
    const GenerationContext& generation) {
  // GenerationContext is intentionally part of the semantic request API even
  // though today's profile vocabulary is context-invariant. Future production
  // overrides must enter this same request path rather than a UI-only query.
  (void)generation;
  return phraseLengthAdmissibilityFromProfile(generationProfileFor(settings));
}

PhraseLengthRequestResult resolveGenerationCompositionForPhraseBars(
    const GenreSettings& settings,
    const GenerationContext& generation,
    uint8_t requestedPhraseBars) {
  PhraseLengthRequestResult result{};
  result.requestedPhraseBars = requestedPhraseBars;

  if (!isSupportedPhraseLength(requestedPhraseBars)) {
    result.rejectReason = PhraseLengthRejectReason::InvalidPhraseLengthDomain;
    return result;
  }

  const GenerationProfileView profile = generationProfileFor(settings);
  if (!isValidGenerationProfile(profile)) {
    result.rejectReason = PhraseLengthRejectReason::CompositionResolutionFailed;
    return result;
  }

  const PhraseLengthAdmissibility admissibility =
      phraseLengthAdmissibilityFromProfile(profile);
  if (!phraseLengthAdmits(admissibility, requestedPhraseBars)) {
    result.rejectReason =
        PhraseLengthRejectReason::NoAdmissibleLawForRequestedLength;
    return result;
  }

  result.composition = resolveGenerationComposition(settings, generation);
  if (result.composition.status != GenerationCompositionStatus::Ok) {
    result.rejectReason = PhraseLengthRejectReason::CompositionResolutionFailed;
    return result;
  }

  WeightedIdentityCandidate exact[kMaxExactPhraseLawCandidates]{};
  uint8_t exactCount = 0;
  for (uint8_t index = 0; index < profile.phraseLaws.count; ++index) {
    const WeightedIdentityCandidate candidate = profile.phraseLaws.candidates[index];
    if (candidate.weight == 0 ||
        phraseBarsFromCandidate(candidate.id) != requestedPhraseBars) {
      continue;
    }
    if (exactCount >= kMaxExactPhraseLawCandidates) {
      result.rejectReason = PhraseLengthRejectReason::CompositionResolutionFailed;
      return result;
    }
    exact[exactCount++] = candidate;
  }

  uint8_t selectedPhrase = 0;
  const WeightedIdentityView exactView{exact, exactCount};
  if (!selectWeightedIdentityFromView(
          exactView,
          GenerationDomain::PhraseLawSelection,
          result.composition.rhythmArchetypeId,
          phraseSelectionSalt(profile),
          generation,
          selectedPhrase)) {
    result.rejectReason = PhraseLengthRejectReason::CompositionResolutionFailed;
    return result;
  }

  result.composition.phraseLaw = phraseLawFromCandidate(selectedPhrase);
  result.composition.phraseBars = phraseBarsFromCandidate(selectedPhrase);
  if (result.composition.phraseBars != requestedPhraseBars) {
    result.rejectReason = PhraseLengthRejectReason::CompositionResolutionFailed;
    return result;
  }

  result.effectivePhraseBars = result.composition.phraseBars;
  result.rejectReason = PhraseLengthRejectReason::None;
  result.status = PhraseLengthRequestStatus::Accepted;
  return result;
}

}  // namespace GroovePuterRhythm
