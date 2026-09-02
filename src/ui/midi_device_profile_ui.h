#pragma once

#include <cstdint>

#include "src/midi/midi_companion_settings.h"

namespace GroovePuterUi {
namespace MidiDeviceProfileUi {

using GroovePuterMidi::MidiDeviceProfile;

constexpr uint8_t kUnsetPreview = 0xFF;

inline bool isKnownProfile(MidiDeviceProfile profile) {
    switch (profile) {
        case MidiDeviceProfile::SeqtrakNative:
        case MidiDeviceProfile::GeneralMidi:
        case MidiDeviceProfile::Custom:
        case MidiDeviceProfile::GenericMidi:
            return true;
    }
    return false;
}

inline const char* shortName(MidiDeviceProfile profile) {
    switch (profile) {
        case MidiDeviceProfile::SeqtrakNative: return "SEQTRAK";
        case MidiDeviceProfile::GeneralMidi: return "GM";
        case MidiDeviceProfile::GenericMidi: return "GENERIC";
        case MidiDeviceProfile::Custom: return "CUSTOM";
    }
    return "SEQTRAK";
}

inline uint8_t encodePreview(MidiDeviceProfile profile) {
    return static_cast<uint8_t>(profile);
}

inline MidiDeviceProfile profileFromPreview(
        uint8_t raw,
        MidiDeviceProfile fallback) {
    const auto profile = static_cast<MidiDeviceProfile>(raw);
    return isKnownProfile(profile) ? profile : fallback;
}

inline MidiDeviceProfile stepSelectableProfile(
        MidiDeviceProfile current,
        int delta) {
    constexpr MidiDeviceProfile kSelectable[] = {
        MidiDeviceProfile::SeqtrakNative,
        MidiDeviceProfile::GeneralMidi,
        MidiDeviceProfile::GenericMidi,
    };
    constexpr int kCount = static_cast<int>(sizeof(kSelectable) /
                                             sizeof(kSelectable[0]));

    int index = -1;
    for (int i = 0; i < kCount; ++i) {
        if (kSelectable[i] == current) {
            index = i;
            break;
        }
    }

    // CUSTOM is display-only until a dedicated custom route editor exists.
    // Leaving it with an arrow moves into the supported preset cycle without
    // ever advertising CUSTOM as a selectable preset.
    if (index < 0) return delta < 0 ? kSelectable[kCount - 1] : kSelectable[0];

    const int direction = delta < 0 ? -1 : 1;
    index = (index + direction + kCount) % kCount;
    return kSelectable[index];
}

}  // namespace MidiDeviceProfileUi
}  // namespace GroovePuterUi
