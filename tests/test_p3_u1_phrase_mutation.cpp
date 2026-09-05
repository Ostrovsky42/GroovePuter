#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "src/dsp/miniacid_engine.h"
#include "src/phrase/runtime_phrase_edit.h"

SerialMock Serial;
SDMock SD;

namespace {

PhraseRuntime::RuntimeSynthEventBuffer makePhrase(uint8_t bars = 2) {
  PhraseRuntime::RuntimeSynthEventBuffer phrase{};
  phrase.lengthTicks = static_cast<uint16_t>(bars * PhraseRuntime::kTicksPerBar);
  phrase.count = 1;
  phrase.events[0].startTick = 360;
  phrase.events[0].durationSubticks =
      96 * PhraseRuntime::kSubticksPerTick;
  phrase.events[0].note = 60;
  phrase.events[0].velocity = 100;
  phrase.events[0].probability = 100;
  return phrase;
}

bool samePhrase(const PhraseRuntime::RuntimeSynthEventBuffer& lhs,
                const PhraseRuntime::RuntimeSynthEventBuffer& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

void testValidationRejectsEveryUnsafeShape() {
  auto valid = makePhrase();
  assert(RuntimePhraseEdit::validate(valid));

  auto invalidCount = valid;
  invalidCount.count = PhraseRuntime::kMaxSynthEvents + 1;
  assert(!RuntimePhraseEdit::validate(invalidCount));

  auto invalidLength = valid;
  invalidLength.lengthTicks = 3 * PhraseRuntime::kTicksPerBar;
  assert(!RuntimePhraseEdit::validate(invalidLength));

  auto zeroDuration = valid;
  zeroDuration.events[0].durationSubticks = 0;
  assert(!RuntimePhraseEdit::validate(zeroDuration));

  auto outsideStart = valid;
  outsideStart.events[0].startTick = outsideStart.lengthTicks;
  assert(!RuntimePhraseEdit::validate(outsideStart));

  auto beyondEnd = makePhrase(1);
  beyondEnd.events[0].startTick = 360;
  beyondEnd.events[0].durationSubticks =
      48 * PhraseRuntime::kSubticksPerTick;
  assert(!RuntimePhraseEdit::validate(beyondEnd));
}

void testValidationRejectsOutOfRangeEventValues() {
  auto valid = makePhrase();

  auto invalidNote = valid;
  invalidNote.events[0].note = 128;
  assert(!RuntimePhraseEdit::validate(invalidNote));

  auto zeroVelocity = valid;
  zeroVelocity.events[0].velocity = 0;
  assert(!RuntimePhraseEdit::validate(zeroVelocity));

  auto invalidVelocity = valid;
  invalidVelocity.events[0].velocity = 128;
  assert(!RuntimePhraseEdit::validate(invalidVelocity));

  auto invalidProbability = valid;
  invalidProbability.events[0].probability = 101;
  assert(!RuntimePhraseEdit::validate(invalidProbability));
}

void testPrepareNeverTouchesLiveState() {
  auto live = makePhrase();
  const auto before = live;
  PhraseRuntime::RuntimeSynthEventBuffer prepared{};

  const auto result = RuntimePhraseEdit::prepare(
      live, prepared, [](PhraseRuntime::RuntimeSynthEventBuffer& candidate) {
        candidate.events[0].durationSubticks =
            120 * PhraseRuntime::kSubticksPerTick;
      });

  assert(result == RuntimePhraseEdit::PrepareResult::Ready);
  assert(samePhrase(live, before));
  assert(prepared.events[0].durationSubticks ==
         120 * PhraseRuntime::kSubticksPerTick);
  assert(RuntimePhraseEdit::validate(prepared));

  PhraseRuntime::RuntimeSynthEventBuffer rejected{};
  const auto rejectedResult = RuntimePhraseEdit::prepare(
      live, rejected, [](PhraseRuntime::RuntimeSynthEventBuffer& candidate) {
        candidate.lengthTicks = PhraseRuntime::kTicksPerBar;
      });
  assert(rejectedResult == RuntimePhraseEdit::PrepareResult::Rejected);
  assert(samePhrase(live, before));
  assert(samePhrase(rejected, live));

  auto invalidLive = live;
  invalidLive.events[0].velocity = 0;
  auto stalePrepared = makePhrase();
  stalePrepared.events[0].note = 72;
  assert(!samePhrase(stalePrepared, invalidLive));

  const auto invalidLiveResult = RuntimePhraseEdit::prepare(
      invalidLive,
      stalePrepared,
      [](PhraseRuntime::RuntimeSynthEventBuffer&) {});
  assert(invalidLiveResult == RuntimePhraseEdit::PrepareResult::Rejected);
  assert(samePhrase(stalePrepared, invalidLive));
}

void testOwnerCommitIsCompleteBufferOrNothing() {
  MiniAcid engine(44100.0f, nullptr);
  auto prepared = makePhrase();
  prepared.events[0].note = 67;

  const auto beforeB = engine.currentPhraseBuffer(1);
  const auto sourceA = engine.currentSequencedSource(0);

  auto& liveA = engine.currentPhraseBuffer(0);
  assert(RuntimePhraseEdit::commit(liveA, prepared));
  assert(samePhrase(engine.currentPhraseBuffer(0), prepared));
  assert(samePhrase(engine.currentPhraseBuffer(1), beforeB));
  assert(engine.currentSequencedSource(0) == sourceA);

  auto invalid = prepared;
  invalid.events[0].durationSubticks = 0;
  const auto committedA = engine.currentPhraseBuffer(0);
  assert(!RuntimePhraseEdit::commit(liveA, invalid));
  assert(samePhrase(engine.currentPhraseBuffer(0), committedA));
  assert(samePhrase(engine.currentPhraseBuffer(1), beforeB));
  assert(engine.currentSequencedSource(0) == sourceA);
}

void testSnappedInsertCreatesExactlyOneEvent() {
  PhraseRuntime::RuntimeSynthEventBuffer phrase{};
  phrase.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  phrase.count = 0;

  const auto result = RuntimePhraseEdit::insertSnapped(
      phrase, 371, 24, 64, 100);
  assert(result == RuntimePhraseEdit::EventEditResult::Changed);
  assert(phrase.count == 1);
  assert(phrase.events[0].startTick == 360);
  assert(phrase.events[0].durationSubticks ==
         24 * PhraseRuntime::kSubticksPerTick);
  assert(phrase.events[0].note == 64);
  assert(phrase.events[0].velocity == 100);
  assert(phrase.events[0].probability == 100);
  assert(RuntimePhraseEdit::validate(phrase));
}

void testSnappedInsertRejectsInvalidMidiValuesWithoutMutation() {
  PhraseRuntime::RuntimeSynthEventBuffer phrase{};
  phrase.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  const auto before = phrase;

  assert(RuntimePhraseEdit::insertSnapped(phrase, 120, 24, 128, 100) ==
         RuntimePhraseEdit::EventEditResult::Rejected);
  assert(samePhrase(phrase, before));

  assert(RuntimePhraseEdit::insertSnapped(phrase, 120, 24, 64, 0) ==
         RuntimePhraseEdit::EventEditResult::Rejected);
  assert(samePhrase(phrase, before));

  assert(RuntimePhraseEdit::insertSnapped(phrase, 120, 24, 64, 128) ==
         RuntimePhraseEdit::EventEditResult::Rejected);
  assert(samePhrase(phrase, before));
}

void testCapacityRejectsWithoutChangingCandidate() {
  PhraseRuntime::RuntimeSynthEventBuffer phrase{};
  phrase.lengthTicks = 8 * PhraseRuntime::kTicksPerBar;
  phrase.count = PhraseRuntime::kMaxSynthEvents;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    phrase.events[i].startTick = static_cast<uint16_t>(i * 12u);
    phrase.events[i].durationSubticks =
        12 * PhraseRuntime::kSubticksPerTick;
    phrase.events[i].note = static_cast<uint8_t>(48 + (i % 24));
    phrase.events[i].velocity = 100;
    phrase.events[i].probability = 100;
  }
  assert(RuntimePhraseEdit::validate(phrase));
  const auto before = phrase;

  const auto result = RuntimePhraseEdit::insertSnapped(
      phrase, 2000, 12, 72, 100);
  assert(result == RuntimePhraseEdit::EventEditResult::CapacityFull);
  assert(samePhrase(phrase, before));
}

void testDeleteIsBoundedAndDeterministic() {
  PhraseRuntime::RuntimeSynthEventBuffer phrase{};
  phrase.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  phrase.count = 3;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    phrase.events[i].startTick = static_cast<uint16_t>(100 + i * 100);
    phrase.events[i].durationSubticks =
        24 * PhraseRuntime::kSubticksPerTick;
    phrase.events[i].note = static_cast<uint8_t>(60 + i);
    phrase.events[i].velocity = 100;
    phrase.events[i].probability = 100;
  }
  const auto before = phrase;

