#pragma once
#ifndef GROOVEPUTER_MIDI_OUTPUT_ROUTE_PROJECTION_H
#define GROOVEPUTER_MIDI_OUTPUT_ROUTE_PROJECTION_H

#include <array>
#include <cstdint>

#include "midi_companion_settings.h"
#include "midi_device_capabilities.h"

namespace GroovePuterMidi {

struct MidiMelodicWireRoute {
    bool enabled{false};
    uint8_t channel{0};
};

struct MidiLiveRouteProjection {
    bool enabled{false};
    MidiLiveTarget target{MidiLiveTarget::SynthA};
    uint8_t channel{0};
    bool usesPerVoiceDrumRoutes{false};
};

struct MidiOutputRouteProjection {
    MidiDeviceProfile profile{MidiDeviceProfile::Custom};
    MidiLiveRouteProjection live{};
    MidiMelodicWireRoute patternSynthA{};
    MidiMelodicWireRoute patternSynthB{};
    std::array<DrumMidiRoute, kMidiDrumVoiceCount> patternDrums{};
    uint16_t drumGateMs{kDefaultDrumGateMs};
    MidiReceiverModeControl receiverModeControl{MidiReceiverModeControl::None};

    // The current MidiOutputSettings model owns one selected live target/channel,
    // not the full PerformanceKeyboard A/B/DX/Drums target table. Keep this
    // limitation explicit so R4 cannot be mistaken for complete USB binding.
    bool completePerformanceTargetTable{false};
};

inline MidiOutputRouteProjection projectMidiOutputRoutes(
        const MidiOutputSettings& input) {
    MidiOutputSettings settings = input;
    sanitizeMidiOutputSettings(settings);
    const MidiDeviceCapabilities capabilities =
        midiDeviceCapabilitiesForProfile(settings.profile);

    MidiOutputRouteProjection projection{};
    projection.profile = settings.profile;
    projection.live.enabled = settings.enabled && settings.liveEnabled;
    projection.live.target = settings.liveTarget;
    projection.live.channel = settings.liveChannel;
    projection.live.usesPerVoiceDrumRoutes =
        settings.liveTarget == MidiLiveTarget::Drums;

    projection.patternSynthA.enabled =
        settings.enabled && settings.patternSynthAEnabled;
    projection.patternSynthA.channel = settings.synthAChannel;
    projection.patternSynthB.enabled =
        settings.enabled && settings.patternSynthBEnabled;
    projection.patternSynthB.channel = settings.synthBChannel;

    projection.patternDrums = settings.drumRoutes;
    for (DrumMidiRoute& route : projection.patternDrums) {
        route.enabled = settings.enabled && settings.drumsEnabled && route.enabled;
    }
    projection.drumGateMs = settings.drumGateMs;
    projection.receiverModeControl = capabilities.receiverModeControl;
    return projection;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_OUTPUT_ROUTE_PROJECTION_H
