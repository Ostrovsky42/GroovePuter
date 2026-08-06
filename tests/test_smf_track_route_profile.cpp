#include <array>
#include <cassert>
#include <cstring>

#include "src/midi/smf_track_route_profile.h"

using namespace GroovePuterMidi;

namespace {

class FakeRouteProfileStorage final : public ISmfTrackRouteProfileStorage {
public:
    SmfTrackRouteProfileStorageReadStatus readSlot(
            std::size_t slot,
            uint8_t* destination,
            std::size_t capacity,
            std::size_t& bytesRead) override {
        bytesRead = 0u;
        if (slot >= kSmfTrackRouteProfileSlotCount || !destination) {
            return SmfTrackRouteProfileStorageReadStatus::Error;
        }
        if (failReads_) return SmfTrackRouteProfileStorageReadStatus::Error;
        if (!used_[slot]) {
            return SmfTrackRouteProfileStorageReadStatus::NotFound;
        }
        if (capacity < slots_[slot].size()) {
            return SmfTrackRouteProfileStorageReadStatus::Error;
        }
        std::memcpy(destination, slots_[slot].data(), slots_[slot].size());
        bytesRead = slots_[slot].size();
        return SmfTrackRouteProfileStorageReadStatus::Ok;
    }

    bool writeSlot(std::size_t slot,
                   const uint8_t* data,
                   std::size_t size) override {
        if (failWrites_ || slot >= kSmfTrackRouteProfileSlotCount || !data ||
            size != SmfTrackRouteProfileCodec::kEncodedSize) {
            return false;
        }
        std::memcpy(slots_[slot].data(), data, size);
        used_[slot] = true;
        lastWrittenSlot_ = slot;
        ++writes_;
        return true;
    }

    void corrupt(std::size_t slot, std::size_t byte) {
        assert(slot < kSmfTrackRouteProfileSlotCount);
        assert(byte < SmfTrackRouteProfileCodec::kEncodedSize);
        assert(used_[slot]);
        slots_[slot][byte] ^= 0x5Au;
    }

    void setReadFailure(bool enabled) { failReads_ = enabled; }
    void setWriteFailure(bool enabled) { failWrites_ = enabled; }
    std::size_t writes() const { return writes_; }
    std::size_t lastWrittenSlot() const { return lastWrittenSlot_; }

private:
    std::array<bool, kSmfTrackRouteProfileSlotCount> used_{};
    std::array<
        std::array<uint8_t, SmfTrackRouteProfileCodec::kEncodedSize>,
        kSmfTrackRouteProfileSlotCount> slots_{};
    bool failReads_{false};
    bool failWrites_{false};
    std::size_t writes_{0u};
    std::size_t lastWrittenSlot_{kSmfTrackRouteProfileSlotCount};
};

SmfTrackRouteProfileIdentity identityFor(const char* path,
                                         uint8_t note,
                                         uint32_t fileSize = 1024u) {
    SmfFileIndex index{};
    index.format = 1u;
    index.division = 480u;
    index.trackCount = 3u;
    index.declaredTrackCount = 3u;

    SmfTrackRouteFingerprint fingerprint(path, fileSize, index);
    SmfStreamEvent event{};
    event.trackIndex = 1u;
    event.ordinal = 2u;
    event.event.tick = 96u;
    event.event.sequence = 2u;
    event.event.kind = SmfEventKind::NoteOn;
    event.event.channel = 4u;
    event.event.data1 = note;
    event.event.data2 = 100u;
    fingerprint.observe(event);
    return fingerprint.identity();
}

}  // namespace

int main() {
    const SmfTrackRouteProfileIdentity identity =
        identityFor("/MIDI/Arrangement.mid", 60u);
    const SmfTrackRouteProfileIdentity normalizedPath =
        identityFor("\\midi\\arrangement.mid", 60u);
    const SmfTrackRouteProfileIdentity changedNote =
        identityFor("/midi/arrangement.mid", 61u);
    const SmfTrackRouteProfileIdentity changedSize =
        identityFor("/midi/arrangement.mid", 60u, 2048u);

    assert(identity.valid());
    assert(identity.matches(normalizedPath));
    assert(!identity.matches(changedNote));
    assert(!identity.matches(changedSize));

    SmfTrackRouteProfile profile;
    profile.reset(identity);
    profile.sequence = 7u;
    profile.destinationChannels[0] = 0;
    profile.destinationChannels[1] = 7;
    profile.destinationChannels[2] = 9;

    SmfTrackRouteProfileCodec::EncodedProfile encoded =
        SmfTrackRouteProfileCodec::encode(profile);
    SmfTrackRouteProfile decoded;
    assert(SmfTrackRouteProfileCodec::decode(
        encoded.data(), encoded.size(), decoded));
    assert(decoded.sequence == 7u);
    assert(decoded.identity.matches(identity));
    assert(decoded.destinationChannels[0] == 0);
    assert(decoded.destinationChannels[1] == 7);
    assert(decoded.destinationChannels[2] == 9);
    assert(decoded.destinationChannels[3] == kSmfTrackOutputRouteAuto);

    encoded[40] ^= 0x01u;
    assert(!SmfTrackRouteProfileCodec::decode(
        encoded.data(), encoded.size(), decoded));

    FakeRouteProfileStorage storage;
    SmfTrackRouteProfilePersistence persistence(storage);
    profile.sequence = 0u;
    assert(persistence.save(profile));
    assert(storage.writes() == 1u);

    SmfTrackRouteProfile loaded;
    assert(persistence.load(identity, loaded) ==
           SmfTrackRouteProfileLoadStatus::Loaded);
    assert(loaded.identity.matches(identity));
    assert(loaded.destinationChannels[1] == 7);
    assert(loaded.destinationChannels[2] == 9);

    assert(persistence.load(changedNote, loaded) ==
           SmfTrackRouteProfileLoadStatus::Stale);
    assert(loaded.identity.matches(changedNote));
    for (int8_t destination : loaded.destinationChannels) {
        assert(destination == kSmfTrackOutputRouteAuto);
    }

    SmfTrackRouteProfile changedProfile;
    changedProfile.reset(changedNote);
    changedProfile.destinationChannels[1] = 8;
    assert(persistence.save(changedProfile));
    assert(storage.writes() == 2u);
    assert(storage.lastWrittenSlot() == 0u);
    assert(persistence.load(changedNote, loaded) ==
           SmfTrackRouteProfileLoadStatus::Loaded);
    assert(loaded.destinationChannels[1] == 8);
    assert(persistence.load(identity, loaded) ==
           SmfTrackRouteProfileLoadStatus::Stale);

    storage.corrupt(0u, 10u);
    assert(persistence.load(changedNote, loaded) ==
           SmfTrackRouteProfileLoadStatus::Corrupt);
    for (int8_t destination : loaded.destinationChannels) {
        assert(destination == kSmfTrackOutputRouteAuto);
    }

    storage.setReadFailure(true);
    assert(persistence.load(identity, loaded) ==
           SmfTrackRouteProfileLoadStatus::StorageError);
    storage.setReadFailure(false);
    storage.setWriteFailure(true);
    assert(!persistence.save(profile));

    return 0;
}
