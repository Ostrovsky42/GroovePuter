#pragma once

#include <cstdint>

enum class MusicalEventType : uint8_t {
    NoteOn,
    NoteOff,
    AllNotesOff,
};

enum class MusicalEventSource : uint8_t {
    PerformanceKeyboard,
    PatternPlayer,
    Arpeggiator,
    MidiInput,
};

enum class MusicalEventTarget : uint8_t {
    SynthA,
    SynthB,
};

// channel is a zero-based logical channel. Internal synth outputs ignore it;
// a future MIDI sink may map 0..15 to MIDI channels 1..16.
struct MusicalEvent {
    MusicalEventType type{MusicalEventType::AllNotesOff};
    MusicalEventSource source{MusicalEventSource::PerformanceKeyboard};
    MusicalEventTarget target{MusicalEventTarget::SynthA};
    uint8_t channel{0};
    uint8_t note{0};
    uint8_t velocity{0};
};
