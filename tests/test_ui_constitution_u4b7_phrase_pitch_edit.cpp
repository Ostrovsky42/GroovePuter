// U4B7 RED: "выше / ниже" — the one operation the beginner screen requires and
// the codebase does not have anywhere.
//
// Today a Phrase note can be moved in time (U4B3 duration), removed (U4B4) and
// undone (U4B5), but its pitch cannot be changed by any owner: neither
// RuntimePhraseEdit nor any src/ui adapter exposes a note-value mutation. The
// readiness criterion of docs/design/PHRASE_SCREEN_FOR_A_FIRST_TIME_USER.md
// ("▲▼ слышит как он поднимается") is unreachable until it exists.
//
// This file is expected to FAIL TO COMPILE until
// src/ui/phrase_notes_pitch_edit.h and RuntimePhraseEdit::transposeEvent exist.
//
// Design commitments encoded here:
//   * one press = one semitone. Deterministic, needs no scale knowledge, and
//     satisfies the Constitution rule that Y carries defensible meaning.
//   * pitch is orthogonal to time: startTick, durationSubticks, count and
//     lengthTicks must be byte-identical after a pitch edit.
//   * the MIDI range boundary rejects rather than silently clamps — a clamp
//     that looks like a working key press is a lie to the beginner.
//   * selection reuses the U4B2 law (most recent onset wins), not a new rule.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "src/ui/phrase_notes_pitch_edit.h"

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

// Deliberately overlapping, exactly like the U4B3 fixture: 96..144 and 120..168
// share tick 130. Overlap is a first-class state of this material, not an edge
// case, so the pitch operation must be specified against it from the start.
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

// Everything about an event except its note. If this differs after a pitch
// edit, pitch and time have been coupled.
bool sameTiming(const Buffer& lhs, const Buffer& rhs) {
  if (lhs.count != rhs.count || lhs.lengthTicks != rhs.lengthTicks) return false;
  for (uint16_t i = 0; i < lhs.count; ++i) {
    if (lhs.events[i].startTick != rhs.events[i].startTick) return false;
    if (lhs.events[i].durationSubticks != rhs.events[i].durationSubticks) {
      return false;
    }
    if (lhs.events[i].velocity != rhs.events[i].velocity) return false;
    if (lhs.events[i].flags != rhs.events[i].flags) return false;
  }
  return true;
}

}  // namespace

int main() {
  using PhraseNotesPitchEdit::Prepared;
  using PhraseNotesPitchEdit::Result;

  // 1. Up one semitone changes exactly the selected note.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    const Result result = PhraseNotesPitchEdit::prepare(live, 130, +1, prepared);
    assert(result == Result::Ready);
    assert(prepared.after.events[1].note == 65);
    assert(prepared.after.events[0].note == 60);
    assert(sameTiming(live, prepared.after));
    assert(same(prepared.before, live));
  }

  // 2. Down one semitone is symmetric.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    const Result result = PhraseNotesPitchEdit::prepare(live, 130, -1, prepared);
    assert(result == Result::Ready);
    assert(prepared.after.events[1].note == 63);
    assert(prepared.after.events[0].note == 60);
    assert(sameTiming(live, prepared.after));
  }

  // 3. Selection obeys the U4B2 law and nothing else. Tick 100 is covered only
  //    by the first note, so that is the one that must move.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    assert(PhraseNotesPitchEdit::prepare(live, 100, +1, prepared) ==
           Result::Ready);
    assert(prepared.after.events[0].note == 61);
    assert(prepared.after.events[1].note == 64);
  }

  // 4. Empty time is not a target. Tick 300 lies past both notes.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    assert(PhraseNotesPitchEdit::prepare(live, 300, +1, prepared) ==
           Result::NoTarget);
    assert(same(prepared.after, live));
  }

  // 5. The MIDI ceiling rejects; it does not clamp. A clamp would leave the key
  //    press looking successful while nothing sounded different.
  {
    Buffer live = makePhrase();
    live.events[1].note = 127;
    assert(RuntimePhraseEdit::validate(live));
    Prepared prepared{};
    assert(PhraseNotesPitchEdit::prepare(live, 130, +1, prepared) ==
           Result::Rejected);
    assert(same(prepared.after, live));
  }

  // 6. The floor rejects the same way.
  {
    Buffer live = makePhrase();
    live.events[1].note = 0;
    assert(RuntimePhraseEdit::validate(live));
    Prepared prepared{};
    assert(PhraseNotesPitchEdit::prepare(live, 130, -1, prepared) ==
           Result::Rejected);
    assert(same(prepared.after, live));
  }

  // 7. Only ±1 is a direction. Zero and multi-step are not silently reinterpreted.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    assert(PhraseNotesPitchEdit::prepare(live, 130, 0, prepared) ==
           Result::Rejected);
    assert(PhraseNotesPitchEdit::prepare(live, 130, +2, prepared) ==
           Result::Rejected);
  }

  // 8. Commit is guarded by the same value contract as U4B3: a Phrase that
  //    moved under the prepared before-image must refuse the commit.
  {
    Buffer live = makePhrase();
    Prepared prepared{};
    assert(PhraseNotesPitchEdit::prepare(live, 130, +1, prepared) ==
           Result::Ready);

    Buffer drifted = makePhrase();
    drifted.events[0].note = 62;
    assert(!PhraseNotesPitchEdit::commitIfUnchanged(drifted, prepared));
    assert(drifted.events[0].note == 62);

    assert(PhraseNotesPitchEdit::commitIfUnchanged(live, prepared));
    assert(live.events[1].note == 65);
  }

  // 9. The domain primitive stands on its own, independent of cursor policy.
  {
    Buffer phrase = makePhrase();
    assert(RuntimePhraseEdit::transposeEvent(phrase, 0, +1) ==
           RuntimePhraseEdit::EventEditResult::Changed);
    assert(phrase.events[0].note == 61);
    assert(RuntimePhraseEdit::transposeEvent(phrase, 9, +1) ==
           RuntimePhraseEdit::EventEditResult::NoTarget);
  }

  std::printf("UI Constitution U4B7 phrase pitch edit: PASS\n");
  return 0;
}
