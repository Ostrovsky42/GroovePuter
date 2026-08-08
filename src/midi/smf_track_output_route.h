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
// Four bits travel with each scheduled SMF event. 0..14 are normal per-track
// route revisions; 15 is reserved for consumer-generated cleanup NoteOffs so a
// second fast route change cannot invalidate cleanup already removed from the
// logical ownership table.
constexpr uint8_t kSmfTrackOutputRouteRevisionCleanup = 0x0Fu;
constexpr uint8_t kSmfTrackOutputRouteRevisionCount = 15u;

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

        const std::size_t wordIndex = trackIndex / 4u;
        const uint8_t byteIndex = static_cast<uint8_t>(trackIndex % 4u);
        const uint32_t shift = static_cast<uint32_t>(byteIndex) * 8u;
        const uint32_t mask = uint32_t{0xFFu} << shift;
        const uint32_t releaseBit = uint32_t{1} << trackIndex;

        uint32_t current =
            packedRoutes_[wordIndex].load(std::memory_order_acquire);
        while (true) {
            const uint8_t previous = static_cast<uint8_t>(
                (current >> shift) & 0xFFu);
            if (decode(previous) == destinationChannel) return true;

            const uint8_t nextEncoded = encodeWithRevision(
                destinationChannel,
                nextRevisionTag(revisionTag(previous)));
            const uint32_t next =
                (current & ~mask) |
                (static_cast<uint32_t>(nextEncoded) << shift);
            if (packedRoutes_[wordIndex].compare_exchange_weak(
                    current,
                    next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                // Existing queued notes retain the old revision and become
                // stale at dispatch. Active physical notes are released by the
                // queue's existing bounded per-track ownership path.
                pendingReleaseMask_.fetch_or(
                    releaseBit, std::memory_order_acq_rel);
                revision_.fetch_add(1u, std::memory_order_acq_rel);
                return true;
            }
        }
    }

    bool replaceDestinations(const int8_t* destinations,
                             uint16_t trackCount,
                             uint32_t generation) {
        if (!destinations || trackCount == 0u ||
            trackCount > kSmfTrackOutputRouteCapacity ||
            generation == 0u || smfSessionGeneration() != generation ||
            !ensureSession(generation, trackCount)) {
            return false;
        }

        uint32_t packed[kPackedRouteWords]{};
        for (uint16_t track = 0u; track < trackCount; ++track) {
            const int8_t destination = destinations[track];
            if (destination < kSmfTrackOutputRouteAuto ||
                destination >=
                    static_cast<int8_t>(kSmfSeqtrakOutputChannelCount)) {
                return false;
            }
            const std::size_t word = track / 4u;
            const uint32_t shift = static_cast<uint32_t>(track % 4u) * 8u;
            packed[word] |= static_cast<uint32_t>(
                encodeWithRevision(destination, 0u)) << shift;
        }

        SmfSessionMutationGuard guard(generation);
        if (!guard ||
            boundGeneration_.load(std::memory_order_acquire) != generation) {
            return false;
        }
        for (std::size_t word = 0u; word < kPackedRouteWords; ++word) {
            packedRoutes_[word].store(packed[word], std::memory_order_release);
        }
        trackCount_.store(trackCount, std::memory_order_release);
        pendingReleaseMask_.store(0u, std::memory_order_release);
        producerRouteStamp_.store(kInvalidProducerRouteStamp,
                                  std::memory_order_release);
        revision_.fetch_add(1u, std::memory_order_acq_rel);
        return true;
    }

    int8_t destinationFor(uint16_t trackIndex, uint16_t trackCountHint) {
        uint8_t encoded = 0u;
        if (!readEncodedRoute(trackIndex, trackCountHint, encoded)) {
            return kSmfTrackOutputRouteAuto;
        }
        return decode(encoded);
    }

    // SmfPlayerTask is the only producer of scheduled SMF notes. Capture the
    // destination and its revision from the same atomic byte, then remember the
    // revision for exactly the next queue publication. This closes the cross-
    // core race where the UI changes a route between lookup and enqueue.
    int8_t destinationForProducer(uint16_t trackIndex,
                                  uint16_t trackCountHint) {
        uint8_t encoded = 0u;
        const bool available = readEncodedRoute(
            trackIndex, trackCountHint, encoded);
        const uint8_t tag = available ? revisionTag(encoded) : 0u;
        producerRouteStamp_.store(
            makeProducerRouteStamp(trackIndex, tag),
            std::memory_order_release);
        return available ? decode(encoded) : kSmfTrackOutputRouteAuto;
    }

    uint8_t consumeProducerRevisionTag(uint8_t trackIndex) {
        const uint32_t stamp = producerRouteStamp_.exchange(
            kInvalidProducerRouteStamp, std::memory_order_acq_rel);
        if (stamp != kInvalidProducerRouteStamp &&
            producerRouteStampTrack(stamp) == trackIndex) {
            return producerRouteStampRevision(stamp);
        }
        return revisionTagForRealtime(trackIndex);
    }

    // Dispatcher-side read: bounded, allocation-free and non-mutating. Missing
    // routes (including physical SMF tracks beyond the 32 override slots) use
    // the default revision zero and AUTO routing.
    uint8_t revisionTagForRealtime(uint16_t trackIndex) const {
        const uint32_t generation = smfSessionGeneration();
        if (generation == 0u ||
            boundGeneration_.load(std::memory_order_acquire) != generation) {
            return 0u;
        }
        const uint16_t count = trackCount_.load(std::memory_order_acquire);
        if (trackIndex >= count ||
            trackIndex >= kSmfTrackOutputRouteCapacity) {
            return 0u;
        }
        const std::size_t wordIndex = trackIndex / 4u;
        const uint8_t byteIndex = static_cast<uint8_t>(trackIndex % 4u);
        const uint32_t word =
            packedRoutes_[wordIndex].load(std::memory_order_acquire);
        const uint8_t encoded = static_cast<uint8_t>(
            (word >> (static_cast<uint32_t>(byteIndex) * 8u)) & 0xFFu);
        return revisionTag(encoded);
    }

    bool takePendingReleaseTrack(uint8_t& trackIndex) {
        uint32_t pending = pendingReleaseMask_.load(std::memory_order_acquire);
        while (pending != 0u) {
            const uint32_t bit = pending & (~pending + 1u);
            const uint32_t next = pending & ~bit;
            if (pendingReleaseMask_.compare_exchange_weak(
                    pending,
                    next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                trackIndex = trailingZeroes(bit);
                return true;
            }
        }
        return false;
    }

private:
    static constexpr std::size_t kPackedRouteWords =
        kSmfTrackOutputRouteCapacity / 4u;
    static constexpr uint32_t kInitializingGeneration = 0xFFFFFFFFu;
    static constexpr uint32_t kInvalidProducerRouteStamp = 0xFFFFFFFFu;
    static constexpr uint8_t kDestinationMask = 0x0Fu;
    static constexpr uint8_t kRevisionShift = 4u;

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
            pendingReleaseMask_.store(0u, std::memory_order_relaxed);
            producerRouteStamp_.store(kInvalidProducerRouteStamp,
                                      std::memory_order_relaxed);
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

    bool readEncodedRoute(uint16_t trackIndex,
                          uint16_t trackCountHint,
                          uint8_t& encoded) {
        const uint32_t generation = smfSessionGeneration();
        if (generation == 0u ||
            !ensureSession(generation, trackCountHint)) {
            encoded = 0u;
            return false;
        }
        const uint16_t count = trackCount_.load(std::memory_order_acquire);
        if (trackIndex >= count ||
            trackIndex >= kSmfTrackOutputRouteCapacity) {
            encoded = 0u;
            return false;
        }
        const std::size_t wordIndex = trackIndex / 4u;
        const uint8_t byteIndex = static_cast<uint8_t>(trackIndex % 4u);
        const uint32_t word =
            packedRoutes_[wordIndex].load(std::memory_order_acquire);
        encoded = static_cast<uint8_t>(
            (word >> (static_cast<uint32_t>(byteIndex) * 8u)) & 0xFFu);
        return true;
    }

    static constexpr uint8_t encode(int8_t destinationChannel) {
        return destinationChannel == kSmfTrackOutputRouteAuto
            ? 0u
            : static_cast<uint8_t>(destinationChannel + 1);
    }

    static constexpr int8_t decode(uint8_t encoded) {
        const uint8_t destination = encoded & kDestinationMask;
        return destination == 0u
            ? kSmfTrackOutputRouteAuto
            : static_cast<int8_t>(destination - 1u);
    }

    static constexpr uint8_t revisionTag(uint8_t encoded) {
        return static_cast<uint8_t>(encoded >> kRevisionShift);
    }

    static constexpr uint8_t encodeWithRevision(int8_t destinationChannel,
                                                uint8_t revision) {
        return static_cast<uint8_t>(
            ((revision & 0x0Fu) << kRevisionShift) |
            (encode(destinationChannel) & kDestinationMask));
    }

    static constexpr uint8_t nextRevisionTag(uint8_t current) {
        return current + 1u >= kSmfTrackOutputRouteRevisionCount
            ? 0u
            : static_cast<uint8_t>(current + 1u);
    }

    static constexpr uint32_t makeProducerRouteStamp(uint16_t trackIndex,
                                                     uint8_t revision) {
        return (static_cast<uint32_t>(trackIndex & 0xFFu) << 8u) |
               static_cast<uint32_t>(revision & 0x0Fu);
    }

    static constexpr uint8_t producerRouteStampTrack(uint32_t stamp) {
        return static_cast<uint8_t>((stamp >> 8u) & 0xFFu);
    }

    static constexpr uint8_t producerRouteStampRevision(uint32_t stamp) {
        return static_cast<uint8_t>(stamp & 0x0Fu);
    }

    static uint8_t trailingZeroes(uint32_t value) {
        uint8_t count = 0u;
        while ((value & 1u) == 0u) {
            value >>= 1u;
            ++count;
        }
        return count;
    }

    std::atomic<uint32_t> packedRoutes_[kPackedRouteWords]{};
    std::atomic<uint32_t> revision_{0u};
    std::atomic<uint32_t> boundGeneration_{0u};
    std::atomic<uint16_t> trackCount_{0u};
    std::atomic<uint32_t> pendingReleaseMask_{0u};
    std::atomic<uint32_t> producerRouteStamp_{kInvalidProducerRouteStamp};
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
