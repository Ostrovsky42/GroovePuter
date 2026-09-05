#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "src/phrase/runtime_phrase_edit.h"

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

Buffer makePhrase(uint8_t bars,
                  uint16_t startTick,
                  uint16_t durationTicks) {
  Buffer phrase{};
  phrase.lengthTicks = static_cast<uint16_t>(bars * PhraseRuntime::kTicksPerBar);
  phrase.count = 1;
  phrase.events[0].startTick = startTick;
  phrase.events[0].durationSubticks =
      static_cast<uint16_t>(durationTicks * PhraseRuntime::kSubticksPerTick);
  phrase.events[0].note = 60;
  phrase.events[0].velocity = 100;
  phrase.events[0].probability = 100;
  return phrase;
}

bool same(const Buffer& lhs, const Buffer& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

void testGridChoicesAreExactMusicalQuanta() {
  assert(RuntimePhraseEdit::gridTicks(RuntimePhraseEdit::Grid::Eighth) == 48);
  assert(RuntimePhraseEdit::gridTicks(RuntimePhraseEdit::Grid::Sixteenth) == 24);
  assert(RuntimePhraseEdit::gridTicks(RuntimePhraseEdit::Grid::ThirtySecond) == 12);
}

void testExtendAndShortenByExactlyOneGrid() {
  auto phrase = makePhrase(2, 360, 96);

  assert(RuntimePhraseEdit::resizeEventByGrid(
             phrase, 0, +1, RuntimePhraseEdit::Grid::Sixteenth) ==
         RuntimePhraseEdit::EventEditResult::Changed);
  assert(phrase.count == 1);
  assert(phrase.events[0].startTick == 360);
  assert(phrase.events[0].durationSubticks ==
         120 * PhraseRuntime::kSubticksPerTick);
  assert(RuntimePhraseEdit::validate(phrase));

  assert(RuntimePhraseEdit::resizeEventByGrid(
             phrase, 0, -1, RuntimePhraseEdit::Grid::Eighth) ==
         RuntimePhraseEdit::EventEditResult::Changed);
  assert(phrase.events[0].durationSubticks ==
         72 * PhraseRuntime::kSubticksPerTick);
  assert(RuntimePhraseEdit::validate(phrase));
}

void testShortenCannotCreateZeroLengthEvent() {
  auto phrase = makePhrase(1, 120, 12);
  const auto before = phrase;

  assert(RuntimePhraseEdit::resizeEventByGrid(
             phrase, 0, -1, RuntimePhraseEdit::Grid::ThirtySecond) ==
         RuntimePhraseEdit::EventEditResult::Rejected);
  assert(same(phrase, before));
}

void testExtendCannotCrossPhraseEnd() {
  auto phrase = makePhrase(1, 360, 24);
  const auto before = phrase;

  assert(RuntimePhraseEdit::resizeEventByGrid(
             phrase, 0, +1, RuntimePhraseEdit::Grid::Sixteenth) ==
         RuntimePhraseEdit::EventEditResult::Rejected);
  assert(same(phrase, before));
}

void testMissingTargetAndInvalidDirectionAreNoMutation() {
  auto phrase = makePhrase(2, 360, 24);
  const auto before = phrase;

  assert(RuntimePhraseEdit::resizeEventByGrid(
             phrase, 7, +1, RuntimePhraseEdit::Grid::Sixteenth) ==
         RuntimePhraseEdit::EventEditResult::NoTarget);
  assert(same(phrase, before));

  assert(RuntimePhraseEdit::resizeEventByGrid(
             phrase, 0, 0, RuntimePhraseEdit::Grid::Sixteenth) ==
         RuntimePhraseEdit::EventEditResult::Rejected);
  assert(same(phrase, before));
}

}  // namespace

int main() {
  testGridChoicesAreExactMusicalQuanta();
  testExtendAndShortenByExactlyOneGrid();
  testShortenCannotCreateZeroLengthEvent();
  testExtendCannotCrossPhraseEnd();
  testMissingTargetAndInvalidDirectionAreNoMutation();
  std::puts("P3-U1 phrase duration/grid semantics: OK");
  return 0;
}
