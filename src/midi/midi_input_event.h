#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_EVENT_H
#define GROOVEPUTER_MIDI_INPUT_EVENT_H

#include <cstdint>

namespace GroovePuterMidi {

enum class InputSource : uint8_t {
    Qwerty,
    Usb,
    Uart,
};

enum class InputKind : uint8_t {
    NoteOn,
    NoteOff,
    Sustain,
    AllNotesOff,
    AllSoundOff,
    Mod,        // CC1 (mod button / wheel); value = the controller value
    PitchBend,  // 14-bit value is preserved in MidiInputEvent::pitchBend14
    ControlChange, // raw CC for explicitly configured semantic performance bindings
};

enum class ResetReason : uint8_t {
    Detach,
    InputDisabled,
    FilterChanged,
    Overflow,
    SourcePanic,
    TargetChanged,
};

struct InputSession {
    InputSource source{InputSource::Qwerty};
    uint32_t generation{0};
};

struct InputKey {
    InputSource source{InputSource::Qwerty};
    uint32_t generation{0};
    uint8_t channel{0};
    uint8_t key{0};
    uint8_t cable{0};
};

struct MidiInputEvent {
    InputKey id{};
    InputKind kind{InputKind::NoteOff};
    uint8_t velocity{0};
    uint32_t atMicros{0};
    uint8_t controller{0};
    uint16_t pitchBend14{8192}; // MIDI 1.0 unsigned 14-bit value; 8192 is centre
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_INPUT_EVENT_H
