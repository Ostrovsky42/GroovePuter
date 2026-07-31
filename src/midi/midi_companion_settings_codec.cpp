#include "midi_companion_settings_codec.h"

namespace GroovePuterMidi {
namespace {

constexpr uint8_t kMagic[4] = {'G', 'P', 'M', 'D'};
constexpr std::size_t kHeaderSize = 8;
constexpr std::size_t kChecksumSize = 4;
constexpr std::size_t kChecksumOffset = MidiSettingsCodec::kEncodedSize - kChecksumSize;

constexpr uint8_t kEnabledFlag = 1u << 0;
constexpr uint8_t kLiveEnabledFlag = 1u << 1;
constexpr uint8_t kPatternSynthAEnabledFlag = 1u << 2;
constexpr uint8_t kPatternSynthBEnabledFlag = 1u << 3;
constexpr uint8_t kDrumsEnabledFlag = 1u << 4;

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

uint16_t readU16(const uint8_t* source) {
    return static_cast<uint16_t>(source[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(source[1]) << 8);
}

uint32_t readU32(const uint8_t* source) {
    return static_cast<uint32_t>(source[0]) |
           (static_cast<uint32_t>(source[1]) << 8) |
           (static_cast<uint32_t>(source[2]) << 16) |
           (static_cast<uint32_t>(source[3]) << 24);
}

uint8_t settingsFlags(const MidiOutputSettings& settings) {
    uint8_t flags = 0;
    if (settings.enabled) flags |= kEnabledFlag;
    if (settings.liveEnabled) flags |= kLiveEnabledFlag;
    if (settings.patternSynthAEnabled) flags |= kPatternSynthAEnabledFlag;
    if (settings.patternSynthBEnabled) flags |= kPatternSynthBEnabledFlag;
    if (settings.drumsEnabled) flags |= kDrumsEnabledFlag;
    return flags;
}

}  // namespace

MidiSettingsCodec::EncodedSettings MidiSettingsCodec::encode(
    const MidiOutputSettings& source) {
    MidiOutputSettings settings = source;
    sanitizeMidiOutputSettings(settings);

    EncodedSettings encoded{};
    encoded[0] = kMagic[0];
    encoded[1] = kMagic[1];
    encoded[2] = kMagic[2];
    encoded[3] = kMagic[3];
    writeU16(&encoded[4], kSchemaVersion);
    writeU16(&encoded[6], static_cast<uint16_t>(kPayloadSize));

    std::size_t cursor = kHeaderSize;
    encoded[cursor++] = static_cast<uint8_t>(settings.profile);
    encoded[cursor++] = settingsFlags(settings);
    encoded[cursor++] = static_cast<uint8_t>(settings.liveTarget);
    encoded[cursor++] = settings.liveChannel;
    encoded[cursor++] = settings.synthAChannel;
    encoded[cursor++] = settings.synthBChannel;
    writeU16(&encoded[cursor], settings.drumGateMs);
    cursor += 2;

    for (const DrumMidiRoute& route : settings.drumRoutes) {
        encoded[cursor++] = route.enabled ? 1u : 0u;
        encoded[cursor++] = route.channel;
        encoded[cursor++] = route.note;
    }

    const uint32_t checksum = crc32(encoded.data(), kChecksumOffset);
    writeU32(&encoded[kChecksumOffset], checksum);
    return encoded;
}

bool MidiSettingsCodec::decode(const uint8_t* data,
                               std::size_t size,
                               MidiOutputSettings& output) {
    if (!data || size != kEncodedSize) return false;
    if (data[0] != kMagic[0] || data[1] != kMagic[1] ||
        data[2] != kMagic[2] || data[3] != kMagic[3]) {
        return false;
    }
    if (readU16(&data[4]) != kSchemaVersion ||
        readU16(&data[6]) != kPayloadSize) {
        return false;
    }
    if (readU32(&data[kChecksumOffset]) != crc32(data, kChecksumOffset)) {
        return false;
    }

    MidiOutputSettings decoded{};
    std::size_t cursor = kHeaderSize;
    decoded.profile = static_cast<MidiDeviceProfile>(data[cursor++]);
    const uint8_t flags = data[cursor++];
    decoded.enabled = (flags & kEnabledFlag) != 0;
    decoded.liveEnabled = (flags & kLiveEnabledFlag) != 0;
    decoded.patternSynthAEnabled = (flags & kPatternSynthAEnabledFlag) != 0;
    decoded.patternSynthBEnabled = (flags & kPatternSynthBEnabledFlag) != 0;
    decoded.drumsEnabled = (flags & kDrumsEnabledFlag) != 0;
    decoded.liveTarget = static_cast<MidiLiveTarget>(data[cursor++]);
    decoded.liveChannel = data[cursor++];
    decoded.synthAChannel = data[cursor++];
    decoded.synthBChannel = data[cursor++];
    decoded.drumGateMs = readU16(&data[cursor]);
    cursor += 2;

    for (DrumMidiRoute& route : decoded.drumRoutes) {
        const uint8_t enabled = data[cursor++];
        if (enabled > 1u) return false;
        route.enabled = enabled != 0;
        route.channel = data[cursor++];
        route.note = data[cursor++];
    }

    if (cursor != kChecksumOffset || !isValidMidiOutputSettings(decoded)) {
        return false;
    }

    output = decoded;
    return true;
}

uint32_t MidiSettingsCodec::crc32(const uint8_t* data, std::size_t size) {
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

bool MidiSettingsPersistence::save(const MidiOutputSettings& settings) {
    const MidiSettingsCodec::EncodedSettings encoded = MidiSettingsCodec::encode(settings);
    return storage_.write(encoded.data(), encoded.size());
}

MidiSettingsLoadStatus MidiSettingsPersistence::load(
    MidiOutputSettings& output,
    MidiDeviceProfile fallbackProfile) {
    MidiSettingsCodec::EncodedSettings encoded{};
    std::size_t bytesRead = 0;
    const MidiSettingsStorageReadStatus status =
        storage_.read(encoded.data(), encoded.size(), bytesRead);

    if (status == MidiSettingsStorageReadStatus::Error) {
        return MidiSettingsLoadStatus::StorageError;
    }
    if (status == MidiSettingsStorageReadStatus::NotFound) {
        output = makeDefaultMidiOutputSettings(fallbackProfile);
        return MidiSettingsLoadStatus::DefaultsFromMissing;
    }

    MidiOutputSettings decoded{};
    if (bytesRead != encoded.size() ||
        !MidiSettingsCodec::decode(encoded.data(), bytesRead, decoded)) {
        output = makeDefaultMidiOutputSettings(fallbackProfile);
        return MidiSettingsLoadStatus::DefaultsFromCorrupt;
    }

    output = decoded;
    return MidiSettingsLoadStatus::Loaded;
}

}  // namespace GroovePuterMidi
