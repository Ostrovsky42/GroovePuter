#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_MESSAGE_H
#define GROOVEPUTER_MIDI_INPUT_MESSAGE_H

#include <cstdint>
#include <type_traits>

using MidiInputTransportId = uint8_t;
using MidiInputSessionId = uint32_t;

constexpr MidiInputTransportId kInvalidMidiInputTransportId = 0u;
constexpr MidiInputSessionId kInvalidMidiInputSessionId = 0u;

enum class MidiInputMessageType : uint8_t {
    NoteOff,
    NoteOn,
    ControlChange,
    PitchBend,
};

// Transport-independent channel message published by a physical MIDI adapter.
//
// transportId identifies the adapter instance. sessionId identifies one
// connection/lifetime of that adapter so a later NoteOff cannot be confused
// with a note from an earlier connection after reconnect.
//
// channel is always zero-based (0..15). data1/value semantics are determined by
// type:
//   NoteOn/NoteOff  data1 = note (0..127), value = velocity (0..127)
//   ControlChange   data1 = controller (0..127), value = controller value (0..127)
//   PitchBend       data1 = 0, value = normalized unsigned bend (0..16383)
//
// R2 deliberately does not contain a resolved target, logical drum lane,
// persistence state or transport-specific packet details. Those belong to
// later routing/ownership or adapter checkpoints.
struct NormalizedMidiInputMessage {
    MidiInputSessionId sessionId{kInvalidMidiInputSessionId};
    MidiInputTransportId transportId{kInvalidMidiInputTransportId};
    MidiInputMessageType type{MidiInputMessageType::NoteOff};
    uint8_t channel{0};
    uint8_t data1{0};
    uint16_t value{0};
};

static_assert(std::is_standard_layout<NormalizedMidiInputMessage>::value,
              "MIDI input message must remain standard-layout");
static_assert(std::is_trivially_copyable<NormalizedMidiInputMessage>::value,
              "MIDI input message must remain trivially copyable");
static_assert(sizeof(NormalizedMidiInputMessage) == 12,
              "R2 memory contract changed: re-audit ingress DRAM");

#endif  // GROOVEPUTER_MIDI_INPUT_MESSAGE_H
