#include "../src/midi/scheduled_musical_event_queue.h"

#include <cassert>
#include <cstddef>

namespace {

MusicalEvent noteEvent(MusicalEventType type,
                       MusicalEventTarget target,
                       uint8_t note) {
    return MusicalEvent{
        type,
        MusicalEventSource::PatternPlayer,
        target,
        0,
        note,
        100,
    };
}

void testTimestampAndPublicationOrder() {
    ScheduledMusicalEventQueue queue;
    assert(queue.tryPush(noteEvent(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA, 60),
                         9, 32));
    assert(queue.tryPush(noteEvent(MusicalEventType::NoteOff,
                                   MusicalEventTarget::SynthA, 60),
                         9, 96));

    ScheduledMusicalEvent first{};
    ScheduledMusicalEvent second{};
    assert(queue.tryPop(first));
    assert(queue.tryPop(second));
    assert(first.blockSequence == 9);
    assert(first.frameOffset == 32);
    assert(second.frameOffset == 96);
    assert(first.publicationSequence < second.publicationSequence);
    assert(first.generation == 0);
    assert(second.generation == 0);
    assert(!queue.tryPop(first));
}

void testGenerationInvalidation() {
    ScheduledMusicalEventQueue queue;
    assert(queue.generationFor(MusicalEventTarget::SynthA) == 0);
    queue.invalidateTarget(MusicalEventTarget::SynthA);
    assert(queue.generationFor(MusicalEventTarget::SynthA) == 1);
    assert(queue.takePendingAllNotesOffMask() ==
           ScheduledMusicalEventQueue::kSynthAMask);
    assert(queue.takePendingAllNotesOffMask() == 0);

    assert(queue.tryPush(noteEvent(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA, 64),
                         11, 4));
    ScheduledMusicalEvent scheduled{};
    assert(queue.tryPop(scheduled));
    assert(scheduled.generation == 1);
}

void testNoteOnOverflowIsObservableWithoutPanic() {
    ScheduledMusicalEventQueue queue;
    for (std::size_t i = 0; i < ScheduledMusicalEventQueue::kCapacity; ++i) {
        assert(queue.tryPush(noteEvent(MusicalEventType::NoteOn,
                                       MusicalEventTarget::SynthA,
                                       static_cast<uint8_t>(i % 128)),
                             static_cast<uint32_t>(i / 8),
                             static_cast<uint16_t>(i % 512)));
    }
    assert(!queue.tryPush(noteEvent(MusicalEventType::NoteOn,
                                    MusicalEventTarget::SynthA, 70),
                          99, 10));
    assert(queue.droppedNoteOnCount() == 1);
    assert(queue.droppedCriticalCount() == 0);
    assert(queue.takePendingAllNotesOffMask() == 0);
}

void testCriticalOverflowInvalidatesTarget() {
    ScheduledMusicalEventQueue queue;
    for (std::size_t i = 0; i < ScheduledMusicalEventQueue::kCapacity; ++i) {
        assert(queue.tryPush(noteEvent(MusicalEventType::NoteOn,
                                       MusicalEventTarget::SynthB,
                                       static_cast<uint8_t>(i % 128)),
                             static_cast<uint32_t>(i / 8),
                             static_cast<uint16_t>(i % 512)));
    }
    assert(!queue.tryPush(noteEvent(MusicalEventType::NoteOff,
                                    MusicalEventTarget::SynthB, 72),
                          99, 11));
    assert(queue.droppedCriticalCount() == 1);
    assert(queue.generationFor(MusicalEventTarget::SynthB) == 1);
    assert(queue.takePendingAllNotesOffMask() ==
           ScheduledMusicalEventQueue::kSynthBMask);
}

void testApproximateSizeAndDiscard() {
    ScheduledMusicalEventQueue queue;
    assert(queue.approximateSize() == 0);
    assert(queue.tryPush(noteEvent(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA, 60),
                         1, 0));
    assert(queue.approximateSize() == 1);
    queue.discardPending();
    assert(queue.approximateSize() == 0);
}

}  // namespace

int main() {
    testTimestampAndPublicationOrder();
    testGenerationInvalidation();
    testNoteOnOverflowIsObservableWithoutPanic();
    testCriticalOverflowInvalidatesTarget();
    testApproximateSizeAndDiscard();
}
