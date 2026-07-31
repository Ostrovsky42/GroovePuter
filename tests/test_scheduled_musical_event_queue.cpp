#include "../src/midi/pattern_drum_gate_scheduler.h"
#include "../src/midi/scheduled_musical_event_queue.h"

#include <cassert>
#include <cstddef>

namespace {

MusicalEvent noteEvent(MusicalEventType type,
                       MusicalEventTarget target,
                       uint8_t note,
                       uint8_t channel = 0) {
    return MusicalEvent{
        type,
        MusicalEventSource::PatternPlayer,
        target,
        channel,
        note,
        100,
    };
}

ScheduledMusicalEvent scheduledDrumHit(uint8_t voice,
                                       uint32_t block,
                                       uint16_t frame,
                                       uint32_t generation = 0,
                                       uint32_t publication = 1) {
    ScheduledMusicalEvent scheduled{};
    scheduled.event = noteEvent(
        MusicalEventType::NoteOn, MusicalEventTarget::Drums, 60, voice);
    scheduled.blockSequence = block;
    scheduled.frameOffset = frame;
    scheduled.generation = generation;
    scheduled.publicationSequence = publication;
    return scheduled;
}

void testTimestampAndPublicationOrder() {
    ScheduledMusicalEventQueue queue;
    assert(queue.tryPush(noteEvent(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA, 60), 9, 32));
    assert(queue.tryPush(noteEvent(MusicalEventType::NoteOff,
                                   MusicalEventTarget::SynthA, 60), 9, 96));

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
                                   MusicalEventTarget::SynthA, 64), 11, 4));
    ScheduledMusicalEvent scheduled{};
    assert(queue.tryPop(scheduled));
    assert(scheduled.generation == 1);
}

void testDrumGenerationIsIndependent() {
    ScheduledMusicalEventQueue queue;
    assert(queue.generationFor(MusicalEventTarget::Drums) == 0);
    assert(queue.generationFor(MusicalEventTarget::SynthA) == 0);
    assert(queue.generationFor(MusicalEventTarget::SynthB) == 0);

    assert(queue.tryPush(noteEvent(MusicalEventType::NoteOn,
                                   MusicalEventTarget::Drums, 60, 4), 12, 55));
    queue.invalidateTarget(MusicalEventTarget::Drums);
    assert(queue.generationFor(MusicalEventTarget::Drums) == 1);
    assert(queue.generationFor(MusicalEventTarget::SynthA) == 0);
    assert(queue.generationFor(MusicalEventTarget::SynthB) == 0);
    assert(queue.takePendingAllNotesOffMask() ==
           ScheduledMusicalEventQueue::kDrumsMask);

    ScheduledMusicalEvent stale{};
    assert(queue.tryPop(stale));
    assert(stale.event.target == MusicalEventTarget::Drums);
    assert(stale.event.channel == 4);
    assert(stale.generation == 0);
    assert(!scheduledMusicalEventGenerationIsCurrent(
        stale, queue.generationFor(MusicalEventTarget::Drums)));
}

void testLifecycleBarrier() {
    ScheduledMusicalEventQueue queue;
    assert(queue.tryPush(noteEvent(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA, 60), 20, 10));
    assert(queue.tryPush(noteEvent(MusicalEventType::AllNotesOff,
                                   MusicalEventTarget::SynthA, 0), 20, 20));
    assert(queue.generationFor(MusicalEventTarget::SynthA) == 1);
    assert(queue.takePendingAllNotesOffMask() ==
           ScheduledMusicalEventQueue::kSynthAMask);
    assert(queue.tryPush(noteEvent(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA, 62), 20, 20));

    ScheduledMusicalEvent stale{};
    ScheduledMusicalEvent current{};
    assert(queue.tryPop(stale));
    assert(queue.tryPop(current));
    assert(stale.event.note == 60);
    assert(stale.generation == 0);
    assert(current.event.note == 62);
    assert(current.generation == 1);
    assert(!queue.tryPop(stale));
}

void testPatternDrumGateUsesSampleTimeline() {
    PatternDrumGateScheduler gates;
    const ScheduledMusicalEvent kick = scheduledDrumHit(0, 10, 50);
    assert(gates.scheduleOrExtend(kick, 80, 1000, 100));
    assert(gates.activeCount() == 1);
    assert(gates.releaseCount(0) == 1);

    ScheduledMusicalEvent gate{};
    assert(gates.peekEarliest(gate));
    assert(gate.event.type == MusicalEventType::NoteOff);
    assert(gate.event.target == MusicalEventTarget::Drums);
    assert(gate.event.channel == 0);
    assert(gate.event.note == 60);
    // 80 frames after block 10 / frame 50 => block 11 / frame 30.
    assert(gate.blockSequence == 11);
    assert(gate.frameOffset == 30);
}

