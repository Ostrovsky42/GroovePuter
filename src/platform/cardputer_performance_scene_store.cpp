#include "cardputer_performance_scene_store.h"

#ifdef ARDUINO
#include <Preferences.h>
#include <cstring>

namespace GroovePuterPlatform {
namespace {
constexpr const char* kNamespace = "gp_perf";
constexpr const char* kSceneKey = "scenes_v1";
constexpr const char* kConfigKey = "keyboard_v1";
constexpr std::size_t kConfigRecordSize = 21;

uint16_t crc16(const uint8_t* bytes, std::size_t size) {
    uint16_t crc = 0xffffu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= static_cast<uint16_t>(bytes[i]) << 8u;
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1u) ^ 0x1021u)
                                  : static_cast<uint16_t>(crc << 1u);
    }
    return crc;
}

void encodeConfig(const PerformanceKeyboardConfig& c, uint8_t (&bytes)[kConfigRecordSize]) {
    bytes[0] = 1;
    const uint8_t fields[18] = {c.role, c.leadMode, c.arpPitchSource,
        c.harmonyChangePolicy, c.arpDirection, c.arpRate, c.arpOctaves,
        c.gatePercent, c.chordMode, c.chordInversion, c.chordSpread,
        c.voiceLeading, c.rootPitchClass, c.scale, c.velocity,
        static_cast<uint8_t>(c.octaveShift), c.arpEnabled, c.latchEnabled};
    std::memcpy(bytes + 1, fields, sizeof(fields));
    const uint16_t crc = crc16(bytes, kConfigRecordSize - 2u);
    bytes[kConfigRecordSize - 2u] = static_cast<uint8_t>(crc & 0xffu);
    bytes[kConfigRecordSize - 1u] = static_cast<uint8_t>(crc >> 8u);
}

bool decodeConfig(const uint8_t (&bytes)[kConfigRecordSize], PerformanceKeyboardConfig& c) {
    const uint16_t stored = static_cast<uint16_t>(bytes[kConfigRecordSize - 2u] |
        (static_cast<uint16_t>(bytes[kConfigRecordSize - 1u]) << 8u));
    if (bytes[0] != 1u || crc16(bytes, kConfigRecordSize - 2u) != stored) return false;
    c.role = bytes[1]; c.leadMode = bytes[2]; c.arpPitchSource = bytes[3];
    c.harmonyChangePolicy = bytes[4]; c.arpDirection = bytes[5]; c.arpRate = bytes[6];
    c.arpOctaves = bytes[7]; c.gatePercent = bytes[8]; c.chordMode = bytes[9];
    c.chordInversion = bytes[10]; c.chordSpread = bytes[11]; c.voiceLeading = bytes[12];
    c.rootPitchClass = bytes[13]; c.scale = bytes[14]; c.velocity = bytes[15];
    c.octaveShift = static_cast<int8_t>(bytes[16]); c.arpEnabled = bytes[17];
    c.latchEnabled = bytes[18];
    return true;
}
}

bool loadCardputerPerformanceScenes(GroovePuterPerformance::PerformanceSceneBank& bank) {
    Preferences preferences;
    if (!preferences.begin(kNamespace, true)) return false;
    uint8_t bytes[GroovePuterPerformance::PerformanceSceneBank::kSerializedSize]{};
    const bool loaded = preferences.getBytesLength(kSceneKey) == sizeof(bytes) &&
        preferences.getBytes(kSceneKey, bytes, sizeof(bytes)) == sizeof(bytes) &&
        bank.deserialize(bytes, sizeof(bytes));
    preferences.end();
    return loaded;
}

bool saveCardputerPerformanceScenes(
    const GroovePuterPerformance::PerformanceSceneBank& bank) {
    uint8_t bytes[GroovePuterPerformance::PerformanceSceneBank::kSerializedSize]{};
    if (!bank.serialize(bytes, sizeof(bytes))) return false;
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    const bool saved = preferences.putBytes(kSceneKey, bytes, sizeof(bytes)) == sizeof(bytes);
    preferences.end();
    return saved;
}

bool loadCardputerPerformanceConfig(PerformanceKeyboardConfig& config) {
    Preferences preferences;
    if (!preferences.begin(kNamespace, true)) return false;
    uint8_t bytes[kConfigRecordSize]{};
    const bool loaded = preferences.getBytesLength(kConfigKey) == sizeof(bytes) &&
        preferences.getBytes(kConfigKey, bytes, sizeof(bytes)) == sizeof(bytes) &&
        decodeConfig(bytes, config);
    preferences.end();
    return loaded;
}

bool saveCardputerPerformanceConfig(const PerformanceKeyboardConfig& config) {
    uint8_t bytes[kConfigRecordSize]{};
    encodeConfig(config, bytes);
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    const bool saved = preferences.putBytes(kConfigKey, bytes, sizeof(bytes)) == sizeof(bytes);
    preferences.end();
    return saved;
}
} // namespace GroovePuterPlatform
#endif
