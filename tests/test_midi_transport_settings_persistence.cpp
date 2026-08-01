#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_companion_settings_codec.h"

using namespace GroovePuterMidi;

namespace {

uint32_t crc32(const uint8_t* data, std::size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= static_cast<uint32_t>(data[i]);
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = static_cast<uint32_t>(
                -static_cast<int32_t>(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

void writeU16(uint8_t* destination, uint16_t value) {
    destination[0] = static_cast<uint8_t>(value & 0xFFu);
    destination[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
}

void writeU32(uint8_t* destination, uint32_t value) {
    destination[0] = static_cast<uint8_t>(value & 0xFFu);
    destination[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
    destination[2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
    destination[3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
}

class FakeStorage final : public IMidiSettingsStorage {
public:
    MidiSettingsStorageReadStatus read(uint8_t* destination,
                                       std::size_t capacity,
                                       std::size_t& bytesRead) override {
        bytesRead = 0;
        if (!present) return MidiSettingsStorageReadStatus::NotFound;
        if (storedSize > capacity) return MidiSettingsStorageReadStatus::Error;
        std::memcpy(destination, bytes.data(), storedSize);
        bytesRead = storedSize;
        return MidiSettingsStorageReadStatus::Ok;
    }

    bool write(const uint8_t* source, std::size_t size) override {
        if (size > bytes.size()) return false;
        std::memcpy(bytes.data(), source, size);
        storedSize = size;
        present = true;
        ++writeCount;
        return true;
    }

    std::array<uint8_t, MidiSettingsCodec::kEncodedSize> bytes{};
    std::size_t storedSize{0};
    uint32_t writeCount{0};
    bool present{false};
};

std::array<uint8_t, MidiSettingsCodec::kLegacyEncodedSize> makeLegacyV1(
        const MidiOutputSettings& source) {
    const auto current = MidiSettingsCodec::encode(source);
    std::array<uint8_t, MidiSettingsCodec::kLegacyEncodedSize> legacy{};

    constexpr std::size_t kLegacyChecksumOffset =
        MidiSettingsCodec::kLegacyEncodedSize - 4u;
    std::memcpy(legacy.data(), current.data(), kLegacyChecksumOffset);
    writeU16(&legacy[4], MidiSettingsCodec::kLegacySchemaVersion);
    writeU16(&legacy[6],
             static_cast<uint16_t>(MidiSettingsCodec::kLegacyPayloadSize));
    writeU32(&legacy[kLegacyChecksumOffset],
             crc32(legacy.data(), kLegacyChecksumOffset));
    return legacy;
}

}  // namespace

int main() {
    const MidiOutputSettings defaults =
        makeDefaultMidiOutputSettings(MidiDeviceProfile::SeqtrakNative);
    assert(defaults.transportClockSource ==
           TransportClockSource::GroovePuterInternal);
    assert(defaults.externalFollowEnabled);

    MidiOutputSettings source = defaults;
    source.profile = MidiDeviceProfile::Custom;
    source.liveChannel = 11;
    source.drumGateMs = 137;
    source.transportClockSource = TransportClockSource::SeqtrakExternal;
    source.externalFollowEnabled = false;

    const auto encoded = MidiSettingsCodec::encode(source);
    assert(encoded.size() == MidiSettingsCodec::kEncodedSize);
    MidiOutputSettings decoded{};
    assert(MidiSettingsCodec::decode(encoded.data(), encoded.size(), decoded));
    assert(decoded == source);

    applyMidiDeviceProfile(MidiDeviceProfile::GeneralMidi, decoded);
    assert(decoded.profile == MidiDeviceProfile::GeneralMidi);
    assert(decoded.transportClockSource == TransportClockSource::SeqtrakExternal);
    assert(!decoded.externalFollowEnabled);

    const auto legacy = makeLegacyV1(source);
    MidiOutputSettings migrated = source;
    assert(MidiSettingsCodec::decode(legacy.data(), legacy.size(), migrated));
    assert(migrated.profile == source.profile);
    assert(migrated.liveChannel == source.liveChannel);
    assert(migrated.drumGateMs == source.drumGateMs);
    assert(migrated.transportClockSource ==
           TransportClockSource::GroovePuterInternal);
    assert(migrated.externalFollowEnabled);

    FakeStorage storage;
    std::memcpy(storage.bytes.data(), legacy.data(), legacy.size());
    storage.storedSize = legacy.size();
    storage.present = true;
    MidiSettingsPersistence persistence(storage);
    MidiOutputSettings loaded{};
    assert(persistence.load(loaded) == MidiSettingsLoadStatus::Loaded);
    assert(storage.writeCount == 0);
    assert(loaded.transportClockSource ==
           TransportClockSource::GroovePuterInternal);
    assert(loaded.externalFollowEnabled);

    loaded.transportClockSource = TransportClockSource::SeqtrakExternal;
    loaded.externalFollowEnabled = false;
    assert(persistence.save(loaded));
    assert(storage.writeCount == 1);
    assert(storage.storedSize == MidiSettingsCodec::kEncodedSize);

    MidiOutputSettings reloaded{};
    assert(persistence.load(reloaded) == MidiSettingsLoadStatus::Loaded);
    assert(reloaded == loaded);

    MidiOutputSettings invalid = loaded;
    invalid.transportClockSource = static_cast<TransportClockSource>(255);
    assert(!isValidMidiOutputSettings(invalid));
    sanitizeMidiOutputSettings(invalid);
    assert(invalid.transportClockSource ==
           TransportClockSource::GroovePuterInternal);
    assert(isValidMidiOutputSettings(invalid));

    return 0;
}
