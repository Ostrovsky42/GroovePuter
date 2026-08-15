#pragma once
#ifndef GROOVEPUTER_CARDPUTER_MIDI_SETTINGS_SESSION_H
#define GROOVEPUTER_CARDPUTER_MIDI_SETTINGS_SESSION_H

namespace GroovePuterPlatform {

// Implemented in the Arduino-only platform translation unit. Desktop builds
// deliberately keep this as a no-op so persistence does not pull NVS or the
// MIDI settings codec into the SDL link.
void initializeCardputerMidiSettingsSession();

// A lightweight member of the root UI object. On Cardputer it may call the
// idempotent initializer for compatibility, but boot now restores persisted
// MIDI settings explicitly before the USB dispatcher starts.
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
