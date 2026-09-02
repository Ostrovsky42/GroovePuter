#pragma once
#ifndef GROOVEPUTER_MIDI_DEVICE_PROFILE_RUNTIME_H
#define GROOVEPUTER_MIDI_DEVICE_PROFILE_RUNTIME_H

#include <cstdint>

#include "midi_companion_settings.h"
#include "midi_transport_capabilities.h"

namespace GroovePuterMidi {

// Control-side owner for the currently applied device profile/settings snapshot.
// Construction is deliberately trivial; Cardputer boot initializes it after
// persistence load. R3 does not bind this snapshot to USB note lanes yet.
class MidiDeviceProfileRuntime {
public:
    void initialize(const MidiOutputSettings& input) {
        settings_ = input;
        sanitizeMidiOutputSettings(settings_);
        revision_ = 0;
        initialized_ = true;
        midiTransportCapabilityRuntime().setDeviceProfile(settings_.profile);
    }

    // Applies device-owned defaults transactionally to a copy, then commits the
    // complete settings snapshot and transport capability identity together.
    // Returns true only when the committed settings changed.
    bool applyProfile(MidiDeviceProfile profile) {
        ensureInitialized();

        MidiOutputSettings candidate = settings_;
        applyMidiDeviceProfile(profile, candidate);
        if (candidate == settings_) {
            midiTransportCapabilityRuntime().setDeviceProfile(settings_.profile);
            return false;
        }

        settings_ = candidate;
        ++revision_;
        midiTransportCapabilityRuntime().setDeviceProfile(settings_.profile);
        return true;
    }

    // Keeps transport user intent in the same owned settings snapshot without
    // conflating it with device capabilities.
    bool updateTransportControl(TransportClockSource source,
                                bool externalFollowEnabled) {
        ensureInitialized();

        const TransportClockSource normalized = normalizeTransportClockSource(
            static_cast<uint8_t>(source));
        if (settings_.transportClockSource == normalized &&
            settings_.externalFollowEnabled == externalFollowEnabled) {
            return false;
        }

        settings_.transportClockSource = normalized;
        settings_.externalFollowEnabled = externalFollowEnabled;
        ++revision_;
        return true;
    }

    const MidiOutputSettings& settings() const { return settings_; }
    MidiDeviceProfile profile() const { return settings_.profile; }
    uint32_t revision() const { return revision_; }
    bool initialized() const { return initialized_; }

private:
    void ensureInitialized() {
        if (initialized_) return;
        initialize(makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative));
    }

    MidiOutputSettings settings_{};
    uint32_t revision_{0};
    bool initialized_{false};
};

inline MidiDeviceProfileRuntime& midiDeviceProfileRuntime() {
    static MidiDeviceProfileRuntime runtime;
    return runtime;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_DEVICE_PROFILE_RUNTIME_H
