#pragma once
#ifndef GROOVEPUTER_MUSICAL_EVENT_QUEUE_H
#define GROOVEPUTER_MUSICAL_EVENT_QUEUE_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#include "musical_event.h"

// Fixed-capacity event handoff from the audio-side PatternPlayer to the control
// loop. The realtime producer never allocates or blocks. Control-plane engine
// mutations are already serialized against the audio task by AudioMutationGate.
class MusicalEventQueue {
public:
    // One ring slot remains empty as the head/tail sentinel. The storage size is
    // 64 entries and the actual usable event capacity is therefore 63.
    static constexpr std::size_t kStorageSize = 64;
    static constexpr std::size_t kCapacity = kStorageSize - 1;
    static constexpr uint8_t kSynthAMask = 1u << 0;
    static constexpr uint8_t kSynthBMask = 1u << 1;

#if defined(__GNUC__)
    static_assert(__atomic_always_lock_free(sizeof(uint8_t), nullptr),
                  "8-bit atomics must remain lock-free on the pinned toolchain");
    static_assert(__atomic_always_lock_free(sizeof(uint16_t), nullptr),
                  "16-bit atomics must remain lock-free on the pinned toolchain");
    static_assert(__atomic_always_lock_free(sizeof(uint32_t), nullptr),
                  "32-bit atomics must remain lock-free on the pinned toolchain");
#endif

    bool tryPush(const MusicalEvent& event) {
        // On Cardputer, realtime PatternPlayer publication is owned by the
        // dedicated AudioTask. Offline WAV rendering runs synchronously on the
        // Arduino loop task and must never enqueue a burst of untimed MIDI.
        // Control-plane cleanup events degrade to a target-scoped panic.
        if (!isRealtimeProducerContext()) {
            suppressedNonRealtime_.fetch_add(1u, std::memory_order_relaxed);
            if (event.type == MusicalEventType::AllNotesOff) {
                // Stop/scene/render lifecycle is serialized against AudioTask.
                // Discard stale queued notes before the final panic is routed.
                const uint16_t head = head_.load(std::memory_order_acquire);
                tail_.store(head, std::memory_order_release);
            }
            if (event.type != MusicalEventType::NoteOn) {
                pendingAllNotesOffMask_.fetch_or(targetMask(event.target),
                                                 std::memory_order_release);
            }
            return false;
        }

        const uint16_t head = head_.load(std::memory_order_relaxed);
        const uint16_t next = static_cast<uint16_t>((head + 1u) % kStorageSize);
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
        tail_.store(static_cast<uint16_t>((tail + 1u) % kStorageSize),
                    std::memory_order_release);
        return true;
    }

    uint8_t takePendingAllNotesOffMask() {
        return pendingAllNotesOffMask_.exchange(0u, std::memory_order_acq_rel);
    }

    // Control-plane lifecycle helper. Call only while the audio producer is
    // quiescent (for example, after transport stop under AudioMutationGate).
    void discardPending() {
        const uint16_t head = head_.load(std::memory_order_acquire);
        tail_.store(head, std::memory_order_release);
        pendingAllNotesOffMask_.store(0u, std::memory_order_release);
    }

    uint32_t droppedCount() const {
        return dropped_.load(std::memory_order_relaxed);
    }

    uint32_t suppressedNonRealtimeCount() const {
        return suppressedNonRealtime_.load(std::memory_order_relaxed);
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

    static constexpr uint8_t targetMask(MusicalEventTarget target) {
        return target == MusicalEventTarget::SynthB ? kSynthBMask : kSynthAMask;
    }

    MusicalEvent events_[kStorageSize]{};
    std::atomic<uint16_t> head_{0};
    std::atomic<uint16_t> tail_{0};
    std::atomic<uint8_t> pendingAllNotesOffMask_{0};
    std::atomic<uint32_t> dropped_{0};
    std::atomic<uint32_t> suppressedNonRealtime_{0};
};

#endif  // GROOVEPUTER_MUSICAL_EVENT_QUEUE_H
