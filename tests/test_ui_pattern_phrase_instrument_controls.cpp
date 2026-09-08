#include <cstdint>
#include <cstdio>

#include "src/ui/phrase_instrument_controls.h"

namespace {
int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "Pattern/Phrase instrument controls FAIL: %s\n", message);
  ++g_failures;
}

void testLengthCycle() {
  expect(PhraseInstrumentControls::nextLengthBars(1, 1) == 2,
         "1 bar did not advance to 2");
  expect(PhraseInstrumentControls::nextLengthBars(2, 1) == 4,
         "2 bars did not advance to 4");
  expect(PhraseInstrumentControls::nextLengthBars(4, 1) == 8,
         "4 bars did not advance to 8");
  expect(PhraseInstrumentControls::nextLengthBars(8, 1) == 1,
         "8 bars did not wrap to 1");
  expect(PhraseInstrumentControls::nextLengthBars(1, -1) == 8,
         "reverse length cycle did not wrap to 8");
}

void testGridIsFiniteMusicalSelector() {
  const uint16_t lengthTicks = PhraseRuntime::kTicksPerBar;
  PhraseNotesCursor::State cursor{};
  cursor.grid = RuntimePhraseEdit::Grid::Eighth;

  cursor = PhraseNotesCursor::changeGrid(cursor, 1, lengthTicks);
  expect(cursor.grid == RuntimePhraseEdit::Grid::Sixteenth,
         "GRID forward did not move 1/8 -> 1/16");
  cursor = PhraseNotesCursor::changeGrid(cursor, 1, lengthTicks);
  expect(cursor.grid == RuntimePhraseEdit::Grid::ThirtySecond,
         "GRID forward did not move 1/16 -> 1/32");
  cursor = PhraseNotesCursor::changeGrid(cursor, 1, lengthTicks);
  expect(cursor.grid == RuntimePhraseEdit::Grid::Eighth,
         "GRID forward did not wrap 1/32 -> 1/8");

  cursor = PhraseNotesCursor::changeGrid(cursor, -1, lengthTicks);
  expect(cursor.grid == RuntimePhraseEdit::Grid::ThirtySecond,
         "GRID reverse did not wrap 1/8 -> 1/32");
  cursor = PhraseNotesCursor::changeGrid(cursor, -1, lengthTicks);
  expect(cursor.grid == RuntimePhraseEdit::Grid::Sixteenth,
         "GRID reverse did not move 1/32 -> 1/16");
  cursor = PhraseNotesCursor::changeGrid(cursor, -1, lengthTicks);
  expect(cursor.grid == RuntimePhraseEdit::Grid::Eighth,
         "GRID reverse did not move 1/16 -> 1/8");
}

void testBarNavigationIsCursorOnly() {
  PhraseNotesCursor::State cursor{};
  cursor.grid = RuntimePhraseEdit::Grid::Sixteenth;
  cursor.cell = 5;  // 120 ticks into bar 1.

  const uint16_t lengthTicks =
      static_cast<uint16_t>(4 * PhraseRuntime::kTicksPerBar);
  const auto next = PhraseInstrumentControls::jumpBar(cursor, 1, lengthTicks);

  expect(PhraseNotesCursor::focusBar(next) == 1,
         "next-bar navigation did not move to bar 2");
  expect(PhraseNotesCursor::tick(next) % PhraseRuntime::kTicksPerBar ==
             PhraseNotesCursor::tick(cursor) % PhraseRuntime::kTicksPerBar,
         "bar navigation did not preserve the within-bar edit position");

  const auto previous = PhraseInstrumentControls::jumpBar(next, -1, lengthTicks);
  expect(previous.cell == cursor.cell,
         "previous-bar navigation did not return to the original cursor");

  const auto atStart = PhraseInstrumentControls::jumpBar(cursor, -1, lengthTicks);
  expect(PhraseNotesCursor::focusBar(atStart) == 0,
         "previous-bar navigation escaped before bar 1");

  PhraseNotesCursor::State last = cursor;
  last.cell = static_cast<uint8_t>(
      3 * (PhraseRuntime::kTicksPerBar /
           PhraseNotesCursor::quantumTicks(last.grid)) + 5);
  const auto atEnd = PhraseInstrumentControls::jumpBar(last, 1, lengthTicks);
  expect(PhraseNotesCursor::focusBar(atEnd) == 3,
         "next-bar navigation escaped past the final bar");
}

void testLengthCommandDelegatesToDomain() {
  uint8_t requestedBars = 0;
  int calls = 0;
  const bool changed = PhraseInstrumentControls::applyLengthChange(
      static_cast<uint16_t>(2 * PhraseRuntime::kTicksPerBar), 1,
      [&](uint8_t bars) {
        ++calls;
        requestedBars = bars;
        return true;
      });

  expect(changed, "accepted domain length command was reported as rejected");
  expect(calls == 1, "length change did not call the domain exactly once");
  expect(requestedBars == 4,
         "length change did not request the next musical bar count");
}

void testRejectedLengthDoesNotPretendToChange() {
  const bool changed = PhraseInstrumentControls::applyLengthChange(
      static_cast<uint16_t>(4 * PhraseRuntime::kTicksPerBar), 1,
      [](uint8_t) { return false; });
  expect(!changed, "rejected domain length command was reported as changed");
}
}  // namespace

int main() {
  testLengthCycle();
  testGridIsFiniteMusicalSelector();
  testBarNavigationIsCursorOnly();
  testLengthCommandDelegatesToDomain();
  testRejectedLengthDoesNotPretendToChange();

  if (g_failures == 0) {
    std::printf("Pattern/Phrase instrument controls: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "Pattern/Phrase instrument controls: %d failure(s)\n",
               g_failures);
  return 1;
}
