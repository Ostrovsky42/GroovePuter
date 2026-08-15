#include "cardputer_midi_settings_session.h"

#ifdef ARDUINO

#include <Arduino.h>
#include <Preferences.h>

#include <cstddef>
#include <cstdint>

#include "src/midi/midi_companion_settings_codec.h"
#include "src/midi/midi_device_profile_runtime.h"
#include "src/midi/midi_pattern_startup_routes.h"
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

        GroovePuterMidi::MidiOutputSettings loadedSettings =
            GroovePuterMidi::makeDefaultMidiOutputSettings(
                GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
        const GroovePuterMidi::MidiSettingsLoadStatus status =
            persistence_.load(loadedSettings);

        GroovePuterMidi::MidiDeviceProfileRuntime& profileRuntime =
            GroovePuterMidi::midiDeviceProfileRuntime();
        profileRuntime.initialize(loadedSettings);
        const GroovePuterMidi::MidiOutputSettings& settings =
            profileRuntime.settings();

        // R5 publishes only the derived Pattern wire snapshot needed by the USB
        // output. R5a guarantees this session runs before the dispatcher task,
        // and UsbMidiOutput consumes the snapshot once in begin().
        GroovePuterMidi::publishMidiPatternStartupRoutes(settings);

        GroovePuterMidi::TransportClockRuntime& runtime =
            GroovePuterMidi::transportClockRuntime();
        runtime.applyPersistedControl(settings.transportClockSource,
                                      settings.externalFollowEnabled);
        initialized_ = true;
        runtime.setControlChangedCallback(&persistControlChange);

        Serial.printf(
            "[MIDI-SETTINGS] load=%u schema=%u profile=%s source=%s follow=%u\n",
            static_cast<unsigned>(status),
            static_cast<unsigned>(
                GroovePuterMidi::MidiSettingsCodec::kSchemaVersion),
            GroovePuterMidi::midiDeviceProfileName(settings.profile),
            GroovePuterMidi::transportClockSourceName(
                settings.transportClockSource),
            static_cast<unsigned>(settings.externalFollowEnabled ? 1 : 0));
    }

    bool persist(GroovePuterMidi::TransportClockSource source,
                 bool externalFollowEnabled) {
        if (!initialized_) initialize();

        GroovePuterMidi::MidiDeviceProfileRuntime& profileRuntime =
            GroovePuterMidi::midiDeviceProfileRuntime();
        profileRuntime.updateTransportControl(source, externalFollowEnabled);
        const GroovePuterMidi::MidiOutputSettings& settings =
            profileRuntime.settings();

        const bool saved = persistence_.save(settings);
        Serial.printf("[MIDI-SETTINGS] save=%u source=%s follow=%u\n",
                      static_cast<unsigned>(saved ? 1 : 0),
                      GroovePuterMidi::transportClockSourceName(
                          settings.transportClockSource),
                      static_cast<unsigned>(
                          settings.externalFollowEnabled ? 1 : 0));
        return saved;
    }

private:
    static void persistControlChange(
            GroovePuterMidi::TransportClockSource source,
            bool externalFollowEnabled);

    CardputerMidiSettingsStorage storage_;
    GroovePuterMidi::MidiSettingsPersistence persistence_{storage_};
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
