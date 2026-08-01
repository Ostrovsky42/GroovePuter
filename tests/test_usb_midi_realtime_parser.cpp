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
    return 0;
}
