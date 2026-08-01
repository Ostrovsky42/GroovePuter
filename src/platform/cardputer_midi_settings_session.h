#pragma once

#include <cstddef>
#include <cstdint>

#include "src/midi/midi_companion_settings_codec.h"
#include "src/midi/transport_clock_runtime.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <Preferences.h>
#endif

namespace GroovePuterPlatform {

// NVS adapter for the existing versioned MIDI settings codec. Reads accept the
// original 44-byte schema-v1 record and the current schema-v2 record. NVS I/O
// is performed only from UI setup/control paths, never from AudioTask,
// MidiDispatchTask or SmfPlayerTask.
class CardputerMidiSettingsStorage final
    : public GroovePuterMidi::IMidiSettingsStorage {
public:
    GroovePuterMidi::MidiSettingsStorageReadStatus read(
            uint8_t* destination,
            std::size_t capacity,
            std::size_t& bytesRead) override {
        bytesRead = 0;
#ifdef ARDUINO
        Preferences preferences;
        if (!preferences.begin(kNamespace, true)) {
            return GroovePuterMidi::MidiSettingsStorageReadStatus::Error;
        }
        if (!preferences.isKey(kKey)) {
            preferences.end();
            return GroovePuterMidi::MidiSettingsStorageReadStatus::NotFound;
        }
        const std::size_t storedSize = preferences.getBytesLength(kKey);
        if (storedSize == 0 || storedSize > capacity) {
            preferences.end();
            return GroovePuterMidi::MidiSettingsStorageReadStatus::Error;
        }
        const std::size_t readSize =
            preferences.getBytes(kKey, destination, storedSize);
        preferences.end();
        if (readSize != storedSize) {
            return GroovePuterMidi::MidiSettingsStorageReadStatus::Error;
        }
        bytesRead = readSize;
        return GroovePuterMidi::MidiSettingsStorageReadStatus::Ok;
#else
        (void)destination;
        (void)capacity;
        return GroovePuterMidi::MidiSettingsStorageReadStatus::NotFound;
#endif
    }

    bool write(const uint8_t* data, std::size_t size) override {
#ifdef ARDUINO
        Preferences preferences;
        if (!preferences.begin(kNamespace, false)) return false;
        const std::size_t written = preferences.putBytes(kKey, data, size);
        preferences.end();
        return written == size;
#else
        (void)data;
        (void)size;
        return true;
#endif
    }

private:
    static constexpr const char* kNamespace = "grooveputer";
    static constexpr const char* kKey = "midi_cfg";
};

class CardputerMidiSettingsSession {
public:
    void initialize() {
        if (initialized_) return;

        settings_ = GroovePuterMidi::makeDefaultMidiOutputSettings(
            GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
        const GroovePuterMidi::MidiSettingsLoadStatus status =
            persistence_.load(settings_);

        GroovePuterMidi::TransportClockRuntime& runtime =
            GroovePuterMidi::transportClockRuntime();
        runtime.applyPersistedControl(settings_.transportClockSource,
                                      settings_.externalFollowEnabled);
        runtime.setControlChangedCallback(&persistControlChange);
        initialized_ = true;

#ifdef ARDUINO
        Serial.printf(
            "[MIDI-SETTINGS] load=%u schema=%u source=%s follow=%u\n",
            static_cast<unsigned>(status),
            static_cast<unsigned>(GroovePuterMidi::MidiSettingsCodec::kSchemaVersion),
            GroovePuterMidi::transportClockSourceName(
                settings_.transportClockSource),
            static_cast<unsigned>(settings_.externalFollowEnabled ? 1 : 0));
#endif
    }

    bool persist(GroovePuterMidi::TransportClockSource source,
                 bool externalFollowEnabled) {
        if (!initialized_) initialize();
        settings_.transportClockSource =
            GroovePuterMidi::normalizeTransportClockSource(
                static_cast<uint8_t>(source));
        settings_.externalFollowEnabled = externalFollowEnabled;
        const bool saved = persistence_.save(settings_);
#ifdef ARDUINO
        Serial.printf("[MIDI-SETTINGS] save=%u source=%s follow=%u\n",
                      static_cast<unsigned>(saved ? 1 : 0),
                      GroovePuterMidi::transportClockSourceName(
                          settings_.transportClockSource),
                      static_cast<unsigned>(externalFollowEnabled ? 1 : 0));
#endif
        return saved;
    }

private:
    static void persistControlChange(
            GroovePuterMidi::TransportClockSource source,
            bool externalFollowEnabled);

    CardputerMidiSettingsStorage storage_;
    GroovePuterMidi::MidiSettingsPersistence persistence_{storage_};
    GroovePuterMidi::MidiOutputSettings settings_{};
    bool initialized_{false};
};

inline CardputerMidiSettingsSession& cardputerMidiSettingsSession() {
    static CardputerMidiSettingsSession session;
    return session;
}

inline void CardputerMidiSettingsSession::persistControlChange(
        GroovePuterMidi::TransportClockSource source,
        bool externalFollowEnabled) {
    cardputerMidiSettingsSession().persist(source, externalFollowEnabled);
}

// Constructed by the root UI object during setup. It performs one bounded NVS
// read before user interaction and registers immediate UI-thread persistence
// for later C/G control changes.
class CardputerMidiSettingsBinding {
public:
    CardputerMidiSettingsBinding() {
        cardputerMidiSettingsSession().initialize();
    }
};

}  // namespace GroovePuterPlatform
