#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/ui/phrase_notes_duration_edit.h"

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using Event = PhraseRuntime::RuntimeSynthEvent;
using Grid = RuntimePhraseEdit::Grid;

Event makeEvent(uint16_t startTick, uint16_t durationTicks, uint8_t note) {
  Event event{};
  event.startTick = startTick;
  event.durationSubticks = static_cast<uint16_t>(
      durationTicks * PhraseRuntime::kSubticksPerTick);
  event.note = note;
  event.velocity = 100;
  event.probability = 100;
  return event;
}

Buffer makePhrase() {
  Buffer phrase{};
  phrase.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  phrase.count = 2;
  phrase.events[0] = makeEvent(96, 48, 60);
  phrase.events[1] = makeEvent(120, 48, 64);
  assert(RuntimePhraseEdit::validate(phrase));
  return phrase;
}

bool same(const Buffer& lhs, const Buffer& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(Buffer)) == 0;
}

const Event* findByNote(const Buffer& phrase, uint8_t note) {
  for (uint16_t i = 0; i < phrase.count; ++i) {
    if (phrase.events[i].note == note) return &phrase.events[i];
  }
  return nullptr;
}

}  // namespace

int main() {
  using PhraseNotesDurationEdit::Prepared;
  using PhraseNotesDurationEdit::Result;

  {
    const Buffer live = makePhrase();
    const Buffer before = live;
    Prepared prepared{};

    // Tick 130 is covered by both notes. U4B2 selection law chooses the most
    // recent onset, so only note 64 may grow.
    const Result result = PhraseNotesDurationEdit::prepare(
        live, 130, Grid::Sixteenth, +1, prepared);
    assert(result == Result::Ready);
    assert(same(live, before));
    assert(same(prepared.before, before));
    assert(findByNote(prepared.after, 60)->durationSubticks ==
           48 * PhraseRuntime::kSubticksPerTick);
    assert(findByNote(prepared.after, 64)->durationSubticks ==
           72 * PhraseRuntime::kSubticksPerTick);

    Buffer committed = live;
    assert(PhraseNotesDurationEdit::commitIfUnchanged(committed, prepared));
    assert(same(committed, prepared.after));
  }

  {
    Buffer live = makePhrase();
    std::swap(live.events[0], live.events[1]);
    assert(RuntimePhraseEdit::validate(live));
    Prepared prepared{};
    assert(PhraseNotesDurationEdit::prepare(
               live, 130, Grid::Sixteenth, +1, prepared) == Result::Ready);
    assert(findByNote(prepared.after, 64)->durationSubticks ==
           72 * PhraseRuntime::kSubticksPerTick);
    assert(findByNote(prepared.after, 60)->durationSubticks ==
           48 * PhraseRuntime::kSubticksPerTick);
  }

  {
    Buffer live{};
    live.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
    live.count = 1;
    live.events[0] = makeEvent(96, 24, 60);
    const Buffer before = live;
    Prepared prepared{};
    assert(PhraseNotesDurationEdit::prepare(
               live, 100, Grid::Sixteenth, -1, prepared) == Result::Rejected);
    assert(same(live, before));
    assert(same(prepared.before, before));
    assert(same(prepared.after, before));
  }

  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    assert(PhraseNotesDurationEdit::prepare(
               live, 300, Grid::Sixteenth, +1, prepared) == Result::NoTarget);
    assert(same(prepared.before, live));
    assert(same(prepared.after, live));
  }

  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    assert(PhraseNotesDurationEdit::prepare(
               live, 130, Grid::Sixteenth, +1, prepared) == Result::Ready);

    Buffer stale = live;
    stale.events[0].note = 61;
    const Buffer staleBefore = stale;
    assert(!PhraseNotesDurationEdit::commitIfUnchanged(stale, prepared));
    assert(same(stale, staleBefore));
  }

  return 0;
}
