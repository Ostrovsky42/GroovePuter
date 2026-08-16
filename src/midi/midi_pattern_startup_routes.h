#pragma once
#ifndef GROOVEPUTER_MIDI_PATTERN_STARTUP_ROUTES_H
#define GROOVEPUTER_MIDI_PATTERN_STARTUP_ROUTES_H

#include <array>
#include <cstdint>

#include "midi_output_route_projection.h"
#include "midi_performance_route_projection.h"

namespace GroovePuterMidi {

// Minimal derived snapshot consumed once by UsbMidiOutput::begin(). It is not a
// second settings owner: the authoritative state remains MidiDeviceProfileRuntime.
// Publishing a new value later does not mutate an already-started USB output.
//
// The historical R5 name is retained to keep the stacked contract stable. R6
// extends the same single snapshot with predefined Performance routes instead
// of allocating a second control-side startup owner.
struct MidiPatternStartupRoutes {
    bool synthAEnabled{true};
    uint8_t synthAChannel{7};
    bool synthBEnabled{true};
    uint8_t synthBChannel{8};
    std::array<DrumMidiRoute, kMidiDrumVoiceCount> drums{};

    bool performanceRoutesComplete{false};
    MidiMelodicWireRoute performanceSynthA{true, 7};
    MidiMelodicWireRoute performanceSynthB{true, 8};
    MidiMelodicWireRoute performanceDx{true, 9};
    std::array<DrumMidiRoute, kPerformanceDrumLaneCount> performanceDrums{};
    MidiReceiverModeControl receiverModeControl{
        MidiReceiverModeControl::SeqtrakCc26};
};

class MidiPatternStartupRouteRuntime {
public:
    void publish(const MidiOutputRouteProjection& pattern,
                 const MidiPerformanceRouteProjection& performance) {
        routes_.synthAEnabled = pattern.patternSynthA.enabled;
        routes_.synthAChannel = pattern.patternSynthA.channel;
        routes_.synthBEnabled = pattern.patternSynthB.enabled;
        routes_.synthBChannel = pattern.patternSynthB.channel;
        routes_.drums = pattern.patternDrums;
        routes_.performanceRoutesComplete = performance.complete;
        routes_.performanceSynthA = performance.synthA;
        routes_.performanceSynthB = performance.synthB;
        routes_.performanceDx = performance.dx;
        routes_.performanceDrums = performance.drums;
        routes_.receiverModeControl = performance.receiverModeControl;
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
    midiPatternStartupRouteRuntime().publish(
        projectMidiOutputRoutes(settings),
        projectMidiPerformanceRoutes(settings));
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_PATTERN_STARTUP_ROUTES_H
