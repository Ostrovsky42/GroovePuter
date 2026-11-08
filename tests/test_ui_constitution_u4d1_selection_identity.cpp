// U4D1: one selected event, shared by both views, surviving edits and undo.
//
// The piano roll addresses a cursor tick, which cannot distinguish two events
// that start together -- and the data allows that. A list has one row per
// event and must address the event itself. Both views therefore need the same
// selection, and it has to mean "this sound" rather than "whatever is at this
// position", or switching views would silently select something else.
//
// An array index alone is not that identity: delete shifts the array, join
// removes an element, and undo replaces the whole buffer. So the selection
// carries what the sound *is* (start and pitch) alongside the index, re-finds
// itself after any mutation, and falls to the nearest survivor when the sound
// it named is gone.
//
// The insert position is deliberately separate state. Sharing it with the
// selection is what made adding unreachable once LEFT/RIGHT started jumping
// between onsets.

#include <cstdint>
#include <cstdio>

#include "src/ui/phrase_selection_state.h"

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "U4D1 FAIL: %s\n", message);
  ++g_failures;
}

void authorEvent(Buffer& phrase, uint16_t startTick, uint16_t durationTicks,
                 uint8_t note) {
  auto& event = phrase.events[phrase.count++];
  event = PhraseRuntime::RuntimeSynthEvent{};
  event.startTick = startTick;
  event.durationSubticks = static_cast<uint16_t>(
      durationTicks * PhraseRuntime::kSubticksPerTick);
  event.note = note;
  event.velocity = 100;
  event.probability = 100;
}

// Two sounds start together at 48 -- the case a tick cursor cannot separate.
Buffer makePhrase() {
  Buffer phrase{};
  phrase.lengthTicks = PhraseRuntime::kTicksPerBar;
  authorEvent(phrase, 0, 24, 60);
  authorEvent(phrase, 48, 24, 64);
  authorEvent(phrase, 48, 24, 67);
  authorEvent(phrase, 120, 24, 72);
  return phrase;
}

}  // namespace

int main() {
  using PhraseSelectionState::State;

  // 1. Both coincident sounds are reachable, one at a time. This is the whole
  //    reason the selection is not a tick.
  {
    const Buffer phrase = makePhrase();
    State state = PhraseSelectionState::first(phrase);
    expect(state.active && state.eventIndex == 0, "first sound not selected");

    state = PhraseSelectionState::step(phrase, state, 1);
    expect(state.active && state.eventIndex == 1,
           "second sound not reached");
    state = PhraseSelectionState::step(phrase, state, 1);
    expect(state.active && state.eventIndex == 2,
           "the sound sharing a start tick was skipped");
    expect(PhraseSelectionState::resolve(phrase, state).eventIndex == 2,
           "the coincident sound did not resolve back to itself");
  }

  // 2. Ends hold rather than wrap, so the last press is not a jump home.
  {
    const Buffer phrase = makePhrase();
    State last = PhraseSelectionState::first(phrase);
    for (int i = 0; i < 5; ++i) last = PhraseSelectionState::step(phrase, last, 1);
    expect(last.eventIndex == 3, "stepping ran past the last sound");
    State firstState = PhraseSelectionState::first(phrase);
    firstState = PhraseSelectionState::step(phrase, firstState, -1);
    expect(firstState.eventIndex == 0, "stepping ran before the first sound");
  }

  // 3. Editing a length does not move the selection. Changing a sound must not
  //    change which sound you are on -- that is what makes the acceptance
  //    scenario (edit, switch view, still there) possible at all.
  {
    Buffer phrase = makePhrase();
    State state = PhraseSelectionState::first(phrase);
    state = PhraseSelectionState::step(phrase, state, 1);   // the 48/64 sound
    phrase.events[1].durationSubticks =
        static_cast<uint16_t>(48 * PhraseRuntime::kSubticksPerTick);
    const State after = PhraseSelectionState::resolve(phrase, state);
    expect(after.active && after.eventIndex == 1,
           "a length change moved the selection");
    expect(after.note == 64, "the selection lost track of which sound it was");
  }

  // 4. Deleting an earlier sound shifts the array; the selection must follow
  //    the same sound, not the same slot.
  {
    Buffer phrase = makePhrase();
    State state = PhraseSelectionState::first(phrase);
    state = PhraseSelectionState::step(phrase, state, 1);
    state = PhraseSelectionState::step(phrase, state, 1);   // 48/67, index 2
    expect(RuntimePhraseEdit::deleteEvent(phrase, 0) ==
               RuntimePhraseEdit::EventEditResult::Changed,
           "fixture delete failed");
    const State after = PhraseSelectionState::resolve(phrase, state);
    expect(after.active && after.note == 67 && after.startTick == 48,
           "the selection followed the slot instead of the sound");
    expect(after.eventIndex == 1, "the selection index was not repaired");
  }

  // 5. When the selected sound itself is gone, fall to the nearest survivor
  //    rather than to nothing: after a delete or a join the user is still
  //    somewhere, not nowhere.
  {
    Buffer phrase = makePhrase();
    State state = PhraseSelectionState::first(phrase);
    state = PhraseSelectionState::step(phrase, state, 1);   // 48/64
    expect(RuntimePhraseEdit::deleteEvent(phrase, 1) ==
               RuntimePhraseEdit::EventEditResult::Changed,
           "fixture delete failed");
    const State after = PhraseSelectionState::resolve(phrase, state);
    expect(after.active, "losing the selected sound lost the selection");
    expect(after.startTick == 48 && after.note == 67,
           "the fallback did not pick the nearest surviving sound");
  }

  // 6. Undo replaces the whole buffer. The selection must come back to the
  //    same sound, which is the last step of the acceptance scenario.
  {
    Buffer live = makePhrase();
    const Buffer beforeImage = live;
    State state = PhraseSelectionState::first(live);
    state = PhraseSelectionState::step(live, state, 1);
    state = PhraseSelectionState::step(live, state, 1);     // 48/67

    (void)RuntimePhraseEdit::joinNextEvent(live, 2);
    live = beforeImage;                                     // undo
    const State after = PhraseSelectionState::resolve(live, state);
    expect(after.active && after.startTick == 48 && after.note == 67,
           "undo did not restore the selection to the same sound");
  }

  // 7. An empty melody has no selection, and asking for one must not invent
  //    an index into nothing.
  {
    Buffer empty{};
    empty.lengthTicks = PhraseRuntime::kTicksPerBar;
    const State state = PhraseSelectionState::first(empty);
    expect(!state.active, "an empty melody reported a selected sound");
    expect(!PhraseSelectionState::resolve(empty, state).active,
           "resolving against an empty melody invented a selection");
  }

  // 8. The insert position is its own state and never renames the selection.
  {
    const Buffer phrase = makePhrase();
    State state = PhraseSelectionState::first(phrase);
    state = PhraseSelectionState::step(phrase, state, 1);
    State moved = PhraseSelectionState::withInsertTick(state, 240);
    expect(moved.insertTick == 240, "the insert position was not stored");
    expect(moved.eventIndex == state.eventIndex && moved.note == state.note,
           "moving the insert position changed the selected sound");
  }

  if (g_failures == 0) {
    std::printf("UI Constitution U4D1 selection identity: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "UI Constitution U4D1: %d failure(s)\n", g_failures);
  return 1;
}
