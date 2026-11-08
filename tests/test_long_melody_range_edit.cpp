#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/phrase/runtime_phrase_edit.h"
#include "src/state/melody_store.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

#include <vector>

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using Event = PhraseRuntime::RuntimeSynthEvent;

namespace {

Event event(uint16_t startTick, uint16_t durationTicks, uint8_t note) {
  Event out{};
  out.startTick = startTick;
  out.durationSubticks = static_cast<uint16_t>(
      durationTicks * PhraseRuntime::kSubticksPerTick);
  out.note = note;
  out.velocity = 100;
  out.probability = 100;
  return out;
}

bool same(const Buffer& a, const Buffer& b) {
  return std::memcmp(&a, &b, sizeof(Buffer)) == 0;
}

bool sameEvent(const Event& a, const Event& b) {
  return a.startTick == b.startTick &&
         RuntimePhraseEdit::sameEventExceptStart(a, b);
}

Buffer eightBars() {
  Buffer out{};
  out.lengthTicks = 8 * PhraseRuntime::kTicksPerBar;
  out.count = 4;
  out.events[0] = event(48, 12, 60);
  out.events[1] = event(4 * PhraseRuntime::kTicksPerBar + 20, 12, 64);
  out.events[2] = event(5 * PhraseRuntime::kTicksPerBar + 300, 12, 67);
  out.events[3] = event(7 * PhraseRuntime::kTicksPerBar + 350, 16, 72);
  return out;
}

void testOnsetMovePreservesEventAndMaintainsOrder() {
  Buffer phrase{};
  phrase.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  phrase.count = 2;
  phrase.events[0] = event(120, 24, 60);
  phrase.events[1] = event(150, 12, 67);
  const Event moved = phrase.events[1];

  assert(RuntimePhraseEdit::moveEventByGrid(
             phrase, 1, -1, RuntimePhraseEdit::Grid::Sixteenth) ==
         RuntimePhraseEdit::EventEditResult::Changed);
  assert(phrase.events[0].startTick == 120);
  assert(phrase.events[0].note == 60);
  assert(phrase.events[1].startTick == 126);
  assert(phrase.events[1].note == moved.note);
  assert(phrase.events[1].durationSubticks == moved.durationSubticks);
  assert(RuntimePhraseEdit::validate(phrase));
}

void testOnsetMoveRejectsBoundsAndDuplicateWithoutMutation() {
  Buffer phrase{};
  phrase.lengthTicks = 8 * PhraseRuntime::kTicksPerBar;
  phrase.count = 2;
  phrase.events[0] = event(24, 12, 60);
  phrase.events[1] = event(48, 12, 67);
  Buffer before = phrase;
  assert(RuntimePhraseEdit::moveEventByGrid(
             phrase, 0, 1, RuntimePhraseEdit::Grid::Sixteenth) ==
         RuntimePhraseEdit::EventEditResult::Rejected);
  assert(same(phrase, before));

  phrase.events[0] = event(0, 12, 60);
  before = phrase;
  assert(RuntimePhraseEdit::moveEventByGrid(
             phrase, 0, -1, RuntimePhraseEdit::Grid::ThirtySecond) ==
         RuntimePhraseEdit::EventEditResult::Rejected);
  assert(same(phrase, before));

  phrase.count = 1;
  phrase.events[0] = event(8 * PhraseRuntime::kTicksPerBar - 2, 2, 72);
  before = phrase;
  assert(RuntimePhraseEdit::moveEventByGrid(
             phrase, 0, 1, RuntimePhraseEdit::Grid::ThirtySecond) ==
         RuntimePhraseEdit::EventEditResult::Rejected);
  assert(same(phrase, before));
}

void testDurationCanCrossABarBoundaryWithinThePhrase() {
  Buffer phrase{};
  phrase.lengthTicks = 8 * PhraseRuntime::kTicksPerBar;
  phrase.count = 1;
  phrase.events[0] = event(PhraseRuntime::kTicksPerBar - 6, 6, 60);

  assert(RuntimePhraseEdit::resizeEventByGrid(
             phrase, 0, 1, RuntimePhraseEdit::Grid::ThirtySecond) ==
         RuntimePhraseEdit::EventEditResult::Changed);
  assert(phrase.events[0].startTick == PhraseRuntime::kTicksPerBar - 6);
  assert(phrase.events[0].durationSubticks ==
         18 * PhraseRuntime::kSubticksPerTick);
  assert(RuntimePhraseEdit::validate(phrase));
}

void testInsertKeepsTimelineOrdered() {
  Buffer phrase{};
  phrase.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  phrase.count = 2;
  phrase.events[0] = event(192, 12, 67);
  phrase.events[1] = event(96, 12, 60);
  assert(RuntimePhraseEdit::insertSnapped(
             phrase, 144, 24, 64, 100) ==
         RuntimePhraseEdit::EventEditResult::Changed);
  assert(phrase.events[0].startTick == 96);
  assert(phrase.events[1].startTick == 144);
  assert(phrase.events[2].startTick == 192);
}

void testReplaceBarsFiveAndSixPreservesOutsideEvents() {
  Buffer phrase = eightBars();
  Buffer replacement{};
  replacement.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  replacement.count = 2;
  replacement.events[0] = event(10, 20, 80);
  replacement.events[1] = event(
      PhraseRuntime::kTicksPerBar + 300, 16, 84);
  const Event beforeFirst = phrase.events[0];
  const Event beforeLast = phrase.events[3];

  assert(RuntimePhraseEdit::replaceBarRange(phrase, 4, 2, replacement) ==
         RuntimePhraseEdit::RangeEditResult::Changed);
  assert(phrase.lengthTicks == 8 * PhraseRuntime::kTicksPerBar);
  assert(phrase.count == 4);
  assert(sameEvent(phrase.events[0], beforeFirst));
  assert(phrase.events[1].startTick ==
         4 * PhraseRuntime::kTicksPerBar + 10);
  assert(phrase.events[1].note == 80);
  assert(phrase.events[2].startTick ==
         5 * PhraseRuntime::kTicksPerBar + 300);
  assert(phrase.events[2].note == 84);
  assert(sameEvent(phrase.events[3], beforeLast));
  assert(RuntimePhraseEdit::validate(phrase));
}

void testExtractGeneratedRangeRebasesTimeAndRejectsCrossingNotes() {
  Buffer generated = eightBars();
  generated.events[1].note = 81;
  generated.events[2].note = 85;
  const Buffer sourceBefore = generated;
  Buffer selected{};
  selected.lengthTicks = PhraseRuntime::kTicksPerBar;
  selected.count = 1;
  selected.events[0] = event(0, 1, 1);

  assert(RuntimePhraseEdit::extractBarRange(generated, 4, 2, selected) ==
         RuntimePhraseEdit::RangeExtractResult::Ready);
  assert(same(generated, sourceBefore));
  assert(selected.lengthTicks == 2 * PhraseRuntime::kTicksPerBar);
  assert(selected.count == 2);
  assert(selected.events[0].startTick == 20);
  assert(selected.events[0].note == 81);
  assert(selected.events[1].startTick ==
         PhraseRuntime::kTicksPerBar + 300);
  assert(selected.events[1].note == 85);

  Buffer phrase = eightBars();
  assert(RuntimePhraseEdit::replaceBarRange(phrase, 4, 2, selected) ==
         RuntimePhraseEdit::RangeEditResult::Changed);
  assert(phrase.events[0].startTick == sourceBefore.events[0].startTick);
  assert(phrase.events[1].note == 81);
  assert(phrase.events[2].note == 85);
  assert(phrase.events[3].startTick == sourceBefore.events[3].startTick);

  generated.events[0] = event(4 * PhraseRuntime::kTicksPerBar - 4, 12, 60);
  Buffer untouched = selected;
  assert(RuntimePhraseEdit::extractBarRange(generated, 4, 2, selected) ==
         RuntimePhraseEdit::RangeExtractResult::Rejected);
  assert(same(selected, untouched));
}

void testExtractRejectsDuplicateOnsetsWithoutMutation() {
  Buffer generated = eightBars();
  generated.events[2] = event(
      4 * PhraseRuntime::kTicksPerBar + 20, 24, 67);
  assert(RuntimePhraseEdit::validate(generated));

  Buffer selected{};
  selected.lengthTicks = PhraseRuntime::kTicksPerBar;
  selected.count = 1;
  selected.events[0] = event(0, 12, 91);
  const Buffer before = selected;

  assert(RuntimePhraseEdit::extractBarRange(generated, 4, 2, selected) ==
         RuntimePhraseEdit::RangeExtractResult::Rejected);
  assert(same(selected, before));
}

void testExtractedRangeCanAlwaysRoundTripIntoItsSource() {
  uint32_t randomState = 0x61C88647u;
  const auto nextRandom = [&randomState]() {
    randomState = randomState * 1664525u + 1013904223u;
    return randomState;
  };
  constexpr uint8_t ranges[] = {1, 2, 4, 8};

  for (uint16_t trial = 0; trial < 250; ++trial) {
    Buffer source{};
    source.lengthTicks = 8 * PhraseRuntime::kTicksPerBar;
    source.count = static_cast<uint16_t>(nextRandom() % 41u);
    for (uint16_t i = 0; i < source.count; ++i) {
      const uint16_t start = static_cast<uint16_t>(
          nextRandom() % source.lengthTicks);
      const uint16_t remaining = static_cast<uint16_t>(
          source.lengthTicks - start);
      const uint16_t maxDuration = remaining < 24u ? remaining : 24u;
      const uint16_t duration = static_cast<uint16_t>(
          1u + nextRandom() % maxDuration);
      source.events[i] = event(start, duration,
                               static_cast<uint8_t>(48u + nextRandom() % 36u));
    }
    assert(RuntimePhraseEdit::validate(source));

    for (const uint8_t barCount : ranges) {
      for (uint8_t firstBar = 0; firstBar + barCount <= 8; ++firstBar) {
        Buffer selected{};
        selected.count = 1;
        selected.events[0] = event(0, 1, 91);
        const Buffer selectedBefore = selected;
        const auto extractResult = RuntimePhraseEdit::extractBarRange(
            source, firstBar, barCount, selected);
        if (extractResult == RuntimePhraseEdit::RangeExtractResult::Rejected) {
          assert(same(selected, selectedBefore));
          continue;
        }
        Buffer copy = source;
        assert(RuntimePhraseEdit::replaceBarRange(
                   copy, firstBar, barCount, selected) ==
               RuntimePhraseEdit::RangeEditResult::NoChange);
        assert(same(copy, source));
      }
    }
  }
}

void testOneBarReplacementAndRuntimeUndo() {
  Buffer live = eightBars();
  const Buffer before = live;
  Buffer candidate = live;
  Buffer replacement{};
  replacement.lengthTicks = PhraseRuntime::kTicksPerBar;
  replacement.count = 1;
  replacement.events[0] = event(192, 24, 91);

  assert(RuntimePhraseEdit::replaceBarRange(candidate, 0, 1, replacement) ==
         RuntimePhraseEdit::RangeEditResult::Changed);
  assert(same(live, before));
  assert(candidate.events[0].startTick == 192);
  assert(candidate.events[0].note == 91);
  for (uint16_t i = 1; i < before.count; ++i) {
    assert(sameEvent(candidate.events[i], before.events[i]));
  }

  GroovePuterUndo::UndoOwner undo{};
  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  receipt.voiceIndex = 0;
  receipt.source = 1;
  receipt.before = before;
  assert(undo.commitRuntimePrepared(
      GroovePuterUndo::UndoKind::RuntimePhrase, receipt,
      [&]() { assert(RuntimePhraseEdit::commit(live, candidate)); }));
  assert(same(live, candidate));

  const auto undoResult =
      undo.toggleRuntimePrepared<GroovePuterUndo::RuntimePhraseUndoPayload>(
          GroovePuterUndo::UndoKind::RuntimePhrase,
          [](const GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
            return GroovePuterUndo::validRuntimePhraseUndoPayload(retained) &&
                   retained.voiceIndex == 0;
          },
          [&](GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
            GroovePuterUndo::exchangeFixedValue(live, retained.before);
          });
  assert(undoResult == GroovePuterUndo::UndoResult::Restored);
  assert(same(live, before));

  const auto redoResult =
      undo.toggleRuntimePrepared<GroovePuterUndo::RuntimePhraseUndoPayload>(
          GroovePuterUndo::UndoKind::RuntimePhrase,
          [](const GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
            return GroovePuterUndo::validRuntimePhraseUndoPayload(retained) &&
                   retained.voiceIndex == 0;
          },
          [&](GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
            GroovePuterUndo::exchangeFixedValue(live, retained.before);
          });
  assert(redoResult == GroovePuterUndo::UndoResult::Restored);
  assert(same(live, candidate));
}

void testReplaceRangeNoopAndUnsortedReplacement() {
  Buffer phrase = eightBars();
  Buffer replacement{};
  replacement.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  replacement.count = 2;
  replacement.events[0] = event(
      PhraseRuntime::kTicksPerBar + 300, 12, 67);
  replacement.events[1] = event(20, 12, 64);
  const Buffer before = phrase;

  assert(RuntimePhraseEdit::replaceBarRange(phrase, 4, 2, replacement) ==
         RuntimePhraseEdit::RangeEditResult::NoChange);
  assert(same(phrase, before));

  replacement.events[0].note = 84;
  replacement.events[1].note = 80;
  assert(RuntimePhraseEdit::replaceBarRange(phrase, 4, 2, replacement) ==
         RuntimePhraseEdit::RangeEditResult::Changed);
  assert(phrase.events[1].startTick == 4 * PhraseRuntime::kTicksPerBar + 20);
  assert(phrase.events[1].note == 80);
  assert(phrase.events[2].startTick ==
         5 * PhraseRuntime::kTicksPerBar + 300);
  assert(phrase.events[2].note == 84);
}

void testReplaceRangeRejectsBoundaryCrossingAndBadRangesAtomically() {
  Buffer phrase = eightBars();
  phrase.events[0] = event(4 * PhraseRuntime::kTicksPerBar - 4, 12, 60);
  Buffer replacement{};
  replacement.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  replacement.count = 1;
  replacement.events[0] = event(10, 12, 80);
  Buffer before = phrase;
  assert(RuntimePhraseEdit::replaceBarRange(phrase, 4, 2, replacement) ==
         RuntimePhraseEdit::RangeEditResult::Rejected);
  assert(same(phrase, before));

  replacement.lengthTicks = PhraseRuntime::kTicksPerBar;
  assert(RuntimePhraseEdit::replaceBarRange(phrase, 4, 2, replacement) ==
         RuntimePhraseEdit::RangeEditResult::Rejected);
  assert(same(phrase, before));
  assert(RuntimePhraseEdit::replaceBarRange(phrase, 7, 2, replacement) ==
         RuntimePhraseEdit::RangeEditResult::Rejected);
  assert(same(phrase, before));
}

void testReplaceRangeRejectsDuplicateReplacementOnsetsAtomically() {
  Buffer phrase = eightBars();
  Buffer replacement{};
  replacement.lengthTicks = PhraseRuntime::kTicksPerBar;
  replacement.count = 2;
  replacement.events[0] = event(48, 12, 80);
  replacement.events[1] = event(48, 24, 84);
  const Buffer before = phrase;

  assert(RuntimePhraseEdit::replaceBarRange(phrase, 2, 1, replacement) ==
         RuntimePhraseEdit::RangeEditResult::Rejected);
  assert(same(phrase, before));
}

void testReplaceRangeReportsCapacityWithoutPartialMutation() {
  Buffer phrase{};
  phrase.lengthTicks = 8 * PhraseRuntime::kTicksPerBar;
  phrase.count = PhraseRuntime::kMaxSynthEvents;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    phrase.events[i] = event(static_cast<uint16_t>(i * 2), 1,
                             static_cast<uint8_t>(48 + (i % 24)));
  }
  Buffer replacement{};
  replacement.lengthTicks = PhraseRuntime::kTicksPerBar;
  replacement.count = 1;
  replacement.events[0] = event(1, 1, 90);
  const Buffer before = phrase;
  assert(RuntimePhraseEdit::replaceBarRange(phrase, 7, 1, replacement) ==
         RuntimePhraseEdit::RangeEditResult::CapacityFull);
  assert(same(phrase, before));
}

