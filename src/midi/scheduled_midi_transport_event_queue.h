#pragma once
#ifndef GROOVEPUTER_SCHEDULED_MIDI_TRANSPORT_EVENT_QUEUE_H
#define GROOVEPUTER_SCHEDULED_MIDI_TRANSPORT_EVENT_QUEUE_H

#include <cstddef>
#include <cstdint>

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#include "midi_realtime_word.h"
#include "scheduled_midi_transport_event.h"

// Single-producer/single-consumer handoff from AudioTask transport timing to
// MidiDispatchTask. Clock traffic may be dropped under pressure; lifecycle
// traffic has reserved queue capacity and a final fixed-size recovery mailbox.
class ScheduledMidiTransportEventQueue {
public:
    static constexpr std::size_t kStorageSize = 64;
    static constexpr std::size_t kCapacity = kStorageSize - 1;
    static constexpr std::size_t kCriticalReserve = 4;

    bool tryPushClock(uint32_t blockSequence, uint16_t frameOffset) {
        if (!isRealtimeProducerContext()) {
            suppressedNonRealtime_.incrementRelaxed();
            return false;
        }

        const uint32_t head = head_.loadRelaxed();
        const uint32_t tail = tail_.loadAcquire();
        if (sizeFrom(head, tail) >= (kCapacity - kCriticalReserve)) {
            droppedClock_.incrementRelaxed();
            return false;
        }

        ScheduledMidiTransportEvent event{};
        event.type = MidiTransportEventType::Clock;
        event.blockSequence = blockSequence;
        event.frameOffset = frameOffset;
        event.generation = generation_.loadAcquire();
        event.publicationSequence = publicationSequence_.incrementRelaxed();
        return tryPushEvent(event, false);
    }

    bool tryPushLifecycle(MidiTransportEventType type,
                          uint32_t blockSequence,
                          uint16_t frameOffset) {
        if (type == MidiTransportEventType::Clock) return false;
        if (!isRealtimeProducerContext()) {
            suppressedNonRealtime_.incrementRelaxed();
            return false;
        }

        ScheduledMidiTransportEvent event{};
        event.type = type;
        event.blockSequence = blockSequence;
        event.frameOffset = frameOffset;
        event.generation = generation_.incrementRelaxed();
        event.publicationSequence = publicationSequence_.incrementRelaxed();

        if (tryPushEvent(event, true)) return true;

        // Clock producers cannot consume the reserved slots, so this path is
        // only reachable after an extreme burst of lifecycle transitions. Keep
        // the newest critical event in a lock-free mailbox instead of silently
        // losing Start/Stop.
        criticalOverflow_.incrementRelaxed();
        publishCriticalRecovery(event);
        return true;
    }

    bool tryPop(ScheduledMidiTransportEvent& event) {
        const uint32_t tail = tail_.loadRelaxed();
        if (tail == head_.loadAcquire()) return false;

        event = events_[tail];
        tail_.storeRelease((tail + 1u) % kStorageSize);
        return true;
    }

    bool takePendingCriticalRecovery(ScheduledMidiTransportEvent& event) {
        const uint32_t epoch = recoveryEpoch_.loadAcquire();
        if (epoch == consumedRecoveryEpoch_) return false;

        event.type = static_cast<MidiTransportEventType>(
            recoveryType_.loadRelaxed());
        event.blockSequence = recoveryBlockSequence_.loadRelaxed();
        event.frameOffset = static_cast<uint16_t>(
            recoveryFrameOffset_.loadRelaxed());
        event.generation = recoveryGeneration_.loadRelaxed();
        event.publicationSequence = recoveryPublicationSequence_.loadRelaxed();
        consumedRecoveryEpoch_ = epoch;
        criticalRecoveries_.incrementRelaxed();
        return true;
    }

    uint32_t generation() const {
        return generation_.loadAcquire();
    }

    std::size_t approximateSize() const {
        return sizeFrom(head_.loadAcquire(), tail_.loadAcquire());
    }

    uint32_t droppedClockCount() const {
        return droppedClock_.loadRelaxed();
    }

    uint32_t criticalOverflowCount() const {
        return criticalOverflow_.loadRelaxed();
    }

    uint32_t criticalRecoveryCount() const {
        return criticalRecoveries_.loadRelaxed();
    }

    uint32_t suppressedNonRealtimeCount() const {
        return suppressedNonRealtime_.loadRelaxed();
    }

private:
    static bool isRealtimeProducerContext() {
#if defined(ESP32) || defined(ESP_PLATFORM)
        const char* taskName = pcTaskGetName(nullptr);
        return taskName != nullptr && std::strcmp(taskName, "AudioTask") == 0;
#else
        return true;
#endif
    }

    static std::size_t sizeFrom(uint32_t head, uint32_t tail) {
        return head >= tail
            ? static_cast<std::size_t>(head - tail)
            : static_cast<std::size_t>(kStorageSize - tail + head);
    }

    bool tryPushEvent(const ScheduledMidiTransportEvent& event,
                      bool critical) {
        const uint32_t head = head_.loadRelaxed();
        const uint32_t next = (head + 1u) % kStorageSize;
        if (next == tail_.loadAcquire()) {
            if (!critical) droppedClock_.incrementRelaxed();
            return false;
        }

        events_[head] = event;
        head_.storeRelease(next);
        return true;
    }

    void publishCriticalRecovery(const ScheduledMidiTransportEvent& event) {
        recoveryType_.storeRelaxed(static_cast<uint32_t>(event.type));
        recoveryBlockSequence_.storeRelaxed(event.blockSequence);
        recoveryFrameOffset_.storeRelaxed(event.frameOffset);
        recoveryGeneration_.storeRelaxed(event.generation);
        recoveryPublicationSequence_.storeRelaxed(event.publicationSequence);
        const uint32_t nextEpoch = recoveryEpoch_.loadRelaxed() + 1u;
        recoveryEpoch_.storeRelease(nextEpoch);
    }

    ScheduledMidiTransportEvent events_[kStorageSize]{};
    MidiRealtimeWord head_;
    MidiRealtimeWord tail_;
    MidiRealtimeWord generation_;
    MidiRealtimeWord publicationSequence_;
    MidiRealtimeWord droppedClock_;
    MidiRealtimeWord criticalOverflow_;
    MidiRealtimeWord criticalRecoveries_;
    MidiRealtimeWord suppressedNonRealtime_;

    MidiRealtimeWord recoveryType_;
    MidiRealtimeWord recoveryBlockSequence_;
    MidiRealtimeWord recoveryFrameOffset_;
    MidiRealtimeWord recoveryGeneration_;
    MidiRealtimeWord recoveryPublicationSequence_;
    MidiRealtimeWord recoveryEpoch_;
    uint32_t consumedRecoveryEpoch_{0};
};

#endif  // GROOVEPUTER_SCHEDULED_MIDI_TRANSPORT_EVENT_QUEUE_H
