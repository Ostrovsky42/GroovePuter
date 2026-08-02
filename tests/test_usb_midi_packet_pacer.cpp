#include <cassert>
#include <cstdint>

#include "src/midi/usb_midi_packet_pacer.h"

using GroovePuterMidi::UsbMidiPacketPacer;

int main() {
    UsbMidiPacketPacer pacer(1000);
    assert(pacer.waitMicros(100) == 0);

    pacer.recordAttempt(100);
    assert(pacer.waitMicros(100) == 1000);
    assert(pacer.waitMicros(600) == 500);
    assert(pacer.waitMicros(1100) == 0);

    pacer.recordAttempt(UINT32_MAX - 400);
    assert(pacer.waitMicros(UINT32_MAX - 100) == 700);
    assert(pacer.waitMicros(300) == 299);
    assert(pacer.waitMicros(599) == 0);

    pacer.reset();
    assert(pacer.waitMicros(0) == 0);
    return 0;
}
