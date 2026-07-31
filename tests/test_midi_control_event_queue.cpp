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

void fillQueue(MidiControlEventQueue& queue,
               MusicalEventTarget target) {
    for (std::size_t i = 0; i < MidiControlEventQueue::kCapacity; ++i) {
        assert(queue.tryPush(liveEvent(MusicalEventType::NoteOn,
                                       target,
                                       static_cast<uint8_t>(i % 128))));
    }
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
    fillQueue(queue, MusicalEventTarget::SynthA);
    assert(!queue.tryPush(liveEvent(MusicalEventType::NoteOn,
                                    MusicalEventTarget::SynthA, 70)));
    assert(queue.droppedNoteOnCount() == 1);
    assert(queue.droppedCriticalCount() == 0);
    assert(queue.takePendingAllNotesOffMask() == 0);
}

void testCriticalOverflowRequestsSynthBPanic() {
    MidiControlEventQueue queue;
    fillQueue(queue, MusicalEventTarget::SynthB);
    assert(!queue.tryPush(liveEvent(MusicalEventType::NoteOff,
                                    MusicalEventTarget::SynthB, 72)));
    assert(queue.droppedCriticalCount() == 1);
    assert(queue.takePendingAllNotesOffMask() ==
           MidiControlEventQueue::kSynthBMask);
    assert(queue.takePendingAllNotesOffMask() == 0);
}

void testCriticalOverflowRequestsDrumsPanic() {
    MidiControlEventQueue queue;
    fillQueue(queue, MusicalEventTarget::Drums);
    assert(!queue.tryPush(liveEvent(MusicalEventType::AllNotesOff,
                                    MusicalEventTarget::Drums, 0)));
    assert(queue.droppedCriticalCount() == 1);
    assert(queue.takePendingAllNotesOffMask() ==
           MidiControlEventQueue::kDrumsMask);
    assert(queue.takePendingAllNotesOffMask() == 0);
}

void testCriticalOverflowRequestsDxPanic() {
    MidiControlEventQueue queue;
    fillQueue(queue, MusicalEventTarget::Dx);
    assert(!queue.tryPush(liveEvent(MusicalEventType::NoteOff,
                                    MusicalEventTarget::Dx, 65)));
    assert(queue.droppedCriticalCount() == 1);
    assert(queue.takePendingAllNotesOffMask() ==
           MidiControlEventQueue::kDxMask);
    assert(queue.takePendingAllNotesOffMask() == 0);
}

void testIndependentPanicMasks() {
    MidiControlEventQueue queue;
    fillQueue(queue, MusicalEventTarget::SynthA);
    assert(!queue.tryPush(liveEvent(MusicalEventType::NoteOff,
                                    MusicalEventTarget::SynthA, 60)));
    assert(!queue.tryPush(liveEvent(MusicalEventType::NoteOff,
                                    MusicalEventTarget::SynthB, 61)));
    assert(!queue.tryPush(liveEvent(MusicalEventType::NoteOff,
                                    MusicalEventTarget::Drums, 62)));
    assert(!queue.tryPush(liveEvent(MusicalEventType::NoteOff,
                                    MusicalEventTarget::Dx, 63)));
    assert(queue.takePendingAllNotesOffMask() ==
           static_cast<uint8_t>(MidiControlEventQueue::kSynthAMask |
                                MidiControlEventQueue::kSynthBMask |
                                MidiControlEventQueue::kDrumsMask |
                                MidiControlEventQueue::kDxMask));
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
    testCriticalOverflowRequestsSynthBPanic();
    testCriticalOverflowRequestsDrumsPanic();
    testCriticalOverflowRequestsDxPanic();
    testIndependentPanicMasks();
    testApproximateSize();
}
