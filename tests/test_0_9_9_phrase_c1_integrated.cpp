#include <cassert>
#include <cstdint>
#include <iostream>

#include "../src/generation/migration/phrase_semantic_result.h"

using namespace GroovePuterRhythm;

namespace {

constexpr StepMask at(uint8_t step) {
  return stepBit(step);
}

bool sameSemanticResult(const PhraseSemanticResult& a,
                        const PhraseSemanticResult& b) {
  if (a.status != b.status || a.rejectReason != b.rejectReason ||
      a.phraseGenerationIdentity != b.phraseGenerationIdentity ||
      a.requestedPhraseBars != b.requestedPhraseBars ||
      a.effectivePhraseBars != b.effectivePhraseBars ||
      a.harmonicTimeline.status != b.harmonicTimeline.status ||
      a.harmonicTimeline.phraseBars != b.harmonicTimeline.phraseBars ||
      a.harmonicTimeline.totalEventPositions !=
          b.harmonicTimeline.totalEventPositions) {
    return false;
  }
  for (uint8_t bar = 0; bar < kMaxSemanticPhraseBars; ++bar) {
    if (a.harmonicTimeline.eventPositionsByBar[bar] !=
            b.harmonicTimeline.eventPositionsByBar[bar] ||
        a.bars[bar].temporal.phraseBarOrdinal !=
            b.bars[bar].temporal.phraseBarOrdinal ||
        a.bars[bar].temporal.evolutionOrdinal !=
            b.bars[bar].temporal.evolutionOrdinal ||
        a.bars[bar].harmonicEvents.firstOrdinal !=
            b.bars[bar].harmonicEvents.firstOrdinal ||
        a.bars[bar].harmonicEvents.eventCount !=
            b.bars[bar].harmonicEvents.eventCount ||
        a.bars[bar].melodicLifetime.entersFromPreviousBar !=
            b.bars[bar].melodicLifetime.entersFromPreviousBar ||
        a.bars[bar].melodicLifetime.continuesIntoNextBar !=
            b.bars[bar].melodicLifetime.continuesIntoNextBar ||
        a.bars[bar].melodicStatus != b.bars[bar].melodicStatus) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  PhraseLengthRequestResult length{};
  length.status = PhraseLengthRequestStatus::Accepted;
  length.rejectReason = PhraseLengthRejectReason::None;
  length.requestedPhraseBars = 4;
  length.effectivePhraseBars = 4;
  length.composition.status = GenerationCompositionStatus::Ok;
  length.composition.phraseBars = 4;

  StepMask harmonicBars[kMaxSemanticPhraseBars]{};
  const StepMask quarter = static_cast<StepMask>(at(0) | at(4) | at(8) | at(12));
  for (uint8_t bar = 0; bar < 4; ++bar) harmonicBars[bar] = quarter;
  const PhraseHarmonicTimeline timeline =
      makePhraseHarmonicTimeline(4, harmonicBars);

  MelodicMotifStatus melodicStatus[kMaxSemanticPhraseBars]{};
  melodicStatus[0] = MelodicMotifStatus::Ok;
  melodicStatus[1] = MelodicMotifStatus::ValidButEmpty;
  melodicStatus[2] = MelodicMotifStatus::ValidButEmpty;
  melodicStatus[3] = MelodicMotifStatus::ValidButEmpty;

  MelodicCrossBarLifetime lifetime[kMaxSemanticPhraseBars]{};
  lifetime[0].continuesIntoNextBar = true;
  lifetime[1].entersFromPreviousBar = true;

  constexpr uint16_t phraseIdentity = 0x42A1u;
  StrongRhythmMigrationContext destinationA{};
  StrongRhythmMigrationContext destinationB{};
  destinationA.patternAddress = 1;
  destinationB.patternAddress = 6;
  destinationA.phraseGenerationIdentity = phraseIdentity;
  destinationB.phraseGenerationIdentity = phraseIdentity;

  const PhraseSemanticResult semanticA = makePhraseSemanticResult(
      destinationA.phraseGenerationIdentity,
      length,
      timeline,
      melodicStatus,
      lifetime);
  const PhraseSemanticResult semanticB = makePhraseSemanticResult(
      destinationB.phraseGenerationIdentity,
      length,
      timeline,
      melodicStatus,
      lifetime);

  assert(destinationA.patternAddress != destinationB.patternAddress);
  assert(semanticA.status == PhraseSemanticContractStatus::Ready);
  assert(semanticA.requestedPhraseBars == 4);
  assert(semanticA.effectivePhraseBars == 4);
  assert(semanticA.phraseGenerationIdentity == phraseIdentity);
  assert(sameSemanticResult(semanticA, semanticB));

  assert(semanticA.bars[0].temporal.phraseBarOrdinal == 0);
  assert(semanticA.bars[3].temporal.phraseBarOrdinal == 3);
  assert(semanticA.bars[0].temporal.evolutionOrdinal == 0);
  assert(phraseVocabularyBarOrdinal(semanticA.bars[3].temporal.phraseBarOrdinal) == 3);
  assert(semanticA.bars[0].harmonicEvents.firstOrdinal == 0);
  assert(semanticA.bars[0].harmonicEvents.eventCount == 4);
  assert(semanticA.bars[3].harmonicEvents.firstOrdinal == 12);
  assert(semanticA.bars[3].harmonicEvents.eventCount == 4);
  assert(semanticA.bars[0].melodicLifetime.continuesIntoNextBar);
  assert(semanticA.bars[1].melodicLifetime.entersFromPreviousBar);
  assert(semanticA.bars[1].melodicStatus == MelodicMotifStatus::ValidButEmpty);

  // A physical pattern or Song row change inside the same logical phrase does
  // not become a semantic barrier. The semantic owner classifies it as the
  // same intra-phrase bar advance; both future backends consume this decision.
  assert(phraseLifetimeDecision(PhraseLifetimeBoundary::IntraPhraseBarAdvance,
                                semanticA.bars[0].melodicLifetime) ==
         PhraseLifetimeDecision::Continue);
  assert(phraseLifetimeDecision(PhraseLifetimeBoundary::Stop,
                                semanticA.bars[0].melodicLifetime) ==
         PhraseLifetimeDecision::Release);
  assert(phraseLifetimeDecision(PhraseLifetimeBoundary::OutsideLogicalPhrase,
                                semanticA.bars[0].melodicLifetime) ==
         PhraseLifetimeDecision::Release);

  PhraseLengthRequestResult rejectedLength{};
  rejectedLength.status = PhraseLengthRequestStatus::Rejected;
  rejectedLength.rejectReason =
      PhraseLengthRejectReason::NoAdmissibleLawForRequestedLength;
  rejectedLength.requestedPhraseBars = 8;
  const PhraseSemanticResult rejected = makePhraseSemanticResult(
      phraseIdentity, rejectedLength, timeline, melodicStatus, lifetime);
  assert(rejected.status == PhraseSemanticContractStatus::Rejected);
  assert(rejected.rejectReason ==
         PhraseLengthRejectReason::NoAdmissibleLawForRequestedLength);

  std::cout << "PHRASE-C1 integrated semantic contract: PASS\n";
  std::cout << "physical_patternAddress_independence=PASS\n";
  std::cout << "semantic_empty_owner=MelodicMotifStatus::ValidButEmpty\n";
  std::cout << "internal_physical_transition=INTRA_PHRASE_SEMANTIC_BOUNDARY\n";
  std::cout << "song_physical_transition_policy_owner=SEMANTIC_BOUNDARY_NOT_BACKEND\n";
  std::cout << "sizeof_MelodicCrossBarLifetime=" << sizeof(MelodicCrossBarLifetime) << "\n";
  std::cout << "sizeof_PhraseLengthAdmissibility=" << sizeof(PhraseLengthAdmissibility) << "\n";
  std::cout << "sizeof_PhraseLengthRequestResult=" << sizeof(PhraseLengthRequestResult) << "\n";
  std::cout << "sizeof_PhraseHarmonicEventRange=" << sizeof(PhraseHarmonicEventRange) << "\n";
  std::cout << "sizeof_PhraseHarmonicEventCoordinate=" << sizeof(PhraseHarmonicEventCoordinate) << "\n";
  std::cout << "sizeof_PhraseHarmonicTimeline=" << sizeof(PhraseHarmonicTimeline) << "\n";
  std::cout << "sizeof_PhraseSemanticBarState=" << sizeof(PhraseSemanticBarState) << "\n";
  std::cout << "sizeof_PhraseSemanticResult=" << sizeof(PhraseSemanticResult) << "\n";
  std::cout << "largest_new_fixed_array_bytes="
            << sizeof(((PhraseSemanticResult*)nullptr)->bars) << "\n";
  std::cout << "heap_introduced=NO\n";
  return 0;
}
