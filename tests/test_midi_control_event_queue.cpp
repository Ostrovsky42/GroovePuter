#include "../src/midi/midi_control_event_queue.h"

#include <cassert>
#include <cstddef>

namespace {

MusicalEvent liveEvent(MusicalEventType type,
                       MusicalEventTarget target,
                       uint8_t note) {
    return MusicalEvent{
        type,
        MusicalEventSource::PerformanceKeyboard,
        target,
        0,
        note,
        100,
    };
}

void testFifo() {
    MidiControlEventQueue queue;
    assert(queue.tryPush(liveEvent(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA, 60)));
    assert(queue.tryPush(liveEvent(MusicalEventType::NoteOff,
                                   MusicalEventTarget::SynthA, 60)));

    MusicalEvent first{};
    MusicalEvent second{};
    assert(queue.tryPop(first));
    assert(queue.tryPop(second));
    assert(first.type == MusicalEventType::NoteOn);
    assert(second.type == MusicalEventType::NoteOff);
    assert(!queue.tryPop(first));
}

void testNoteOnOverflowDoesNotPanic() {
    MidiControlEventQueue queue;
    for (std::size_t i = 0; i < MidiControlEventQueue::kCapacity; ++i) {
        assert(queue.tryPush(liveEvent(MusicalEventType::NoteOn,
                                       MusicalEventTarget::SynthA,
                                       static_cast<uint8_t>(i % 128))));
    }
    assert(!queue.tryPush(liveEvent(MusicalEventType::NoteOn,
                                    MusicalEventTarget::SynthA, 70)));
    assert(queue.droppedNoteOnCount() == 1);
    assert(queue.droppedCriticalCount() == 0);
    assert(queue.takePendingAllNotesOffMask() == 0);
}

void testCriticalOverflowRequestsTargetPanic() {
    MidiControlEventQueue queue;
    for (std::size_t i = 0; i < MidiControlEventQueue::kCapacity; ++i) {
        assert(queue.tryPush(liveEvent(MusicalEventType::NoteOn,
                                       MusicalEventTarget::SynthB,
                                       static_cast<uint8_t>(i % 128))));
    }
    assert(!queue.tryPush(liveEvent(MusicalEventType::NoteOff,
                                    MusicalEventTarget::SynthB, 72)));
    assert(queue.droppedCriticalCount() == 1);
    assert(queue.takePendingAllNotesOffMask() ==
           MidiControlEventQueue::kSynthBMask);
    assert(queue.takePendingAllNotesOffMask() == 0);
}

void testApproximateSize() {
    MidiControlEventQueue queue;
    assert(queue.approximateSize() == 0);
    assert(queue.tryPush(liveEvent(MusicalEventType::NoteOn,
                                   MusicalEventTarget::SynthA, 60)));
    assert(queue.approximateSize() == 1);
    MusicalEvent event{};
    assert(queue.tryPop(event));
    assert(queue.approximateSize() == 0);
}

}  // namespace

int main() {
    testFifo();
    testNoteOnOverflowDoesNotPanic();
    testCriticalOverflowRequestsTargetPanic();
    testApproximateSize();
}