void testOneHundredEditSwitchUndoAndStoreCycles() {
  Buffer voices[2] = {eightBars(), eightBars()};
  voices[1].events[0].note = 48;
  GroovePuterUndo::UndoOwner undo{};
  std::vector<uint8_t> encoded;

  for (uint16_t cycle = 0; cycle < 100; ++cycle) {
    const uint8_t voice = static_cast<uint8_t>(cycle % 2);
    Buffer& live = voices[voice];
    const Buffer before = live;
    Buffer candidate = live;
    Buffer replacement{};
    replacement.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
    replacement.count = 2;
    replacement.events[0] = event(48, 24,
        static_cast<uint8_t>(60 + (cycle % 24)));
    replacement.events[1] = event(
        PhraseRuntime::kTicksPerBar + 300, 24,
        static_cast<uint8_t>(84 + (cycle % 24)));

    assert(RuntimePhraseEdit::replaceBarRange(candidate, 4, 2, replacement) ==
           RuntimePhraseEdit::RangeEditResult::Changed);
    assert(sameEvent(candidate.events[0], before.events[0]));
    assert(sameEvent(candidate.events[candidate.count - 1],
                     before.events[before.count - 1]));

    int selected = -1;
    for (uint16_t i = 0; i < candidate.count; ++i) {
      if (candidate.events[i].note == replacement.events[0].note) {
        selected = static_cast<int>(i);
        break;
      }
    }
    assert(selected >= 0);
    assert(RuntimePhraseEdit::moveEventByGrid(
               candidate, static_cast<uint16_t>(selected), 1,
               RuntimePhraseEdit::Grid::ThirtySecond) ==
           RuntimePhraseEdit::EventEditResult::Changed);
    assert(RuntimePhraseEdit::resizeEventByGrid(
               candidate, static_cast<uint16_t>(selected), 1,
               RuntimePhraseEdit::Grid::ThirtySecond) ==
           RuntimePhraseEdit::EventEditResult::Changed);
    assert(RuntimePhraseEdit::insertSnapped(
               candidate, 120, 24, 100, 90) ==
           RuntimePhraseEdit::EventEditResult::Changed);
    int inserted = -1;
    for (uint16_t i = 0; i < candidate.count; ++i) {
      if (candidate.events[i].note == 100 && candidate.events[i].startTick == 120) {
        inserted = static_cast<int>(i);
        break;
      }
    }
    assert(inserted >= 0);
    assert(RuntimePhraseEdit::deleteEvent(
               candidate, static_cast<uint16_t>(inserted)) ==
           RuntimePhraseEdit::EventEditResult::Changed);
    assert(RuntimePhraseEdit::validate(candidate));

    GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
    receipt.voiceIndex = voice;
    receipt.source = 1;
    receipt.before = before;
    assert(undo.commitRuntimePrepared(
        GroovePuterUndo::UndoKind::RuntimePhrase, receipt,
        [&]() { assert(RuntimePhraseEdit::commit(live, candidate)); }));
    assert(MelodyStore::encode(live, encoded));
    Buffer decoded{};
    assert(MelodyStore::decode(encoded.data(), encoded.size(), decoded));
    assert(same(live, decoded));

    const auto undoResult =
        undo.toggleRuntimePrepared<GroovePuterUndo::RuntimePhraseUndoPayload>(
            GroovePuterUndo::UndoKind::RuntimePhrase,
            [voice](const GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
              return GroovePuterUndo::validRuntimePhraseUndoPayload(retained) &&
                     retained.voiceIndex == voice;
            },
            [&](GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
              GroovePuterUndo::exchangeFixedValue(live, retained.before);
            });
    assert(undoResult == GroovePuterUndo::UndoResult::Restored);
    assert(same(live, before));

    const auto redoResult =
        undo.toggleRuntimePrepared<GroovePuterUndo::RuntimePhraseUndoPayload>(
            GroovePuterUndo::UndoKind::RuntimePhrase,
            [voice](const GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
              return GroovePuterUndo::validRuntimePhraseUndoPayload(retained) &&
                     retained.voiceIndex == voice;
            },
            [&](GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
              GroovePuterUndo::exchangeFixedValue(live, retained.before);
            });
    assert(redoResult == GroovePuterUndo::UndoResult::Restored);
    assert(same(live, decoded));
  }
}

}  // namespace

int main() {
  testOnsetMovePreservesEventAndMaintainsOrder();
  testOnsetMoveRejectsBoundsAndDuplicateWithoutMutation();
  testDurationCanCrossABarBoundaryWithinThePhrase();
  testInsertKeepsTimelineOrdered();
  testReplaceBarsFiveAndSixPreservesOutsideEvents();
  testExtractGeneratedRangeRebasesTimeAndRejectsCrossingNotes();
  testExtractRejectsDuplicateOnsetsWithoutMutation();
  testExtractedRangeCanAlwaysRoundTripIntoItsSource();
  testOneBarReplacementAndRuntimeUndo();
  testReplaceRangeNoopAndUnsortedReplacement();
  testReplaceRangeRejectsBoundaryCrossingAndBadRangesAtomically();
  testReplaceRangeRejectsDuplicateReplacementOnsetsAtomically();
  testReplaceRangeReportsCapacityWithoutPartialMutation();
  testOneHundredEditSwitchUndoAndStoreCycles();
}
