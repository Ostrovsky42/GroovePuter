#pragma once
#ifndef GROOVEPUTER_CARDPUTER_MIDI_SETTINGS_SESSION_H
#define GROOVEPUTER_CARDPUTER_MIDI_SETTINGS_SESSION_H

#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_device_profile_runtime.h"
#include "src/input/midi_input_router.h"

namespace GroovePuterPlatform {

// Implemented in the Arduino-only platform translation unit. Desktop builds
// deliberately keep persistence out of the SDL link.
void initializeCardputerMidiSettingsSession();

#ifdef ARDUINO
GroovePuterMidi::MidiDeviceProfile pendingCardputerMidiDeviceProfile();
bool selectCardputerMidiDeviceProfileForNextBoot(
    GroovePuterMidi::MidiDeviceProfile profile);
bool cardputerMidiDeviceProfileRestartRequired();
void initializeCardputerMidiInputSettings(MidiInputRouter& router);
MidiInputRoutingConfig cardputerMidiInputRoutingConfig();
bool setCardputerMidiInputRoutingConfig(const MidiInputRoutingConfig& config);
#else
namespace Detail {
struct DesktopMidiProfileSelection {
    bool initialized{false};
    GroovePuterMidi::MidiDeviceProfile profile{
        GroovePuterMidi::MidiDeviceProfile::SeqtrakNative};
};

inline DesktopMidiProfileSelection& desktopMidiProfileSelection() {
    static DesktopMidiProfileSelection selection;
    if (!selection.initialized) {
        selection.profile = GroovePuterMidi::midiDeviceProfileRuntime().profile();
        selection.initialized = true;
    }
    return selection;
}

inline bool validSelectableProfile(GroovePuterMidi::MidiDeviceProfile profile) {
    switch (profile) {
        case GroovePuterMidi::MidiDeviceProfile::SeqtrakNative:
        case GroovePuterMidi::MidiDeviceProfile::GeneralMidi:
        case GroovePuterMidi::MidiDeviceProfile::Custom:
        case GroovePuterMidi::MidiDeviceProfile::GenericMidi:
            return true;
    }
    return false;
}
}  // namespace Detail

inline GroovePuterMidi::MidiDeviceProfile pendingCardputerMidiDeviceProfile() {
    return Detail::desktopMidiProfileSelection().profile;
}

inline bool selectCardputerMidiDeviceProfileForNextBoot(
        GroovePuterMidi::MidiDeviceProfile profile) {
    if (!Detail::validSelectableProfile(profile)) return false;
    Detail::desktopMidiProfileSelection().profile = profile;
    return true;
}

inline bool cardputerMidiDeviceProfileRestartRequired() {
    return pendingCardputerMidiDeviceProfile() !=
           GroovePuterMidi::midiDeviceProfileRuntime().profile();
}
inline MidiInputRoutingConfig& desktopMidiInputConfig() { static MidiInputRoutingConfig c{}; return c; }
inline void initializeCardputerMidiInputSettings(MidiInputRouter& router) { (void)router.setConfig(desktopMidiInputConfig()); }
inline MidiInputRoutingConfig cardputerMidiInputRoutingConfig() { return desktopMidiInputConfig(); }
inline bool setCardputerMidiInputRoutingConfig(const MidiInputRoutingConfig& config) { if (!MidiInputRouter::isValidConfig(config)) return false; desktopMidiInputConfig()=config; return true; }
#endif

// A lightweight member of the root UI object. On Cardputer it may call the
// idempotent initializer for compatibility, but boot restores persisted MIDI
// settings explicitly before the USB dispatcher starts.
class CardputerMidiSettingsBinding {
public:
    CardputerMidiSettingsBinding() {
#ifdef ARDUINO
        initializeCardputerMidiSettingsSession();
#endif
    }
};

}  // namespace GroovePuterPlatform

#endif  // GROOVEPUTER_CARDPUTER_MIDI_SETTINGS_SESSION_H
