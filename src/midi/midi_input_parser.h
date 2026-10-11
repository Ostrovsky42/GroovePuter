#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_PARSER_H
#define GROOVEPUTER_MIDI_INPUT_PARSER_H

#include <cstdint>

#include "midi_input_event.h"

namespace GroovePuterMidi {

struct ParseResult {
    bool hasInput{false};
    MidiInputEvent input{};
    bool hasRealtime{false};
    uint8_t realtimeStatus{0};
};

class MidiInputParser {
public:
    void reset(InputSession session) {
        session_ = session;
        runningStatus_ = 0;
        dataCount_ = 0;
        inSysEx_ = false;
    }

    bool setUsbCable(uint8_t cable) {
        if (cable > 15u) return false;
        selectedUsbCable_ = cable;
        return true;
    }
    uint8_t usbCable() const { return selectedUsbCable_; }

    ParseResult usbPacket(const uint8_t (&packet)[4], uint32_t atMicros) const {
        if ((packet[0] >> 4u) != selectedUsbCable_) return {};
        const uint8_t cin = packet[0] & 0x0fu;
        if (cin != 0x08u && cin != 0x09u && cin != 0x0bu && cin != 0x0eu) return {};
        ParseResult result = channelMessage(packet[1], packet[2], packet[3], cin, atMicros);
        if (result.hasInput) result.input.id.cable = selectedUsbCable_;
        return result;
    }

    ParseResult uartByte(uint8_t byte, uint32_t atMicros) {
        if (byte >= 0xf8u) {
            return ParseResult{false, {}, true, byte};
        }
        if (byte & 0x80u) {
            if (byte == 0xf0u) {
                inSysEx_ = true;
            } else if (byte == 0xf7u) {
                inSysEx_ = false;
            } else {
                inSysEx_ = false;
                runningStatus_ = byte < 0xf0u ? byte : 0;
                dataCount_ = 0;
            }
            return {};
        }
        if (inSysEx_ || runningStatus_ == 0) return {};

        data_[dataCount_++] = byte;
        if (dataCount_ < 2) return {};
        const uint8_t status = runningStatus_;
        const uint8_t data1 = data_[0];
        const uint8_t data2 = data_[1];
        dataCount_ = 0;
        return channelMessage(status, data1, data2, status >> 4u, atMicros);
    }

private:
    ParseResult channelMessage(uint8_t status,
                               uint8_t data1,
                               uint8_t data2,
                               uint8_t cin,
                               uint32_t atMicros) const {
        if ((status & 0x80u) == 0 || data1 >= 128 || data2 >= 128) return {};
        const uint8_t message = status & 0xf0u;
        const uint8_t channel = status & 0x0fu;
        if ((message == 0x80u && cin != 0x08u) ||
            (message == 0x90u && cin != 0x09u) ||
            (message == 0xb0u && cin != 0x0bu) ||
            (message == 0xe0u && cin != 0x0eu)) {
            return {};
        }

        InputKind kind{};
        if (message == 0x80u) {
            kind = InputKind::NoteOff;
        } else if (message == 0x90u) {
            kind = data2 == 0 ? InputKind::NoteOff : InputKind::NoteOn;
        } else if (message == 0xb0u && data1 == 64u) {
            kind = InputKind::Sustain;
        } else if (message == 0xb0u && data1 == 123u) {
            kind = InputKind::AllNotesOff;
        } else if (message == 0xb0u && data1 == 120u) {
            kind = InputKind::AllSoundOff;
        } else if (message == 0xb0u) {
            // Generic CC1/CC94 remain control data. Destructive legacy Mod-button
            // gestures require an explicit mapping and must not be inferred from a CC number.
            kind = InputKind::ControlChange;
        } else if (message == 0xe0u) {
            kind = InputKind::PitchBend;  // data1 = LSB (ignored), data2 = MSB
        } else {
            return {};
        }
        MidiInputEvent input{InputKey{session_.source, session_.generation,
                                      channel, static_cast<uint8_t>(
                                          (message == 0xb0u || message == 0xe0u) ? 0u : data1)},
                             kind, data2, atMicros,
                             static_cast<uint8_t>(message == 0xb0u ? data1 : 0u)};
        if (message == 0xe0u) {
            input.pitchBend14 = static_cast<uint16_t>(
                (static_cast<uint16_t>(data2) << 7u) | data1);
        }
        return ParseResult{true, input, false, 0};
    }

    InputSession session_{};
    uint8_t runningStatus_{0};
    uint8_t data_[2]{};
    uint8_t dataCount_{0};
    bool inSysEx_{false};
    uint8_t selectedUsbCable_{0};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_INPUT_PARSER_H
