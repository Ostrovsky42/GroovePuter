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
};

struct InputKey {
    InputSource source{InputSource::Qwerty};
    uint32_t generation{0};
    uint8_t channel{0};
    uint8_t key{0};
};

struct MidiInputEvent {
    InputKey id{};
    InputKind kind{InputKind::NoteOff};
    uint8_t velocity{0};
    uint32_t atMicros{0};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_INPUT_EVENT_H
