#include "smf_track_route_profile.h"

#include <limits>

namespace GroovePuterMidi {
namespace {

constexpr uint32_t kMagic = 0x52504653u;  // "SFPR" in little-endian storage.
constexpr std::size_t kSequenceOffset = 8u;
constexpr std::size_t kPathHashOffset = 12u;
constexpr std::size_t kContentHashOffset = 20u;
constexpr std::size_t kFileSizeOffset = 28u;
constexpr std::size_t kTrackCountOffset = 32u;
constexpr std::size_t kCapacityOffset = 34u;
constexpr std::size_t kRoutesOffset = 36u;
constexpr std::size_t kCrcOffset = 68u;

void putU16(uint8_t* data, std::size_t offset, uint16_t value) {
    data[offset] = static_cast<uint8_t>(value & 0xFFu);
    data[offset + 1u] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
}

void putU32(uint8_t* data, std::size_t offset, uint32_t value) {
    for (uint8_t byte = 0u; byte < 4u; ++byte) {
        data[offset + byte] = static_cast<uint8_t>(value >> (byte * 8u));
    }
}

void putU64(uint8_t* data, std::size_t offset, uint64_t value) {
    for (uint8_t byte = 0u; byte < 8u; ++byte) {
        data[offset + byte] = static_cast<uint8_t>(value >> (byte * 8u));
    }
}

uint16_t getU16(const uint8_t* data, std::size_t offset) {
    return static_cast<uint16_t>(data[offset]) |
           static_cast<uint16_t>(data[offset + 1u]) << 8u;
}

uint32_t getU32(const uint8_t* data, std::size_t offset) {
    uint32_t value = 0u;
    for (uint8_t byte = 0u; byte < 4u; ++byte) {
        value |= static_cast<uint32_t>(data[offset + byte]) << (byte * 8u);
    }
    return value;
}

uint64_t getU64(const uint8_t* data, std::size_t offset) {
    uint64_t value = 0u;
    for (uint8_t byte = 0u; byte < 8u; ++byte) {
        value |= static_cast<uint64_t>(data[offset + byte]) << (byte * 8u);
    }
    return value;
}

bool validDestination(int8_t destination) {
    return destination >= kSmfTrackOutputRouteAuto &&
           destination < static_cast<int8_t>(kSmfSeqtrakOutputChannelCount);
}

bool newerSequence(uint32_t candidate, uint32_t current) {
    return current == 0u ||
           static_cast<int32_t>(candidate - current) > 0;
}

}  // namespace

SmfTrackRouteFingerprint::SmfTrackRouteFingerprint(
        const char* path,
        uint32_t fileSize,
        const SmfFileIndex& index) {
    identity_.pathHash = hashPath(path);
    identity_.fileSize = fileSize;
    identity_.trackCount = index.trackCount;
    identity_.contentHash = kFnvOffset;
    mixU32(identity_.contentHash, fileSize);
    mixU16(identity_.contentHash, index.format);
    mixU16(identity_.contentHash, index.division);
    mixU16(identity_.contentHash, index.trackCount);
    mixU16(identity_.contentHash, index.declaredTrackCount);
}

void SmfTrackRouteFingerprint::observe(const SmfStreamEvent& event) {
    mixU16(identity_.contentHash, event.trackIndex);
    mixU32(identity_.contentHash, event.ordinal);
    mixU32(identity_.contentHash, event.event.tick);
    mixU32(identity_.contentHash, event.event.sequence);
    mixByte(identity_.contentHash, static_cast<uint8_t>(event.event.kind));
    mixByte(identity_.contentHash, event.event.channel);
    mixByte(identity_.contentHash, event.event.data1);
    mixByte(identity_.contentHash, event.event.data2);
    mixU32(identity_.contentHash, event.event.value);
}

SmfTrackRouteProfileIdentity SmfTrackRouteFingerprint::identity() const {
    return identity_;
}

uint64_t SmfTrackRouteFingerprint::hashPath(const char* path) {
    if (!path || !path[0]) return 0u;
    uint64_t hash = kFnvOffset;
    for (const char* cursor = path; *cursor; ++cursor) {
        uint8_t value = static_cast<uint8_t>(*cursor);
        if (value == static_cast<uint8_t>('\\')) value = '/';
        if (value >= static_cast<uint8_t>('A') &&
            value <= static_cast<uint8_t>('Z')) {
            value = static_cast<uint8_t>(value - 'A' + 'a');
        }
        mixByte(hash, value);
    }
    mixByte(hash, 0u);
    return hash;
}

void SmfTrackRouteFingerprint::mixByte(uint64_t& hash, uint8_t value) {
    hash ^= value;
    hash *= kFnvPrime;
}

void SmfTrackRouteFingerprint::mixU16(uint64_t& hash, uint16_t value) {
    mixByte(hash, static_cast<uint8_t>(value & 0xFFu));
    mixByte(hash, static_cast<uint8_t>((value >> 8u) & 0xFFu));
}

void SmfTrackRouteFingerprint::mixU32(uint64_t& hash, uint32_t value) {
    for (uint8_t byte = 0u; byte < 4u; ++byte) {
        mixByte(hash, static_cast<uint8_t>(value >> (byte * 8u)));
    }
}

SmfTrackRouteProfileCodec::EncodedProfile
SmfTrackRouteProfileCodec::encode(const SmfTrackRouteProfile& profile) {
    EncodedProfile encoded{};
    putU32(encoded.data(), 0u, kMagic);
    putU16(encoded.data(), 4u, kSchemaVersion);
    putU16(encoded.data(), 6u, static_cast<uint16_t>(kEncodedSize));
    putU32(encoded.data(), kSequenceOffset, profile.sequence);
    putU64(encoded.data(), kPathHashOffset, profile.identity.pathHash);
    putU64(encoded.data(), kContentHashOffset, profile.identity.contentHash);
    putU32(encoded.data(), kFileSizeOffset, profile.identity.fileSize);
    putU16(encoded.data(), kTrackCountOffset, profile.identity.trackCount);
    putU16(encoded.data(), kCapacityOffset,
           static_cast<uint16_t>(kSmfTrackOutputRouteCapacity));
    for (std::size_t track = 0u;
         track < kSmfTrackOutputRouteCapacity;
         ++track) {
        const int8_t destination = profile.destinationChannels[track];
        encoded[kRoutesOffset + track] = validDestination(destination)
            ? static_cast<uint8_t>(destination + 1)
            : 0u;
    }
    putU32(encoded.data(), kCrcOffset, crc32(encoded.data(), kCrcOffset));
    return encoded;
}

bool SmfTrackRouteProfileCodec::decode(const uint8_t* data,
                                       std::size_t size,
                                       SmfTrackRouteProfile& output) {
    if (!data || size != kEncodedSize ||
        getU32(data, 0u) != kMagic ||
        getU16(data, 4u) != kSchemaVersion ||
        getU16(data, 6u) != kEncodedSize ||
        getU16(data, kCapacityOffset) != kSmfTrackOutputRouteCapacity ||
        getU32(data, kCrcOffset) != crc32(data, kCrcOffset)) {
        return false;
    }

    SmfTrackRouteProfile decoded;
    decoded.sequence = getU32(data, kSequenceOffset);
    decoded.identity.pathHash = getU64(data, kPathHashOffset);
    decoded.identity.contentHash = getU64(data, kContentHashOffset);
    decoded.identity.fileSize = getU32(data, kFileSizeOffset);
    decoded.identity.trackCount = getU16(data, kTrackCountOffset);
    if (decoded.sequence == 0u || !decoded.identity.valid()) return false;

    for (std::size_t track = 0u;
         track < kSmfTrackOutputRouteCapacity;
         ++track) {
        const uint8_t value = data[kRoutesOffset + track];
        if (value > kSmfSeqtrakOutputChannelCount) return false;
        decoded.destinationChannels[track] = value == 0u
            ? kSmfTrackOutputRouteAuto
            : static_cast<int8_t>(value - 1u);
    }
    output = decoded;
    return true;
}

uint32_t SmfTrackRouteProfileCodec::crc32(const uint8_t* data,
                                          std::size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0u; i < size; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0u; bit < 8u; ++bit) {
            const uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

SmfTrackRouteProfileLoadStatus SmfTrackRouteProfilePersistence::load(
        const SmfTrackRouteProfileIdentity& identity,
        SmfTrackRouteProfile& output) {
    output.reset(identity);
    if (!identity.valid()) {
        return SmfTrackRouteProfileLoadStatus::StorageError;
    }

    bool found = false;
    bool sawSamePath = false;
    bool sawCorrupt = false;
    bool sawStorageError = false;
    SmfTrackRouteProfile best;
    std::array<uint8_t, SmfTrackRouteProfileCodec::kEncodedSize> bytes{};

    for (std::size_t slot = 0u; slot < kSmfTrackRouteProfileSlotCount; ++slot) {
        std::size_t bytesRead = 0u;
        const SmfTrackRouteProfileStorageReadStatus status =
            storage_.readSlot(slot, bytes.data(), bytes.size(), bytesRead);
        if (status == SmfTrackRouteProfileStorageReadStatus::NotFound) continue;
        if (status == SmfTrackRouteProfileStorageReadStatus::Error) {
            sawStorageError = true;
            continue;
        }

        SmfTrackRouteProfile candidate;
        if (!SmfTrackRouteProfileCodec::decode(
                bytes.data(), bytesRead, candidate)) {
            sawCorrupt = true;
            continue;
        }
        if (candidate.identity.pathHash != identity.pathHash) continue;
        sawSamePath = true;
        if (!candidate.identity.matches(identity)) continue;
        if (!found || newerSequence(candidate.sequence, best.sequence)) {
            best = candidate;
            found = true;
        }
    }

    if (found) {
        output = best;
        return SmfTrackRouteProfileLoadStatus::Loaded;
    }
    if (sawSamePath) return SmfTrackRouteProfileLoadStatus::Stale;
    if (sawStorageError) return SmfTrackRouteProfileLoadStatus::StorageError;
    if (sawCorrupt) return SmfTrackRouteProfileLoadStatus::Corrupt;
    return SmfTrackRouteProfileLoadStatus::Missing;
}

bool SmfTrackRouteProfilePersistence::save(
        const SmfTrackRouteProfile& profile) {
    if (!profile.identity.valid()) return false;
    for (std::size_t track = 0u;
         track < kSmfTrackOutputRouteCapacity;
         ++track) {
        if (!validDestination(profile.destinationChannels[track])) return false;
    }

    std::size_t samePathSlot = kSmfTrackRouteProfileSlotCount;
    uint32_t samePathSequence = 0u;
    std::size_t reusableSlot = kSmfTrackRouteProfileSlotCount;
    std::size_t oldestSlot = 0u;
    uint32_t oldestSequence = std::numeric_limits<uint32_t>::max();
    uint32_t newestSequence = 0u;
    std::array<uint8_t, SmfTrackRouteProfileCodec::kEncodedSize> bytes{};

    for (std::size_t slot = 0u; slot < kSmfTrackRouteProfileSlotCount; ++slot) {
        std::size_t bytesRead = 0u;
        const SmfTrackRouteProfileStorageReadStatus status =
            storage_.readSlot(slot, bytes.data(), bytes.size(), bytesRead);
        if (status == SmfTrackRouteProfileStorageReadStatus::Error) return false;
        if (status == SmfTrackRouteProfileStorageReadStatus::NotFound) {
            if (reusableSlot == kSmfTrackRouteProfileSlotCount) reusableSlot = slot;
            continue;
        }

        SmfTrackRouteProfile candidate;
        if (!SmfTrackRouteProfileCodec::decode(
                bytes.data(), bytesRead, candidate)) {
            if (reusableSlot == kSmfTrackRouteProfileSlotCount) reusableSlot = slot;
            continue;
        }
        if (newerSequence(candidate.sequence, newestSequence)) {
            newestSequence = candidate.sequence;
        }
        if (candidate.sequence < oldestSequence) {
            oldestSequence = candidate.sequence;
            oldestSlot = slot;
        }
        if (candidate.identity.pathHash == profile.identity.pathHash &&
            (samePathSlot == kSmfTrackRouteProfileSlotCount ||
             newerSequence(candidate.sequence, samePathSequence))) {
            samePathSlot = slot;
            samePathSequence = candidate.sequence;
        }
    }

    const std::size_t targetSlot =
        samePathSlot != kSmfTrackRouteProfileSlotCount
            ? samePathSlot
            : (reusableSlot != kSmfTrackRouteProfileSlotCount
                   ? reusableSlot
                   : oldestSlot);
    SmfTrackRouteProfile stored = profile;
    stored.sequence = newestSequence == std::numeric_limits<uint32_t>::max()
        ? 1u
        : newestSequence + 1u;
    if (stored.sequence == 0u) stored.sequence = 1u;
    const SmfTrackRouteProfileCodec::EncodedProfile encoded =
        SmfTrackRouteProfileCodec::encode(stored);
    return storage_.writeSlot(targetSlot, encoded.data(), encoded.size());
}

}  // namespace GroovePuterMidi
