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
    "src/midi/smf_track_output_route.h",
    """    int8_t destinationFor(uint16_t trackIndex, uint16_t trackCountHint) {
""",
    """    bool replaceDestinations(const int8_t* destinations,
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
            packed[word] |= static_cast<uint32_t>(encode(destination)) << shift;
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
        revision_.fetch_add(1u, std::memory_order_acq_rel);
        return true;
    }

    int8_t destinationFor(uint16_t trackIndex, uint16_t trackCountHint) {
""",
)

replace_once(
    "src/midi/smf_player_service.h",
    """    virtual bool cycleVelocityBoost() = 0;
    virtual SmfPlayerSnapshot snapshot() const = 0;
""",
    """    virtual bool cycleVelocityBoost() = 0;
    virtual bool persistTrackOutputRoutes(uint32_t generation) {
        (void)generation;
        return false;
    }
    virtual SmfPlayerSnapshot snapshot() const = 0;
""",
)

replace_once(
    "src/platform/cardputer_smf_player.h",
    """#include "src/midi/smf_track_output_route.h"
""",
    """#include "src/midi/smf_track_output_route.h"
#include "src/midi/smf_track_route_profile.h"
""",
)
replace_once(
    "src/platform/cardputer_smf_player.h",
    """    bool cycleVelocityBoost() override;
    GroovePuterMidi::SmfPlayerSnapshot snapshot() const override;
""",
    """    bool cycleVelocityBoost() override;
    bool persistTrackOutputRoutes(uint32_t generation) override;
    GroovePuterMidi::SmfPlayerSnapshot snapshot() const override;
""",
)
replace_once(
    "src/platform/cardputer_smf_player.h",
    """        CycleVelocityBoost,
    };
""",
    """        CycleVelocityBoost,
        PersistTrackRoutes,
    };
""",
)
replace_once(
    "src/platform/cardputer_smf_player.h",
    """    bool loadFile(const char* path);
    bool scanMetadata();
""",
    """    bool loadFile(const char* path);
    bool persistTrackOutputRoutesNow(uint32_t generation);
    bool scanMetadata();
""",
)
replace_once(
    "src/platform/cardputer_smf_player.h",
    """    char loadedPath_[kPathBytes]{};

    StaticQueue_t commandQueueStruct_{};
""",
    """    char loadedPath_[kPathBytes]{};
    GroovePuterMidi::SmfTrackRouteProfileIdentity routeProfileIdentity_{};

    StaticQueue_t commandQueueStruct_{};
""",
)

replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """#include <new>

#include <esp_heap_caps.h>
""",
    """#include <new>

#include <Preferences.h>
#include <esp_heap_caps.h>
""",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """bool followsSeqtrakClock() {
    return transportClockRuntime().source() ==
        TransportClockSource::SeqtrakExternal;
}
}
""",
    """bool followsSeqtrakClock() {
    return transportClockRuntime().source() ==
        TransportClockSource::SeqtrakExternal;
}

class CardputerSmfTrackRouteProfileStorage final
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
}
""",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """bool CardputerSmfPlayerService::cycleVelocityBoost() {
    Command command{};
    command.type = CommandType::CycleVelocityBoost;
    return enqueue(command);
}

SmfPlayerSnapshot CardputerSmfPlayerService::snapshot() const {
""",
    """bool CardputerSmfPlayerService::cycleVelocityBoost() {
    Command command{};
    command.type = CommandType::CycleVelocityBoost;
    return enqueue(command);
}

bool CardputerSmfPlayerService::persistTrackOutputRoutes(uint32_t generation) {
    if (generation == 0u) return false;
    Command command{};
    command.type = CommandType::PersistTrackRoutes;
    command.value = static_cast<int32_t>(generation);
    return enqueue(command);
}

SmfPlayerSnapshot CardputerSmfPlayerService::snapshot() const {
""",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """        case CommandType::CycleVelocityBoost:
            velocityBoost_ = nextSmfVelocityBoost(velocityBoost_);
            portENTER_CRITICAL(&snapshotMux_);
            snapshot_.velocityBoost = velocityBoost_;
            portEXIT_CRITICAL(&snapshotMux_);
            break;
    }
}

bool CardputerSmfPlayerService::loadFile(const char* path) {
""",
    """        case CommandType::CycleVelocityBoost:
            velocityBoost_ = nextSmfVelocityBoost(velocityBoost_);
            portENTER_CRITICAL(&snapshotMux_);
            snapshot_.velocityBoost = velocityBoost_;
            portEXIT_CRITICAL(&snapshotMux_);
            break;
        case CommandType::PersistTrackRoutes:
            persistTrackOutputRoutesNow(static_cast<uint32_t>(command.value));
            break;
    }
}

bool CardputerSmfPlayerService::persistTrackOutputRoutesNow(
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

bool CardputerSmfPlayerService::loadFile(const char* path) {
""",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """    loaded_ = false;
    haveLastProjectTransport_ = false;
""",
    """    loaded_ = false;
    routeProfileIdentity_ = SmfTrackRouteProfileIdentity{};
    haveLastProjectTransport_ = false;
""",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """    fileIndex_ = indexed.index;
    SmfChannelInspectorBuilder inspectorBuilder;
""",
    """    fileIndex_ = indexed.index;
    SmfTrackRouteFingerprint routeFingerprint(
        path, source_.size(), fileIndex_);
    SmfChannelInspectorBuilder inspectorBuilder;
""",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """    while (stream_.next(event)) {
        inspectorBuilder.observe(event.event);
""",
    """    while (stream_.next(event)) {
        routeFingerprint.observe(event);
        inspectorBuilder.observe(event.event);
""",
)
replace_once(
    "src/platform/cardputer_smf_player.cpp",
    """    portENTER_CRITICAL(&snapshotMux_);
    channelInspector_ = inspectorBuilder.snapshot();
    portEXIT_CRITICAL(&snapshotMux_);

    stream_.reset();
""",
    """    portENTER_CRITICAL(&snapshotMux_);
    channelInspector_ = inspectorBuilder.snapshot();
    portEXIT_CRITICAL(&snapshotMux_);

    routeProfileIdentity_ = routeFingerprint.identity();
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

    stream_.reset();
""",
)

