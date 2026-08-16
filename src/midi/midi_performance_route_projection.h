#pragma once
#ifndef GROOVEPUTER_MIDI_PERFORMANCE_ROUTE_PROJECTION_H
#define GROOVEPUTER_MIDI_PERFORMANCE_ROUTE_PROJECTION_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "midi_device_capabilities.h"
#include "midi_output_route_projection.h"

namespace GroovePuterMidi {

constexpr std::size_t kPerformanceDrumLaneCount = 7;

struct MidiPerformanceRouteProjection {
    bool complete{false};
    bool enabled{false};
    MidiMelodicWireRoute synthA{};
    MidiMelodicWireRoute synthB{};
    MidiMelodicWireRoute dx{};
    std::array<DrumMidiRoute, kPerformanceDrumLaneCount> drums{};
    MidiReceiverModeControl receiverModeControl{MidiReceiverModeControl::None};
};

inline MidiPerformanceRouteProjection projectMidiPerformanceRoutes(
        const MidiOutputSettings& input) {
    MidiOutputSettings settings = input;
    sanitizeMidiOutputSettings(settings);

    MidiPerformanceRouteProjection projection{};
    const MidiDeviceCapabilities capabilities =
        midiDeviceCapabilitiesForProfile(settings.profile);
    projection.receiverModeControl = capabilities.receiverModeControl;
    projection.enabled = settings.enabled && settings.liveEnabled;

    // The persisted model still has no full user-defined Performance target
    // table. Custom therefore remains an explicit legacy fallback rather than
    // inventing DX/drum routes from unrelated Pattern fields.
    if (settings.profile == MidiDeviceProfile::Custom) {
        projection.complete = false;
        return projection;
    }

    projection.complete = true;
    switch (settings.profile) {
        case MidiDeviceProfile::SeqtrakNative:
            projection.synthA = {projection.enabled, 7};
            projection.synthB = {projection.enabled, 8};
            projection.dx = {projection.enabled, 9};
            for (uint8_t lane = 0; lane < kPerformanceDrumLaneCount; ++lane) {
                projection.drums[lane] = DrumMidiRoute{
                    projection.enabled && settings.drumsEnabled,
                    lane,
                    60,
                };
            }
            break;
        case MidiDeviceProfile::GeneralMidi: {
            projection.synthA = {projection.enabled, 0};
            projection.synthB = {projection.enabled, 1};
            projection.dx = {projection.enabled, 2};
            constexpr uint8_t kNotes[kPerformanceDrumLaneCount] = {
                36, 38, 42, 46, 43, 47, 37,
            };
            for (uint8_t lane = 0; lane < kPerformanceDrumLaneCount; ++lane) {
                projection.drums[lane] = DrumMidiRoute{
                    projection.enabled && settings.drumsEnabled,
                    9,
                    kNotes[lane],
                };
            }
            break;
        }
        case MidiDeviceProfile::GenericMidi:
            projection.synthA = {projection.enabled, 0};
            projection.synthB = {projection.enabled, 1};
            projection.dx = {projection.enabled, 2};
            for (DrumMidiRoute& route : projection.drums) {
                route = DrumMidiRoute{false, 0, 60};
            }
            break;
        case MidiDeviceProfile::Custom:
            break;
    }
    return projection;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_PERFORMANCE_ROUTE_PROJECTION_H
