#include "cardputer_smf_route_persistence.h"

#ifdef ARDUINO

#include <Arduino.h>
#include <Preferences.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "src/midi/smf_track_output_route.h"
#include "src/midi/smf_track_route_profile.h"
#include "src/midi/smf_track_route_profile_runtime.h"

namespace GroovePuterPlatform {
namespace {

class CardputerSmfTrackRouteProfileStorage final
    : public GroovePuterMidi::ISmfTrackRouteProfileStorage {
public:
    GroovePuterMidi::SmfTrackRouteProfileStorageReadStatus readSlot(
            std::size_t slot,
            uint8_t* destination,
            std::size_t capacity,
            std::size_t& bytesRead) override {
        bytesRead = 0u;
        if (slot >= GroovePuterMidi::kSmfTrackRouteProfileSlotCount ||
            !destination) {
            return GroovePuterMidi::
                SmfTrackRouteProfileStorageReadStatus::Error;
        }

        char key[16]{};
        std::snprintf(key, sizeof(key), "smf_route_%02u",
                      static_cast<unsigned>(slot));
        Preferences preferences;
        if (!preferences.begin(kNamespace, true)) {
            return GroovePuterMidi::
                SmfTrackRouteProfileStorageReadStatus::Error;
        }
        if (!preferences.isKey(key)) {
            preferences.end();
            return GroovePuterMidi::
                SmfTrackRouteProfileStorageReadStatus::NotFound;
        }

        const std::size_t storedSize = preferences.getBytesLength(key);
        if (storedSize == 0u || storedSize > capacity) {
            preferences.end();
            return GroovePuterMidi::
                SmfTrackRouteProfileStorageReadStatus::Error;
        }
        const std::size_t readSize =
            preferences.getBytes(key, destination, storedSize);
        preferences.end();
        if (readSize != storedSize) {
            return GroovePuterMidi::
                SmfTrackRouteProfileStorageReadStatus::Error;
        }
        bytesRead = readSize;
        return GroovePuterMidi::SmfTrackRouteProfileStorageReadStatus::Ok;
    }

    bool writeSlot(std::size_t slot,
                   const uint8_t* data,
                   std::size_t size) override {
        if (slot >= GroovePuterMidi::kSmfTrackRouteProfileSlotCount ||
            !data || size == 0u) {
            return false;
        }

        char key[16]{};
        std::snprintf(key, sizeof(key), "smf_route_%02u",
                      static_cast<unsigned>(slot));
        Preferences preferences;
        if (!preferences.begin(kNamespace, false)) return false;
        const std::size_t written = preferences.putBytes(key, data, size);
        preferences.end();
        return written == size;
    }

private:
    static constexpr const char* kNamespace = "grooveputer";
};

GroovePuterMidi::SmfTrackRouteProfilePersistence& persistence() {
    static CardputerSmfTrackRouteProfileStorage storage;
    static GroovePuterMidi::SmfTrackRouteProfilePersistence instance(storage);
    return instance;
}

const char* loadStatusName(
        GroovePuterMidi::SmfTrackRouteProfileLoadStatus status) {
    using GroovePuterMidi::SmfTrackRouteProfileLoadStatus;
    switch (status) {
        case SmfTrackRouteProfileLoadStatus::Loaded: return "loaded";
        case SmfTrackRouteProfileLoadStatus::Missing: return "missing";
        case SmfTrackRouteProfileLoadStatus::Stale: return "stale";
        case SmfTrackRouteProfileLoadStatus::Corrupt: return "corrupt";
        case SmfTrackRouteProfileLoadStatus::StorageError:
            return "storage-error";
    }
    return "unknown";
}

}  // namespace

void serviceCardputerSmfRoutePersistence() {
    using namespace GroovePuterMidi;
    SmfTrackRouteProfileRuntime& runtime = smfTrackRouteProfileRuntime();

    SmfTrackRouteProfileIdentity identity{};
    uint32_t generation = 0u;
    if (runtime.takeLoadRequest(identity, generation)) {
        SmfTrackRouteProfile profile;
        const SmfTrackRouteProfileLoadStatus status =
            persistence().load(identity, profile);
        const bool applied =
            smfTrackOutputRouteState().replaceDestinations(
                profile.destinationChannels,
                identity.trackCount,
                generation);
        runtime.completeLoad(generation, applied);
        Serial.printf("[SMF-ROUTE] load=%s applied=%u tracks=%u\n",
                      loadStatusName(status),
                      static_cast<unsigned>(applied ? 1u : 0u),
                      static_cast<unsigned>(identity.trackCount));
    }

    if (runtime.takeSaveRequest(identity, generation)) {
        const SmfTrackOutputRouteSnapshot routes =
            smfTrackOutputRouteState().snapshot(identity.trackCount);
        bool saved = false;
        if (routes.generation == generation &&
            routes.trackCount == identity.trackCount) {
            SmfTrackRouteProfile profile;
            profile.reset(identity);
            for (uint16_t track = 0u; track < routes.trackCount; ++track) {
                profile.destinationChannels[track] =
                    routes.destinationFor(track);
            }
            saved = persistence().save(profile);
        }
        Serial.printf("[SMF-ROUTE] save=%u tracks=%u\n",
                      static_cast<unsigned>(saved ? 1u : 0u),
                      static_cast<unsigned>(identity.trackCount));
    }
}

}  // namespace GroovePuterPlatform

#endif  // ARDUINO
