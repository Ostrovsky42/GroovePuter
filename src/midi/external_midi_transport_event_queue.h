#pragma once

#include <cstddef>
#include <cstdint>

#include "external_midi_transport_event.h"
#include "midi_realtime_word.h"

// Single-producer/single-consumer queue for MidiDispatchTask -> AudioTask.
// Clock traffic may be shed before the queue is full so Start/Continue/Stop
// retain reserved capacity. A critical overflow is surfaced explicitly and
// must force the external follower into a safe Lost/Stopped state.
class ExternalMidiTransportEventQueue {
public:
    static constexpr std::size_t kStorageSize = 128;
    static constexpr std::size_t kCapacity = kStorageSize - 1;
    static constexpr std::size_t kCriticalReserve = 8;

    bool tryPushClock(uint32_t timestampMicros, uint32_t pulseOrdinal) {
        const uint32_t head = head_.loadRelaxed();
        const uint32_t tail = tail_.loadAcquire();
        if (sizeFrom(head, tail) >= (kCapacity - kCriticalReserve)) {
            droppedClock_.incrementRelaxed();
            return false;
        }
        return tryPush(makeEvent(ExternalMidiTransportEventType::Clock,
                                 timestampMicros,
                                 pulseOrdinal));
    }

    bool tryPushCritical(ExternalMidiTransportEventType type,
                         uint32_t timestampMicros,
                         uint32_t pulseOrdinal) {
        if (!externalMidiTransportEventIsCritical(type)) {
            invalidEvent_.incrementRelaxed();
            return false;
        }
        if (tryPush(makeEvent(type, timestampMicros, pulseOrdinal))) {
            return true;
        }
        criticalOverflow_.incrementRelaxed();
        failed_.storeRelease(1u);
        return false;
    }

    bool tryPop(ExternalMidiTransportEvent& event) {
        const uint32_t tail = tail_.loadRelaxed();
        if (tail == head_.loadAcquire()) return false;
        event = events_[tail];
        tail_.storeRelease((tail + 1u) % kStorageSize);
        return true;
    }

    std::size_t approximateSize() const {
        return sizeFrom(head_.loadAcquire(), tail_.loadAcquire());
    }

    bool failed() const { return failed_.loadAcquire() != 0u; }
    void clearFailure() { failed_.storeRelease(0u); }

    uint32_t droppedClockCount() const {
        return droppedClock_.loadRelaxed();
    }

    uint32_t criticalOverflowCount() const {
        return criticalOverflow_.loadRelaxed();
    }

    uint32_t invalidEventCount() const {
        return invalidEvent_.loadRelaxed();
    }

private:
    static std::size_t sizeFrom(uint32_t head, uint32_t tail) {
        return head >= tail
            ? static_cast<std::size_t>(head - tail)
            : static_cast<std::size_t>(kStorageSize - tail + head);
    }

    ExternalMidiTransportEvent makeEvent(
            ExternalMidiTransportEventType type,
            uint32_t timestampMicros,
            uint32_t pulseOrdinal) {
        ExternalMidiTransportEvent event{};
        event.type = type;
        event.timestampMicros = timestampMicros;
        event.pulseOrdinal = pulseOrdinal;
        event.receiveSequence = receiveSequence_.incrementRelaxed();
        return event;
    }

    bool tryPush(const ExternalMidiTransportEvent& event) {
        const uint32_t head = head_.loadRelaxed();
        const uint32_t next = (head + 1u) % kStorageSize;
        if (next == tail_.loadAcquire()) return false;
        events_[head] = event;
        head_.storeRelease(next);
        return true;
    }

    ExternalMidiTransportEvent events_[kStorageSize]{};
    MidiRealtimeWord head_;
    MidiRealtimeWord tail_;
    MidiRealtimeWord receiveSequence_;
    MidiRealtimeWord droppedClock_;
    MidiRealtimeWord criticalOverflow_;
    MidiRealtimeWord invalidEvent_;
    MidiRealtimeWord failed_;
};
