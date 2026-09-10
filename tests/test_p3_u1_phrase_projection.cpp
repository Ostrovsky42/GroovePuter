#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/ui/phrase_notes_projection.h"

namespace {

void testCrossBarLongNoteIsOneVisualSpan() {
  PhraseRuntime::RuntimeSynthEventBuffer phrase{};
  phrase.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  phrase.count = 1;
  auto& event = phrase.events[0];
  event.startTick = 360;
  event.durationSubticks = 96 * PhraseRuntime::kSubticksPerTick;
  event.note = 60;
  event.velocity = 100;
  event.probability = 100;

  assert(PhraseNotesProjection::validate(phrase));
  PhraseNotesProjection::NoteSpan span{};
  assert(PhraseNotesProjection::project(phrase, 0, span));
  assert(span.eventIndex == 0);
  assert(span.startSubtick == 360u * PhraseRuntime::kSubticksPerTick);
  assert(span.endSubtick == 456u * PhraseRuntime::kSubticksPerTick);
  assert(span.startTick == 360);
  assert(span.endTick == 456);
  assert(span.note == 60);
  assert(span.crossesBarBoundary);

  // Continuation is presentation data, never another musical event.
  assert(phrase.count == 1);
}

void testProjectionRejectsInvalidLiveShapeWithoutMutation() {
  PhraseRuntime::RuntimeSynthEventBuffer phrase{};
  phrase.lengthTicks = PhraseRuntime::kTicksPerBar;
  phrase.count = 1;
  phrase.events[0].startTick = 360;
  phrase.events[0].durationSubticks = 48 * PhraseRuntime::kSubticksPerTick;
  phrase.events[0].note = 64;

  const auto before = phrase;
  assert(!PhraseNotesProjection::validate(phrase));
  PhraseNotesProjection::NoteSpan span{};
  assert(!PhraseNotesProjection::project(phrase, 0, span));
  assert(phrase.count == before.count);
  assert(phrase.lengthTicks == before.lengthTicks);
  assert(phrase.events[0].startTick == before.events[0].startTick);
  assert(phrase.events[0].durationSubticks == before.events[0].durationSubticks);
}

}  // namespace

int main() {
  testCrossBarLongNoteIsOneVisualSpan();
  testProjectionRejectsInvalidLiveShapeWithoutMutation();
  std::puts("P3-U1 phrase read-only projection: OK");
  return 0;
}
