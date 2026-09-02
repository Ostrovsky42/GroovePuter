#include <cassert>
#include <cstdint>

#include "src/state/generated_phrase_product_state.h"
#include "src/state/phrase_generation_request_state.h"

int main() {
  using namespace GroovePuterState;

  (void)setRequestedPhraseBars(4);
  assert(requestedPhraseBars() == 4);
  assert(cycleRequestedPhraseBars(1) == 8);
  assert(cycleRequestedPhraseBars(1) == 1);
  assert(cycleRequestedPhraseBars(-1) == 8);
  assert(setRequestedPhraseBars(2));
  assert(requestedPhraseBars() == 2);
  assert(!setRequestedPhraseBars(2));

  // Unsupported raw request state fails closed and does not become another
  // successful request. Musical admissibility still belongs to P1R policy.
  assert(!setRequestedPhraseBars(3));
  assert(requestedPhraseBars() == 2);

  resetGeneratedPhraseProductState();
  const auto& empty = generatedPhraseProductState();
  assert(empty.lastOutcome == GeneratedPhraseOutcome::None);
  assert(!empty.accepted.valid);

  publishGeneratedPhraseAccepted(
      8, 8, 1, 15, 8, 12, true, false, 42, 3, 16);
  const auto accepted = generatedPhraseProductState();
  assert(accepted.lastOutcome == GeneratedPhraseOutcome::Accepted);
  assert(accepted.lastRequestedBars == 8);
  assert(accepted.accepted.valid);
  assert(accepted.accepted.bars == 8);
  assert(accepted.accepted.songSlot == 1);
  assert(accepted.accepted.pageIndex == 15);
  assert(accepted.accepted.firstLocalSlot == 8);
  assert(accepted.accepted.songStart == 12);
  assert(accepted.accepted.phraseGenerationIdentity == 42);
  assert(accepted.accepted.progression == 3);
  assert(accepted.accepted.harmonicEventPositions == 16);

  // A rejected/failing next request is a distinct outcome; it does not rewrite
  // the previously accepted physical/semantic snapshot into fake material.
  publishGeneratedPhraseTypedRejection(2);
  const auto rejected = generatedPhraseProductState();
  assert(rejected.lastOutcome == GeneratedPhraseOutcome::TypedRejection);
  assert(rejected.lastRequestedBars == 2);
  assert(rejected.accepted.valid);
  assert(rejected.accepted.phraseGenerationIdentity == 42);

  publishGeneratedPhraseExecutionFailure(4);
  const auto failed = generatedPhraseProductState();
  assert(failed.lastOutcome == GeneratedPhraseOutcome::ExecutionFailure);
  assert(failed.lastRequestedBars == 4);
  assert(failed.accepted.valid);

  static_assert(sizeof(GeneratedPhraseProductState) <= 24,
                "product state must stay fixed and small");
  return 0;
}
