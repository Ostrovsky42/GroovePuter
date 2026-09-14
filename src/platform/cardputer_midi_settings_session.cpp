#include "cardputer_midi_settings_session.h"

#ifdef ARDUINO

#include <Arduino.h>
#include <Preferences.h>

#include <cstddef>
#include <cstdint>

#include "src/midi/midi_companion_settings_codec.h"
#include "src/midi/midi_device_profile_runtime.h"
#include "src/midi/midi_input_settings.h"
#include "src/midi/midi_pattern_startup_routes.h"
#include "src/midi/transport_clock_runtime.h"
#include "src/platform/cardputer_usb_midi_service.h"

namespace GroovePuterPlatform {
namespace {

bool validSelectableProfile(GroovePuterMidi::MidiDeviceProfile profile) {
    switch (profile) {
        case GroovePuterMidi::MidiDeviceProfile::SeqtrakNative:
        case GroovePuterMidi::MidiDeviceProfile::GeneralMidi:
        case GroovePuterMidi::MidiDeviceProfile::Custom:
        case GroovePuterMidi::MidiDeviceProfile::GenericMidi:
            return true;
    }
    return false;
}

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
        pendingProfile_ = settings.profile;

        // R5/R6 publish the frozen Pattern + predefined Performance startup
        // routes before the dispatcher begins. R7 profile edits intentionally
        // do not republish this snapshot; they take effect only on next boot.
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

    GroovePuterMidi::MidiDeviceProfile pendingProfile() {
        if (!initialized_) initialize();
        return pendingProfile_;
    }

    bool restartRequired() {
        if (!initialized_) initialize();
        return pendingProfile_ !=
               GroovePuterMidi::midiDeviceProfileRuntime().profile();
    }

    bool selectProfileForNextBoot(GroovePuterMidi::MidiDeviceProfile profile) {
        if (!validSelectableProfile(profile)) return false;
        if (!initialized_) initialize();
        if (profile == pendingProfile_) return true;

        // Build the persisted candidate from the active runtime snapshot. The
        // active USB/transport capability owner stays unchanged until reboot.
        GroovePuterMidi::MidiOutputSettings candidate =
            GroovePuterMidi::midiDeviceProfileRuntime().settings();
        GroovePuterMidi::applyMidiDeviceProfile(profile, candidate);

        const bool saved = persistence_.save(candidate);
        if (saved) pendingProfile_ = profile;
        Serial.printf(
            "[MIDI-SETTINGS] profile-save=%u selected=%s active=%s reboot=%u\n",
            static_cast<unsigned>(saved ? 1 : 0),
            GroovePuterMidi::midiDeviceProfileName(profile),
            GroovePuterMidi::midiDeviceProfileName(
                GroovePuterMidi::midiDeviceProfileRuntime().profile()),
            static_cast<unsigned>(restartRequired() ? 1 : 0));
        return saved;
    }

    bool persist(GroovePuterMidi::TransportClockSource source,
                 bool externalFollowEnabled) {
        if (!initialized_) initialize();

        GroovePuterMidi::MidiDeviceProfileRuntime& profileRuntime =
            GroovePuterMidi::midiDeviceProfileRuntime();
        profileRuntime.updateTransportControl(source, externalFollowEnabled);

        // Preserve a pending next-boot profile selection when a later transport
        // control change is persisted before reboot.
        GroovePuterMidi::MidiOutputSettings record = profileRuntime.settings();
        if (pendingProfile_ != record.profile) {
            GroovePuterMidi::applyMidiDeviceProfile(pendingProfile_, record);
        }
        record.transportClockSource = source;
        record.externalFollowEnabled = externalFollowEnabled;

        const bool saved = persistence_.save(record);
        Serial.printf(
            "[MIDI-SETTINGS] save=%u profile=%s source=%s follow=%u\n",
            static_cast<unsigned>(saved ? 1 : 0),
            GroovePuterMidi::midiDeviceProfileName(record.profile),
            GroovePuterMidi::transportClockSourceName(source),
            static_cast<unsigned>(externalFollowEnabled ? 1 : 0));
        return saved;
    }

private:
    static void persistControlChange(
            GroovePuterMidi::TransportClockSource source,
            bool externalFollowEnabled);