replace_once(
    "src/ui/pages/sequencer_hub_page_midi.cpp",
    """            if (smfTrackOutputRouteState().setDestination(
                    selectedTrack,
                    midiRouteDraft_,
                    projection.generation,
                    projection.mute.trackCount)) {
                char destination[20]{};
                char toast[40]{};
""",
    """            if (smfTrackOutputRouteState().setDestination(
                    selectedTrack,
                    midiRouteDraft_,
                    projection.generation,
                    projection.mute.trackCount)) {
                const bool saveQueued = service &&
                    service->persistTrackOutputRoutes(projection.generation);
                char destination[20]{};
                char toast[40]{};
""",
)
replace_once(
    "src/ui/pages/sequencer_hub_page_midi.cpp",
    """                std::snprintf(toast,
                              sizeof(toast),
                              "TRK %02u > %s",
                              static_cast<unsigned>(selectedTrack + 1u),
                              destination);
                UI::showToast(toast, 800);
""",
    """                std::snprintf(toast,
                              sizeof(toast),
                              saveQueued
                                  ? "TRK %02u > %s"
                                  : "TRK %02u > %s / SAVE BUSY",
                              static_cast<unsigned>(selectedTrack + 1u),
                              destination);
                UI::showToast(toast, saveQueued ? 800 : 1100);
""",
)

replace_once(
    "tests/run_host_tests.sh",
    """python3 "${ROOT_DIR}/tests/test_seqtrak_master_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_smf_midi_wave_source_regressions.py"
""",
    """python3 "${ROOT_DIR}/tests/test_seqtrak_master_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_smf_midi_wave_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_smf_route_persistence_source_regressions.py"
""",
)
replace_once(
    "tests/run_host_tests.sh",
    """"${BUILD_DIR}/test_smf_routing"

"${CXX}" \
""",
    """"${BUILD_DIR}/test_smf_routing"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_track_route_profile.cpp" \
  "${ROOT_DIR}/src/midi/smf_track_route_profile.cpp" \
  -o "${BUILD_DIR}/test_smf_track_route_profile"

"${BUILD_DIR}/test_smf_track_route_profile"

"${CXX}" \
""",
)

replace_once(
    "tests/test_smf_routing.cpp",
    """    assert(routeSnapshot.destinationFor(2) == 7);
    assert(routeSnapshot.overridden(2));

    const uint32_t nextGeneration = smfBeginSessionOpen();
""",
    """    assert(routeSnapshot.destinationFor(2) == 7);
    assert(routeSnapshot.overridden(2));

    int8_t restored[4] = {
        kSmfTrackOutputRouteAuto, 8, 9, kSmfTrackOutputRouteAuto};
    assert(routes.replaceDestinations(restored, 4, generation));
    routeSnapshot = routes.snapshot(4);
    assert(routeSnapshot.destinationFor(0) == kSmfTrackOutputRouteAuto);
    assert(routeSnapshot.destinationFor(1) == 8);
    assert(routeSnapshot.destinationFor(2) == 9);
    assert(routeSnapshot.destinationFor(3) == kSmfTrackOutputRouteAuto);

    restored[2] = 10;
    assert(!routes.replaceDestinations(restored, 4, generation));
    assert(routes.snapshot(4).destinationFor(2) == 9);

    const uint32_t nextGeneration = smfBeginSessionOpen();
""",
)
