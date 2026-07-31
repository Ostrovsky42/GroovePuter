#pragma once
#ifndef GROOVEPUTER_SCHEDULED_MUSICAL_EVENT_QUEUE_H
#define GROOVEPUTER_SCHEDULED_MUSICAL_EVENT_QUEUE_H

#include <cstddef>
#include <cstdint>

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#include "midi_realtime_word.h"
#include "scheduled_musical_event.h"

// Single-producer/single-consumer handoff from AudioTask PatternPlayer events to
// MidiDispatchTask. Publication never allocates or blocks. Lifecycle changes
// invalidate target generations and request a target-scoped wire cleanup.
class ScheduledMusicalEventQueue {
public:
    static constexpr std::size_t kStorageSize = 128;
    static constexpr std::size_t kCapacity = kStorageSize - 1;
    static constexpr uint8_t kSynthAMask = 1u << 0;
    static constexpr uint8_t kSynthBMask = 1u << 1;

    bool tryPush(const MusicalEvent& event,
                 uint32_t blockSequence,
                 uint16_t frameOffset) {
        if (!isRealtimeProducerContext()) {
            suppressedNonRealtime_.incrementRelaxed();
            if (event.type != MusicalEventType::NoteOn) {
                invalidateTarget(event.target);
            }
            return false;
        }

        const uint32_t head = head_.loadRelaxed();
        const uint32_t next = (head + 1u) % kStorageSize;
        if (next == tail_.loadAcquire()) {
            if (event.type == MusicalEventType::NoteOn) {
                droppedNoteOn_.incrementRelaxed();
            } else {
                droppedCritical_.incrementRelaxed();
                invalidateTarget(event.target);
            }
            return false;
        }

        ScheduledMusicalEvent scheduled{};
        scheduled.event = event;
        scheduled.blockSequence = blockSequence;
        scheduled.frameOffset = frameOffset;
        scheduled.generation = generationFor(event.target);
        scheduled.publicationSequence = publicationSequence_.incrementRelaxed();
        events_[head] = scheduled;
        head_.storeRelease(next);
        return true;
    }

    bool tryPop(ScheduledMusicalEvent& scheduled) {
        const uint32_t tail = tail_.loadRelaxed();
        if (tail == head_.loadAcquire()) return false;

        scheduled = events_[tail];
        tail_.storeRelease((tail + 1u) % kStorageSize);
        return true;
    }

    void invalidateTarget(MusicalEventTarget target) {
        if (target == MusicalEventTarget::SynthB) {
            synthBGeneration_.incrementRelaxed();
            pendingSynthBEpoch_.incrementRelaxed();
        } else {
            synthAGeneration_.incrementRelaxed();
            pendingSynthAEpoch_.incrementRelaxed();
        }
    }

    uint32_t generationFor(MusicalEventTarget target) const {
        return target == MusicalEventTarget::SynthB
            ? synthBGeneration_.loadAcquire()
            : synthAGeneration_.loadAcquire();
    }

    uint8_t takePendingAllNotesOffMask() {
        uint8_t mask = 0;
        const uint32_t synthAEpoch = pendingSynthAEpoch_.loadAcquire();
        const uint32_t synthBEpoch = pendingSynthBEpoch_.loadAcquire();
        if (synthAEpoch != consumedSynthAEpoch_) {
            consumedSynthAEpoch_ = synthAEpoch;
            mask |= kSynthAMask;
        }
        if (synthBEpoch != consumedSynthBEpoch_) {
            consumedSynthBEpoch_ = synthBEpoch;
            mask |= kSynthBMask;
        }
        return mask;
    }

    // Call only when AudioTask publication is quiescent under AudioMutationGate.
    void discardPending() {
        const uint32_t head = head_.loadAcquire();
        tail_.storeRelease(head);
        consumedSynthAEpoch_ = pendingSynthAEpoch_.loadAcquire();
        consumedSynthBEpoch_ = pendingSynthBEpoch_.loadAcquire();
    }

    std::size_t approximateSize() const {
        const uint32_t head = head_.loadAcquire();
        const uint32_t tail = tail_.loadAcquire();
        return head >= tail
            ? static_cast<std::size_t>(head - tail)
            : static_cast<std::size_t>(kStorageSize - tail + head);
    }

    uint32_t droppedNoteOnCount() const {
        return droppedNoteOn_.loadRelaxed();
    }

    uint32_t droppedCriticalCount() const {
        return droppedCritical_.loadRelaxed();
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

    ScheduledMusicalEvent events_[kStorageSize]{};
    MidiRealtimeWord head_;
    MidiRealtimeWord tail_;
    MidiRealtimeWord synthAGeneration_;
    MidiRealtimeWord synthBGeneration_;
    MidiRealtimeWord pendingSynthAEpoch_;
    MidiRealtimeWord pendingSynthBEpoch_;
    MidiRealtimeWord publicationSequence_;
    MidiRealtimeWord droppedNoteOn_;
    MidiRealtimeWord droppedCritical_;
    MidiRealtimeWord suppressedNonRealtime_;
    uint32_t consumedSynthAEpoch_{0};
    uint32_t consumedSynthBEpoch_{0};
};

#endif  // GROOVEPUTER_SCHEDULED_MUSICAL_EVENT_QUEUE_H