    CardputerMidiSettingsStorage storage_;
    GroovePuterMidi::MidiSettingsPersistence persistence_{storage_};
    GroovePuterMidi::MidiDeviceProfile pendingProfile_{
        GroovePuterMidi::MidiDeviceProfile::SeqtrakNative};
    bool initialized_{false};
};

CardputerMidiSettingsSession& settingsSession() {
    static CardputerMidiSettingsSession session;
    return session;
}

class CardputerMidiInputSettingsSession {
public:
    void initialize() {
        if (initialized_) return;
        GroovePuterMidi::MidiInputRoutingConfig loaded =
            GroovePuterMidi::MidiInputSettings::defaultRoutingConfig();
        bool decoded = false;
        Preferences preferences;
        if (preferences.begin(kNamespace, true)) {
            if (preferences.isKey(kKey)) {
                decoded = GroovePuterMidi::MidiInputSettings::decodeRoutingConfig(
                    preferences.getUInt(kKey, 0u), loaded);
            }
            preferences.end();
        }
        config_ = loaded;
        initialized_ = true;
        const bool applied = applyCardputerMidiInputRuntimeRoutingConfig(config_);
        Serial.printf("[MIDI-IN] load=%u enabled=%u mode=%u ch=%u target=%u apply=%u\n",
                      static_cast<unsigned>(decoded ? 1 : 0),
                      static_cast<unsigned>(config_.enabled ? 1 : 0),
                      static_cast<unsigned>(config_.channelMode),
                      static_cast<unsigned>(config_.channel + 1u),
                      static_cast<unsigned>(config_.target),
                      static_cast<unsigned>(applied ? 1 : 0));
    }

    GroovePuterMidi::MidiInputRoutingConfig config() {
        if (!initialized_) initialize();
        return config_;
    }

    bool set(const GroovePuterMidi::MidiInputRoutingConfig& config) {
        if (!GroovePuterMidi::MidiInputDispatcher::isValidConfig(config)) return false;
        if (!initialized_) initialize();
        if (same(config_, config)) return true;
        Preferences preferences;
        if (!preferences.begin(kNamespace, false)) return false;
        const std::size_t written = preferences.putUInt(
            kKey, GroovePuterMidi::MidiInputSettings::encodeRoutingConfig(config));
        preferences.end();
        if (written != sizeof(uint32_t)) return false;
        if (!applyCardputerMidiInputRuntimeRoutingConfig(config)) return false;
        config_ = config;
        return true;
    }

private:
    static bool same(const GroovePuterMidi::MidiInputRoutingConfig& a,
                     const GroovePuterMidi::MidiInputRoutingConfig& b) {
        return a.enabled == b.enabled && a.channelMode == b.channelMode &&
               a.channel == b.channel && a.target == b.target;
    }
    static constexpr const char* kNamespace = "grooveputer";
    static constexpr const char* kKey = "midi_in";
    GroovePuterMidi::MidiInputRoutingConfig config_{};
    bool initialized_{false};
};

CardputerMidiInputSettingsSession& inputSettingsSession() {
    static CardputerMidiInputSettingsSession session;
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

void initializeCardputerMidiInputSettings() {
    inputSettingsSession().initialize();
}

GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRoutingConfig() {
    return inputSettingsSession().config();
}

bool setCardputerMidiInputRoutingConfig(
        const GroovePuterMidi::MidiInputRoutingConfig& config) {
    return inputSettingsSession().set(config);
}

GroovePuterMidi::MidiDeviceProfile pendingCardputerMidiDeviceProfile() {
    return settingsSession().pendingProfile();
}

bool selectCardputerMidiDeviceProfileForNextBoot(
        GroovePuterMidi::MidiDeviceProfile profile) {
    return settingsSession().selectProfileForNextBoot(profile);
}

bool cardputerMidiDeviceProfileRestartRequired() {
    return settingsSession().restartRequired();
}

}  // namespace GroovePuterPlatform

#endif  // ARDUINO
