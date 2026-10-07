#include <cassert>

#include "src/ui/phrase_chord_focus.h"

namespace {
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

void add(Buffer& b, uint16_t start, uint8_t note) {
  auto& e = b.events[b.count++];
  e.startTick = start;
  e.durationSubticks = 24 * PhraseRuntime::kSubticksPerTick;
  e.note = note;
  e.velocity = 100;
  e.probability = 100;
}
}  // namespace

int main() {
  using namespace PhraseChordFocus;
  Buffer b{};
  b.lengthTicks = PhraseRuntime::kTicksPerBar;
  add(b, 0, 67);   // 0  G
  add(b, 0, 60);   // 1  C
  add(b, 0, 64);   // 2  E
  add(b, 24, 62);  // 3  D alone in the next cell

  // C cycles low -> high and wraps; starts at the lowest when unfocused.
  uint8_t pos = 0, total = 0;
  assert(next(b, 0, 24, -1, &pos, &total) == 1 && pos == 0 && total == 3);
  assert(next(b, 0, 24, 1) == 2);
  assert(next(b, 0, 24, 2) == 0);
  assert(next(b, 0, 24, 0) == 1);
  assert(next(b, 24, 24, -1) == -1);  // one note: nothing to cycle

  assert(valid(b, 0, 24, 2));
  assert(!valid(b, 24, 24, 3));  // single note is not a chord focus
  assert(!valid(b, 0, 24, 3));   // D is not in this cell

  // A over a single note adds a major third; over a chord a minor third on top.
  Buffer after{};
  int added = -1;
  assert(prepareAddTone(b, 3, after, added) == AddResult::Ready);
  assert(added == 4 && after.count == 5 && after.events[4].note == 66);
  assert(after.events[4].startTick == 24);
  assert(prepareAddTone(b, 1, after, added) == AddResult::Ready);
  assert(after.events[added].note == 70 && after.events[added].startTick == 0);
  assert(RuntimePhraseEdit::hasOverlappingNotes(after));

  assert(prepareAddTone(b, -1, after, added) == AddResult::NoTarget);
  Buffer full{};
  full.lengthTicks = PhraseRuntime::kTicksPerBar * 8;
  for (int i = 0; i < PhraseRuntime::kMaxSynthEvents; ++i) {
    add(full, static_cast<uint16_t>((i % 128) * 24), 60);
  }
  assert(prepareAddTone(full, 0, after, added) == AddResult::Full);
  Buffer high{};
  high.lengthTicks = PhraseRuntime::kTicksPerBar;
  add(high, 0, 125);
  assert(prepareAddTone(high, 0, after, added) == AddResult::PitchLimit);

  // Recording: keys within the window form one chord; later ones do not.
  assert(sameChordOnset(1000, 1000 + kChordWindowMs));
  assert(!sameChordOnset(1000, 1000 + kChordWindowMs + 1));
  assert(sameChordOnset(0xFFFFFFF0u, 10u));  // millis() wrap
  assert(prepareAddNote(b, 3, 65, 90, after, added) == AddResult::Ready);
  assert(after.events[added].note == 65 && after.events[added].velocity == 90);
  assert(after.events[added].startTick == 24);
  assert(prepareAddNote(b, 1, 64, 90, after, added) ==
         AddResult::AlreadyInChord);  // E already sounds there
  return 0;
}
