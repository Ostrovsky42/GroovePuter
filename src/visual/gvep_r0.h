#pragma once
#ifndef GROOVEPUTER_GVEP_R0_H
#define GROOVEPUTER_GVEP_R0_H

#include <cstddef>
#include <cstdint>

#ifndef GROOVEPUTER_GVEP_R0
#define GROOVEPUTER_GVEP_R0 0
#endif

#if GROOVEPUTER_GVEP_R0
#include <atomic>
#endif

namespace GroovePuterVisual {

constexpr std::size_t kGvepV1PacketSize = 24;
constexpr uint8_t kGvepV1ProtocolVersion = 1;
constexpr uint8_t kGvepMessageEvent = 1;
constexpr uint32_t kGvepTicksPerBar = 384;
constexpr uint8_t kGvepStepsPerBar = 16;

enum class GvepEventType : uint8_t {
    Kick = 0x01,
    Play = 0x20,
    Stop = 0x21,
};

struct GvepEvent {
    GvepEventType type{GvepEventType::Kick};
    uint8_t value{0};
    uint32_t sequence{0};
    uint32_t musicalTick{0};
    uint16_t bar{0};
    uint8_t step{255};
};

inline void gvepWriteLe16(uint8_t* out, uint16_t value) {
    out[0] = static_cast<uint8_t>(value & 0xFFu);
    out[1] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
}

inline void gvepWriteLe32(uint8_t* out, uint32_t value) {
    out[0] = static_cast<uint8_t>(value & 0xFFu);
    out[1] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
    out[2] = static_cast<uint8_t>((value >> 16u) & 0xFFu);
    out[3] = static_cast<uint8_t>((value >> 24u) & 0xFFu);
}

inline void serializeGvepV1Event(const GvepEvent& event,
                                 uint32_t monotonicTimestampUs,
                                 uint8_t out[kGvepV1PacketSize]) {
    out[0] = 'G';
    out[1] = 'V';
    out[2] = 'E';
    out[3] = '1';
    out[4] = kGvepV1ProtocolVersion;
    out[5] = kGvepMessageEvent;
    out[6] = static_cast<uint8_t>(event.type);
    out[7] = 0;  // R0 flags are reserved and must be zero.
    gvepWriteLe32(&out[8], event.sequence);
    gvepWriteLe32(&out[12], event.musicalTick);
    gvepWriteLe32(&out[16], monotonicTimestampUs);
    gvepWriteLe16(&out[20], event.bar);
    out[22] = event.step;
    out[23] = event.value;
}

#if GROOVEPUTER_GVEP_R0

class GvepR0EventBus {
public:
    static constexpr uint32_t kCapacity = 16;

    bool tryPublish(GvepEventType type,
                    uint8_t value,
                    uint32_t musicalTick,
                    uint16_t bar,
                    uint8_t step) {
        const uint32_t write = writeIndex_.load(std::memory_order_relaxed);
        const uint32_t read = readIndex_.load(std::memory_order_acquire);
        const uint32_t depth = write - read;
        if (depth >= kCapacity) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        GvepEvent& slot = queue_[write & (kCapacity - 1u)];
        slot.type = type;
        slot.value = value;
        slot.sequence = nextSequence_++;
        slot.musicalTick = musicalTick;
        slot.bar = bar;
        slot.step = step;

        writeIndex_.store(write + 1u, std::memory_order_release);
        published_.fetch_add(1, std::memory_order_relaxed);
        updateHighWater(depth + 1u);
        return true;
    }

    bool tryPop(GvepEvent& out) {
        const uint32_t read = readIndex_.load(std::memory_order_relaxed);
        const uint32_t write = writeIndex_.load(std::memory_order_acquire);
        if (read == write) return false;

        out = queue_[read & (kCapacity - 1u)];
        readIndex_.store(read + 1u, std::memory_order_release);
        popped_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    uint32_t publishedCount() const {
        return published_.load(std::memory_order_relaxed);
    }

    uint32_t poppedCount() const {
        return popped_.load(std::memory_order_relaxed);
    }

    uint32_t droppedCount() const {
        return dropped_.load(std::memory_order_relaxed);
    }

    uint32_t highWaterMark() const {
        return highWater_.load(std::memory_order_relaxed);
    }

private:
    void updateHighWater(uint32_t depth) {
        uint32_t current = highWater_.load(std::memory_order_relaxed);
        while (depth > current &&
               !highWater_.compare_exchange_weak(
                   current,
                   depth,
                   std::memory_order_relaxed,
                   std::memory_order_relaxed)) {
        }
    }

    static_assert((kCapacity & (kCapacity - 1u)) == 0u,
                  "GVEP R0 queue capacity must be a power of two");

    GvepEvent queue_[kCapacity]{};
    std::atomic<uint32_t> writeIndex_{0};
    std::atomic<uint32_t> readIndex_{0};
    std::atomic<uint32_t> published_{0};
    std::atomic<uint32_t> popped_{0};
    std::atomic<uint32_t> dropped_{0};
    std::atomic<uint32_t> highWater_{0};
    uint32_t nextSequence_{0};
};

GvepR0EventBus& gvepR0EventBus();

inline void publishGvepR0Event(GvepEventType type,
                               uint8_t value,
                               uint32_t musicalTick,
                               uint16_t bar,
                               uint8_t step) {
    gvepR0EventBus().tryPublish(type, value, musicalTick, bar, step);
}

#else

inline void publishGvepR0Event(GvepEventType,
                               uint8_t,
                               uint32_t,
                               uint16_t,
                               uint8_t) {
}

#endif

}  // namespace GroovePuterVisual

#endif  // GROOVEPUTER_GVEP_R0_H
