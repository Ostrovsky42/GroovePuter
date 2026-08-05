#pragma once
#ifndef GROOVEPUTER_SMF_TRACK_OUTPUT_ROUTE_H
#define GROOVEPUTER_SMF_TRACK_OUTPUT_ROUTE_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "smf_session_generation.h"

namespace GroovePuterMidi {

constexpr std::size_t kSmfTrackOutputRouteCapacity = 32u;
constexpr uint8_t kSmfSeqtrakOutputChannelCount = 10u;
constexpr int8_t kSmfTrackOutputRouteAuto = -1;

struct SmfTrackOutputRouteSnapshot {
    uint32_t generation{0};
    uint32_t revision{0};
    uint16_t trackCount{0};
    int8_t destinationChannels[kSmfTrackOutputRouteCapacity]{};

    int8_t destinationFor(uint16_t trackIndex) const {
        if (trackIndex >= trackCount ||
            trackIndex >= kSmfTrackOutputRouteCapacity) {
            return kSmfTrackOutputRouteAuto;
        }
        return destinationChannels[trackIndex];
    }

    bool overridden(uint16_t trackIndex) const {
        return destinationFor(trackIndex) != kSmfTrackOutputRouteAuto;
    }
};

class SmfTrackOutputRouteState {
public:
    SmfTrackOutputRouteSnapshot snapshot(uint16_t trackCountHint) {
        while (true) {
            const uint32_t before = smfSessionGeneration();
            if (before == 0u || !ensureSession(before, trackCountHint)) {
                return emptySnapshot();
            }

            SmfTrackOutputRouteSnapshot result = emptySnapshot();
            result.trackCount = trackCount_.load(std::memory_order_acquire);
            result.revision = revision_.load(std::memory_order_acquire);
            for (std::size_t wordIndex = 0u;
                 wordIndex < kPackedRouteWords;
                 ++wordIndex) {
                const uint32_t word =
                    packedRoutes_[wordIndex].load(std::memory_order_acquire);
                for (uint8_t byteIndex = 0u; byteIndex < 4u; ++byteIndex) {
                    const std::size_t trackIndex = wordIndex * 4u + byteIndex;
                    if (trackIndex >= result.trackCount) break;
                    const uint8_t encoded = static_cast<uint8_t>(
                        (word >> (byteIndex * 8u)) & 0xFFu);
                    result.destinationChannels[trackIndex] = decode(encoded);
                }
            }

            const uint32_t after = smfSessionGeneration();
            if (before == after &&
                boundGeneration_.load(std::memory_order_acquire) == after) {
                result.generation = after;
                return result;
            }
        }
    }

