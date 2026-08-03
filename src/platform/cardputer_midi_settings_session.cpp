#include "cardputer_midi_settings_session.h"

#ifdef ARDUINO

#include <Arduino.h>
#include <Preferences.h>

#include <cstddef>
#include <cstdint>

#include "src/midi/midi_companion_settings_codec.h"
#include "src/midi/midi_transport_capabilities.h"
#include "src/midi/transport_clock_runtime.h"

namespace GroovePuterPlatform {
namespace {

class CardputerMidiSettingsStorage final
    : public GroovePuterMidi::IMidiSettingsStorage {
public:
    GroovePuterMidi::MidiSettingsStorageReadStatus read(
            uint8_t* destination,
            std::size_t capacity,
            std::size_t& bytesRead) override {
        bytesRead = 0;
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
    }

    bool write(const uint8_t* data, std::size_t size) override {
        Preferences preferences;
        if (!preferences.begin(kNamespace, false)) return false;
        const std::size_t written = preferences.putBytes(kKey, data, size);
        preferences.end();
        return written == size;
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

        GroovePuterMidi::midiTransportCapabilityRuntime().setDeviceProfile(
            settings_.profile);

        GroovePuterMidi::TransportClockRuntime& runtime =
            GroovePuterMidi::transportClockRuntime();
        runtime.applyPersistedControl(settings_.transportClockSource,
                                      settings_.externalFollowEnabled);
        initialized_ = true;
        runtime.setControlChangedCallback(&persistControlChange);

        Serial.printf(
            "[MIDI-SETTINGS] load=%u schema=%u profile=%s source=%s follow=%u\n",
            static_cast<unsigned>(status),
            static_cast<unsigned>(
                GroovePuterMidi::MidiSettingsCodec::kSchemaVersion),
            GroovePuterMidi::midiDeviceProfileName(settings_.profile),
            GroovePuterMidi::transportClockSourceName(
                settings_.transportClockSource),
            static_cast<unsigned>(settings_.externalFollowEnabled ? 1 : 0));
    }

    bool persist(GroovePuterMidi::TransportClockSource source,
                 bool externalFollowEnabled) {
        if (!initialized_) initialize();
        settings_.transportClockSource =
            GroovePuterMidi::normalizeTransportClockSource(
                static_cast<uint8_t>(source));
        settings_.externalFollowEnabled = externalFollowEnabled;
        const bool saved = persistence_.save(settings_);
        Serial.printf("[MIDI-SETTINGS] save=%u source=%s follow=%u\n",
                      static_cast<unsigned>(saved ? 1 : 0),
                      GroovePuterMidi::transportClockSourceName(
                          settings_.transportClockSource),
                      static_cast<unsigned>(externalFollowEnabled ? 1 : 0));
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

CardputerMidiSettingsSession& settingsSession() {
    static CardputerMidiSettingsSession session;
    return session;
}

void CardputerMidiSettingsSession::persistControlChange(
        GroovePuterMidi::TransportClockSource source,
        bool externalFollowEnabled) {
    settingsSession().persist(source, externalFollowEnabled);
}

}  // namespace

void initializeCardputerMidiSettingsSession() {
    settingsSession().initialize();
}

}  // namespace GroovePuterPlatform

#endif  // ARDUINO
