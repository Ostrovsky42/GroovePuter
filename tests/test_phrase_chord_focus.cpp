#include <cassert>
#include <string>

#include "src/ui/phrase_chord_focus.h"
#include "src/ui/project_key.h"

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

  // A over a single note builds the whole triad at once; over a chord it adds
  // one tone on top. Chromatic (no key): major then minor third.
  Buffer after{};
  int added = -1;
  assert(prepareAddTone(b, 3, after, added) == AddResult::Ready);
  assert(added == 5 && after.count == 6);
  assert(after.events[4].note == 66 && after.events[5].note == 69);
  assert(after.events[4].startTick == 24 && after.events[5].startTick == 24);
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

  // In the project key: diatonic thirds, so the triad stays in the scale.
  using namespace GroovePuterRhythm;
  assert(chordToneAbove(60, true, 0, kScaleMajor) == 64);   // C -> E
  assert(chordToneAbove(64, false, 0, kScaleMajor) == 67);  // E -> G
  assert(chordToneAbove(71, true, 0, kScaleMajor) == 74);   // B -> D
  assert(chordToneAbove(60, true, 0, kScaleDorian) == 63);  // C -> Eb
  assert(chordToneAbove(62, true, 0, kScaleDorian) == 65);  // D -> F
  assert(chordToneAbove(69, true, 9, kScaleMinor) == 72);   // A minor: A -> C
  assert(chordToneAbove(63, true, 0, kScalePentatonicMinor) == 67);  // Eb -> G
  assert(chordToneAbove(61, true, 0, kScaleMajor) == 64);   // off-scale C# -> E
  assert(chordToneAbove(60, true, 0, kScaleChromatic) == 64);
  assert(chordToneAbove(60, false, 0, kScaleChromatic) == 63);
  assert(prepareAddTone(b, 3, after, added, 0, kScaleDorian) == AddResult::Ready);
  assert(after.events[4].note == 65 && after.events[5].note == 69);  // D F A, not D F# A
  // Over the C Dorian triad C Eb G a fourth A adds Bb: a seventh chord.
  Buffer triad{};
  triad.lengthTicks = PhraseRuntime::kTicksPerBar;
  add(triad, 0, 60);
  assert(prepareAddTone(triad, 0, after, added, 0, kScaleDorian) == AddResult::Ready);
  assert(after.events[1].note == 63 && after.events[2].note == 67);
  Buffer seventh = after;
  assert(prepareAddTone(seventh, 0, after, added, 0, kScaleDorian) == AddResult::Ready);
  assert(after.count == 4 && after.events[added].note == 70);

  // Alt+C: the chord becomes one note per grid step, low to high and around,
  // for as long as the chord lasts, stopping at the next note.
  Buffer arp{};
  arp.lengthTicks = PhraseRuntime::kTicksPerBar;
  add(arp, 0, 67);
  add(arp, 0, 60);
  add(arp, 0, 64);
  arp.events[0].durationSubticks = 96 * PhraseRuntime::kSubticksPerTick;  // a beat
  add(arp, 192, 62);
  uint16_t steps = 0;
  assert(prepareArpeggio(arp, 0, 24, 24, after, steps) == AddResult::Ready);
  assert(steps == 4 && after.count == 5);
  const uint8_t expect[4] = {60, 64, 67, 60};
  for (int k = 0; k < 4; ++k) {
    assert(after.events[k].startTick == k * 24 && after.events[k].note == expect[k]);
    assert(after.events[k].durationSubticks == 24 * PhraseRuntime::kSubticksPerTick);
  }
  assert(after.events[4].note == 62 && after.events[4].startTick == 192);
  assert(!RuntimePhraseEdit::hasOverlappingNotes(after));
  // A later note cuts the cycle, but never below one pass over the chord.
  arp.events[3].startTick = 72;
  assert(prepareArpeggio(arp, 0, 24, 24, after, steps) == AddResult::Ready);
  assert(steps == 3 && after.events[3].note == 62);
  // Not a chord.
  assert(prepareArpeggio(arp, 48, 24, 24, after, steps) == AddResult::NoChord);
  // A one-step chord (as H makes it) still plays every note: C E G, not C E.
  Buffer shortChord{};
  shortChord.lengthTicks = PhraseRuntime::kTicksPerBar;
  add(shortChord, 0, 67);
  add(shortChord, 0, 60);
  add(shortChord, 0, 64);
  assert(prepareArpeggio(shortChord, 0, 24, 24, after, steps) == AddResult::Ready);
  assert(steps == 3 && after.events[0].note == 60 && after.events[1].note == 64 &&
         after.events[2].note == 67 && after.events[2].startTick == 48);
  // No room for all chord notes before the next one: it says how many.
  assert(prepareArpeggio(b, 0, 24, 24, after, steps) == AddResult::Blocked);  // D next
  assert(steps == 3);
  arp.events[3].startTick = 48;
  assert(prepareArpeggio(arp, 0, 24, 24, after, steps) == AddResult::Blocked);

  // Up/Down along the key: whole and half steps as the scale has them; a
  // note outside the key lands on the nearest key note that way.
  assert(ProjectKey::step(60, +1, 0, kScaleMajor) == 62);
  assert(ProjectKey::step(64, +1, 0, kScaleMajor) == 65);
  assert(ProjectKey::step(60, -1, 0, kScaleMajor) == 59);
  assert(ProjectKey::step(61, +1, 0, kScaleMajor) == 62);
  assert(ProjectKey::step(61, -1, 0, kScaleMajor) == 60);
  assert(ProjectKey::step(63, +1, 0, kScalePentatonicMinor) == 65);
  assert(ProjectKey::step(60, +1, 0, kScaleChromatic) == 61);
  assert(ProjectKey::step(127, +1, 0, kScaleMajor) == -1);
  assert(ProjectKey::nextScale(kScaleChromatic) == kScaleMinor);
  char label[16];
  ProjectKey::format(9, kScaleMinor, label, sizeof(label));
  assert(std::string(label) == "KEY A MIN");

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
