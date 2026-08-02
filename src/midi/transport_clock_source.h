#pragma once

#include <cstdint>

namespace GroovePuterMidi {

enum class TransportClockSource : uint8_t {
    GroovePuterInternal = 0,
    SeqtrakExternal,
};

inline constexpr TransportClockSource normalizeTransportClockSource(
        uint8_t raw) {
    return raw == static_cast<uint8_t>(TransportClockSource::SeqtrakExternal)
        ? TransportClockSource::SeqtrakExternal
        : TransportClockSource::GroovePuterInternal;
}

inline constexpr const char* transportClockSourceName(
        TransportClockSource source) {
    return source == TransportClockSource::SeqtrakExternal
        ? "SEQ MASTER"
        : "GP MASTER";
}

inline constexpr bool transportClockSourcePublishesOutboundClock(
        TransportClockSource source) {
    return source == TransportClockSource::GroovePuterInternal;
}

}  // namespace GroovePuterMidi
