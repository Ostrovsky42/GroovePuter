#pragma once
#ifndef GROOVEPUTER_CARDPUTER_MIDI_SETTINGS_SESSION_H
#define GROOVEPUTER_CARDPUTER_MIDI_SETTINGS_SESSION_H

#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_device_profile_runtime.h"
#include "src/midi/midi_input_dispatcher.h"

namespace GroovePuterPlatform {

// Implemented in the Arduino-only platform translation unit. Desktop builds
// deliberately keep persistence out of the SDL link.
void initializeCardputerMidiSettingsSession();

#ifdef ARDUINO
void initializeCardputerMidiInputSettings();
GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRoutingConfig();
bool setCardputerMidiInputRoutingConfig(
    const GroovePuterMidi::MidiInputRoutingConfig& config);

GroovePuterMidi::MidiDeviceProfile pendingCardputerMidiDeviceProfile();
bool selectCardputerMidiDeviceProfileForNextBoot(
    GroovePuterMidi::MidiDeviceProfile profile);
bool pendingCardputerMidiOutputEnabled();
bool selectCardputerMidiOutputEnabledForNextBoot(bool enabled);
bool cardputerMidiSettingsRestartRequired();
#else
inline GroovePuterMidi::MidiInputRoutingConfig& desktopMidiInputRoutingConfig() {
    static GroovePuterMidi::MidiInputRoutingConfig config{};
    return config;
}
inline void initializeCardputerMidiInputSettings() {}
inline GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRoutingConfig() {
    return desktopMidiInputRoutingConfig();
}
inline bool setCardputerMidiInputRoutingConfig(
        const GroovePuterMidi::MidiInputRoutingConfig& config) {
    if (!GroovePuterMidi::MidiInputDispatcher::isValidConfig(config)) return false;
    desktopMidiInputRoutingConfig() = config;
    return true;
}

namespace Detail {
struct DesktopMidiProfileSelection {
    bool initialized{false};
    GroovePuterMidi::MidiDeviceProfile profile{
        GroovePuterMidi::MidiDeviceProfile::SeqtrakNative};
    bool outputEnabled{true};
};

inline DesktopMidiProfileSelection& desktopMidiProfileSelection() {
    static DesktopMidiProfileSelection selection;
    if (!selection.initialized) {
        selection.profile = GroovePuterMidi::midiDeviceProfileRuntime().profile();
        selection.outputEnabled =
            GroovePuterMidi::midiDeviceProfileRuntime().settings().enabled;
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

inline bool pendingCardputerMidiOutputEnabled() {
    return Detail::desktopMidiProfileSelection().outputEnabled;
}

inline bool selectCardputerMidiOutputEnabledForNextBoot(bool enabled) {
    Detail::desktopMidiProfileSelection().outputEnabled = enabled;
    return true;
}

inline bool cardputerMidiSettingsRestartRequired() {
    const GroovePuterMidi::MidiDeviceProfileRuntime& runtime =
        GroovePuterMidi::midiDeviceProfileRuntime();
    return pendingCardputerMidiDeviceProfile() != runtime.profile() ||
           pendingCardputerMidiOutputEnabled() != runtime.settings().enabled;
}
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
