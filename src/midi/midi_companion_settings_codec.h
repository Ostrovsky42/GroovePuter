#pragma once
#ifndef GROOVEPUTER_MIDI_COMPANION_SETTINGS_CODEC_H
#define GROOVEPUTER_MIDI_COMPANION_SETTINGS_CODEC_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "midi_companion_settings.h"

namespace GroovePuterMidi {

class MidiSettingsCodec {
public:
    static constexpr uint16_t kSchemaVersion = 1;
    static constexpr std::size_t kPayloadSize = 32;
    static constexpr std::size_t kEncodedSize = 44;
    using EncodedSettings = std::array<uint8_t, kEncodedSize>;

    static EncodedSettings encode(const MidiOutputSettings& settings);

    static bool decode(const uint8_t* data,
                       std::size_t size,
                       MidiOutputSettings& output);

private:
    static uint32_t crc32(const uint8_t* data, std::size_t size);
};

enum class MidiSettingsStorageReadStatus : uint8_t {
    Ok,
    NotFound,
    Error,
};

class IMidiSettingsStorage {
public:
    virtual ~IMidiSettingsStorage() = default;

    virtual MidiSettingsStorageReadStatus read(uint8_t* destination,
                                               std::size_t capacity,
                                               std::size_t& bytesRead) = 0;
    virtual bool write(const uint8_t* data, std::size_t size) = 0;
};

enum class MidiSettingsLoadStatus : uint8_t {
    Loaded,
    DefaultsFromMissing,
    DefaultsFromCorrupt,
    StorageError,
};

class MidiSettingsPersistence {
public:
    explicit MidiSettingsPersistence(IMidiSettingsStorage& storage)
        : storage_(storage) {}

    bool save(const MidiOutputSettings& settings);

    MidiSettingsLoadStatus load(
        MidiOutputSettings& output,
        MidiDeviceProfile fallbackProfile = MidiDeviceProfile::SeqtrakNative);

private:
    IMidiSettingsStorage& storage_;
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_COMPANION_SETTINGS_CODEC_H
