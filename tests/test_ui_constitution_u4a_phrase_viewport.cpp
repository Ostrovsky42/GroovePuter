#include <cassert>
#include <cstdint>

#include "src/ui/phrase_notes_cursor.h"
#include "src/ui/phrase_notes_viewport.h"
#include "src/ui/ui_view_continuity.h"

int main() {
  using PhraseNotesViewport::Window;
  using RuntimePhraseEdit::Grid;

  {
    const Window w = PhraseNotesViewport::resolve(384, 0);
    assert(w.totalBars == 1);
    assert(w.focusBar == 0);
    assert(w.startBar == 0);
    assert(w.barCount == 1);
  }

  {
    const Window w0 = PhraseNotesViewport::resolve(2 * 384, 0);
    const Window w1 = PhraseNotesViewport::resolve(2 * 384, 1);
    assert(w0.startBar == 0 && w0.barCount == 2);
    assert(w1.startBar == 0 && w1.barCount == 2);
  }

  for (uint8_t bars : {uint8_t{4}, uint8_t{8}}) {
    const uint16_t length = static_cast<uint16_t>(bars * 384u);
    for (uint8_t focus = 0; focus < bars; ++focus) {
      const Window w = PhraseNotesViewport::resolve(length, focus);
      assert(w.totalBars == bars);
      assert(w.focusBar == focus);
      assert(w.barCount == 2);
      assert(w.startBar <= focus);
      assert(focus < static_cast<uint8_t>(w.startBar + w.barCount));
      assert(static_cast<uint8_t>(w.startBar + w.barCount) <= bars);
    }
  }

  {
    PhraseNotesCursor::State cursor{};
    cursor.grid = Grid::ThirtySecond;
    cursor.cell = 64;  // bar 3, tick 768
    assert(PhraseNotesCursor::focusBar(cursor) == 2);
    const Window w = PhraseNotesViewport::resolve(
        4 * PhraseRuntime::kTicksPerBar,
        PhraseNotesCursor::focusBar(cursor));
    assert(w.startBar == 1);
    assert(w.focusBar == 2);
  }
  {
    PhraseNotesCursor::State cursor{};
    cursor.grid = Grid::ThirtySecond;
    cursor.cell = 224;  // bar 8, tick 2688
    assert(PhraseNotesCursor::focusBar(cursor) == 7);
    const Window w = PhraseNotesViewport::resolve(
        8 * PhraseRuntime::kTicksPerBar,
        PhraseNotesCursor::focusBar(cursor));
    assert(w.startBar == 6);
    assert(w.focusBar == 7);
  }

  const Window invalid = PhraseNotesViewport::resolve(3 * 384, 0);
  assert(invalid.totalBars == 0);
  assert(invalid.barCount == 0);

  UI::UiViewContinuityState continuity{};
  continuity.phraseCursorCell[0] = 64;
  continuity.phraseCursorCell[1] = 224;
  continuity.phraseGrid[0] = static_cast<uint8_t>(Grid::ThirtySecond);
  continuity.phraseGrid[1] = static_cast<uint8_t>(Grid::ThirtySecond);
  assert(continuity.phraseCursorCell[0] == 64);
  assert(continuity.phraseCursorCell[1] == 224);
  static_assert(sizeof(UI::UiViewContinuityState) <= 16,
                "Phrase cursor continuity must stay inside the tiny UI budget");

  return 0;
}
