#include <cassert>
#include <cstdint>

#include "src/midi/usb_midi_realtime_parser.h"

using namespace GroovePuterMidi;

int main() {
    ExternalMidiTransportEventType type{};
    assert(parseUsbMidiRealtimeTransport(0x0f, 0xf8, type));
    assert(type == ExternalMidiTransportEventType::Clock);
    assert(parseUsbMidiRealtimeTransport(0x1f, 0xfa, type));
    assert(type == ExternalMidiTransportEventType::Start);
    assert(parseUsbMidiRealtimeTransport(0x0f, 0xfb, type));
    assert(type == ExternalMidiTransportEventType::Continue);
    assert(parseUsbMidiRealtimeTransport(0x0f, 0xfc, type));
    assert(type == ExternalMidiTransportEventType::Stop);
    assert(!parseUsbMidiRealtimeTransport(0x09, 0xf8, type));
    assert(!parseUsbMidiRealtimeTransport(0x0f, 0xfe, type));

    // DIN serial bytes: realtime is recognised byte by byte, everything else
    // (channel status, data, active sensing, SPP, SysEx) is not transport.
    assert(parseMidiRealtimeTransportByte(0xf8, type));
    assert(type == ExternalMidiTransportEventType::Clock);
    assert(parseMidiRealtimeTransportByte(0xfa, type));
    assert(type == ExternalMidiTransportEventType::Start);
    assert(parseMidiRealtimeTransportByte(0xfc, type));
    assert(type == ExternalMidiTransportEventType::Stop);
    const uint8_t notTransport[] = {0x90, 0x3c, 0x7f, 0xfe, 0xf2, 0xf0, 0xf7};
    for (uint8_t byte : notTransport) {
        assert(!parseMidiRealtimeTransportByte(byte, type));
    }
    return 0;
}
