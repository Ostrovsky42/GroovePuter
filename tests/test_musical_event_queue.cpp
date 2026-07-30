#include <cassert>
#include <cstdint>

#include "src/input/musical_event_queue.h"

static MusicalEvent event(MusicalEventType type,
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

int main() {
    MusicalEventQueue queue;

    assert(queue.tryPush(event(MusicalEventType::NoteOn,
                               MusicalEventTarget::SynthA, 36)));
    assert(queue.tryPush(event(MusicalEventType::NoteOff,
                               MusicalEventTarget::SynthA, 36)));

    MusicalEvent out{};
    assert(queue.tryPop(out));
    assert(out.type == MusicalEventType::NoteOn);
    assert(out.note == 36);
    assert(queue.tryPop(out));
    assert(out.type == MusicalEventType::NoteOff);
    assert(!queue.tryPop(out));

    // A ring with one sentinel slot holds capacity - 1 events.
    for (std::size_t i = 0; i < MusicalEventQueue::kCapacity - 1; ++i) {
        assert(queue.tryPush(event(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA,
                                   static_cast<uint8_t>(24 + (i % 40)))));
    }
    assert(!queue.tryPush(event(MusicalEventType::NoteOff,
                                MusicalEventTarget::SynthB, 48)));
    assert(queue.droppedCount() == 1);
    assert(queue.takePendingAllNotesOffMask() == MusicalEventQueue::kSynthBMask);
    assert(queue.takePendingAllNotesOffMask() == 0);

    std::size_t popped = 0;
    while (queue.tryPop(out)) ++popped;
    assert(popped == MusicalEventQueue::kCapacity - 1);

    // Dropped NoteOn is observable but does not request a destructive panic.
    for (std::size_t i = 0; i < MusicalEventQueue::kCapacity - 1; ++i) {
        assert(queue.tryPush(event(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA, 36)));
    }
    assert(!queue.tryPush(event(MusicalEventType::NoteOn,
                                MusicalEventTarget::SynthA, 37)));
    assert(queue.takePendingAllNotesOffMask() == 0);
    assert(queue.droppedCount() == 2);

    return 0;
}
