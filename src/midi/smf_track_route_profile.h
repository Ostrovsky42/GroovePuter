#pragma once
#ifndef GROOVEPUTER_SMF_TRACK_ROUTE_PROFILE_H
#define GROOVEPUTER_SMF_TRACK_ROUTE_PROFILE_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "smf_stream.h"
#include "smf_track_output_route.h"

namespace GroovePuterMidi {

constexpr std::size_t kSmfTrackRouteProfileSlotCount = 16u;

struct SmfTrackRouteProfileIdentity {
    uint64_t pathHash{0u};
    uint64_t contentHash{0u};
    uint32_t fileSize{0u};
    uint16_t trackCount{0u};

    bool valid() const {
        return pathHash != 0u && contentHash != 0u &&
               trackCount > 0u &&
               trackCount <= kSmfTrackOutputRouteCapacity;
    }

    bool matches(const SmfTrackRouteProfileIdentity& other) const {
        return pathHash == other.pathHash &&
               contentHash == other.contentHash &&
               fileSize == other.fileSize &&
               trackCount == other.trackCount;
    }
};

struct SmfTrackRouteProfile {
    SmfTrackRouteProfileIdentity identity{};
    uint32_t sequence{0u};
    int8_t destinationChannels[kSmfTrackOutputRouteCapacity]{};

    SmfTrackRouteProfile() { clearRoutes(); }

    void reset(const SmfTrackRouteProfileIdentity& nextIdentity) {
        identity = nextIdentity;
        sequence = 0u;
        clearRoutes();
    }

    void clearRoutes() {
        for (auto& destination : destinationChannels) {
            destination = kSmfTrackOutputRouteAuto;
        }
    }
};

class SmfTrackRouteFingerprint {
public:
    SmfTrackRouteFingerprint(const char* path,
                             uint32_t fileSize,
                             const SmfFileIndex& index);

    void observe(const SmfStreamEvent& event);
    SmfTrackRouteProfileIdentity identity() const;

    static uint64_t hashPath(const char* path);

private:
    static constexpr uint64_t kFnvOffset = 14695981039346656037ull;
    static constexpr uint64_t kFnvPrime = 1099511628211ull;

    static void mixByte(uint64_t& hash, uint8_t value);
    static void mixU16(uint64_t& hash, uint16_t value);
    static void mixU32(uint64_t& hash, uint32_t value);

    SmfTrackRouteProfileIdentity identity_{};
};

class SmfTrackRouteProfileCodec {
public:
    static constexpr uint16_t kSchemaVersion = 1u;
    static constexpr std::size_t kEncodedSize = 72u;
    using EncodedProfile = std::array<uint8_t, kEncodedSize>;

    static EncodedProfile encode(const SmfTrackRouteProfile& profile);
    static bool decode(const uint8_t* data,
                       std::size_t size,
                       SmfTrackRouteProfile& output);

private:
    static uint32_t crc32(const uint8_t* data, std::size_t size);
};

enum class SmfTrackRouteProfileStorageReadStatus : uint8_t {
    Ok,
    NotFound,
    Error,
};

class ISmfTrackRouteProfileStorage {
public:
    virtual ~ISmfTrackRouteProfileStorage() = default;

    virtual SmfTrackRouteProfileStorageReadStatus readSlot(
        std::size_t slot,
        uint8_t* destination,
        std::size_t capacity,
        std::size_t& bytesRead) = 0;
    virtual bool writeSlot(std::size_t slot,
                           const uint8_t* data,
                           std::size_t size) = 0;
};

enum class SmfTrackRouteProfileLoadStatus : uint8_t {
    Loaded,
    Missing,
    Stale,
    Corrupt,
    StorageError,
};

class SmfTrackRouteProfilePersistence {
public:
    explicit SmfTrackRouteProfilePersistence(
        ISmfTrackRouteProfileStorage& storage)
        : storage_(storage) {}

    SmfTrackRouteProfileLoadStatus load(
        const SmfTrackRouteProfileIdentity& identity,
        SmfTrackRouteProfile& output);

    bool save(const SmfTrackRouteProfile& profile);

private:
    ISmfTrackRouteProfileStorage& storage_;
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_TRACK_ROUTE_PROFILE_H
