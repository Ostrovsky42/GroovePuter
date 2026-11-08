#pragma once

#include <cstdint>

#include "external_midi_transport_event.h"

namespace GroovePuterMidi {

inline constexpr uint8_t kUsbMidiCinSingleByte = 0x0f;

// One byte of a serial (DIN) MIDI stream. Realtime bytes may appear anywhere,
// even between the data bytes of another message, so no parser state is needed.
inline constexpr bool parseMidiRealtimeTransportByte(
        uint8_t status,
        ExternalMidiTransportEventType& type) {
    switch (status) {
        case 0xf8:
            type = ExternalMidiTransportEventType::Clock;
            return true;
        case 0xfa:
            type = ExternalMidiTransportEventType::Start;
            return true;
        case 0xfb:
            type = ExternalMidiTransportEventType::Continue;
            return true;
        case 0xfc:
            type = ExternalMidiTransportEventType::Stop;
            return true;
        default:
            return false;
    }
}

inline constexpr bool parseUsbMidiRealtimeTransport(
        uint8_t packetHeader,
        uint8_t status,
        ExternalMidiTransportEventType& type) {
    if ((packetHeader & 0x0fu) != kUsbMidiCinSingleByte) return false;
    return parseMidiRealtimeTransportByte(status, type);
}

}  // namespace GroovePuterMidi
