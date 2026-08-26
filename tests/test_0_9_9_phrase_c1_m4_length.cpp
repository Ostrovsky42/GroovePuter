#include <cassert>
#include <cstdint>
#include <iostream>

#include "../scenes.h"
#include "../src/dsp/genre_manager.h"
#include "../src/generation/composition/phrase_length_request.h"

using namespace GroovePuterRhythm;

int main() {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::LoFi);
  settings.recipe = 0;

  GenerationContext generation{};
  generation.projectSeed = 0x0C1C0A1u;
  generation.phraseOrdinal = 7;

  const PhraseLengthAdmissibility admissibility =
      phraseLengthAdmissibilityFor(settings, generation);
  assert(!phraseLengthAdmits(admissibility, 1));
  assert(!phraseLengthAdmits(admissibility, 2));
  assert(phraseLengthAdmits(admissibility, 4));
  assert(phraseLengthAdmits(admissibility, 8));

  constexpr uint8_t supported[] = {1, 2, 4, 8};
  for (uint8_t requested : supported) {
    const PhraseLengthRequestResult resolved =
        resolveGenerationCompositionForPhraseBars(settings, generation, requested);
    assert((resolved.status == PhraseLengthRequestStatus::Accepted) ==
           phraseLengthAdmits(admissibility, requested));
    if (resolved.status == PhraseLengthRequestStatus::Accepted) {
      assert(resolved.rejectReason == PhraseLengthRejectReason::None);
      assert(resolved.requestedPhraseBars == requested);
      assert(resolved.effectivePhraseBars == requested);
      assert(resolved.composition.phraseBars == requested);
    } else {
      assert(resolved.rejectReason ==
             PhraseLengthRejectReason::NoAdmissibleLawForRequestedLength);
      assert(resolved.effectivePhraseBars == 0);
    }
  }

  const PhraseLengthRequestResult invalid =
      resolveGenerationCompositionForPhraseBars(settings, generation, 3);
  assert(invalid.status == PhraseLengthRequestStatus::Rejected);
  assert(invalid.rejectReason ==
         PhraseLengthRejectReason::InvalidPhraseLengthDomain);
  assert(invalid.requestedPhraseBars == 3);
  assert(invalid.effectivePhraseBars == 0);

  const PhraseLengthRequestResult exact8 =
      resolveGenerationCompositionForPhraseBars(settings, generation, 8);
  assert(exact8.status == PhraseLengthRequestStatus::Accepted);
  assert(exact8.effectivePhraseBars == 8);

  std::cout << "PHRASE-C1 M4 exact length: PASS\n";
  std::cout << "lifecycle=caller_request_not_profile_phraseBars\n";
  std::cout << "lofi_admissible=4,8\n";
  std::cout << "request8_effective8=PASS\n";
  std::cout << "request3=REJECT:INVALID_PHRASE_LENGTH_DOMAIN\n";
  std::cout << "request2=REJECT:NO_ADMISSIBLE_LAW_FOR_REQUESTED_LENGTH\n";
  std::cout << "admissibility_generation_agreement=PASS\n";
  return 0;
}
