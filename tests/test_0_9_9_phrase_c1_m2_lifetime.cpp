#include <cassert>
#include <iostream>

#include "../src/generation/composition/phrase_semantic_contract.h"

using namespace GroovePuterRhythm;

int main() {
  MelodicMotifPlan local{};
  local.onsets = static_cast<StepMask>(1u << 4u);
  MelodicCrossBarLifetime localLifetime{};
  uint8_t localBits = phraseNoteLifetimeStateBits(local, localLifetime);
  assert(phraseNoteLifetimeHas(localBits, PhraseNoteLifetimeState::StartedHere));
  assert(phraseNoteLifetimeHas(localBits, PhraseNoteLifetimeState::EndsHere));
  assert(!phraseNoteLifetimeHas(localBits, PhraseNoteLifetimeState::ContinuesOutOfThisBar));

  MelodicMotifPlan crossing{};
  crossing.onsets = static_cast<StepMask>(1u << 12u);
  MelodicCrossBarLifetime out{};
  out.continuesIntoNextBar = true;
  uint8_t outBits = phraseNoteLifetimeStateBits(crossing, out);
  assert(phraseNoteLifetimeHas(outBits, PhraseNoteLifetimeState::StartedHere));
  assert(phraseNoteLifetimeHas(outBits, PhraseNoteLifetimeState::ContinuesOutOfThisBar));
  assert(!phraseNoteLifetimeHas(outBits, PhraseNoteLifetimeState::EndsHere));
  assert(phraseLifetimeDecision(PhraseLifetimeBoundary::IntraPhraseBarAdvance, out) ==
         PhraseLifetimeDecision::Continue);

  MelodicMotifPlan entered{};
  MelodicCrossBarLifetime in{};
  in.entersFromPreviousBar = true;
  uint8_t inBits = phraseNoteLifetimeStateBits(entered, in);
  assert(phraseNoteLifetimeHas(inBits, PhraseNoteLifetimeState::ContinuesIntoThisBar));
  assert(phraseNoteLifetimeHas(inBits, PhraseNoteLifetimeState::EndsHere));

  assert(phraseLifetimeDecision(PhraseLifetimeBoundary::Stop, out) ==
         PhraseLifetimeDecision::Release);
  assert(phraseLifetimeDecision(PhraseLifetimeBoundary::OutsideLogicalPhrase, out) ==
         PhraseLifetimeDecision::Release);

  // The generation contract is structural state, never the legacy playback
  // note value -2. No note-number field exists in MelodicCrossBarLifetime.
  static_assert(sizeof(MelodicCrossBarLifetime) <= 2, "bounded lifetime carrier");

  std::cout << "PHRASE-C1 M2 lifetime: PASS\n";
  std::cout << "bar_local=STARTED_HERE|ENDS_HERE\n";
  std::cout << "cross_bar_out=STARTED_HERE|CONTINUES_OUT\n";
  std::cout << "cross_bar_in=CONTINUES_IN|ENDS_HERE\n";
  std::cout << "stop=RELEASE outside_phrase=RELEASE intra_phrase=CONTINUE\n";
  std::cout << "legacy_note_minus_2_generation_owner=NO\n";
  std::cout << "backend_neutral_decision=PASS\n";
  return 0;
}
