#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_MESSAGE_H
#define GROOVEPUTER_MIDI_INPUT_MESSAGE_H

#include <cstdint>
#include <type_traits>

// Opaque transport/session identity. The core deliberately does not encode
// physical transport kinds (USB/UART/BLE/etc.) in this type. An adapter owns a
// non-zero transportId and advances/replaces its non-zero sessionId whenever a
// physical connection/session is replaced.
using MidiInputTransportId = uint8_t;
using MidiInputSessionId = uint16_t;

static constexpr MidiInputTransportId kInvalidMidiInputTransportId = 0u;
static constexpr MidiInputSessionId kInvalidMidiInputSessionId = 0u;

// MIDI 1.0 channel-voice messages only. System Common/System Realtime/SysEx
// remain outside this domain; Clock/Start/Continue/Stop already have a separate
// transport owner in GroovePuter.
enum class MidiInputMessageType : uint8_t {
    NoteOff = 0x80,
    NoteOn = 0x90,
    PolyPressure = 0xA0,
    ControlChange = 0xB0,
    ProgramChange = 0xC0,
    ChannelPressure = 0xD0,
    PitchBend = 0xE0,
};

// Transport-independent POD passed from a physical MIDI adapter into the
// bounded MIDI-input ingress queue. timestampMicros is an adapter-supplied
// monotonic 32-bit microsecond timestamp; unsigned subtraction provides
// wrap-safe short-interval deltas. It is carried now so later sequencer capture
// does not need to infer arrival time from delayed consumer dispatch.
struct NormalizedMidiInputMessage {
    uint32_t timestampMicros{0};
    MidiInputSessionId sessionId{kInvalidMidiInputSessionId};
    MidiInputTransportId transportId{kInvalidMidiInputTransportId};
    MidiInputMessageType type{MidiInputMessageType::NoteOff};
    uint8_t channel{0};
    uint8_t data1{0};
    uint8_t data2{0};

    static bool fromMidi1ChannelVoice(uint8_t status,
                                      uint8_t data1,
                                      uint8_t data2,
                                      MidiInputTransportId transportId,
                                      MidiInputSessionId sessionId,
                                      uint32_t timestampMicros,
                                      NormalizedMidiInputMessage& out) {
        if (transportId == kInvalidMidiInputTransportId ||
            sessionId == kInvalidMidiInputSessionId ||
            status < 0x80u || status >= 0xF0u ||
            (data1 & 0x80u) != 0u) {
            return false;
        }

        const uint8_t statusClass = status & 0xF0u;
        const bool oneDataByte = statusClass == 0xC0u || statusClass == 0xD0u;
        if (!oneDataByte && (data2 & 0x80u) != 0u) {
            return false;
        }

        NormalizedMidiInputMessage message{};
        message.timestampMicros = timestampMicros;
        message.sessionId = sessionId;
        message.transportId = transportId;
        message.channel = status & 0x0Fu;
        message.data1 = data1;
        message.data2 = oneDataByte ? 0u : data2;

        switch (statusClass) {
            case 0x80u:
                message.type = MidiInputMessageType::NoteOff;
                break;
            case 0x90u:
                // MIDI convention: NoteOn with velocity zero is NoteOff. Make
                // that canonical at normalization so routing/ownership never
                // needs two NoteOff interpretations.
                message.type = data2 == 0u
                    ? MidiInputMessageType::NoteOff
                    : MidiInputMessageType::NoteOn;
                break;
            case 0xA0u:
                message.type = MidiInputMessageType::PolyPressure;
                break;
            case 0xB0u:
                message.type = MidiInputMessageType::ControlChange;
                break;
            case 0xC0u:
                message.type = MidiInputMessageType::ProgramChange;
                break;
            case 0xD0u:
                message.type = MidiInputMessageType::ChannelPressure;
                break;
            case 0xE0u:
                message.type = MidiInputMessageType::PitchBend;
                break;
            default:
                return false;
        }

        out = message;
        return true;
    }

    bool isValid() const {
        if (transportId == kInvalidMidiInputTransportId ||
            sessionId == kInvalidMidiInputSessionId ||
            channel > 15u || data1 > 127u) {
            return false;
        }

        switch (type) {
            case MidiInputMessageType::NoteOff:
            case MidiInputMessageType::PolyPressure:
            case MidiInputMessageType::ControlChange:
            case MidiInputMessageType::PitchBend:
                return data2 <= 127u;
            case MidiInputMessageType::NoteOn:
                // Zero-velocity NoteOn is deliberately not a normalized state.
                return data2 > 0u && data2 <= 127u;
            case MidiInputMessageType::ProgramChange:
            case MidiInputMessageType::ChannelPressure:
                return data2 == 0u;
        }
        return false;
    }

    uint8_t note() const { return data1; }
    uint8_t velocity() const { return data2; }
    uint8_t controller() const { return data1; }
    uint8_t controllerValue() const { return data2; }

    uint16_t pitchBend14() const {
        return static_cast<uint16_t>(data1) |
               (static_cast<uint16_t>(data2) << 7u);
    }
};

static_assert(sizeof(NormalizedMidiInputMessage) == 12u,
              "Normalized MIDI input message must remain a compact 12-byte POD");
static_assert(std::is_trivially_copyable<NormalizedMidiInputMessage>::value,
              "Normalized MIDI input message must remain trivially copyable");

#endif  // GROOVEPUTER_MIDI_INPUT_MESSAGE_H