    bool setDestination(uint16_t trackIndex,
                        int8_t destinationChannel,
                        uint32_t generation,
                        uint16_t trackCountHint) {
        if (generation == 0u || smfSessionGeneration() != generation ||
            !ensureSession(generation, trackCountHint)) {
            return false;
        }

        SmfSessionMutationGuard guard(generation);
        if (!guard ||
            boundGeneration_.load(std::memory_order_acquire) != generation) {
            return false;
        }

        const uint16_t count = trackCount_.load(std::memory_order_acquire);
        if (trackIndex >= count ||
            trackIndex >= kSmfTrackOutputRouteCapacity ||
            destinationChannel < kSmfTrackOutputRouteAuto ||
            destinationChannel >=
                static_cast<int8_t>(kSmfSeqtrakOutputChannelCount)) {
            return false;
        }

        const uint8_t encoded = encode(destinationChannel);
        const std::size_t wordIndex = trackIndex / 4u;
        const uint8_t byteIndex = static_cast<uint8_t>(trackIndex % 4u);
        const uint32_t shift = static_cast<uint32_t>(byteIndex) * 8u;
        const uint32_t mask = uint32_t{0xFFu} << shift;

        uint32_t current =
            packedRoutes_[wordIndex].load(std::memory_order_acquire);
        while (true) {
            const uint8_t previous = static_cast<uint8_t>(
                (current >> shift) & 0xFFu);
            if (previous == encoded) return true;
            const uint32_t next =
                (current & ~mask) | (static_cast<uint32_t>(encoded) << shift);
            if (packedRoutes_[wordIndex].compare_exchange_weak(
                    current,
                    next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                revision_.fetch_add(1u, std::memory_order_acq_rel);
                return true;
            }
        }
    }

    int8_t destinationFor(uint16_t trackIndex, uint16_t trackCountHint) {
        const uint32_t generation = smfSessionGeneration();
        if (generation == 0u ||
            !ensureSession(generation, trackCountHint)) {
            return kSmfTrackOutputRouteAuto;
        }
        const uint16_t count = trackCount_.load(std::memory_order_acquire);
        if (trackIndex >= count ||
            trackIndex >= kSmfTrackOutputRouteCapacity) {
            return kSmfTrackOutputRouteAuto;
        }
        const std::size_t wordIndex = trackIndex / 4u;
        const uint8_t byteIndex = static_cast<uint8_t>(trackIndex % 4u);
        const uint32_t word =
            packedRoutes_[wordIndex].load(std::memory_order_acquire);
        const uint8_t encoded = static_cast<uint8_t>(
            (word >> (static_cast<uint32_t>(byteIndex) * 8u)) & 0xFFu);
        return decode(encoded);
    }

private:
    static constexpr std::size_t kPackedRouteWords =
        kSmfTrackOutputRouteCapacity / 4u;
    static constexpr uint32_t kInitializingGeneration = 0xFFFFFFFFu;

    static SmfTrackOutputRouteSnapshot emptySnapshot() {
        SmfTrackOutputRouteSnapshot result{};
        for (auto& destination : result.destinationChannels) {
            destination = kSmfTrackOutputRouteAuto;
        }
        return result;
    }

    static constexpr uint16_t boundedTrackCount(uint16_t trackCount) {
        return trackCount > kSmfTrackOutputRouteCapacity
            ? static_cast<uint16_t>(kSmfTrackOutputRouteCapacity)
            : trackCount;
    }

    bool ensureSession(uint32_t generation, uint16_t trackCountHint) {
        const uint16_t boundedHint = boundedTrackCount(trackCountHint);
        while (true) {
            uint32_t bound = boundGeneration_.load(std::memory_order_acquire);
            if (bound == generation) {
                raiseTrackCount(boundedHint);
                return true;
            }
            if (bound == kInitializingGeneration) continue;
            if (!boundGeneration_.compare_exchange_weak(
                    bound,
                    kInitializingGeneration,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                continue;
            }

            for (auto& word : packedRoutes_) {
                word.store(0u, std::memory_order_relaxed);
            }
            trackCount_.store(boundedHint, std::memory_order_relaxed);
            revision_.fetch_add(1u, std::memory_order_relaxed);
            boundGeneration_.store(generation, std::memory_order_release);
            return true;
        }
    }

    void raiseTrackCount(uint16_t trackCountHint) {
        uint16_t current = trackCount_.load(std::memory_order_acquire);
        while (current < trackCountHint &&
               !trackCount_.compare_exchange_weak(
                   current,
                   trackCountHint,
                   std::memory_order_acq_rel,
                   std::memory_order_acquire)) {
        }
    }

    static constexpr uint8_t encode(int8_t destinationChannel) {
        return destinationChannel == kSmfTrackOutputRouteAuto
            ? 0u
            : static_cast<uint8_t>(destinationChannel + 1);
    }

    static constexpr int8_t decode(uint8_t encoded) {
        return encoded == 0u
            ? kSmfTrackOutputRouteAuto
            : static_cast<int8_t>(encoded - 1u);
    }

    std::atomic<uint32_t> packedRoutes_[kPackedRouteWords]{};
    std::atomic<uint32_t> revision_{0u};
    std::atomic<uint32_t> boundGeneration_{0u};
    std::atomic<uint16_t> trackCount_{0u};
};

static_assert(kSmfTrackOutputRouteCapacity % 4u == 0u,
              "SMF output routes must pack into complete 32-bit words");
static_assert(sizeof(std::atomic<uint32_t>) == sizeof(uint32_t),
              "SMF route words must remain one bounded 32-bit atomic each");

inline SmfTrackOutputRouteState& smfTrackOutputRouteState() {
    static SmfTrackOutputRouteState state;
    return state;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_TRACK_OUTPUT_ROUTE_H
