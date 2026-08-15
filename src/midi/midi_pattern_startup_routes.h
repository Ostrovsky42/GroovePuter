#pragma once
#ifndef GROOVEPUTER_MIDI_PATTERN_STARTUP_ROUTES_H
#define GROOVEPUTER_MIDI_PATTERN_STARTUP_ROUTES_H

#include <array>
#include <cstdint>

#include "midi_output_route_projection.h"

namespace GroovePuterMidi {

// Minimal derived snapshot consumed once by UsbMidiOutput::begin(). It is not a
// second settings owner: the authoritative state remains MidiDeviceProfileRuntime.
// Publishing a new value later does not mutate an already-started USB output.
struct MidiPatternStartupRoutes {
    bool synthAEnabled{true};
    uint8_t synthAChannel{7};
    bool synthBEnabled{true};
    uint8_t synthBChannel{8};
    std::array<DrumMidiRoute, kMidiDrumVoiceCount> drums{};
};

class MidiPatternStartupRouteRuntime {
public:
    void publish(const MidiOutputRouteProjection& projection) {
        routes_.synthAEnabled = projection.patternSynthA.enabled;
        routes_.synthAChannel = projection.patternSynthA.channel;
        routes_.synthBEnabled = projection.patternSynthB.enabled;
        routes_.synthBChannel = projection.patternSynthB.channel;
        routes_.drums = projection.patternDrums;
        published_ = true;
        ++revision_;
    }

    bool snapshot(MidiPatternStartupRoutes& routes) const {
        if (!published_) return false;
        routes = routes_;
        return true;
    }

    bool published() const { return published_; }
    uint32_t revision() const { return revision_; }

private:
    MidiPatternStartupRoutes routes_{};
    uint32_t revision_{0};
    bool published_{false};
};

inline MidiPatternStartupRouteRuntime& midiPatternStartupRouteRuntime() {
    static MidiPatternStartupRouteRuntime runtime;
    return runtime;
}

inline void publishMidiPatternStartupRoutes(const MidiOutputSettings& settings) {
    midiPatternStartupRouteRuntime().publish(projectMidiOutputRoutes(settings));
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_PATTERN_STARTUP_ROUTES_H
