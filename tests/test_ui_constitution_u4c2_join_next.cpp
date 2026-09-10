// U4C2: "continue instead of the next sound".
//
// Lengthening a note past the next attack cannot make it sound longer: the
// voice is monophonic and the next attack releases it. So the editor could
// draw a longer block that no one could hear, which is what made long notes
// feel worse here than through Patterns -- there a TIE step is not an attack,
// so nothing cuts the note.
//
// The answer is an explicit command, not a hidden effect of "longer": removing
// a neighbouring sound is a musical decision and must be asked for. One action,
// one undo, and a refusal that says why when there is nothing to join.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "src/ui/phrase_notes_join_edit.h"

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using Event = PhraseRuntime::RuntimeSynthEvent;
using Grid = RuntimePhraseEdit::Grid;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "U4C2 FAIL: %s\n", message);
  ++g_failures;
}

Event makeEvent(uint16_t startTick, uint16_t durationTicks, uint8_t note,
                uint8_t flags = 0) {
  Event event{};
  event.startTick = startTick;
  event.durationSubticks = static_cast<uint16_t>(
      durationTicks * PhraseRuntime::kSubticksPerTick);
  event.note = note;
  event.velocity = 100;
  event.probability = 100;
  event.flags = flags;
  return event;
}

// Selected note at 0 (24 ticks), next at 48 (72 ticks) so the neighbour ends
// later -- the case where "the later of the two ends" actually matters.
Buffer makePhrase() {
  Buffer phrase{};
  phrase.lengthTicks = PhraseRuntime::kTicksPerBar;
  phrase.count = 3;
  phrase.events[0] = makeEvent(0, 24, 60, PhraseRuntime::kEventAccent);
  phrase.events[1] = makeEvent(48, 72, 67, PhraseRuntime::kEventSlide);
  phrase.events[2] = makeEvent(240, 24, 72);
  return phrase;
}

bool same(const Buffer& a, const Buffer& b) {
  return std::memcmp(&a, &b, sizeof(Buffer)) == 0;
}

uint16_t endTick(const Event& e) {
  return static_cast<uint16_t>(
      e.startTick + e.durationSubticks / PhraseRuntime::kSubticksPerTick);
}

}  // namespace

int main() {
  using PhraseNotesJoinEdit::Prepared;
  using PhraseNotesJoinEdit::Result;

  // 1. The next attack is gone and the selected sound reaches the later end.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    const Result result =
        PhraseNotesJoinEdit::prepare(live, 0, Grid::Sixteenth, prepared);
    expect(result == Result::Ready, "join refused a plain neighbour");
    expect(prepared.after.count == live.count - 1,
           "the next sound was not removed");
    expect(endTick(prepared.after.events[0]) == 120,
           "the joined sound did not reach the later of the two ends");
    expect(same(prepared.before, live), "before-image was not the live value");
  }

  // 2. The surviving sound is the selected one: its pitch and its expressive
  //    parameters are kept, not the neighbour's. Joining must not silently
  //    change what the note is.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    (void)PhraseNotesJoinEdit::prepare(live, 0, Grid::Sixteenth, prepared);
    const Event& joined = prepared.after.events[0];
    expect(joined.note == 60, "the joined sound took the neighbour's pitch");
    expect((joined.flags & PhraseRuntime::kEventAccent) != 0,
           "the joined sound lost its accent");
    expect((joined.flags & PhraseRuntime::kEventSlide) == 0,
           "the joined sound inherited the neighbour's slide");
    expect(joined.startTick == 0, "the joined sound moved in time");
  }

  // 3. Unrelated sounds are untouched. Only the named neighbour goes.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    (void)PhraseNotesJoinEdit::prepare(live, 0, Grid::Sixteenth, prepared);
    bool foundLast = false;
    for (uint16_t i = 0; i < prepared.after.count; ++i) {
      if (prepared.after.events[i].startTick == 240 &&
          prepared.after.events[i].note == 72) {
        foundLast = true;
      }
    }
    expect(foundLast, "join removed a sound it was not asked about");
  }

  // 4. Nothing to join: refuse and change nothing, so the caller can explain.
  {
    const Buffer live = makePhrase();
    Prepared prepared{};
    expect(PhraseNotesJoinEdit::prepare(live, 250, Grid::Sixteenth, prepared) ==
               Result::NoNext,
           "join did not report the absence of a next sound");
    expect(same(prepared.after, live), "a refused join changed the melody");
  }

  // 5. No selection at all is its own refusal, not a silent success.
  {
    Buffer empty{};
    empty.lengthTicks = PhraseRuntime::kTicksPerBar;
    Prepared prepared{};
    expect(PhraseNotesJoinEdit::prepare(empty, 0, Grid::Sixteenth, prepared) ==
               Result::NoTarget,
           "join on an empty melody did not report having no target");
  }

  // 6. Two coincident next attacks are refused. Removing one would leave the
  //    other still cutting the note, so the command would appear to do nothing.
  {
    Buffer live = makePhrase();
    live.count = 3;
    live.events[0] = makeEvent(0, 24, 60);
    live.events[1] = makeEvent(48, 24, 67);
    live.events[2] = makeEvent(48, 24, 71);
    Prepared prepared{};
    expect(PhraseNotesJoinEdit::prepare(live, 0, Grid::Sixteenth, prepared) ==
               Result::Ambiguous,
           "join did not refuse two sounds starting together");
    expect(same(prepared.after, live), "a refused join changed the melody");
  }

  // 7. Commit is guarded by the same value contract as its siblings, so one
  //    undo restores both sounds exactly.
  {
    Buffer live = makePhrase();
    Prepared prepared{};
    (void)PhraseNotesJoinEdit::prepare(live, 0, Grid::Sixteenth, prepared);

    Buffer drifted = makePhrase();
    drifted.events[2].note = 71;
    expect(!PhraseNotesJoinEdit::commitIfUnchanged(drifted, prepared),
           "commit accepted a melody that had moved underneath it");
    expect(PhraseNotesJoinEdit::commitIfUnchanged(live, prepared),
           "commit refused the melody it was prepared against");
    expect(live.count == 2, "the committed melody still holds both sounds");
    expect(same(prepared.before, makePhrase()),
           "the before-image no longer restores the original melody");
  }

  if (g_failures == 0) {
    std::printf("UI Constitution U4C2 join next: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "UI Constitution U4C2 join next: %d failure(s)\n",
               g_failures);
  return 1;
}
