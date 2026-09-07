#include <cassert>
#include <cstdint>

#include "src/midi/midi_input_parser.h"

using namespace GroovePuterMidi;

namespace {

void usbNormalizesVelocityZeroIntoNoteOff() {
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, 7});

    const uint8_t on[4] = {0x09, 0x90, 60, 100};
    const uint8_t off[4] = {0x09, 0x90, 60, 0};
    const ParseResult acceptedOn = parser.usbPacket(on, 10);
    const ParseResult acceptedOff = parser.usbPacket(off, 20);

    assert(acceptedOn.hasInput);
    assert(acceptedOn.input.kind == InputKind::NoteOn);
    assert(acceptedOn.input.id.source == InputSource::Usb);
    assert(acceptedOn.input.id.generation == 7);
    assert(acceptedOn.input.id.channel == 0);
    assert(acceptedOn.input.id.key == 60);
    assert(acceptedOn.input.velocity == 100);
    assert(acceptedOn.input.atMicros == 10);

    assert(acceptedOff.hasInput);
    assert(acceptedOff.input.kind == InputKind::NoteOff);
    assert(acceptedOff.input.velocity == 0);
}

void usbRejectsNonzeroCableAndMalformedPacket() {
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, 3});
    const uint8_t cableOne[4] = {0x19, 0x90, 60, 100};
    const uint8_t malformed[4] = {0x09, 0x80, 60, 64};

    assert(!parser.usbPacket(cableOne, 1).hasInput);
    assert(!parser.usbPacket(malformed, 2).hasInput);
}

void uartKeepsRunningStatusAcrossRealtime() {
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Uart, 11});

    assert(!parser.uartByte(0x90, 1).hasInput);
    assert(!parser.uartByte(60, 2).hasInput);
    const ParseResult clock = parser.uartByte(0xf8, 3);
    assert(clock.hasRealtime && clock.realtimeStatus == 0xf8);
    const ParseResult on = parser.uartByte(100, 4);
    assert(on.hasInput && on.input.kind == InputKind::NoteOn);
    const ParseResult nextOn = parser.uartByte(61, 5);
    assert(!nextOn.hasInput);
    const ParseResult completedNextOn = parser.uartByte(0, 6);
    assert(completedNextOn.hasInput && completedNextOn.input.kind == InputKind::NoteOff);
}

void uartSkipsSysExButPreservesRealtime() {
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Uart, 11});
    const uint8_t stream[] = {0xf0, 1, 0xf8, 2, 0xf7};
    bool receivedInput = false;
    bool receivedClock = false;
    for (uint8_t byte : stream) {
        const ParseResult result = parser.uartByte(byte, byte);
        receivedInput |= result.hasInput;
        receivedClock |= result.hasRealtime && result.realtimeStatus == 0xf8;
    }
    assert(!receivedInput);
    assert(receivedClock);
}

}  // namespace

int main() {
    usbNormalizesVelocityZeroIntoNoteOff();
    usbRejectsNonzeroCableAndMalformedPacket();
    uartKeepsRunningStatusAcrossRealtime();
    uartSkipsSysExButPreservesRealtime();
}
