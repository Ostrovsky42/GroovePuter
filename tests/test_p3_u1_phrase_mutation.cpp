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
}

void testEngineCommitIsCompleteBufferOrNothing() {
  MiniAcid engine(44100.0f, nullptr);
  auto prepared = makePhrase();
  prepared.events[0].note = 67;

  const auto beforeA = engine.currentPhraseBuffer(0);
  const auto beforeB = engine.currentPhraseBuffer(1);
  const auto sourceA = engine.currentSequencedSource(0);

  assert(engine.commitPreparedPhrase(0, prepared));
  assert(samePhrase(engine.currentPhraseBuffer(0), prepared));
  assert(samePhrase(engine.currentPhraseBuffer(1), beforeB));
  assert(engine.currentSequencedSource(0) == sourceA);

  auto invalid = prepared;
  invalid.events[0].durationSubticks = 0;
  const auto committedA = engine.currentPhraseBuffer(0);
  assert(!engine.commitPreparedPhrase(0, invalid));
  assert(samePhrase(engine.currentPhraseBuffer(0), committedA));

  assert(!engine.commitPreparedPhrase(-1, prepared));
  assert(!engine.commitPreparedPhrase(NUM_303_VOICES, prepared));
  assert(samePhrase(engine.currentPhraseBuffer(0), committedA));
  assert(samePhrase(engine.currentPhraseBuffer(1), beforeB));

  (void)beforeA;
}

}  // namespace

int main() {
  testValidationRejectsEveryUnsafeShape();
  testPrepareNeverTouchesLiveState();
  testEngineCommitIsCompleteBufferOrNothing();
  std::puts("P3-U1 bounded phrase prepare/validate/commit: OK");
  return 0;
}
