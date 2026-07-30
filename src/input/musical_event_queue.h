#pragma once
#ifndef GROOVEPUTER_MUSICAL_EVENT_QUEUE_H
#define GROOVEPUTER_MUSICAL_EVENT_QUEUE_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "musical_event.h"

// Fixed-capacity event handoff from the audio-side PatternPlayer to the control
// loop. The realtime producer never allocates or blocks. Control-plane engine
// mutations are already serialized against the audio task by AudioMutationGate,
// so producer calls never run concurrently with the audio producer.
class MusicalEventQueue {
public:
    static constexpr std::size_t kCapacity = 64;
    static constexpr uint8_t kSynthAMask = 1u << 0;
    static constexpr uint8_t kSynthBMask = 1u << 1;

    bool tryPush(const MusicalEvent& event) {
        const uint16_t head = head_.load(std::memory_order_relaxed);
        const uint16_t next = static_cast<uint16_t>((head + 1u) % kCapacity);
        if (next == tail_.load(std::memory_order_acquire)) {
            dropped_.fetch_add(1u, std::memory_order_relaxed);
            if (event.type != MusicalEventType::NoteOn) {
                pendingAllNotesOffMask_.fetch_or(targetMask(event.target),
                                                 std::memory_order_release);
            }
            return false;
        }

        events_[head] = event;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool tryPop(MusicalEvent& event) {
        const uint16_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;

        event = events_[tail];
        tail_.store(static_cast<uint16_t>((tail + 1u) % kCapacity),
                    std::memory_order_release);
        return true;
    }

    uint8_t takePendingAllNotesOffMask() {
        return pendingAllNotesOffMask_.exchange(0u, std::memory_order_acq_rel);
    }

    uint32_t droppedCount() const {
        return dropped_.load(std::memory_order_relaxed);
    }

private:
    static constexpr uint8_t targetMask(MusicalEventTarget target) {
        return target == MusicalEventTarget::SynthB ? kSynthBMask : kSynthAMask;
    }

    MusicalEvent events_[kCapacity]{};
    std::atomic<uint16_t> head_{0};
    std::atomic<uint16_t> tail_{0};
    std::atomic<uint8_t> pendingAllNotesOffMask_{0};
    std::atomic<uint32_t> dropped_{0};
};

#endif  // GROOVEPUTER_MUSICAL_EVENT_QUEUE_H