  assert(RuntimePhraseEdit::deleteEvent(phrase, 1) ==
         RuntimePhraseEdit::EventEditResult::Changed);
  assert(phrase.count == 2);
  assert(phrase.events[0].note == before.events[0].note);
  assert(phrase.events[1].note == before.events[2].note);
  assert(phrase.events[2].startTick == 0);
  assert(phrase.events[2].durationSubticks == 0);
  assert(RuntimePhraseEdit::validate(phrase));

  const auto afterDelete = phrase;
  assert(RuntimePhraseEdit::deleteEvent(phrase, 7) ==
         RuntimePhraseEdit::EventEditResult::NoTarget);
  assert(samePhrase(phrase, afterDelete));
}

void testCoverageUsesLatestOnsetThenLowestIndex() {
  PhraseRuntime::RuntimeSynthEventBuffer phrase{};
  phrase.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  phrase.count = 4;

  phrase.events[0].startTick = 300;
  phrase.events[0].durationSubticks = 100 * PhraseRuntime::kSubticksPerTick;
  phrase.events[1].startTick = 340;
  phrase.events[1].durationSubticks = 80 * PhraseRuntime::kSubticksPerTick;
  phrase.events[2].startTick = 340;
  phrase.events[2].durationSubticks = 60 * PhraseRuntime::kSubticksPerTick;
  phrase.events[3].startTick = 360;
  phrase.events[3].durationSubticks = 24 * PhraseRuntime::kSubticksPerTick;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    phrase.events[i].note = static_cast<uint8_t>(60 + i);
    phrase.events[i].velocity = 100;
    phrase.events[i].probability = 100;
  }
  assert(RuntimePhraseEdit::validate(phrase));

  assert(RuntimePhraseEdit::eventCoveringTick(phrase, 350) == 1);
  assert(RuntimePhraseEdit::eventCoveringTick(phrase, 370) == 3);
  assert(RuntimePhraseEdit::eventCoveringTick(phrase, 410) == 1);
  assert(RuntimePhraseEdit::eventCoveringTick(phrase, 420) == -1);
  assert(RuntimePhraseEdit::eventCoveringTick(phrase, 500) == -1);
}

}  // namespace

int main() {
  testValidationRejectsEveryUnsafeShape();
  testValidationRejectsOutOfRangeEventValues();
  testPrepareNeverTouchesLiveState();
  testOwnerCommitIsCompleteBufferOrNothing();
  testSnappedInsertCreatesExactlyOneEvent();
  testSnappedInsertRejectsInvalidMidiValuesWithoutMutation();
  testCapacityRejectsWithoutChangingCandidate();
  testDeleteIsBoundedAndDeterministic();
  testCoverageUsesLatestOnsetThenLowestIndex();
  std::puts("P3-U1 bounded phrase mutation: OK");
  return 0;
}
