#pragma once
#ifndef GROOVEPUTER_SMF_TRACK_ROUTE_PROFILE_RUNTIME_H
#define GROOVEPUTER_SMF_TRACK_ROUTE_PROFILE_RUNTIME_H

#include <cstdint>

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#else
#include <mutex>
#endif

#include "smf_track_route_profile.h"

namespace GroovePuterMidi {

class SmfTrackRouteProfileRuntime {
public:
    bool requestLoad(const SmfTrackRouteProfileIdentity& identity,
                     uint32_t generation) {
        if (!identity.valid() || generation == 0u) return false;
        Guard guard(*this);
        identity_ = identity;
        identityGeneration_ = generation;
        readyGeneration_ = 0u;
        loadPending_ = true;
        savePending_ = false;
        return true;
    }

    bool takeLoadRequest(SmfTrackRouteProfileIdentity& identity,
                         uint32_t& generation) {
        Guard guard(*this);
        if (!loadPending_) return false;
        identity = identity_;
        generation = identityGeneration_;
        loadPending_ = false;
        return true;
    }

    void completeLoad(uint32_t generation, bool applied) {
        Guard guard(*this);
        if (identityGeneration_ != generation) return;
        readyGeneration_ = applied ? generation : 0u;
    }

    bool readyFor(uint32_t generation) const {
        Guard guard(*this);
        return generation != 0u && readyGeneration_ == generation;
    }

    bool requestSave(uint32_t generation) {
        Guard guard(*this);
        if (generation == 0u || identityGeneration_ != generation ||
            readyGeneration_ != generation) {
            return false;
        }
        savePending_ = true;
        return true;
    }

    bool takeSaveRequest(SmfTrackRouteProfileIdentity& identity,
                         uint32_t& generation) {
        Guard guard(*this);
        if (!savePending_ || readyGeneration_ == 0u ||
            readyGeneration_ != identityGeneration_) {
            return false;
        }
        identity = identity_;
        generation = identityGeneration_;
        savePending_ = false;
        return true;
    }

private:
    class Guard {
    public:
        explicit Guard(const SmfTrackRouteProfileRuntime& runtime)
            : runtime_(runtime) {
#if defined(ESP32) || defined(ESP_PLATFORM)
            portENTER_CRITICAL(&runtime_.lock_);
#else
            runtime_.lock_.lock();
#endif
        }
        ~Guard() {
#if defined(ESP32) || defined(ESP_PLATFORM)
            portEXIT_CRITICAL(&runtime_.lock_);
#else
            runtime_.lock_.unlock();
#endif
        }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

    private:
        const SmfTrackRouteProfileRuntime& runtime_;
    };

#if defined(ESP32) || defined(ESP_PLATFORM)
    mutable portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
#else
    mutable std::mutex lock_;
#endif
    SmfTrackRouteProfileIdentity identity_{};
    uint32_t identityGeneration_{0u};
    uint32_t readyGeneration_{0u};
    bool loadPending_{false};
    bool savePending_{false};
};

inline SmfTrackRouteProfileRuntime& smfTrackRouteProfileRuntime() {
    static SmfTrackRouteProfileRuntime runtime;
    return runtime;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_TRACK_ROUTE_PROFILE_RUNTIME_H
