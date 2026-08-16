#pragma once
#ifndef GROOVEPUTER_MIDI_DEVICE_CAPABILITIES_H
#define GROOVEPUTER_MIDI_DEVICE_CAPABILITIES_H

#include <cstdint>

#include "midi_companion_settings.h"
#include "midi_transport_capabilities.h"

namespace GroovePuterMidi {

enum class MidiDrumMappingKind : uint8_t {
    None = 0,
    SeqtrakNative,
    GeneralMidiPercussion,
    UserDefined,
};

enum class MidiReceiverModeControl : uint8_t {
    None = 0,
    SeqtrakCc26,
};

struct MidiDeviceCapabilities {
    MidiDeviceProfile profile{MidiDeviceProfile::Custom};
    MidiDrumMappingKind drumMapping{MidiDrumMappingKind::UserDefined};
    MidiTransportCapabilities transport{};
    MidiReceiverModeControl receiverModeControl{MidiReceiverModeControl::None};
    bool fixedSynthChannelDefaults{false};
    bool vendorSpecificControls{false};
};

constexpr MidiDeviceCapabilities midiDeviceCapabilitiesForProfile(
        MidiDeviceProfile profile) {
    MidiDeviceCapabilities capabilities{};
    capabilities.profile = profile;
    capabilities.transport = midiTransportCapabilitiesForProfile(profile);

    switch (profile) {
        case MidiDeviceProfile::SeqtrakNative:
            capabilities.drumMapping = MidiDrumMappingKind::SeqtrakNative;
            capabilities.receiverModeControl = MidiReceiverModeControl::SeqtrakCc26;
            capabilities.fixedSynthChannelDefaults = true;
            capabilities.vendorSpecificControls = true;
            return capabilities;
        case MidiDeviceProfile::GeneralMidi:
            capabilities.drumMapping = MidiDrumMappingKind::GeneralMidiPercussion;
            capabilities.fixedSynthChannelDefaults = true;
            return capabilities;
        case MidiDeviceProfile::GenericMidi:
            capabilities.drumMapping = MidiDrumMappingKind::None;
            return capabilities;
        case MidiDeviceProfile::Custom:
            capabilities.drumMapping = MidiDrumMappingKind::UserDefined;
            return capabilities;
    }

    capabilities.profile = MidiDeviceProfile::Custom;
    capabilities.drumMapping = MidiDrumMappingKind::UserDefined;
    capabilities.transport = conservativeCustomTransportCapabilities();
    return capabilities;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_DEVICE_CAPABILITIES_H
