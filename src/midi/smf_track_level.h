#pragma once
#ifndef GROOVEPUTER_SMF_TRACK_LEVEL_H
#define GROOVEPUTER_SMF_TRACK_LEVEL_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "smf_session_generation.h"

namespace GroovePuterMidi {

constexpr std::size_t kSmfTrackLevelCapacity = 64u;
constexpr uint8_t kSmfTrackLevelDefault = 100u;

constexpr uint8_t clampSmfTrackLevel(int value) {
    return value < 0 ? 0u : (value > 100 ? 100u : static_cast<uint8_t>(value));
}

constexpr uint8_t applySmfTrackLevelVelocity(uint8_t velocity,
                                             uint8_t levelPercent) {
    if (velocity == 0u || levelPercent == 0u) return 0u;
    const uint16_t scaled = static_cast<uint16_t>(
        (static_cast<uint16_t>(velocity) * levelPercent + 50u) / 100u);
    if (scaled == 0u) return 1u;
    return scaled > 127u ? 127u : static_cast<uint8_t>(scaled);
}

class SmfTrackLevelState {
public:
    uint8_t levelFor(uint16_t trackIndex) {
        if (trackIndex >= kSmfTrackLevelCapacity) return kSmfTrackLevelDefault;
        const uint32_t generation = smfSessionGeneration();
        if (generation != 0u && !ensureSession(generation)) {
            return kSmfTrackLevelDefault;
        }
        const uint32_t bound = boundGeneration_.load(std::memory_order_acquire);
        if (bound == 0u || bound == kInitializingGeneration) {
            return kSmfTrackLevelDefault;
        }
        const std::size_t wordIndex = trackIndex / 4u;
        const uint32_t shift = static_cast<uint32_t>(trackIndex % 4u) * 8u;
        const uint32_t word = packedLevels_[wordIndex].load(std::memory_order_acquire);
        return static_cast<uint8_t>((word >> shift) & 0xFFu);
    }

    bool setLevel(uint16_t trackIndex,
                  uint8_t levelPercent,
                  uint32_t generation) {
        if (trackIndex >= kSmfTrackLevelCapacity || generation == 0u ||
            smfSessionGeneration() != generation || !ensureSession(generation)) {
            return false;
        }
        SmfSessionMutationGuard guard(generation);
        if (!guard || boundGeneration_.load(std::memory_order_acquire) != generation) {
            return false;
        }
        const uint8_t bounded = clampSmfTrackLevel(levelPercent);
        const std::size_t wordIndex = trackIndex / 4u;
        const uint32_t shift = static_cast<uint32_t>(trackIndex % 4u) * 8u;
        const uint32_t mask = uint32_t{0xFFu} << shift;
        uint32_t current = packedLevels_[wordIndex].load(std::memory_order_acquire);
        while (true) {
            const uint32_t next =
                (current & ~mask) | (static_cast<uint32_t>(bounded) << shift);
            if (next == current) return true;
            if (packedLevels_[wordIndex].compare_exchange_weak(
                    current, next, std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }
        }
    }

    bool adjustLevel(uint16_t trackIndex,
                     int deltaPercent,
                     uint32_t generation,
                     uint8_t& result) {
        if (generation == 0u || smfSessionGeneration() != generation) return false;
        result = clampSmfTrackLevel(
            static_cast<int>(levelFor(trackIndex)) + deltaPercent);
        return setLevel(trackIndex, result, generation);
    }

private:
    static constexpr std::size_t kPackedWords = kSmfTrackLevelCapacity / 4u;
    static constexpr uint32_t kInitializingGeneration = 0xFFFFFFFFu;
    static constexpr uint32_t kDefaultWord =
        static_cast<uint32_t>(kSmfTrackLevelDefault) |
        (static_cast<uint32_t>(kSmfTrackLevelDefault) << 8u) |
        (static_cast<uint32_t>(kSmfTrackLevelDefault) << 16u) |
        (static_cast<uint32_t>(kSmfTrackLevelDefault) << 24u);

    bool ensureSession(uint32_t generation) {
        while (true) {
            uint32_t bound = boundGeneration_.load(std::memory_order_acquire);
            if (bound == generation) return true;
            if (bound == kInitializingGeneration) continue;
            if (!boundGeneration_.compare_exchange_weak(
                    bound, kInitializingGeneration, std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                continue;
            }
            for (auto& word : packedLevels_) {
                word.store(kDefaultWord, std::memory_order_relaxed);
            }
            boundGeneration_.store(generation, std::memory_order_release);
            return true;
        }
    }

    std::atomic<uint32_t> packedLevels_[kPackedWords]{};
    std::atomic<uint32_t> boundGeneration_{0u};
};

static_assert(kSmfTrackLevelCapacity % 4u == 0u,
              "SMF track levels must pack into complete 32-bit words");
static_assert(sizeof(std::atomic<uint32_t>) == sizeof(uint32_t),
              "SMF track level words must remain bounded 32-bit atomics");

inline SmfTrackLevelState& smfTrackLevelState() {
    static SmfTrackLevelState state;
    return state;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_TRACK_LEVEL_H
