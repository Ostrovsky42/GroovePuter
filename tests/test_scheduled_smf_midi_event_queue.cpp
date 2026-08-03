#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/midi/scheduled_smf_midi_event_queue.h"

int main() {
    ScheduledSmfMidiEventQueue queue;
    assert(queue.generation() == 0);
    assert(queue.approximateSize() == 0);

    // Tempo re-anchoring invalidates future deadlines without requesting a
    // wire panic, so active notes can retain ownership until rebuilt NoteOffs.
    const uint32_t tempoGeneration = queue.invalidateScheduledEvents();
    assert(tempoGeneration == 1);
    uint32_t noPanicGeneration = 0;
    assert(!queue.takePendingPanic(noPanicGeneration));

    assert(!queue.tryPushNoteOn(16, 60, 100, 1, 0));
    assert(!queue.tryPushNoteOn(0, 128, 100, 1, 0));
    assert(queue.invalidEventCount() == 2);

    const std::size_t normalCapacity =
        ScheduledSmfMidiEventQueue::kCapacity -
        ScheduledSmfMidiEventQueue::kCriticalReserve;
    for (std::size_t i = 0; i < normalCapacity; ++i) {
        assert(queue.tryPushNoteOn(
            static_cast<uint8_t>(i % 16),
            static_cast<uint8_t>(36 + (i % 48)),
            100,
            10,
            static_cast<uint16_t>(i % 512)));
    }
    assert(queue.approximateSize() == normalCapacity);
    assert(!queue.tryPushNoteOn(0, 60, 100, 10, 0));
    assert(queue.droppedNoteOnCount() == 1);

    for (std::size_t i = 0;
         i < ScheduledSmfMidiEventQueue::kCriticalReserve;
         ++i) {
        assert(queue.tryPushNoteOff(
            static_cast<uint8_t>(i % 16),
            static_cast<uint8_t>(48 + i),
            0,
            10,
            static_cast<uint16_t>(i)));
    }
    assert(queue.approximateSize() == ScheduledSmfMidiEventQueue::kCapacity);

    assert(!queue.tryPushNoteOff(0, 60, 0, 10, 0));
    assert(queue.criticalOverflowCount() == 1);
    assert(queue.generation() == 2);

    uint32_t panicGeneration = 0;
    assert(queue.takePendingPanic(panicGeneration));
    assert(panicGeneration == 2);
    assert(queue.panicRecoveryCount() == 1);
    assert(!queue.takePendingPanic(panicGeneration));

    ScheduledSmfMidiEvent event{};
    std::size_t popped = 0;
    while (queue.tryPop(event)) {
        assert(event.generation == 1);
        assert(!scheduledSmfMidiEventGenerationIsCurrent(
            event, queue.generation()));
        ++popped;
    }
    assert(popped == ScheduledSmfMidiEventQueue::kCapacity);

    assert(queue.tryPushNoteOn(8, 64, 127, 42, 511, 0, 37));
    assert(queue.tryPop(event));
    assert(event.type == ScheduledSmfMidiEventType::NoteOn);
    assert(event.channel == 8);
    assert(event.note == 64);
    assert(event.velocity == 127);
    assert(event.trackIndex == 37);
    assert(event.blockSequence == 42);
    assert(event.frameOffset == 511);
    assert(event.generation == 2);
    assert(scheduledSmfMidiEventFrameIsValid(event, 512));
    assert(!scheduledSmfMidiEventFrameIsValid(event, 511));

    const uint32_t generation2 = queue.invalidateAndRequestPanic();
    assert(generation2 == 3);
    assert(queue.generation() == 3);
    assert(queue.takePendingPanic(panicGeneration));
    assert(panicGeneration == 3);

    assert(queue.tryPushNoteOff(3, 72, 12, 50, 10, 0, 99));
    assert(queue.tryPop(event));
    assert(event.type == ScheduledSmfMidiEventType::NoteOff);
    assert(event.trackIndex == 63);
    assert(event.generation == 3);
    assert(scheduledSmfMidiEventGenerationIsCurrent(event, queue.generation()));

    assert(queue.tryPushNoteOn(7, 60, 100, 70, 20, 9));
    assert(queue.tryPop(event));
    assert(event.projectTransportEpoch == 9);
    assert(scheduledSmfMidiEventTransportEpochIsCurrent(event, 9));
    assert(!scheduledSmfMidiEventTransportEpochIsCurrent(event, 10));

    queue.reportTransportFailure();
    assert(queue.transportFailed());
    assert(!queue.tryPushNoteOn(0, 60, 100, 60, 0));
    assert(!queue.tryPushNoteOff(0, 60, 0, 60, 0));

    uint32_t reportedGeneration = 0;
    assert(queue.takePendingTransportFailure(reportedGeneration));
    assert(reportedGeneration == 4);
    assert(!queue.takePendingTransportFailure(reportedGeneration));

    queue.reportTransportRecovery();
    assert(!queue.transportFailed());
    assert(queue.takePendingTransportRecovery());
    assert(!queue.takePendingTransportRecovery());
    assert(queue.tryPushNoteOn(1, 65, 90, 61, 12));
    assert(queue.tryPop(event));
    assert(event.generation == reportedGeneration);

    return 0;
}
