#pragma once
#ifndef GROOVEPUTER_USB_MIDI_CHANNEL_VOICE_PARSER_H
#define GROOVEPUTER_USB_MIDI_CHANNEL_VOICE_PARSER_H

#include <cstdint>

#include "src/input/midi_input_message.h"

namespace GroovePuterMidi {

// USB-MIDI 1.0 Event Packet adapter for channel-voice traffic. The parser is
// pure and platform-independent: it has no TinyUSB dependency and accepts the
// four packet bytes explicitly so host tests can prove framing before the sole
// Cardputer TinyUSB reader is wired to the R2 queue.
//
// Returns false for System Common, SysEx, System Realtime, malformed CIN/status
// combinations, invalid MIDI data bytes, or invalid transport/session IDs.
inline bool parseUsbMidiChannelVoice(
    uint8_t header,
    uint8_t byte1,
    uint8_t byte2,
    uint8_t byte3,
    MidiInputTransportId transportId,
    MidiInputSessionId sessionId,
    uint32_t timestampMicros,
    NormalizedMidiInputMessage& out) {
    const uint8_t cin = header & 0x0Fu;
    const uint8_t statusClass = byte1 & 0xF0u;

    // USB-MIDI CIN 0x8..0xE maps exactly to MIDI channel-voice status classes
    // 0x80..0xE0. Requiring both closes malformed-packet ambiguity instead of
    // trusting byte1 alone.
    if (cin < 0x08u || cin > 0x0Eu) return false;
    if (statusClass != static_cast<uint8_t>(cin << 4u)) return false;

    // Program Change and Channel Pressure contain only one MIDI data byte.
    // byte3 is USB packet padding and must not be interpreted or validated.
    const bool oneDataByte = cin == 0x0Cu || cin == 0x0Du;
    return NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        byte1,
        byte2,
        oneDataByte ? 0u : byte3,
        transportId,
        sessionId,
        timestampMicros,
        out);
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_USB_MIDI_CHANNEL_VOICE_PARSER_H
