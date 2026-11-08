#include "performance_keyboard_config.h"

namespace {

uint16_t crc16(const uint8_t* bytes, std::size_t size) {
    uint16_t crc = 0xffffu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= static_cast<uint16_t>(bytes[i]) << 8u;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u)
                ? static_cast<uint16_t>((crc << 1u) ^ 0x1021u)
                : static_cast<uint16_t>(crc << 1u);
        }
    }
    return crc;
}

bool decodeLegacyFields(const uint8_t* bytes, PerformanceKeyboardConfig& c) {
    c.role = static_cast<GroovePuterPerformance::PerformanceRole>(bytes[0]);
    c.leadMode = static_cast<GroovePuterPerformance::LeadMode>(bytes[1]);
    c.arpPitchSource = static_cast<GroovePuterPerformance::ArpPitchSource>(bytes[2]);
    c.harmonyChangePolicy = static_cast<GroovePuterPerformance::HarmonyChangePolicy>(bytes[3]);
    c.arpDirection = static_cast<PerformanceArpDirection>(bytes[4]);
    c.arpRate = static_cast<PerformanceRate>(bytes[5]);
    c.arpOctaves = bytes[6];
    c.gatePercent = bytes[7];
    c.chordMode = static_cast<PerformanceChordMode>(bytes[8]);
    c.chordInversion = bytes[9];
    c.chordSpread = static_cast<PerformanceSpread>(bytes[10]);
    c.voiceLeading = static_cast<PerformanceVoiceLeading>(bytes[11]);
    c.rootPitchClass = bytes[12];
    c.scale = static_cast<PerformanceScale>(bytes[13]);
    c.velocity = bytes[14];
    c.octaveShift = static_cast<int8_t>(bytes[15]);
    if (bytes[16] > 1u || bytes[17] > 1u) return false;
    c.arpEnabled = bytes[16] != 0;
    c.latchEnabled = bytes[17] != 0;
    return true;
}

bool decodeLegacy(const uint8_t* bytes, PerformanceKeyboardConfig& out) {
    PerformanceKeyboardConfig candidate{};
    if (!decodeLegacyFields(bytes, candidate)) return false;
    if (!validPerformanceKeyboardConfig(candidate)) return false;
    out = candidate;
    return true;
}

}  // namespace

bool PerformanceKeyboardConfigCodec::encode(
    const PerformanceKeyboardConfig& c, uint8_t* out, std::size_t capacity) {
    if (out == nullptr || capacity < kCurrentRecordSize ||
        !validPerformanceKeyboardConfig(c)) return false;
    out[0] = kCurrentVersion;
    const uint8_t fields[18] = {
        static_cast<uint8_t>(c.role), static_cast<uint8_t>(c.leadMode),
        static_cast<uint8_t>(c.arpPitchSource),
        static_cast<uint8_t>(c.harmonyChangePolicy),
        static_cast<uint8_t>(c.arpDirection), static_cast<uint8_t>(c.arpRate),
        c.arpOctaves, c.gatePercent, static_cast<uint8_t>(c.chordMode),
        c.chordInversion, static_cast<uint8_t>(c.chordSpread),
        static_cast<uint8_t>(c.voiceLeading), c.rootPitchClass,
        static_cast<uint8_t>(c.scale), c.velocity,
        static_cast<uint8_t>(c.octaveShift), static_cast<uint8_t>(c.arpEnabled),
        static_cast<uint8_t>(c.latchEnabled)};
    for (std::size_t i = 0; i < sizeof(fields); ++i) out[1u + i] = fields[i];
    out[19] = c.ratchetCount;
    out[20] = c.euclideanLength;
    out[21] = c.euclideanPulses;
    out[22] = c.euclideanRotation;
    out[23] = c.strumMs;
    out[24] = static_cast<uint8_t>(c.strumDirection);
    out[25] = static_cast<uint8_t>(c.voiceMode);
    out[26] = static_cast<uint8_t>(c.target);
    out[27] = static_cast<uint8_t>(c.mutationSeed & 0xffu);
    out[28] = static_cast<uint8_t>((c.mutationSeed >> 8u) & 0xffu);
    out[29] = static_cast<uint8_t>((c.mutationSeed >> 16u) & 0xffu);
    out[30] = static_cast<uint8_t>((c.mutationSeed >> 24u) & 0xffu);
    out[31] = c.mutationSkipPercent;
    out[32] = c.mutationOctaveJumpPercent;
    out[33] = c.mutationDeviatePercent;
    const uint16_t crc = crc16(out, kCurrentRecordSize - 2u);
    out[34] = static_cast<uint8_t>(crc & 0xffu);
    out[35] = static_cast<uint8_t>(crc >> 8u);
    return true;
}

bool PerformanceKeyboardConfigCodec::decode(
    const uint8_t* bytes, std::size_t size, PerformanceKeyboardConfig& out) {
    if (bytes == nullptr) return false;
    if (size == kLegacyRecordSize) return decodeLegacy(bytes, out);
    if (size == kLegacyV1RecordSize && bytes[0] == 1u) {
        const uint16_t stored = static_cast<uint16_t>(bytes[19] |
            (static_cast<uint16_t>(bytes[20]) << 8u));
        if (crc16(bytes, 19u) != stored) return false;
        return decodeLegacy(bytes + 1u, out);
    }
    if (size != kCurrentRecordSize || bytes[0] != kCurrentVersion) return false;
    const uint16_t stored = static_cast<uint16_t>(bytes[34] |
        (static_cast<uint16_t>(bytes[35]) << 8u));
    if (crc16(bytes, kCurrentRecordSize - 2u) != stored) return false;

    PerformanceKeyboardConfig candidate{};
    if (!decodeLegacyFields(bytes + 1, candidate)) return false;
    candidate.ratchetCount = bytes[19];
    candidate.euclideanLength = bytes[20];
    candidate.euclideanPulses = bytes[21];
    candidate.euclideanRotation = bytes[22];
    candidate.strumMs = bytes[23];
    candidate.strumDirection = static_cast<PerformanceStrumDirection>(bytes[24]);
    candidate.voiceMode = static_cast<PerformanceVoiceMode>(bytes[25]);
    candidate.target = static_cast<MusicalEventTarget>(bytes[26]);
    candidate.mutationSeed = static_cast<uint32_t>(bytes[27]) |
        (static_cast<uint32_t>(bytes[28]) << 8u) |
        (static_cast<uint32_t>(bytes[29]) << 16u) |
        (static_cast<uint32_t>(bytes[30]) << 24u);
    candidate.mutationSkipPercent = bytes[31];
    candidate.mutationOctaveJumpPercent = bytes[32];
    candidate.mutationDeviatePercent = bytes[33];
    if (!validPerformanceKeyboardConfig(candidate)) return false;
    out = candidate;
    return true;
}
