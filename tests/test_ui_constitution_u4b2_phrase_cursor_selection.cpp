#include <cassert>
#include <cstdint>

#include "src/ui/phrase_notes_cursor.h"
#include "src/ui/phrase_notes_selection.h"
#include "src/ui/ui_view_continuity.h"

namespace {

PhraseRuntime::RuntimeSynthEvent makeEvent(uint16_t startTick,
                                           uint16_t durationTicks,
                                           uint8_t note) {
  PhraseRuntime::RuntimeSynthEvent event{};
  event.startTick = startTick;
  event.durationSubticks = static_cast<uint16_t>(
      durationTicks * PhraseRuntime::kSubticksPerTick);
  event.note = note;
  event.velocity = 100;
  event.probability = 100;
  return event;
}

PhraseRuntime::RuntimeSynthEventBuffer makePhrase() {
  PhraseRuntime::RuntimeSynthEventBuffer phrase{};
  phrase.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  phrase.count = 2;
  phrase.events[0] = makeEvent(96, 48, 60);
  phrase.events[1] = makeEvent(120, 48, 64);
  assert(RuntimePhraseEdit::validate(phrase));
  return phrase;
}

}  // namespace

int main() {
  using PhraseNotesCursor::State;
  using RuntimePhraseEdit::Grid;

  const uint16_t eightBars = 8 * PhraseRuntime::kTicksPerBar;

  for (const auto entry : {
           std::pair<Grid, uint16_t>{Grid::Eighth, 48},
           std::pair<Grid, uint16_t>{Grid::Sixteenth, 24},
           std::pair<Grid, uint16_t>{Grid::ThirtySecond, 12},
       }) {
    State state{};
    state.grid = entry.first;
    state = PhraseNotesCursor::move(state, +1, eightBars);
    assert(PhraseNotesCursor::tick(state) == entry.second);
    state = PhraseNotesCursor::move(state, -1, eightBars);
    assert(PhraseNotesCursor::tick(state) == 0);
    state = PhraseNotesCursor::move(state, -1, eightBars);
    assert(PhraseNotesCursor::tick(state) == 0);
  }

  {
    State state{};
    state.grid = Grid::ThirtySecond;
    state.cell = 255;
    state = PhraseNotesCursor::move(state, +1, eightBars);
    assert(state.cell == 255);
    assert(PhraseNotesCursor::tick(state) == 3060);
  }

  {
    State state{};
    state.grid = Grid::ThirtySecond;
    state.cell = 200;
    state = PhraseNotesCursor::clamp(state, 4 * PhraseRuntime::kTicksPerBar);
    assert(state.cell == 127);
    assert(PhraseNotesCursor::focusBar(state) == 3);
  }

  {
    State state{};
    state.grid = Grid::Eighth;
    state.cell = 1;  // tick 48, representable on every supported grid.
    state = PhraseNotesCursor::changeGrid(state, +1, eightBars);
    assert(state.grid == Grid::Sixteenth);
    assert(PhraseNotesCursor::tick(state) == 48);
    state = PhraseNotesCursor::changeGrid(state, +1, eightBars);
    assert(state.grid == Grid::ThirtySecond);
    assert(PhraseNotesCursor::tick(state) == 48);
    state = PhraseNotesCursor::changeGrid(state, +1, eightBars);
    assert(state.grid == Grid::ThirtySecond);
    state = PhraseNotesCursor::changeGrid(state, -1, eightBars);
    assert(state.grid == Grid::Sixteenth);
    state = PhraseNotesCursor::changeGrid(state, -1, eightBars);
    assert(state.grid == Grid::Eighth);
    state = PhraseNotesCursor::changeGrid(state, -1, eightBars);
    assert(state.grid == Grid::Eighth);
  }

  {
    assert(PhraseNotesCursor::gridLabel(Grid::Eighth) == std::string("1/8"));
    assert(PhraseNotesCursor::gridLabel(Grid::Sixteenth) == std::string("1/16"));
    assert(PhraseNotesCursor::gridLabel(Grid::ThirtySecond) == std::string("1/32"));
  }

  {
    auto phrase = makePhrase();
    auto selection = PhraseNotesSelection::derive(phrase, 110);
    assert(selection.active);
    assert(selection.span.startTick == 96);
    assert(selection.span.note == 60);

    selection = PhraseNotesSelection::derive(phrase, 130);
    assert(selection.active);
    assert(selection.span.startTick == 120);
    assert(selection.span.note == 64);

    selection = PhraseNotesSelection::derive(phrase, 168);
    assert(!selection.active);

    std::swap(phrase.events[0], phrase.events[1]);
    assert(RuntimePhraseEdit::validate(phrase));
    selection = PhraseNotesSelection::derive(phrase, 130);
    assert(selection.active);
    assert(selection.span.startTick == 120);
    assert(selection.span.note == 64);
  }

  {
    UI::UiViewContinuityState continuity{};
    continuity.phraseCursorCell[0] = 255;
    continuity.phraseCursorCell[1] = 7;
    continuity.phraseGrid[0] = static_cast<uint8_t>(Grid::ThirtySecond);
    continuity.phraseGrid[1] = static_cast<uint8_t>(Grid::Eighth);
    assert(continuity.phraseCursorCell[0] == 255);
    static_assert(sizeof(UI::UiViewContinuityState) <= 16,
                  "U4B2 cursor continuity must stay inside the tiny UI budget");
  }

  return 0;
}