void testPatternDrumRetrigExtendsGateAndRetainsReleaseCount() {
    PatternDrumGateScheduler gates;
    assert(gates.scheduleOrExtend(scheduledDrumHit(2, 10, 50, 3, 1),
                                  80, 1000, 100));
    assert(gates.scheduleOrExtend(scheduledDrumHit(2, 11, 0, 3, 2),
                                  80, 1000, 100));
    assert(gates.releaseCount(2) == 2);

    ScheduledMusicalEvent gate{};
    assert(gates.peekEarliest(gate));
    assert(gate.event.channel == 2);
    // The stale block 11 / frame 30 gate was replaced; the retrigger owns an
    // extended deadline at block 11 / frame 80.
    assert(gate.blockSequence == 11);
    assert(gate.frameOffset == 80);
    assert(gate.generation == 3);
    assert(gate.publicationSequence == 2);
}

void testPatternDrumGateOrderingAndClear() {
    PatternDrumGateScheduler gates;
    assert(gates.scheduleOrExtend(scheduledDrumHit(6, 20, 90),
                                  50, 1000, 100));
    assert(gates.scheduleOrExtend(scheduledDrumHit(1, 20, 10),
                                  50, 1000, 100));

    ScheduledMusicalEvent earliest{};
    assert(gates.peekEarliest(earliest));
    assert(earliest.event.channel == 1);
    assert(earliest.blockSequence == 20);
    assert(earliest.frameOffset == 60);

    gates.consume(1);
    assert(!gates.active(1));
    assert(gates.active(6));
    assert(gates.releaseCount(1) == 0);
    gates.clear();
    assert(gates.activeCount() == 0);
    assert(!gates.peekEarliest(earliest));
}

void testPatternDrumGateRejectsInvalidEvents() {
    PatternDrumGateScheduler gates;
    ScheduledMusicalEvent invalid = scheduledDrumHit(8, 1, 0);
    assert(!gates.scheduleOrExtend(invalid, 80, 1000, 100));
    invalid = scheduledDrumHit(0, 1, 0);
    invalid.event.target = MusicalEventTarget::SynthA;
    assert(!gates.scheduleOrExtend(invalid, 80, 1000, 100));
    invalid = scheduledDrumHit(0, 1, 0);
    invalid.event.type = MusicalEventType::NoteOff;
    assert(!gates.scheduleOrExtend(invalid, 80, 1000, 100));
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
                                    MusicalEventTarget::SynthA, 70), 99, 10));
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
                                    MusicalEventTarget::SynthB, 72), 99, 11));
    assert(queue.droppedCriticalCount() == 1);
    assert(queue.generationFor(MusicalEventTarget::SynthB) == 1);
    assert(queue.takePendingAllNotesOffMask() ==
           ScheduledMusicalEventQueue::kSynthBMask);
}

void testDrumCriticalOverflowRequestsDrumPanic() {
    ScheduledMusicalEventQueue queue;
    for (std::size_t i = 0; i < ScheduledMusicalEventQueue::kCapacity; ++i) {
        assert(queue.tryPush(noteEvent(MusicalEventType::NoteOn,
                                       MusicalEventTarget::SynthA,
                                       static_cast<uint8_t>(i % 128)),
                             static_cast<uint32_t>(i / 8),
                             static_cast<uint16_t>(i % 512)));
    }
    assert(!queue.tryPush(noteEvent(MusicalEventType::NoteOff,
                                    MusicalEventTarget::Drums, 60, 2), 99, 12));
    assert(queue.droppedCriticalCount() == 1);
    assert(queue.generationFor(MusicalEventTarget::Drums) == 1);
    assert(queue.takePendingAllNotesOffMask() ==
           ScheduledMusicalEventQueue::kDrumsMask);
}

void testApproximateSizeAndDiscard() {
    ScheduledMusicalEventQueue queue;
    assert(queue.approximateSize() == 0);
    assert(queue.tryPush(noteEvent(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA, 60), 1, 0));
    assert(queue.approximateSize() == 1);
    queue.invalidateTarget(MusicalEventTarget::Drums);
    queue.discardPending();
    assert(queue.approximateSize() == 0);
    assert(queue.takePendingAllNotesOffMask() == 0);
}

}  // namespace

int main() {
    testTimestampAndPublicationOrder();
    testGenerationInvalidation();
    testDrumGenerationIsIndependent();
    testLifecycleBarrier();
    testPatternDrumGateUsesSampleTimeline();
    testPatternDrumRetrigExtendsGateAndRetainsReleaseCount();
    testPatternDrumGateOrderingAndClear();
    testPatternDrumGateRejectsInvalidEvents();
    testNoteOnOverflowIsObservableWithoutPanic();
    testCriticalOverflowInvalidatesTarget();
    testDrumCriticalOverflowRequestsDrumPanic();
    testApproximateSizeAndDiscard();
}
