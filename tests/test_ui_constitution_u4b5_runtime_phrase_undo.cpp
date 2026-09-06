#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "src/phrase/runtime_synth_events.h"
#include "src/state/scene_revision.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using Event = PhraseRuntime::RuntimeSynthEvent;
using GroovePuterUndo::RuntimePhraseUndoPayload;
using GroovePuterUndo::UndoKind;
using GroovePuterUndo::UndoOwner;
using GroovePuterUndo::UndoResult;

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

Buffer makePhrase(uint16_t durationTicks) {
  Buffer phrase{};
  phrase.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  phrase.count = 1;
  phrase.events[0] = makeEvent(96, durationTicks, 60);
  return phrase;
}

bool same(const Buffer& lhs, const Buffer& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(Buffer)) == 0;
}

void exchangeRuntimeState(Buffer& live,
                          uint8_t& source,
                          RuntimePhraseUndoPayload& retained) {
  GroovePuterUndo::exchangeFixedValue(live, retained.before);
  const uint8_t oldSource = source;
  source = retained.source;
  retained.source = oldSource;
}

}  // namespace

int main() {
  static_assert(std::is_trivially_copyable<RuntimePhraseUndoPayload>::value,
                "runtime Phrase receipt must be a bounded fixed value");
  static_assert(sizeof(RuntimePhraseUndoPayload) <=
                    GroovePuterUndo::kUndoPayloadBytes,
                "runtime Phrase receipt must fit the single existing Undo slot");
  static_assert(GroovePuterUndo::kUndoPayloadBytes == 1536,
                "U4B5 must not grow the authoritative Undo owner");
  static_assert(sizeof(UndoOwner) <= 1552,
                "U4B5 must not add a second resident Undo payload");

  {
    UndoOwner owner{};
    GroovePuterState::restoreSceneRevision({42, 42});

    Buffer live = makePhrase(24);
    const Buffer beforeBuffer = live;
    const Buffer afterBuffer = makePhrase(48);
    uint8_t source = 1;

    RuntimePhraseUndoPayload before{};
    before.voiceIndex = 0;
    before.source = source;
    before.before = beforeBuffer;

    const auto revisionBefore = GroovePuterState::sceneRevisionSnapshot();
    const bool committed = owner.commitRuntimePrepared(
        UndoKind::RuntimePhrase, before, [&]() { live = afterBuffer; });

    assert(committed);
    assert(same(live, afterBuffer));
    assert(owner.hasUndo());
    assert(owner.kind() == UndoKind::RuntimePhrase);
    assert(!owner.nextIsRedo());

    const auto revisionAfterCommit = GroovePuterState::sceneRevisionSnapshot();
    assert(revisionAfterCommit.currentRevision == revisionBefore.currentRevision);
    assert(revisionAfterCommit.persistedRevision == revisionBefore.persistedRevision);
    assert(!GroovePuterState::sceneDirty());

    const UndoResult undo = owner.toggleRuntimePrepared<RuntimePhraseUndoPayload>(
        UndoKind::RuntimePhrase,
        [&](const RuntimePhraseUndoPayload& retained) {
          return retained.voiceIndex == 0;
        },
        [&](RuntimePhraseUndoPayload& retained) {
          exchangeRuntimeState(live, source, retained);
        });
    assert(undo == UndoResult::Restored);
    assert(same(live, beforeBuffer));
    assert(source == 1);
    assert(owner.nextIsRedo());
    assert(!GroovePuterState::sceneDirty());

    const UndoResult redo = owner.toggleRuntimePrepared<RuntimePhraseUndoPayload>(
        UndoKind::RuntimePhrase,
        [&](const RuntimePhraseUndoPayload& retained) {
          return retained.voiceIndex == 0;
        },
        [&](RuntimePhraseUndoPayload& retained) {
          exchangeRuntimeState(live, source, retained);
        });
    assert(redo == UndoResult::Restored);
    assert(same(live, afterBuffer));
    assert(source == 1);
    assert(!owner.nextIsRedo());
    assert(!GroovePuterState::sceneDirty());
  }

  {
    UndoOwner owner{};
    GroovePuterState::restoreSceneRevision({100, 100});
    Buffer live = makePhrase(24);
    const Buffer afterBuffer = makePhrase(48);
    uint8_t source = 1;

    RuntimePhraseUndoPayload before{};
    before.voiceIndex = 1;
    before.source = source;
    before.before = live;

    assert(owner.commitRuntimePrepared(
        UndoKind::RuntimePhrase, before, [&]() { live = afterBuffer; }));
    assert(GroovePuterState::sceneRevisionSnapshot().currentRevision == 100);

    GroovePuterState::markSceneMutated();
    const Buffer staleBefore = live;
    const UndoResult result = owner.toggleRuntimePrepared<RuntimePhraseUndoPayload>(
        UndoKind::RuntimePhrase,
        [&](const RuntimePhraseUndoPayload&) { return true; },
        [&](RuntimePhraseUndoPayload& retained) {
          exchangeRuntimeState(live, source, retained);
        });
    assert(result == UndoResult::Expired);
    assert(same(live, staleBefore));
  }

  return 0;
}
