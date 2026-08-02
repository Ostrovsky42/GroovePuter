#pragma once

namespace GroovePuterPlatform {

// Implemented in the Arduino-only platform translation unit. Desktop builds
// deliberately keep this as a no-op so persistence does not pull NVS or the
// MIDI settings codec into the SDL link.
void initializeCardputerMidiSettingsSession();

// A lightweight member of the root UI object. On Cardputer it restores the
// versioned MIDI settings record before user interaction. On desktop it has no
// platform dependency and performs no work.
class CardputerMidiSettingsBinding {
public:
    CardputerMidiSettingsBinding() {
#ifdef ARDUINO
        initializeCardputerMidiSettingsSession();
#endif
    }
};

}  // namespace GroovePuterPlatform
