// U4C1: LEFT/RIGHT walk the grid, and every cell can hold a selection.
//
// This file first pinned the opposite rule -- one press jumps to the next
// onset -- and that rule was wrong in use. It read well in dense material and
// broke everything else: a gap in the middle of a melody was jumped over
// entirely, so there was nowhere to stand to add a sound, and an emptied
// melody trapped the cursor with nothing to jump to. Reported from the device,
// not caught here, because the fixture only ever had sounds to jump between.
//
// What survives from that work, and is the part that mattered, is selecting by
// cell rather than by the tick at its left edge: onsets do not have to sit on
// the grid -- swing and micro-timing move them off it by construction -- and a
// cell that contains one selects it.

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
  (void)phrase;
  return PhraseNotesCursor::tick(
      PhraseNotesCursor::move(state, direction, PhraseRuntime::kTicksPerBar));
}

}  // namespace

int main() {
  const Buffer phrase = makePhrase();

  // 1. One press, one cell. At a 1/32 grid (12 ticks) that is 12 ticks, and
  //    the sound at 96 is not teleported to.
  expect(tickAfterMove(phrase, 0, 1) == 12, "RIGHT moved more than one cell");
  expect(tickAfterMove(phrase, 84, 1) == 96,
         "RIGHT did not arrive on the cell holding the next sound");

  // 2. Backward is symmetric.
  expect(tickAfterMove(phrase, 96, -1) == 84, "LEFT moved more than one cell");
  expect(tickAfterMove(phrase, 12, -1) == 0, "LEFT did not reach the start");

  // 3. A gap is walked, not jumped. This is the case that made adding
  //    impossible: between 96 and 264 there must be somewhere to stand.
  {
    uint16_t tick = 96;
    int steps = 0;
    while (tick < 264 && steps < 32) {
      tick = tickAfterMove(phrase, tick, 1);
      ++steps;
    }
    expect(steps > 1, "the gap was crossed in one press, leaving nowhere to add");
    expect(tick == 264, "walking the gap did not arrive at the next sound");
  }

  // 4. The ends hold rather than wrap.
  expect(tickAfterMove(phrase, 0, -1) == 0, "LEFT wrapped before the start");

  // 5. An empty melody is still navigable, so ENTER always has a position.
  {
    Buffer empty{};
    empty.lengthTicks = PhraseRuntime::kTicksPerBar;
    expect(tickAfterMove(empty, 48, 1) == 60,
           "an emptied melody trapped the cursor");
    expect(tickAfterMove(empty, 48, -1) == 36,
           "an emptied melody trapped the cursor going back");
  }

  // 6. Only +/-1 is a direction.
  {
    PhraseNotesCursor::State state{};
    state.grid = RuntimePhraseEdit::Grid::ThirtySecond;
    state.cell = 8;
    const PhraseNotesCursor::State unchanged =
        PhraseNotesCursor::move(state, 0, phrase.lengthTicks);
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
    state.cell = 4;   // cell [96,120) contains the onset at 100
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
