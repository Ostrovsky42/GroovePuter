#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/midi/usb_midi_channel_voice_parser.h"

namespace {

void testThreeByteChannelVoice() {
    NormalizedMidiInputMessage out{};

    assert(GroovePuterMidi::parseUsbMidiChannelVoice(
        0x09u, 0x92u, 60u, 100u, 3u, 77u, 1234u, out));
    assert(out.type == MidiInputMessageType::NoteOn);
    assert(out.transportId == 3u);
    assert(out.sessionId == 77u);
    assert(out.channel == 2u);
    assert(out.note() == 60u);
    assert(out.velocity() == 100u);
    assert(out.timestampMicros == 1234u);

    assert(GroovePuterMidi::parseUsbMidiChannelVoice(
        0x08u, 0x82u, 60u, 17u, 3u, 77u, 1235u, out));
    assert(out.type == MidiInputMessageType::NoteOff);
    assert(out.velocity() == 17u);

    assert(GroovePuterMidi::parseUsbMidiChannelVoice(
        0x0Au, 0xA4u, 64u, 45u, 3u, 77u, 1236u, out));
    assert(out.type == MidiInputMessageType::PolyPressure);
    assert(out.channel == 4u);
    assert(out.data1 == 64u && out.data2 == 45u);

    assert(GroovePuterMidi::parseUsbMidiChannelVoice(
        0x0Bu, 0xB5u, 64u, 127u, 3u, 77u, 1237u, out));
    assert(out.type == MidiInputMessageType::ControlChange);
    assert(out.controller() == 64u);
    assert(out.controllerValue() == 127u);

    assert(GroovePuterMidi::parseUsbMidiChannelVoice(
        0x0Eu, 0xE6u, 0u, 64u, 3u, 77u, 1238u, out));
    assert(out.type == MidiInputMessageType::PitchBend);
    assert(out.pitchBend14() == 8192u);
}

void testOneDataBytePacketsIgnoreUsbPadding() {
    NormalizedMidiInputMessage out{};

    assert(GroovePuterMidi::parseUsbMidiChannelVoice(
        0x0Cu, 0xC3u, 12u, 0xFFu, 1u, 9u, 10u, out));
    assert(out.type == MidiInputMessageType::ProgramChange);
    assert(out.channel == 3u);
    assert(out.data1 == 12u);
    assert(out.data2 == 0u);

    assert(GroovePuterMidi::parseUsbMidiChannelVoice(
        0x0Du, 0xD7u, 88u, 0xFEu, 1u, 9u, 11u, out));
    assert(out.type == MidiInputMessageType::ChannelPressure);
    assert(out.channel == 7u);
    assert(out.data1 == 88u);
    assert(out.data2 == 0u);
}

void testVelocityZeroCanonicalizesBeforeRouting() {
    NormalizedMidiInputMessage out{};
    assert(GroovePuterMidi::parseUsbMidiChannelVoice(
        0x09u, 0x91u, 67u, 0u, 1u, 2u, 12u, out));
    assert(out.type == MidiInputMessageType::NoteOff);
    assert(out.note() == 67u);
    assert(out.velocity() == 0u);
}

void testCableNumberDoesNotAffectCin() {
    NormalizedMidiInputMessage out{};
    assert(GroovePuterMidi::parseUsbMidiChannelVoice(
        0x79u, 0x90u, 60u, 90u, 2u, 3u, 13u, out));
    assert(out.type == MidiInputMessageType::NoteOn);
    assert(out.channel == 0u);
}

void testRejectsRealtimeSystemAndMalformedFraming() {
    NormalizedMidiInputMessage out{};
    out.timestampMicros = 0xDEADBEEFu;

    // Existing realtime transport parser owns CIN 0xF / status 0xF8 etc.
    assert(!GroovePuterMidi::parseUsbMidiChannelVoice(
        0x0Fu, 0xF8u, 0u, 0u, 1u, 1u, 20u, out));
    assert(out.timestampMicros == 0xDEADBEEFu);

    // SysEx/System Common CIN values stay outside this parser.
    assert(!GroovePuterMidi::parseUsbMidiChannelVoice(
        0x04u, 0xF0u, 1u, 2u, 1u, 1u, 21u, out));
    assert(!GroovePuterMidi::parseUsbMidiChannelVoice(
        0x02u, 0xF1u, 1u, 0u, 1u, 1u, 22u, out));

    // CIN/status class must agree.
    assert(!GroovePuterMidi::parseUsbMidiChannelVoice(
        0x09u, 0x80u, 60u, 0u, 1u, 1u, 23u, out));
    assert(!GroovePuterMidi::parseUsbMidiChannelVoice(
        0x0Bu, 0x90u, 60u, 100u, 1u, 1u, 24u, out));

    // Data bytes must remain 7-bit for three-byte messages.
    assert(!GroovePuterMidi::parseUsbMidiChannelVoice(
        0x09u, 0x90u, 0x80u, 100u, 1u, 1u, 25u, out));
    assert(!GroovePuterMidi::parseUsbMidiChannelVoice(
        0x09u, 0x90u, 60u, 0x80u, 1u, 1u, 26u, out));

    assert(!GroovePuterMidi::parseUsbMidiChannelVoice(
        0x09u, 0x90u, 60u, 100u,
        kInvalidMidiInputTransportId, 1u, 27u, out));
    assert(!GroovePuterMidi::parseUsbMidiChannelVoice(
        0x09u, 0x90u, 60u, 100u,
        1u, kInvalidMidiInputSessionId, 28u, out));
}

}  // namespace

int main() {
    testThreeByteChannelVoice();
    testOneDataBytePacketsIgnoreUsbPadding();
    testVelocityZeroCanonicalizesBeforeRouting();
    testCableNumberDoesNotAffectCin();
    testRejectsRealtimeSystemAndMalformedFraming();
    std::cout << "0.9.10 R3b1 USB MIDI channel-voice framing: PASS\n";
    return 0;
}
