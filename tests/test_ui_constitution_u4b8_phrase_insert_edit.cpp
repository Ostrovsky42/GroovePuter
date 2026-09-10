// U4B8: adding a sound. The last operation the beginner screen names but
// cannot perform.
//
// RuntimePhraseEdit::insertSnapped has existed since P3-U1 with no UI caller,
// so the Phrase editor could change and delete notes but never create one --
// an editor you can only subtract with. This adds the UI policy adapter, in
// the same shape as its duration (U4B3), delete (U4B4) and pitch (U4B7)
// siblings.
//
// The only new decision is which pitch a new sound gets. Silence is not an
// option and neither is a dialog, so the rule is: continue the melody -- take
// the pitch of the nearest sound that starts before the cursor, and fall back
// to middle C when there is nothing to continue. Deterministic, needs no
// theory, and Up/Down immediately corrects it.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "src/ui/phrase_notes_insert_edit.h"

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

// Two sounds early in the bar, leaving the rest empty.
Buffer makePhrase() {
  Buffer phrase{};
  phrase.lengthTicks = PhraseRuntime::kTicksPerBar;
  phrase.count = 2;
  phrase.events[0] = makeEvent(0, 12, 60);
  phrase.events[1] = makeEvent(48, 12, 67);
  assert(RuntimePhraseEdit::validate(phrase));
  return phrase;
}

bool same(const Buffer& lhs, const Buffer& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(Buffer)) == 0;
}

const Event* eventAt(const Buffer& phrase, uint16_t startTick) {
  for (uint16_t i = 0; i < phrase.count; ++i) {
    if (phrase.events[i].startTick == startTick) return &phrase.events[i];
  }
  return nullptr;
}

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "U4B8 FAIL: %s\n", message);
  ++g_failures;
}

}  // namespace

int main() {
  using PhraseNotesInsertEdit::Prepared;
  using PhraseNotesInsertEdit::Result;

  // 1. A sound appears at the cursor, snapped to the grid.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    const Result result = PhraseNotesInsertEdit::prepare(
        live, 100, Grid::Sixteenth, prepared);
    expect(result == Result::Ready, "insert refused on empty time");
    expect(prepared.after.count == live.count + 1, "no sound was added");
    // 100 snapped down to a 1/16 grid (24 ticks) is 96.
    expect(eventAt(prepared.after, 96) != nullptr,
           "the new sound did not land on the grid position under the cursor");
    expect(same(prepared.before, live), "before-image was not the live value");
  }

  // 2. Its length is one grid step, so the block the user sees matches the
  //    grid they are working on.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    assert(PhraseNotesInsertEdit::prepare(live, 100, Grid::Sixteenth,
                                          prepared) == Result::Ready);
    const Event* added = eventAt(prepared.after, 96);
    expect(added != nullptr && added->durationSubticks ==
               RuntimePhraseEdit::gridTicks(Grid::Sixteenth) *
                   PhraseRuntime::kSubticksPerTick,
           "the new sound is not one grid step long");
  }

  // 3. Pitch continues the melody: the nearest sound starting before the
  //    cursor. At tick 100 that is the sound at 48, note 67.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    assert(PhraseNotesInsertEdit::prepare(live, 100, Grid::Sixteenth,
                                          prepared) == Result::Ready);
    const Event* added = eventAt(prepared.after, 96);
    expect(added != nullptr && added->note == 67,
           "the new sound did not continue the preceding pitch");
  }

  // 4. With nothing to continue, middle C. An empty phrase must still be
  //    fillable, which is the whole point of the operation.
  {
    Buffer live{};
    live.lengthTicks = PhraseRuntime::kTicksPerBar;
    live.count = 0;
    Prepared prepared{};
    expect(PhraseNotesInsertEdit::prepare(live, 0, Grid::Sixteenth, prepared) ==
               Result::Ready,
           "cannot add the first sound to an empty melody");
    const Event* added = eventAt(prepared.after, 0);
    expect(added != nullptr && added->note == 60,
           "the first sound did not fall back to middle C");
    expect(added != nullptr && added->velocity == 100,
           "the new sound has no usable velocity");
  }

  // 5. An occupied grid position is refused rather than silently stacked. Two
  //    events on one tick are a state the player resolves by taking the first,
  //    so creating one through the editor would hide a sound.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    expect(PhraseNotesInsertEdit::prepare(live, 48, Grid::Sixteenth,
                                          prepared) == Result::Occupied,
           "insert stacked a second sound on an occupied position");
    expect(same(prepared.after, live), "a refused insert changed the melody");
  }

  // 6. Commit is guarded by the same value contract as its siblings.
  {
    Buffer live = makePhrase();
    Prepared prepared{};
    assert(PhraseNotesInsertEdit::prepare(live, 100, Grid::Sixteenth,
                                          prepared) == Result::Ready);

    Buffer drifted = makePhrase();
    drifted.events[0].note = 62;
    expect(!PhraseNotesInsertEdit::commitIfUnchanged(drifted, prepared),
           "commit accepted a melody that had moved underneath it");
    expect(PhraseNotesInsertEdit::commitIfUnchanged(live, prepared),
           "commit refused the melody it was prepared against");
    expect(live.count == 3, "the committed melody has no new sound");
  }

  // 7. A full melody reports capacity rather than failing silently.
  {
    Buffer live{};
    live.lengthTicks = 8 * PhraseRuntime::kTicksPerBar;
    for (uint16_t i = 0; i < PhraseRuntime::kMaxSynthEvents; ++i) {
      live.events[i] = makeEvent(static_cast<uint16_t>(i * 24), 12, 60);
    }
    live.count = PhraseRuntime::kMaxSynthEvents;
    assert(RuntimePhraseEdit::validate(live));
    Prepared prepared{};
    const uint16_t freeTick = static_cast<uint16_t>(
        PhraseRuntime::kMaxSynthEvents * 24 + 24);
    expect(PhraseNotesInsertEdit::prepare(live, freeTick, Grid::Sixteenth,
                                          prepared) == Result::Full,
           "a full melody did not report being full");
  }

  if (g_failures == 0) {
    std::printf("UI Constitution U4B8 phrase insert: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "UI Constitution U4B8 phrase insert: %d failure(s)\n",
               g_failures);
  return 1;
}
