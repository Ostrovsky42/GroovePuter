#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/ui/phrase_notes_delete_edit.h"

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using Event = PhraseRuntime::RuntimeSynthEvent;

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
  phrase.count = 3;
  phrase.events[0] = makeEvent(96, 96, 60);
  phrase.events[1] = makeEvent(120, 48, 64);
  phrase.events[2] = makeEvent(240, 24, 67);
  assert(RuntimePhraseEdit::validate(phrase));
  return phrase;
}

bool same(const Buffer& lhs, const Buffer& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(Buffer)) == 0;
}

bool containsNote(const Buffer& phrase, uint8_t note) {
  for (uint16_t i = 0; i < phrase.count; ++i) {
    if (phrase.events[i].note == note) return true;
  }
  return false;
}

}  // namespace

int main() {
  using PhraseNotesDeleteEdit::Prepared;
  using PhraseNotesDeleteEdit::Result;

  {
    const Buffer live = makePhrase();
    const Buffer before = live;
    Prepared prepared{};

    // Tick 130 is covered by note 60 and later-onset note 64. The derived
    // selection law must delete only the most recent onset.
    assert(PhraseNotesDeleteEdit::prepare(live, 130, prepared) == Result::Ready);
    assert(same(live, before));
    assert(same(prepared.before, before));
    assert(prepared.after.count == 2);
    assert(containsNote(prepared.after, 60));
    assert(!containsNote(prepared.after, 64));
    assert(containsNote(prepared.after, 67));

    Buffer committed = live;
    assert(PhraseNotesDeleteEdit::commitIfUnchanged(committed, prepared));
    assert(same(committed, prepared.after));
  }

  {
    Buffer live = makePhrase();
    std::swap(live.events[0], live.events[2]);
    assert(RuntimePhraseEdit::validate(live));
    Prepared prepared{};
    assert(PhraseNotesDeleteEdit::prepare(live, 130, prepared) == Result::Ready);
    assert(prepared.after.count == 2);
    assert(containsNote(prepared.after, 60));
    assert(!containsNote(prepared.after, 64));
    assert(containsNote(prepared.after, 67));
  }

  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    assert(PhraseNotesDeleteEdit::prepare(live, 300, prepared) == Result::NoTarget);
    assert(same(prepared.before, live));
    assert(same(prepared.after, live));
  }

  {
    Buffer live{};
    live.lengthTicks = PhraseRuntime::kTicksPerBar;
    live.count = 1;
    live.events[0] = makeEvent(0, 24, 60);
    Prepared prepared{};
    assert(PhraseNotesDeleteEdit::prepare(live, 0, prepared) == Result::Ready);
    assert(prepared.after.count == 0);
    assert(RuntimePhraseEdit::validate(prepared.after));
  }

  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    assert(PhraseNotesDeleteEdit::prepare(live, 130, prepared) == Result::Ready);

    Buffer stale = live;
    stale.events[2].note = 68;
    const Buffer staleBefore = stale;
    assert(!PhraseNotesDeleteEdit::commitIfUnchanged(stale, prepared));
    assert(same(stale, staleBefore));
  }

  return 0;
}
