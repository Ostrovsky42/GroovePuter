// Pattern/Phrase instrument closure: the event drawn as selected must be the
// event a destructive edit changes. Cursor time is an insertion/navigation
// position and cannot identify coincident or off-grid events.

#include <cstdint>
#include <cstdio>

#include "src/ui/phrase_notes_delete_edit.h"
#include "src/ui/phrase_notes_duration_edit.h"
#include "src/ui/phrase_notes_pitch_edit.h"

namespace {
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "Selected-event edit targeting FAIL: %s\n", message);
  ++g_failures;
}

void add(Buffer& phrase, uint16_t startTick, uint16_t durationTicks, uint8_t note) {
  auto& event = phrase.events[phrase.count++];
  event = PhraseRuntime::RuntimeSynthEvent{};
  event.startTick = startTick;
  event.durationSubticks = static_cast<uint16_t>(
      durationTicks * PhraseRuntime::kSubticksPerTick);
  event.note = note;
  event.velocity = 100;
  event.probability = 100;
}

Buffer fixture() {
  Buffer phrase{};
  phrase.lengthTicks = PhraseRuntime::kTicksPerBar;
  add(phrase, 17, 24, 60);   // deliberately off GRID
  add(phrase, 48, 24, 64);   // coincident pair
  add(phrase, 48, 24, 67);
  add(phrase, 120, 24, 72);
  return phrase;
}
}  // namespace

int main() {
  // Two sounds can start at the same tick. Selecting the second one must not
  // silently edit the first one just because both share cursor-time truth.
  {
    const Buffer phrase = fixture();
    PhraseNotesPitchEdit::Prepared prepared{};
    const auto result = PhraseNotesPitchEdit::prepareSelected(
        phrase, 2, 1, prepared);
    expect(result == PhraseNotesPitchEdit::Result::Ready,
           "selected pitch edit was not prepared");
    expect(prepared.after.events[1].note == 64,
           "pitch edit changed the other coincident sound");
    expect(prepared.after.events[2].note == 68,
           "pitch edit did not change the selected sound");
  }

  // A musician may select an event that does not land on the current GRID.
  // GRID controls edit increments; it is not event identity.
  {
    const Buffer phrase = fixture();
    PhraseNotesDurationEdit::Prepared prepared{};
    const auto result = PhraseNotesDurationEdit::prepareSelected(
        phrase, 0, RuntimePhraseEdit::Grid::Sixteenth, 1, prepared);
    expect(result == PhraseNotesDurationEdit::Result::Ready,
           "off-grid selected duration edit was rejected");
    expect(prepared.after.events[0].durationSubticks >
               phrase.events[0].durationSubticks,
           "duration edit did not change the selected off-grid sound");
    expect(prepared.after.events[1].durationSubticks ==
               phrase.events[1].durationSubticks,
           "duration edit changed a neighbouring sound");
  }

  // Delete must be index-addressed from the resolved selection as well. With
  // coincident starts, deleting by cursor time cannot express this operation.
  {
    const Buffer phrase = fixture();
    PhraseNotesDeleteEdit::Prepared prepared{};
    const auto result = PhraseNotesDeleteEdit::prepareSelected(
        phrase, 2, prepared);
    expect(result == PhraseNotesDeleteEdit::Result::Ready,
           "selected delete was not prepared");
    expect(prepared.after.count == phrase.count - 1,
           "selected delete did not remove exactly one sound");
    expect(prepared.after.events[1].note == 64,
           "selected delete removed the wrong coincident sound");
    expect(prepared.after.events[2].note == 72,
           "selected delete did not remove the selected sound");
  }

  if (g_failures == 0) {
    std::printf("Selected-event edit targeting: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "Selected-event edit targeting: %d failure(s)\n",
               g_failures);
  return 1;
}
