#pragma once
#ifndef GROOVEPUTER_MUSICAL_EVENT_QUEUE_H
#define GROOVEPUTER_MUSICAL_EVENT_QUEUE_H

#include <cstddef>
#include <cstdint>

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <atomic>
#endif

#include "musical_event.h"

// ESP32-S3's pinned GCC does not advertise std::atomic as always lock-free.
// Use native aligned 32-bit SPSC words there so the audio producer never enters
// a libatomic lock. Host builds retain standard atomics for race-safe tests.
class RealtimeQueueWord {
public:
    uint32_t loadRelaxed() const {
#if defined(ESP32) || defined(ESP_PLATFORM)
        return value_;
#else
        return value_.load(std::memory_order_relaxed);
#endif
    }

    uint32_t loadAcquire() const {
#if defined(ESP32) || defined(ESP_PLATFORM)
        const uint32_t value = value_;
        asm volatile("memw" ::: "memory");
        return value;
#else
        return value_.load(std::memory_order_acquire);
#endif
    }

    void storeRelease(uint32_t value) {
#if defined(ESP32) || defined(ESP_PLATFORM)
        asm volatile("memw" ::: "memory");
        value_ = value;
#else
        value_.store(value, std::memory_order_release);
#endif
    }

    // Only the producer writes counters and panic epochs.
    void incrementRelaxed() {
#if defined(ESP32) || defined(ESP_PLATFORM)
        value_ = value_ + 1u;
#else
        value_.fetch_add(1u, std::memory_order_relaxed);
#endif
    }

private:
#if defined(ESP32) || defined(ESP_PLATFORM)
    alignas(4) volatile uint32_t value_{0};
#else
    std::atomic<uint32_t> value_{0};
#endif
};

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

    bool tryPush(const MusicalEvent& event) {
        // On Cardputer, realtime PatternPlayer publication is owned by the
        // dedicated AudioTask. Offline WAV rendering runs synchronously on the
        // Arduino loop task and must never enqueue a burst of untimed MIDI.
        // Control-plane cleanup events degrade to a target-scoped panic.
        if (!isRealtimeProducerContext()) {
            suppressedNonRealtime_.incrementRelaxed();
            if (event.type == MusicalEventType::AllNotesOff) {
                // Stop/scene/render lifecycle is serialized against AudioTask.
                // Discard stale queued notes before the final panic is routed.
                const uint32_t head = head_.loadAcquire();
                tail_.storeRelease(head);
            }
            if (event.type != MusicalEventType::NoteOn) {
                markPendingAllNotesOff(event.target);
            }
            return false;
        }

        const uint32_t head = head_.loadRelaxed();
        const uint32_t next = (head + 1u) % kStorageSize;
        if (next == tail_.loadAcquire()) {
            dropped_.incrementRelaxed();
            if (event.type != MusicalEventType::NoteOn) {
                markPendingAllNotesOff(event.target);
            }
            return false;
        }

        events_[head] = event;
        head_.storeRelease(next);
        return true;
    }

    bool tryPop(MusicalEvent& event) {
        const uint32_t tail = tail_.loadRelaxed();
        if (tail == head_.loadAcquire()) return false;

        event = events_[tail];
        tail_.storeRelease((tail + 1u) % kStorageSize);
        return true;
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

    // Control-plane lifecycle helper. Call only while the audio producer is
    // quiescent (for example, after transport stop under AudioMutationGate).
    void discardPending() {
        const uint32_t head = head_.loadAcquire();
        tail_.storeRelease(head);
        consumedSynthAEpoch_ = pendingSynthAEpoch_.loadAcquire();
        consumedSynthBEpoch_ = pendingSynthBEpoch_.loadAcquire();
    }

    uint32_t droppedCount() const {
        return dropped_.loadRelaxed();
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

    static constexpr uint8_t targetMask(MusicalEventTarget target) {
        return target == MusicalEventTarget::SynthB ? kSynthBMask : kSynthAMask;
    }

    void markPendingAllNotesOff(MusicalEventTarget target) {
        if (targetMask(target) == kSynthBMask) {
            pendingSynthBEpoch_.incrementRelaxed();
        } else {
            pendingSynthAEpoch_.incrementRelaxed();
        }
    }

    MusicalEvent events_[kStorageSize]{};
    RealtimeQueueWord head_;
    RealtimeQueueWord tail_;
    RealtimeQueueWord pendingSynthAEpoch_;
    RealtimeQueueWord pendingSynthBEpoch_;
    RealtimeQueueWord dropped_;
    RealtimeQueueWord suppressedNonRealtime_;
    uint32_t consumedSynthAEpoch_{0};
    uint32_t consumedSynthBEpoch_{0};
};

#endif  // GROOVEPUTER_MUSICAL_EVENT_QUEUE_H
