#pragma once

namespace GroovePuterUi {
namespace MidiOutputModeUi {

// Internal audio is always available. This label describes whether the
// persisted external MIDI output routes are also enabled.
inline const char* outputModeName(bool externalMidiEnabled) {
    return externalMidiEnabled ? "INT + MIDI" : "INTERNAL";
}

}  // namespace MidiOutputModeUi
}  // namespace GroovePuterUi
