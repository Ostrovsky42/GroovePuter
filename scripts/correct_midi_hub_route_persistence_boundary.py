#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(relative: str, old: str, new: str) -> None:
    path = ROOT / relative
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{relative}: expected one anchor, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "src/platform/cardputer_smf_player.h",
    '#include "src/midi/smf_track_route_profile.h"\n',
    '#include "src/midi/smf_track_route_profile.h"\n'
    '#include "src/midi/smf_track_route_profile_runtime.h"\n',
)
replace_once(
    "src/platform/cardputer_smf_player.h",
    """        CycleVelocityBoost,
        PersistTrackRoutes,
    };
""",
    """        CycleVelocityBoost,
    };
""",
)
replace_once(
    "src/platform/cardputer_smf_player.h",
    """    bool loadFile(const char* path);
    bool persistTrackOutputRoutesNow(uint32_t generation);
    bool scanMetadata();
""",
    """    bool loadFile(const char* path);
    bool scanMetadata();
""",
)
replace_once(
    "src/platform/cardputer_smf_player.h",
    """    char loadedPath_[kPathBytes]{};
    GroovePuterMidi::SmfTrackRouteProfileIdentity routeProfileIdentity_{};

    StaticQueue_t commandQueueStruct_{};
""",
    """    char loadedPath_[kPathBytes]{};

    StaticQueue_t commandQueueStruct_{};
""",
)

replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """#include <Preferences.h>
#include <esp_heap_caps.h>
""",
    """#include <esp_heap_caps.h>
""",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """class CardputerSmfTrackRouteProfileStorage final
    : public ISmfTrackRouteProfileStorage {
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
        char key[16]{};
        std::snprintf(key, sizeof(key), "smf_route_%02u",
                      static_cast<unsigned>(slot));
        Preferences preferences;
        if (!preferences.begin(kNamespace, true)) {
            return SmfTrackRouteProfileStorageReadStatus::Error;
        }
        if (!preferences.isKey(key)) {
            preferences.end();
            return SmfTrackRouteProfileStorageReadStatus::NotFound;
        }
        const std::size_t storedSize = preferences.getBytesLength(key);
        if (storedSize == 0u || storedSize > capacity) {
            preferences.end();
            return SmfTrackRouteProfileStorageReadStatus::Error;
        }
        const std::size_t readSize =
            preferences.getBytes(key, destination, storedSize);
        preferences.end();
        if (readSize != storedSize) {
            return SmfTrackRouteProfileStorageReadStatus::Error;
        }
        bytesRead = readSize;
        return SmfTrackRouteProfileStorageReadStatus::Ok;
    }

    bool writeSlot(std::size_t slot,
                   const uint8_t* data,
                   std::size_t size) override {
        if (slot >= kSmfTrackRouteProfileSlotCount || !data || size == 0u) {
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

CardputerSmfTrackRouteProfileStorage& smfTrackRouteProfileStorage() {
    static CardputerSmfTrackRouteProfileStorage storage;
    return storage;
}

const char* routeProfileLoadStatusName(SmfTrackRouteProfileLoadStatus status) {
    switch (status) {
        case SmfTrackRouteProfileLoadStatus::Loaded: return "loaded";
        case SmfTrackRouteProfileLoadStatus::Missing: return "missing";
        case SmfTrackRouteProfileLoadStatus::Stale: return "stale";
        case SmfTrackRouteProfileLoadStatus::Corrupt: return "corrupt";
        case SmfTrackRouteProfileLoadStatus::StorageError: return "storage-error";
    }
    return "unknown";
}
""",
    "",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """bool CardputerSmfPlayerService::persistTrackOutputRoutes(uint32_t generation) {
    if (generation == 0u) return false;
    Command command{};
    command.type = CommandType::PersistTrackRoutes;
    command.value = static_cast<int32_t>(generation);
    return enqueue(command);
}
""",
    """bool CardputerSmfPlayerService::persistTrackOutputRoutes(uint32_t generation) {
    return smfTrackRouteProfileRuntime().requestSave(generation);
}
""",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """        case CommandType::PersistTrackRoutes:
            persistTrackOutputRoutesNow(static_cast<uint32_t>(command.value));
            break;
""",
    "",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """bool CardputerSmfPlayerService::persistTrackOutputRoutesNow(
        uint32_t generation) {
    if (!loaded_ || !routeProfileIdentity_.valid() ||
        generation == 0u || generation != smfSessionGeneration()) {
        return false;
    }
    const SmfTrackOutputRouteSnapshot routes =
        smfTrackOutputRouteState().snapshot(fileIndex_.trackCount);
    if (routes.generation != generation ||
        routes.trackCount != routeProfileIdentity_.trackCount) {
        return false;
    }

    SmfTrackRouteProfile profile;
    profile.reset(routeProfileIdentity_);
    for (uint16_t track = 0u; track < routes.trackCount; ++track) {
        profile.destinationChannels[track] = routes.destinationFor(track);
    }
    SmfTrackRouteProfilePersistence persistence(
        smfTrackRouteProfileStorage());
    const bool saved = persistence.save(profile);
    Serial.printf("[SMF-ROUTE] save=%u tracks=%u\n",
                  static_cast<unsigned>(saved ? 1u : 0u),
                  static_cast<unsigned>(routes.trackCount));
    return saved;
}

""",
    "",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """    loaded_ = false;
    routeProfileIdentity_ = SmfTrackRouteProfileIdentity{};
    haveLastProjectTransport_ = false;
""",
    """    loaded_ = false;
    haveLastProjectTransport_ = false;
""",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """    routeProfileIdentity_ = routeFingerprint.identity();
    SmfTrackRouteProfile routeProfile;
    SmfTrackRouteProfilePersistence persistence(
        smfTrackRouteProfileStorage());
    const SmfTrackRouteProfileLoadStatus routeLoadStatus =
        persistence.load(routeProfileIdentity_, routeProfile);
    const uint32_t routeGeneration = smfSessionGeneration();
    if (!smfTrackOutputRouteState().replaceDestinations(
            routeProfile.destinationChannels,
            routeProfileIdentity_.trackCount,
            routeGeneration)) {
        routeProfileIdentity_ = SmfTrackRouteProfileIdentity{};
        publishSnapshot(SmfPlayerState::Error, "Route restore failed");
        source_.close();
        portENTER_CRITICAL(&snapshotMux_);
        loadedPath_[0] = '\0';
        portEXIT_CRITICAL(&snapshotMux_);
        return false;
    }
    Serial.printf("[SMF-ROUTE] load=%s tracks=%u\n",
                  routeProfileLoadStatusName(routeLoadStatus),
                  static_cast<unsigned>(routeProfileIdentity_.trackCount));

""",
    """    const SmfTrackRouteProfileIdentity routeIdentity =
        routeFingerprint.identity();
    const uint32_t routeGeneration = smfSessionGeneration();
    if (!smfTrackRouteProfileRuntime().requestLoad(
            routeIdentity, routeGeneration)) {
        publishSnapshot(SmfPlayerState::Error, "Route profile failed");
        source_.close();
        portENTER_CRITICAL(&snapshotMux_);
        loadedPath_[0] = '\0';
        portEXIT_CRITICAL(&snapshotMux_);
        return false;
    }

""",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """bool CardputerSmfPlayerService::startFromTick(uint32_t tick) {
    // Only a successful mounted USB write may clear endpoint backpressure.
""",
    """bool CardputerSmfPlayerService::startFromTick(uint32_t tick) {
    const uint32_t routeGeneration = smfSessionGeneration();
    if (!smfTrackRouteProfileRuntime().readyFor(routeGeneration)) {
        publishSnapshot(snapshot().state, "ROUTES SYNCING");
        return false;
    }

    // Only a successful mounted USB write may clear endpoint backpressure.
""",
)

replace_once(
    "src/ui/miniacid_display.cpp",
    '#include "src/platform/cardputer_ui_session.h"\n',
    '#include "src/platform/cardputer_ui_session.h"\n'
    '#include "src/platform/cardputer_smf_route_persistence.h"\n',
)
replace_once(
    "src/ui/miniacid_display.cpp",
    """void MiniAcidDisplay::servicePersistence_() {
    const unsigned long now = millis();
""",
    """void MiniAcidDisplay::servicePersistence_() {
    GroovePuterPlatform::serviceCardputerSmfRoutePersistence();
    const unsigned long now = millis();
""",
)
replace_once(
    "tests/run_host_tests.sh",
    """"${BUILD_DIR}/test_smf_track_route_profile"

"${CXX}" \
""",
    """"${BUILD_DIR}/test_smf_track_route_profile"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_track_route_profile_runtime.cpp" \
  -o "${BUILD_DIR}/test_smf_track_route_profile_runtime"

"${BUILD_DIR}/test_smf_track_route_profile_runtime"

"${CXX}" \
""",
)
