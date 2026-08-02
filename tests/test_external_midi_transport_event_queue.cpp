#include <cassert>
#include <cstdint>

#include "src/midi/external_midi_transport_event_queue.h"

int main() {
    ExternalMidiTransportEventQueue queue;
    ExternalMidiTransportEvent event{};

    assert(queue.tryPushClock(1000, 1));
    assert(queue.tryPushCritical(
        ExternalMidiTransportEventType::Start, 1100, 1));
    assert(queue.tryPushCritical(
        ExternalMidiTransportEventType::Continue, 1200, 2));
    assert(queue.tryPushCritical(
        ExternalMidiTransportEventType::Stop, 1300, 3));

    assert(queue.tryPop(event));
    assert(event.type == ExternalMidiTransportEventType::Clock);
    assert(event.timestampMicros == 1000);
    assert(event.pulseOrdinal == 1);
    assert(event.receiveSequence == 1);

    assert(queue.tryPop(event));
    assert(event.type == ExternalMidiTransportEventType::Start);
    assert(event.receiveSequence == 2);
    assert(queue.tryPop(event));
    assert(event.type == ExternalMidiTransportEventType::Continue);
    assert(event.receiveSequence == 3);
    assert(queue.tryPop(event));
    assert(event.type == ExternalMidiTransportEventType::Stop);
    assert(event.receiveSequence == 4);
    assert(!queue.tryPop(event));

    assert(!queue.tryPushCritical(
        ExternalMidiTransportEventType::Clock, 0, 0));
    assert(queue.invalidEventCount() == 1);

    ExternalMidiTransportEventQueue pressure;
    const std::size_t clockLimit =
        ExternalMidiTransportEventQueue::kCapacity -
        ExternalMidiTransportEventQueue::kCriticalReserve;
    for (std::size_t i = 0; i < clockLimit; ++i) {
        assert(pressure.tryPushClock(
            static_cast<uint32_t>(i * 1000u),
            static_cast<uint32_t>(i + 1u)));
    }
    assert(!pressure.tryPushClock(999999, 999));
    assert(pressure.droppedClockCount() == 1);

    const ExternalMidiTransportEventType critical[] = {
        ExternalMidiTransportEventType::Start,
        ExternalMidiTransportEventType::Continue,
        ExternalMidiTransportEventType::Stop,
        ExternalMidiTransportEventType::Start,
        ExternalMidiTransportEventType::Continue,
        ExternalMidiTransportEventType::Stop,
        ExternalMidiTransportEventType::Start,
        ExternalMidiTransportEventType::Stop,
    };
    for (std::size_t i = 0; i <
            ExternalMidiTransportEventQueue::kCriticalReserve; ++i) {
        assert(pressure.tryPushCritical(
            critical[i], 2000000u + static_cast<uint32_t>(i), 999));
    }
    assert(pressure.approximateSize() ==
           ExternalMidiTransportEventQueue::kCapacity);
    assert(!pressure.tryPushCritical(
        ExternalMidiTransportEventType::Stop, 3000000, 1000));
    assert(pressure.failed());
    assert(pressure.criticalOverflowCount() == 1);
    pressure.clearFailure();
    assert(!pressure.failed());
    pressure.discardPending();
    assert(pressure.approximateSize() == 0);

    ExternalMidiTransportEventQueue wrap;
    for (uint32_t round = 0; round < 3; ++round) {
        for (uint32_t i = 0; i < 64; ++i) {
            assert(wrap.tryPushClock(round * 100000u + i, round * 64u + i));
        }
        for (uint32_t i = 0; i < 64; ++i) {
            assert(wrap.tryPop(event));
            assert(event.type == ExternalMidiTransportEventType::Clock);
        }
        assert(!wrap.tryPop(event));
    }

    return 0;
}
