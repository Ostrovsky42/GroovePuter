#pragma once

#include <cstdint>

#if !defined(ESP32) && !defined(ESP_PLATFORM)
#include <atomic>
#endif

// Aligned 32-bit cross-core word for the bounded MIDI queues. The ESP32-S3
// implementation avoids libatomic locks in the AudioTask path; host builds use
// std::atomic so queue tests can exercise the same acquire/release contract.
class MidiRealtimeWord {
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

    void storeRelaxed(uint32_t value) {
#if defined(ESP32) || defined(ESP_PLATFORM)
        value_ = value;
#else
        value_.store(value, std::memory_order_relaxed);
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

    uint32_t incrementRelaxed() {
#if defined(ESP32) || defined(ESP_PLATFORM)
        const uint32_t value = value_ + 1u;
        value_ = value;
        return value;
#else
        return value_.fetch_add(1u, std::memory_order_relaxed) + 1u;
#endif
    }

private:
#if defined(ESP32) || defined(ESP_PLATFORM)
    alignas(4) volatile uint32_t value_{0};
#else
    std::atomic<uint32_t> value_{0};
#endif
};
