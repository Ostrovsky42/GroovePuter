#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "src/phrase/runtime_phrase_edit.h"

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

bool same(const Buffer& lhs, const Buffer& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

Buffer crossBarPhrase() {
  Buffer phrase{};
  phrase.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  phrase.count = 1;
  phrase.events[0].startTick = 360;
  phrase.events[0].durationSubticks =
      96 * PhraseRuntime::kSubticksPerTick;
  phrase.events[0].note = 60;
  phrase.events[0].velocity = 100;
  phrase.events[0].probability = 100;
  return phrase;
}

void testSupportedLengthsAreExact() {
  assert(RuntimePhraseEdit::lengthTicksForBars(1) == 384);
  assert(RuntimePhraseEdit::lengthTicksForBars(2) == 768);
  assert(RuntimePhraseEdit::lengthTicksForBars(4) == 1536);
  assert(RuntimePhraseEdit::lengthTicksForBars(8) == 3072);
  assert(RuntimePhraseEdit::lengthTicksForBars(3) == 0);
}

void testUnsafeShrinkRejectsWithoutTruncation() {
  auto phrase = crossBarPhrase();
  const auto before = phrase;

  assert(RuntimePhraseEdit::setLengthBars(phrase, 1) ==
         RuntimePhraseEdit::LengthEditResult::Rejected);
  assert(same(phrase, before));
  assert(phrase.events[0].startTick == 360);
  assert(phrase.events[0].durationSubticks ==
         96 * PhraseRuntime::kSubticksPerTick);
}

void testSafeShrinkCommitsWholeBufferShape() {
  Buffer phrase{};
  phrase.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  phrase.count = 1;
  phrase.events[0].startTick = 120;
  phrase.events[0].durationSubticks =
      24 * PhraseRuntime::kSubticksPerTick;
  phrase.events[0].note = 64;
  phrase.events[0].velocity = 100;
  phrase.events[0].probability = 100;

  assert(RuntimePhraseEdit::setLengthBars(phrase, 1) ==
         RuntimePhraseEdit::LengthEditResult::Changed);
  assert(phrase.lengthTicks == PhraseRuntime::kTicksPerBar);
  assert(phrase.count == 1);
  assert(phrase.events[0].startTick == 120);
  assert(RuntimePhraseEdit::validate(phrase));
}

void testGrowPreservesMaterialAndInvalidLengthIsNoMutation() {
  Buffer phrase{};
  phrase.lengthTicks = PhraseRuntime::kTicksPerBar;
  phrase.count = 1;
  phrase.events[0].startTick = 240;
  phrase.events[0].durationSubticks =
      48 * PhraseRuntime::kSubticksPerTick;
  phrase.events[0].note = 67;
  phrase.events[0].velocity = 90;
  phrase.events[0].probability = 100;
  const auto original = phrase;

  assert(RuntimePhraseEdit::setLengthBars(phrase, 8) ==
         RuntimePhraseEdit::LengthEditResult::Changed);
  assert(phrase.lengthTicks == 8 * PhraseRuntime::kTicksPerBar);
  assert(phrase.count == original.count);
  assert(std::memcmp(phrase.events, original.events,
                     sizeof(phrase.events)) == 0);
  assert(RuntimePhraseEdit::validate(phrase));

  const auto grown = phrase;
  assert(RuntimePhraseEdit::setLengthBars(phrase, 3) ==
         RuntimePhraseEdit::LengthEditResult::Rejected);
  assert(same(phrase, grown));

  assert(RuntimePhraseEdit::setLengthBars(phrase, 8) ==
         RuntimePhraseEdit::LengthEditResult::NoChange);
  assert(same(phrase, grown));
}

}  // namespace

int main() {
  testSupportedLengthsAreExact();
  testUnsafeShrinkRejectsWithoutTruncation();
  testSafeShrinkCommitsWholeBufferShape();
  testGrowPreservesMaterialAndInvalidLengthIsNoMutation();
  std::puts("P3-U1 phrase length semantics: OK");
  return 0;
}
