// U4C1: LEFT/RIGHT select the previous/next sound, not the next grid step.
//
// Stepping by grid meant a beginner pressed RIGHT four times to reach the next
// note on a 1/32 grid, landing on empty ticks in between with "NO SOUND HERE"
// under the editor. Selecting sounds is what the screen is for.
//
// This is deliberately NOT a change of selection identity. U4B2's law stands:
// the selected object is derived from cursor coverage, and no buffer event
// index is ever persisted. What moves is the cursor -- it lands on onsets
// instead of on grid multiples -- so the derived selection, the continuity
// state and the insertion position all keep working unchanged.

#include <cstdint>
#include <cstdio>

#include "src/ui/phrase_notes_cursor.h"
#include "src/ui/phrase_notes_selection.h"

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "U4C1 FAIL: %s\n", message);
  ++g_failures;
}

Buffer makePhrase() {
  Buffer phrase{};
  phrase.lengthTicks = PhraseRuntime::kTicksPerBar;   // 384
  phrase.count = 3;
  const uint16_t starts[3] = {0, 96, 264};
  for (uint16_t i = 0; i < 3; ++i) {
    phrase.events[i].startTick = starts[i];
    phrase.events[i].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
    phrase.events[i].note = static_cast<uint8_t>(60 + i);
    phrase.events[i].velocity = 100;
    phrase.events[i].probability = 100;
  }
  return phrase;
}

uint16_t tickAfterMove(const Buffer& phrase, uint16_t fromTick, int direction) {
  PhraseNotesCursor::State state{};
  state.grid = RuntimePhraseEdit::Grid::ThirtySecond;
  state.cell = static_cast<uint8_t>(
      fromTick / PhraseNotesCursor::quantumTicks(state.grid));
  return PhraseNotesCursor::tick(
      PhraseNotesCursor::moveToOnset(state, phrase, direction));
}

}  // namespace

int main() {
  const Buffer phrase = makePhrase();

  // 1. Forward lands on the next onset, however far away, in one press.
  expect(tickAfterMove(phrase, 0, 1) == 96,
         "RIGHT did not reach the next sound in one press");
  expect(tickAfterMove(phrase, 96, 1) == 264,
         "RIGHT did not cross the gap to the next sound");

  // 2. Backward is symmetric.
  expect(tickAfterMove(phrase, 264, -1) == 96, "LEFT skipped a sound");
  expect(tickAfterMove(phrase, 96, -1) == 0, "LEFT did not reach the first sound");

  // 3. From between two sounds, direction decides which one.
  expect(tickAfterMove(phrase, 150, 1) == 264,
         "RIGHT from empty time did not go forward");
  expect(tickAfterMove(phrase, 150, -1) == 96,
         "LEFT from empty time did not go back");

  // 4. The ends hold still rather than wrapping. Wrapping would make the last
  //    press before the end of a melody jump to its beginning, which reads as
  //    a glitch rather than a move.
  expect(tickAfterMove(phrase, 264, 1) == 264, "RIGHT wrapped past the last sound");
  expect(tickAfterMove(phrase, 0, -1) == 0, "LEFT wrapped before the first sound");

  // 5. An empty melody has nothing to select and must not move or crash: the
  //    cursor stays where it is so Enter still has a position to add at.
  {
    Buffer empty{};
    empty.lengthTicks = PhraseRuntime::kTicksPerBar;
    expect(tickAfterMove(empty, 48, 1) == 48,
           "moving in an empty melody displaced the insertion point");
    expect(tickAfterMove(empty, 48, -1) == 48,
           "moving back in an empty melody displaced the insertion point");
  }

  // 6. Only +/-1 is a direction; anything else is a no-op rather than a guess.
  {
    PhraseNotesCursor::State state{};
    state.grid = RuntimePhraseEdit::Grid::ThirtySecond;
    state.cell = 8;
    const PhraseNotesCursor::State unchanged =
        PhraseNotesCursor::moveToOnset(state, phrase, 0);
    expect(PhraseNotesCursor::tick(unchanged) == PhraseNotesCursor::tick(state),
           "a zero direction moved the cursor");
  }

  // 7. Off-grid onsets must be selectable. This is the case the first version
  //    of this file missed by using only grid-aligned starts: swing and
  //    micro-timing put projected onsets between grid cells by construction,
  //    so a cursor that addresses cells while selection reads a single tick
  //    selects nothing at all -- pressing RIGHT would land on a note and
  //    report "NO SOUND HERE".
  {
    Buffer offGrid{};
    offGrid.lengthTicks = PhraseRuntime::kTicksPerBar;
    offGrid.count = 2;
    offGrid.events[0].startTick = 0;
    offGrid.events[1].startTick = 100;   // not a multiple of any grid
    for (uint16_t i = 0; i < 2; ++i) {
      offGrid.events[i].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
      offGrid.events[i].note = static_cast<uint8_t>(60 + i);
      offGrid.events[i].velocity = 100;
      offGrid.events[i].probability = 100;
    }

    PhraseNotesCursor::State state{};
    state.grid = RuntimePhraseEdit::Grid::Sixteenth;
    state = PhraseNotesCursor::moveToOnset(state, offGrid, 1);
    const uint16_t landed = PhraseNotesCursor::tick(state);
    const uint16_t quantum = PhraseNotesCursor::quantumTicks(state.grid);
    const auto selection =
        PhraseNotesSelection::deriveInCell(offGrid, landed, quantum);
    expect(selection.active, "an off-grid sound could not be selected at all");
    expect(selection.active && selection.eventIndex == 1,
           "moving right selected the wrong sound");
  }

  if (g_failures == 0) {
    std::printf("UI Constitution U4C1 onset navigation: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "UI Constitution U4C1 onset navigation: %d failure(s)\n",
               g_failures);
  return 1;
}
